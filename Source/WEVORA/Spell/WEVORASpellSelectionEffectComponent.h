#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASpellSelectionEffectComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UPointLightComponent;
class UWEVORASpellWeavingComponent;

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellSelectionEffect
{
	GENERATED_BODY()
	/** Optional looping system. User parameters are listed in Spell/README.md. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	TObjectPtr<UNiagaraSystem> System = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	FLinearColor Color = FLinearColor::White;
};

/** Cosmetic selection preview only; does not implement projectiles or damage. */
UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORASpellSelectionEffectComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UWEVORASpellSelectionEffectComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Effects")
	TMap<EWEVORASpellElement, FWEVORASpellSelectionEffect> ElementEffects;
	/** A short preview when Q is pressed outside weaving. Zero disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Effects", meta=(ClampMin="0.0"))
	float IdlePreviewDuration = 0.35f;
	/** Asset-free placeholder glow, also usable alongside Niagara. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Effects")
	bool bEnablePreviewLight = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Effects", meta=(ClampMin="0.0"))
	float PreviewLightIntensity = 5000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Effects", meta=(ClampMin="0.0"))
	float PreviewLightRadius = 180.0f;
	/** Refresh after changing profiles/settings at runtime. Does not start an idle preview. */
	UFUNCTION(BlueprintCallable, Category="Spell|Effects")
	void RefreshEffect();
	UFUNCTION(BlueprintPure, Category="Spell|Effects")
	bool IsPreviewActive() const { return bPreviewActive; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UWEVORASpellWeavingComponent> Weaving;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Niagara;
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> PreviewLight;
	FTimerHandle IdlePreviewTimer;
	EWEVORASpellElement LastElement = EWEVORASpellElement::Fire;
	bool bPreviewActive = false;
	UFUNCTION()
	void HandlePresentationChanged(EWEVORAWeavingState State, const FWEVORASpellContext& Context);
	void StopPreview();
	void ApplyEffect(EWEVORAWeavingState State, const FWEVORASpellContext& Context);
};
