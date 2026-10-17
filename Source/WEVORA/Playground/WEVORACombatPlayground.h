#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WEVORACombatPlayground.generated.h"

class AWEVORACharacter;
class AWEVORAPlaygroundTarget;

/** Authored trial settings and repeatable reset. Only exists in the independent playground map. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORACombatPlayground : public AActor
{
	GENERATED_BODY()
public:
	AWEVORACombatPlayground();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Field")
	FTransform PlayerStart = FTransform(FRotator::ZeroRotator, FVector(0, 0, 120));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Field")
	FVector BoundsExtent = FVector(10000, 10000, 6000);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Field")
	float MinimumHeight = -300;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Tuning", meta=(ClampMin="0.1", ClampMax="4"))
	float PlayerSpeedMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Tuning", meta=(ClampMin="1"))
	float TargetHealth = 150;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Tuning", meta=(ClampMin="0"))
	float TargetSpeedMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Tuning", meta=(ClampMin="0.1"))
	float ProjectileSpeedMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Tuning", meta=(ClampMin="0.1"))
	float ProjectileRadiusMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Tuning", meta=(ClampMin="0.1"))
	float DamageMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Tuning", meta=(ClampMin="0.1"))
	float ChargeTime = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Debug")
	bool bDebugOverlay = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Debug")
	bool bShowGuide = true;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Playground")
	int32 ResetCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Playground")
	FString LastResetReason;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Playground")
	TArray<TObjectPtr<AWEVORAPlaygroundTarget>> Targets;
	UFUNCTION(BlueprintCallable, Category="Playground")
	void ResetTrial();
	void ToggleDebug();
	void ToggleGuide();
	void ToggleLightningAssist();
	bool IsOutsideBounds(FVector Position) const;
	static AWEVORACombatPlayground* Find(const UWorld* World);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	TWeakObjectPtr<AWEVORACharacter> Pilot;
	float DefeatedTime = 0;
	TArray<float> BaseTargetSpeeds;
	void ResetWithReason(const FString& Reason);
};
