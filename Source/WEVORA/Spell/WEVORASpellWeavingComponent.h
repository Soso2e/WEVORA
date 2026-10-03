#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WEVORAGestureRecognizer.h"
#include "WEVORASpellWeavingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWEVORAOnSpellCast, const FWEVORASpellContext&, Context);

UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORASpellWeavingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWEVORASpellWeavingComponent();
	UPROPERTY(BlueprintAssignable, Category="Spell")
	FWEVORAOnSpellCast OnSpellCast;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Recognition")
	FWEVORAGestureThresholds Thresholds;
	/** Template mouse Look commonly negates MouseY. Set false for positive-up mappings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Recognition")
	bool bInvertLookY = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Debug")
	bool bShowDebug = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Debug")
	bool bLogEvents = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spell")
	EWEVORAWeavingState State = EWEVORAWeavingState::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spell")
	FWEVORASpellContext Context;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spell")
	FWEVORASpellContext LastCastContext;
	UFUNCTION(BlueprintCallable, Category="Spell")
	void BeginWeave();
	UFUNCTION(BlueprintCallable, Category="Spell")
	void ReleaseWeave();
	UFUNCTION(BlueprintCallable, Category="Spell")
	void CycleElement();
	UFUNCTION(BlueprintCallable, Category="Spell")
	void BeginShape();
	UFUNCTION(BlueprintCallable, Category="Spell")
	void EndShape();
	UFUNCTION(BlueprintCallable, Category="Spell")
	void RecordLook(FVector2D LookDelta);
	UFUNCTION(BlueprintCallable, Category="Spell")
	void CancelWeave();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	FWEVORAGestureRecognizer Recorder;
	double ShapeStartTime = 0.0;
	bool bHasLastCast = false;
	void SetState(EWEVORAWeavingState NewState);
	void ClearGesture();
	uint64 DebugKey() const;
};
