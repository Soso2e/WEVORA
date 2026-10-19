#include "Solo/WEVORAArenaConfig.h"

UWEVORAArenaConfig::UWEVORAArenaConfig()
{
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
