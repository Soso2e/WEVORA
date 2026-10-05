#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASpellCastComponent.generated.h"

class UWEVORASpellWeavingComponent;
class AWEVORASpellProjectile;

/** Bridges completed composition to runtime behaviour without changing the input state machine. */
UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORASpellCastComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWEVORASpellCastComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	TSubclassOf<AWEVORASpellProjectile> ProjectileClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	TMap<EWEVORASpellElement, FWEVORASpellProjectileParameters> ElementParameters;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="1"))
	float AimDistance = 10000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="0"))
	float SpawnDistance = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Movement", meta=(ClampMin="0", ClampMax="1"))
	float HorizontalVelocityInheritance = 0.4f;
	/** Planar recoil in cm/s; Thrust uses the stronger setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Movement", meta=(ClampMin="0"))
	float RecoilSpeed = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Movement", meta=(ClampMin="0"))
	float ThrustRecoilSpeed = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Debug")
	bool bLogEvents = true;
	/** Single small resolution seam for future energy/modifier/situation rules. */
	bool ResolveSpell(const FWEVORASpellContext& Context, FWEVORASpellProjectileParameters& OutParameters) const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UWEVORASpellWeavingComponent> Weaving;
	UFUNCTION()
	void HandleCast(const FWEVORASpellContext& Context);
};
