#include "Solo/WEVORARunUpgrades.h"
#include "WEVORACharacter.h"
#include "AI/WEVORAHealthComponent.h"
#include "Movement/WEVORAManaComponent.h"
#include "Spell/WEVORASpellCastComponent.h"

void UWEVORARunUpgrades::BeginRun(AWEVORACharacter* Player)
{
	Reset(); Pilot = Player;
	BaseMaxHealth = Player->HealthComponent->MaxHealth;
	BaseRadius = Player->SpellCastComponent->RunRadiusMultiplier;
	BaseSpeed = Player->RunSpeedMultiplier;
	BaseRecovery = Player->ManaComponent->RunRecoveryMultiplier;
}

bool UWEVORARunUpgrades::Apply(const FWEVORARunUpgrade& Upgrade)
{
	AWEVORACharacter* Player = Pilot.Get();
	if (!Player || !Player->HasAuthority() || !Player->HealthComponent->IsAlive() || Upgrade.ID.IsNone()
		|| Stacks.FindRef(Upgrade.ID) >= FMath::Clamp(Upgrade.MaxStacks,1,10)) { return false; }
	const float Values[] = { Upgrade.RadiusBonus, Upgrade.SpeedBonus, Upgrade.RecoveryBonus, Upgrade.MaxHealthBonus, Upgrade.Heal };
	for (float Value : Values) { if (!FMath::IsFinite(Value) || Value < 0) { return false; } }
	++Stacks.FindOrAdd(Upgrade.ID);
	RadiusBonus = FMath::Min(2.0f, RadiusBonus + FMath::Min(1.0f, Upgrade.RadiusBonus));
	SpeedBonus = FMath::Min(1.0f, SpeedBonus + FMath::Min(1.0f, Upgrade.SpeedBonus));
	RecoveryBonus = FMath::Min(2.0f, RecoveryBonus + FMath::Min(1.0f, Upgrade.RecoveryBonus));
	HealthBonus = FMath::Min(200.0f, HealthBonus + FMath::Min(100.0f, Upgrade.MaxHealthBonus));
	Player->SpellCastComponent->RunRadiusMultiplier = BaseRadius * (1 + RadiusBonus);
	Player->RunSpeedMultiplier = BaseSpeed * (1 + SpeedBonus);
	Player->ManaComponent->RunRecoveryMultiplier = BaseRecovery * (1 + RecoveryBonus);
	Player->HealthComponent->SetMaxHealth(BaseMaxHealth + HealthBonus);
	Player->HealthComponent->Heal(FMath::Min(100.0f, Upgrade.Heal));
	return true;
}

void UWEVORARunUpgrades::Reset()
{
	if (AWEVORACharacter* Player = Pilot.Get())
	{
		Player->SpellCastComponent->RunRadiusMultiplier = BaseRadius;
		Player->RunSpeedMultiplier = BaseSpeed;
		Player->ManaComponent->RunRecoveryMultiplier = BaseRecovery;
		Player->HealthComponent->SetMaxHealth(BaseMaxHealth);
	}
	Pilot.Reset(); Stacks.Empty();
	RadiusBonus = SpeedBonus = RecoveryBonus = HealthBonus = 0;
}
