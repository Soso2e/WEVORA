#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spell/WEVORASpellEffectComponent.h"
#include "WEVORAImpactEffect.generated.h"

class UInstancedStaticMeshComponent;

/** Bounded cosmetic ballistic fragments; no collision, damage or physics bodies. */
UCLASS()
class WEVORA_API AWEVORAImpactEffect : public AActor
{
	GENERATED_BODY()
public:
	AWEVORAImpactEffect();
	void Initialize(const FWEVORAImpactEffectSettings& Settings, UMaterialInterface* Material, FVector Normal);
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Impact")
	TObjectPtr<UInstancedStaticMeshComponent> Fragments;
private:
	FWEVORAImpactEffectSettings Config;
	TArray<FVector> Velocities;
	TArray<FVector> Spins;
	TArray<FVector> Scales;
	float Age = 0;
};
