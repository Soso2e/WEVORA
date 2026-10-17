#pragma once
#include "CoreMinimal.h"
#include "AI/WEVORAEnemy.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORAPlaygroundTarget.generated.h"

/** Reuses enemy health, reactions, burning and optional ranged AI. No new damage path. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORAPlaygroundTarget : public AWEVORAEnemy
{
	GENERATED_BODY()
public:
	AWEVORAPlaygroundTarget();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground")
	FString TargetName = TEXT("Stationary");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground")
	bool bCombatAI = false;
	/** Local path axis, normalized at runtime. Zero amplitude produces a stationary target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Motion")
	FVector MotionAxis = FVector::RightVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Motion", meta=(ClampMin="0", Units="cm"))
	float MotionAmplitude = 0;
	/** Maximum sine-path speed, not angular speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground|Motion", meta=(ClampMin="0", Units="cm/s"))
	float MotionSpeed = 450;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playground", meta=(ClampMin="0.1", Units="s"))
	float RespawnDelay = 3;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Playground")
	FVector MeasuredVelocity = FVector::ZeroVector;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Playground")
	float LastHitTime = -100;
	UFUNCTION(BlueprintCallable, Category="Playground")
	void ResetTarget();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	FTransform InitialTransform;
	float MotionTime = 0;
	FTimerHandle RespawnTimer;
	UFUNCTION()
	void OnDefeated();
	UFUNCTION()
	void OnReceivedSpell(const FWEVORASpellData& Spell);
};
