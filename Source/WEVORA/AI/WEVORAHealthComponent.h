#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WEVORAHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWEVORAHealthChanged, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWEVORADeath);

/** Receives standard Unreal damage; UI and respawn can subscribe independently. */
UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORAHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWEVORAHealthComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin="1"))
	float MaxHealth = 100.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health")
	float Health = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	bool bDestroyOwnerOnDeath = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	bool bShowDamageFeedback = false;
	UPROPERTY(BlueprintAssignable, Category="Health")
	FWEVORAHealthChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable, Category="Health")
	FWEVORADeath OnDeath;
	UFUNCTION(BlueprintPure, Category="Health")
	bool IsAlive() const { return Health > 0.0f; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
};
