#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WEVORAManaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWEVORAManaChanged, float, Mana, float, MaxMana);

/** Flight resource. Recovery is allowed on the ground only, so empty mana cannot pulse hover. */
UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORAManaComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWEVORAManaComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana", meta=(ClampMin="1"))
	float MaxMana = 100.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Mana")
	float Mana = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana", meta=(ClampMin="0"))
	float GroundRecoveryPerSecond = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana", meta=(ClampMin="0"))
	float RecoveryDelay = 0.75f;
	UPROPERTY(BlueprintAssignable, Category="Mana")
	FWEVORAManaChanged OnManaChanged;
	/** Insufficient mana consumes the remainder and returns false. */
	UFUNCTION(BlueprintCallable, Category="Mana")
	bool ConsumeMana(float Amount);
	UFUNCTION(BlueprintCallable, Category="Mana")
	void RestoreMana(float Amount);
	UFUNCTION(BlueprintPure, Category="Mana")
	float GetManaRatio() const;
	void UpdateRecovery(float DeltaSeconds, bool bGrounded);
protected:
	virtual void BeginPlay() override;
private:
	float TimeSinceConsumption = 0.0f;
	void SetMana(float Value);
};
