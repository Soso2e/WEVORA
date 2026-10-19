#include "Solo/WEVORAArenaConfig.h"

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
		Wave.EnemyClass = AWEVORAEnemy::StaticClass();
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
