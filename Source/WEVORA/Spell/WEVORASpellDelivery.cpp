#include "Spell/WEVORASpellDelivery.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "AI/WEVORAHealthComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/HitResult.h"

void AWEVORASpellDelivery::InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch)
{
	Launch = InLaunch;
	BeginDeliveryEvent();
}

bool AWEVORASpellDelivery::DeliverToTarget(AActor* Target, const FHitResult& Hit)
{
	return DeliverToTargetZones(Target, {Hit});
}

bool AWEVORASpellDelivery::DeliverToTargetZones(AActor* Target, const TArray<FHitResult>& Hits)
{
	if (!HasAuthority() || !IsValid(Target) || Target == GetOwner() || Target == GetInstigator()
		|| Hits.IsEmpty() || DeliveredTargets.Contains(Target)) { return false; }
	DeliveredTargets.Add(Target);
	const FHitResult& Hit = Hits[0];
	UWEVORASpellReactionComponent* Receiver = Target->FindComponentByClass<UWEVORASpellReactionComponent>();
	const UWEVORAHealthComponent* Health = Target->FindComponentByClass<UWEVORAHealthComponent>();
	const float Before = Health ? Health->Health : 0;
	const bool bApplied = Receiver && Receiver->ReceiveSpellBatch(Launch.Spell, Hits, GetInstigatorController(), this);
	if (UWEVORASpellCastComponent* Source = GetOwner() ? GetOwner()->FindComponentByClass<UWEVORASpellCastComponent>() : nullptr)
	{
		FWEVORASpellImpact Result;
		Result.Target = Target;
		Result.Location = Hit.ImpactPoint;
		Result.Spell = Launch.Spell;
		Result.DirectDamage = Health ? FMath::Max(0.0f, Before - Health->Health) : 0;
		Result.bApplied = bApplied;
		Source->RecordImpact(Result);
	}
	return bApplied;
}
