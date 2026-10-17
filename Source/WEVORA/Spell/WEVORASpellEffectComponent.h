#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASpellEffectComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UNiagaraSystem;
class UNiagaraComponent;

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORAAttackEffectProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effect")
	TObjectPtr<UMaterialInterface> Material;
	/** Optional authored replacement; receives User.SpellColor, SpellRadius and SpellMaterial. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effect")
	TObjectPtr<UNiagaraSystem> System;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effect")
	FLinearColor Color = FLinearColor(1, 0.15f, 0.02f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effect", meta=(ClampMin="0.1"))
	float Scale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effect", meta=(ClampMin="0"))
	float AnimationSpeed = 1.0f;
};

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORAImpactEffectSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
	TObjectPtr<UStaticMesh> FragmentMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
	TObjectPtr<UMaterialInterface> FallbackMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin="0", ClampMax="64"))
	int32 FragmentCount = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin="0.05", ClampMax="5"))
	float Lifetime = 0.8f;
	/** Fragment size in cm for a 100cm mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin="0.1"))
	float Size = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin="0"))
	float Speed = 240.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin="0"))
	float Gravity = 650.0f;
};

/** Cosmetic mesh animation, usable on any attack actor. Niagara is an optional replacement. */
UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORASpellEffectComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UWEVORASpellEffectComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack Effect")
	TMap<EWEVORASpellElement, FWEVORAAttackEffectProfile> ElementEffects;
	/** By default the cast snapshot's color overrides the profile color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack Effect")
	bool bUseLaunchColor = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack Effect")
	FWEVORAImpactEffectSettings Impact;
	UFUNCTION(BlueprintCallable, Category="Attack Effect")
	void StartEffect(const FWEVORASpellLaunch& Launch);
	UFUNCTION(BlueprintCallable, Category="Attack Effect")
	void StopEffect();
	UFUNCTION(BlueprintCallable, Category="Attack Effect")
	void SpawnImpact(const FHitResult& Hit);
	/** Complex face material when available; visible mesh fallback for collision proxies. */
	static UMaterialInterface* ResolveHitMaterial(const FHitResult& Hit);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Pieces;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> NiagaraEffect;
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> SegmentMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultMaterial;
	FWEVORAAttackEffectProfile ActiveProfile;
	EWEVORASpellElement ActiveElement = EWEVORASpellElement::Fire;
	float Radius = 16;
	float Age = 0;
	float TrailLength = 100;
	float ImpactPower = 1;
	FLinearColor ImpactColor = FLinearColor::White;
};
