#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASpellDelivery.generated.h"

/** Delivery owns world transport only. Targets own reaction rules. */
UCLASS(Abstract, Blueprintable)
class WEVORA_API AWEVORASpellDelivery : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Spell")
	FWEVORASpellLaunch Launch;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Debug")
	bool bLogEvents = true;
	/** Deferred spawn initialization seam for Projectile / Sweep / Area / Field. */
	UFUNCTION(BlueprintNativeEvent, Category="Spell")
	void InitializeSpell(const FWEVORASpellLaunch& InLaunch);
	virtual void InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch);
	UFUNCTION(BlueprintCallable, Category="Spell")
	bool DeliverToTarget(AActor* Target, const FHitResult& Hit);
};
