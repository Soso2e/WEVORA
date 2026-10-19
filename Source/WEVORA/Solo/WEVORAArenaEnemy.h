#pragma once
#include "CoreMinimal.h"
#include "AI/WEVORAEnemy.h"
#include "WEVORAArenaEnemy.generated.h"

class UWEVORALocalizedStatusComponent;
class UMaterialInstanceDynamic;

/** Run combatant, sharing Health/AI/Delivery/Zone rules, without practice respawn behavior. */
UCLASS()
class WEVORA_API AWEVORAArenaEnemy : public AWEVORAEnemy
{
	GENERATED_BODY()
public:
	AWEVORAArenaEnemy();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UWEVORALocalizedStatusComponent> LocalStatuses;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBossStyle = false;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DangerMarker;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DangerLine;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<USphereComponent>> ZoneColliders;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY() TMap<FName,TObjectPtr<UStaticMeshComponent>> ZoneVisuals;
	UPROPERTY() TMap<FName,TObjectPtr<UMaterialInstanceDynamic>> Tints;
	UFUNCTION() void RefreshZones();
};
