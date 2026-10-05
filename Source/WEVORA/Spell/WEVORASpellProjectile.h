#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASpellProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;

/** Shared Fire/Wind runtime. Consumes a snapshot, never reads live weaving input. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORASpellProjectile : public AActor
{
	GENERATED_BODY()
public:
	AWEVORASpellProjectile();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPointLightComponent> PreviewLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Spell")
	FWEVORASpellLaunch Launch;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Debug")
	bool bLogEvents = true;
	/** Call between SpawnActorDeferred and FinishSpawningActor. */
	void InitializeSpell(const FWEVORASpellLaunch& InLaunch);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	bool bHitConsumed = false;
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);
};
