#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WEVORAReticleWidget.generated.h"

/** Asset-free, non-interactive local-player reticle. */
UCLASS()
class WEVORA_API UWEVORAReticleWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
		const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
};
