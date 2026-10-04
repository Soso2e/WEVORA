#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WEVORAEnemyProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS(Blueprintable)
class WEVORA_API AWEVORAEnemyProjectile : public AActor
{
	GENERATED_BODY()
public:
	AWEVORAEnemyProjectile();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0"))
	float Damage = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="1"))
	float Speed = 1600.0f;
protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);
};
