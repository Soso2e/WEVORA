#pragma once

#include "CoreMinimal.h"
#include "Spell/WEVORASpellDelivery.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASpellProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class UWEVORASpellEffectComponent;

/** Shared Fire/Wind runtime. Consumes a snapshot, never reads live weaving input. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORASpellProjectile : public AWEVORASpellDelivery
{
	GENERATED_BODY()
public:
	AWEVORASpellProjectile();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UWEVORASpellEffectComponent> AttackEffect;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPointLightComponent> PreviewLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> Movement;
	virtual void InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch) override;
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	bool bHitConsumed = false;
	FVector PreviousDebugPosition;
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);
};
