#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spell/WEVORASpellEffectComponent.h"
#include "WEVORAImpactEffect.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/** Bounded cosmetic ballistic fragments; no collision, damage or physics bodies. */
UCLASS()
class WEVORA_API AWEVORAImpactEffect : public AActor
{
	GENERATED_BODY()
public:
	AWEVORAImpactEffect();
	void Initialize(const FWEVORAImpactEffectSettings& Settings, UMaterialInterface* Material, FVector Normal);
	void InitializeFlash(UMaterialInterface* Material, FLinearColor Color, float Radius);
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Impact")
	TObjectPtr<UInstancedStaticMeshComponent> Fragments;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Impact")
	TObjectPtr<UStaticMeshComponent> Flash;
private:
	FWEVORAImpactEffectSettings Config;
	TArray<FVector> Velocities;
	TArray<FVector> Spins;
	TArray<FVector> Scales;
	float Age = 0;
	float FlashRadius = 35;
};
