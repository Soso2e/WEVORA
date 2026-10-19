#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WEVORARunUpgrades.generated.h"

class AWEVORACharacter;
UENUM(BlueprintType)
enum class EWEVORAUpgradeFamily : uint8 { Element, Mobility, Survival };

USTRUCT(BlueprintType)
struct FWEVORARunUpgrade
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EWEVORAUpgradeFamily Family = EWEVORAUpgradeFamily::Element;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float RadiusBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float SpeedBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float RecoveryBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="100")) float MaxHealthBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="100")) float Heal = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="10")) int32 MaxStacks = 4;
};

/** Captures runtime baselines once per run, restores them on every terminal path. */
UCLASS()
class WEVORA_API UWEVORARunUpgrades : public UActorComponent
{
	GENERATED_BODY()
public:
	void BeginRun(AWEVORACharacter* Player);
	bool Apply(const FWEVORARunUpgrade& Upgrade);
	void Reset();
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) TMap<FName,int32> Stacks;
private:
	TWeakObjectPtr<AWEVORACharacter> Pilot;
	float BaseMaxHealth = 100, BaseRadius = 1, BaseSpeed = 1, BaseRecovery = 1;
	float RadiusBonus = 0, SpeedBonus = 0, RecoveryBonus = 0, HealthBonus = 0;
};
