#include "Spell/WEVORASpellDelivery.h"
#include "Spell/WEVORASpellReactionComponent.h"

void AWEVORASpellDelivery::InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch)
{
	Launch = InLaunch;
}

bool AWEVORASpellDelivery::DeliverToTarget(AActor* Target, const FHitResult& Hit)
{
	if (!HasAuthority() || !IsValid(Target) || Target == GetOwner() || Target == GetInstigator()) { return false; }
	UWEVORASpellReactionComponent* Receiver = Target->FindComponentByClass<UWEVORASpellReactionComponent>();
	return Receiver && Receiver->ReceiveSpell(Launch.Spell, Hit, GetInstigatorController(), this);
}
