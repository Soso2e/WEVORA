#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORAReticleWidget.generated.h"

/** Asset-free, non-interactive local-player reticle. */
UCLASS()
class WEVORA_API UWEVORAReticleWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell Trail")
	bool bShowGestureTrail = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell Trail", meta=(ClampMin="1"))
	float TrailInputScale = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell Trail", meta=(ClampMin="0.05"))
	float TrailLifetime = 0.45f;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
		const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	struct FTrailPoint { FVector2D Position; float Time; };
	TArray<FTrailPoint> Trail;
	FVector2D TrailCursor = FVector2D::ZeroVector;
	float TrailTime = 0.0f;
	EWEVORAWeavingState PreviousState = EWEVORAWeavingState::Idle;
	EWEVORASpellElement TrailElement = EWEVORASpellElement::Fire;
	TWeakObjectPtr<class UWEVORASpellWeavingComponent> TrailSource;
	FDelegateHandle SampleHandle;
	void UpdateTrailSource();
	void UnbindTrailSource();
	void RecordTrailSample(FVector2D Delta);
	UFUNCTION()
	void UpdateTrailState(EWEVORAWeavingState State, const FWEVORASpellContext& Context);
};
