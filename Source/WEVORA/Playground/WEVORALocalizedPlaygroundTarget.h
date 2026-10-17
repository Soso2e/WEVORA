#pragma once
#include "CoreMinimal.h"
#include "Playground/WEVORAPlaygroundTarget.h"
#include "WEVORALocalizedPlaygroundTarget.generated.h"

class UWEVORALocalizedStatusComponent;
class USphereComponent;
class UMaterialInstanceDynamic;

/** Model-free sample only. The status and resolver foundation works on any Actor. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORALocalizedPlaygroundTarget : public AWEVORAPlaygroundTarget
{
	GENERATED_BODY()
public:
	AWEVORALocalizedPlaygroundTarget();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UWEVORALocalizedStatusComponent> LocalStatuses;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TArray<TObjectPtr<USphereComponent>> ZoneColliders;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY()
	TObjectPtr<class UMaterialInterface> ZoneMaterial;
	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMeshComponent>> ZoneVisuals;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> ZoneTints;
	UFUNCTION()
	void RefreshZoneVisuals();
};
