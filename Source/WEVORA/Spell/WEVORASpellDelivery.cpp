#include "Spell/WEVORASpellDelivery.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "AI/WEVORAHealthComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/HitResult.h"

void AWEVORASpellDelivery::InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch)
{
	Launch = InLaunch;
}

bool AWEVORASpellDelivery::DeliverToTarget(AActor* Target, const FHitResult& Hit)
{
	if (!HasAuthority() || !IsValid(Target) || Target == GetOwner() || Target == GetInstigator()) { return false; }
	UWEVORASpellReactionComponent* Receiver = Target->FindComponentByClass<UWEVORASpellReactionComponent>();
	const UWEVORAHealthComponent* Health = Target->FindComponentByClass<UWEVORAHealthComponent>();
	const float Before = Health ? Health->Health : 0;
	const bool bApplied = Receiver && Receiver->ReceiveSpell(Launch.Spell, Hit, GetInstigatorController(), this);
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
