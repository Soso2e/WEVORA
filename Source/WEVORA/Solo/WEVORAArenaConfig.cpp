#include "Solo/WEVORAArenaConfig.h"
#include "Solo/WEVORAArenaEnemy.h"

UWEVORAArenaConfig::UWEVORAArenaConfig()
{
	FWEVORARunUpgrade Element;
	Element.ID = TEXT("WiderSpells"); Element.Title = FText::FromString(TEXT("Element - Wider spells"));
	Element.Description = FText::FromString(TEXT("+20% projectile and area radius. Fire + Wind reactions always available."));
	Element.RadiusBonus = 0.2f; Upgrades.Add(Element);
	FWEVORARunUpgrade Mobility;
	Mobility.ID = TEXT("Airflow"); Mobility.Family = EWEVORAUpgradeFamily::Mobility;
	Mobility.Title = FText::FromString(TEXT("Mobility - Airflow"));
	Mobility.Description = FText::FromString(TEXT("+15% flight / ground speed, +25% ground mana recovery."));
	Mobility.SpeedBonus = 0.15f; Mobility.RecoveryBonus = 0.25f; Upgrades.Add(Mobility);
	FWEVORARunUpgrade Survival;
	Survival.ID = TEXT("SecondWind"); Survival.Family = EWEVORAUpgradeFamily::Survival;
	Survival.Title = FText::FromString(TEXT("Survival - Second wind"));
	Survival.Description = FText::FromString(TEXT("+30 max HP and heal 40 HP for this run."));
	Survival.MaxHealthBonus = 30; Survival.Heal = 40; Upgrades.Add(Survival);
	SpawnOffsets = { FVector(1800,0,450), FVector(0,2200,700), FVector(-2000,0,450), FVector(0,-2400,900) };
	for (int32 Index = 0; Index < 5; ++Index)
	{
		FWEVORAArenaWave Wave;
		Wave.EnemyClass = AWEVORAArenaEnemy::StaticClass();
		Wave.Count = 4 + Index * 2;
		Wave.Health = 60 + Index * 15;
		Wave.AliveCap = 3 + Index / 2;
		Wave.SpeedMultiplier = 1 + Index * 0.08f;
		Wave.DamageMultiplier = 1 + Index * 0.1f;
		Wave.bBoss = Index == 4;
		if (Wave.bBoss) { Wave.Count = 1; Wave.AliveCap = 1; Wave.Health = 650; }
		Waves.Add(Wave);
	}
}

FString UWEVORAArenaConfig::Validate() const
{
	if (Waves.IsEmpty() || Waves.Num()>32 || !Waves.Last().bBoss) { return TEXT("Trial needs 1-32 waves ending in a boss"); }
	if (SpawnOffsets.IsEmpty() || !FMath::IsFinite(PreparationSeconds) || PreparationSeconds<0 || PreparationSeconds>60
		|| !FMath::IsFinite(WaveTimeout) || WaveTimeout<10 || BoundsExtent.ContainsNaN()
		|| BoundsExtent.GetMin()<500 || !FMath::IsFinite(MinimumHeight) || MinimumHeight>=BoundsExtent.Z)
	{
		return TEXT("Invalid arena bounds, preparation, timeout or spawn positions");
	}
	for (const FVector& Offset:SpawnOffsets)
	{
		if (Offset.ContainsNaN() || FMath::Abs(Offset.X)>BoundsExtent.X-200 || FMath::Abs(Offset.Y)>BoundsExtent.Y-200
			|| Offset.Z>BoundsExtent.Z-200 || Offset.Z<MinimumHeight+200) { return TEXT("Spawn position outside safe arena bounds"); }
	}
	for (const FWEVORAArenaWave& Wave:Waves)
	{
		if (!Wave.EnemyClass || Wave.EnemyClass->HasAnyClassFlags(CLASS_Abstract) || Wave.Count<1 || Wave.Count>100 || Wave.AliveCap<1 || Wave.AliveCap>16
			|| !FMath::IsFinite(Wave.SpawnInterval) || Wave.SpawnInterval<0.1f || !FMath::IsFinite(Wave.Health) || Wave.Health<1
			|| !FMath::IsFinite(Wave.SpeedMultiplier) || Wave.SpeedMultiplier<0.1f || Wave.SpeedMultiplier>4
			|| !FMath::IsFinite(Wave.DamageMultiplier) || Wave.DamageMultiplier<0 || Wave.DamageMultiplier>10
			|| !FMath::IsFinite(Wave.BossWindup) || Wave.BossWindup<0.1f || !FMath::IsFinite(Wave.BossRecovery) || Wave.BossRecovery<0.1f)
		{
			return TEXT("Invalid wave class, count, cap, cadence or combat values");
		}
	}
	if (Trial.ID.IsNone() || !UWEVORASoloProgression::IsSupported(Trial.Reward)) { return TEXT("Trial reward is not implemented"); }
	TSet<FName> IDs; int32 AvailableSelections=0;
	for (const FWEVORARunUpgrade& Upgrade:Upgrades)
	{
		if (Upgrade.ID.IsNone() || IDs.Contains(Upgrade.ID) || Upgrade.MaxStacks<1 || Upgrade.MaxStacks>10
			|| static_cast<uint8>(Upgrade.Family)>static_cast<uint8>(EWEVORAUpgradeFamily::Survival)) { return TEXT("Invalid upgrade ID, family or stack cap"); }
		const float Values[]={Upgrade.RadiusBonus,Upgrade.SpeedBonus,Upgrade.RecoveryBonus,Upgrade.MaxHealthBonus,Upgrade.Heal};
		for (float Value:Values) { if (!FMath::IsFinite(Value) || Value<0) { return TEXT("Invalid upgrade values"); } }
		IDs.Add(Upgrade.ID); AvailableSelections+=Upgrade.MaxStacks;
	}
	if (AvailableSelections<Waves.Num()-1) { return TEXT("Not enough upgrade stacks for this trial"); }
	return FString();
}
