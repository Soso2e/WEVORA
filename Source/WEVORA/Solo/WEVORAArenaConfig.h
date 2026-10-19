#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/WEVORAEnemy.h"
#include "WEVORAArenaConfig.generated.h"

UENUM(BlueprintType)
enum class EWEVORARunState : uint8 { Idle, Preparing, WaveActive, UpgradeChoice, BossFight, Victory, Defeat };

USTRUCT(BlueprintType)
struct FWEVORAArenaWave
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AWEVORAEnemy> EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="100")) int32 Count = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="16")) int32 AliveCap = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float SpawnInterval = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float Health = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float SpeedMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float DamageMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBoss = false;
};

/** Prototype pacing, not a fixed product wave count or duration. */
UCLASS(BlueprintType)
class WEVORA_API UWEVORAArenaConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UWEVORAArenaConfig();
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FWEVORAArenaWave> Waves;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> SpawnOffsets;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float PreparationSeconds = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="10")) float WaveTimeout = 300;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BoundsExtent = FVector(11000,11000,6000);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinimumHeight = -400;
};
