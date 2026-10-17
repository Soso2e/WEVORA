#pragma once
#include "CoreMinimal.h"
#include "Spell/WEVORASpellDelivery.h"
#include "WEVORASpellVolume.generated.h"

class UWEVORASpellEffectComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/** A reusable LOS-tested volume: single directional Sweep/Area or timed Field pulses. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORASpellVolume : public AWEVORASpellDelivery
{
	GENERATED_BODY()
public:
	AWEVORASpellVolume();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UWEVORASpellEffectComponent> AttackEffect;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Outline;
	virtual void InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	/** Timer entry point; target-level reaction is still the only damage authority. */
	void Pulse();
private:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> OutlineMaterial;
	FTimerHandle PulseTimer;
};
