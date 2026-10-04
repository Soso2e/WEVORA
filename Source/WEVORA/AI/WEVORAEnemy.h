#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "WEVORAEnemy.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UFloatingPawnMovement;
class UWEVORAHealthComponent;
class AWEVORAEnemyProjectile;

/** Floating ranged enemy. Direct swept steering, without a NavMesh dependency. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORAEnemy : public APawn
{
	GENERATED_BODY()
public:
	AWEVORAEnemy();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Muzzle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UFloatingPawnMovement> Movement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UWEVORAHealthComponent> HealthComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0", Units="cm"))
	float DetectionRange = 3000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0", Units="cm"))
	float AttackRange = 1500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0", Units="cm"))
	float PreferredDistance = 650.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0"))
	float MoveSpeed = 450.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.1", Units="s"))
	float AttackInterval = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0", Units="s"))
	float WindupTime = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0"))
	float AttackDamage = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="1"))
	float ProjectileSpeed = 1600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	TSubclassOf<AWEVORAEnemyProjectile> ProjectileClass;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AI")
	TObjectPtr<APawn> Target;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Attack")
	bool bWindingUp = false;
	/** Hook for custom sound / animation / Niagara, alongside the placeholder muzzle cue. */
	UFUNCTION(BlueprintImplementableEvent, Category="Attack")
	void OnAttackStarted();
	UFUNCTION(BlueprintImplementableEvent, Category="Attack")
	void OnAttackFired();
private:
	float NextAttackTime = 0.0f;
	float FireTime = 0.0f;
	bool IsValidTarget(const APawn* Candidate) const;
	bool CanSeeTarget() const;
	void SetWindupVisual(bool bActive);
	void Fire();
};
