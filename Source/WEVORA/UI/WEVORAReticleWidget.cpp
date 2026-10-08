#include "UI/WEVORAReticleWidget.h"
#include "WEVORACharacter.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Rendering/DrawElements.h"

int32 UWEVORAReticleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled) + 1;
	const AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(GetOwningPlayerPawn());
	if (!Pilot || !Pilot->SpellCastComponent || !Pilot->SpellWeavingComponent) { return Layer; }
	FWEVORASpellContext Context = Pilot->SpellWeavingComponent->Context;
	if (Context.Gesture == EWEVORAGesture::None) { Context.Gesture = EWEVORAGesture::Thrust; }
	FWEVORASpellLaunch Launch;
	if (!Pilot->ResolveSpellLaunch(Context, Launch)) { return Layer; }
	FWEVORAAimSolution Aim;
	if (!Pilot->SpellCastComponent->GetAimSolution(Launch.Parameters.Radius, Aim)) { return Layer; }
	const FVector2D Center = Geometry.GetLocalSize() * 0.5f;
	const FLinearColor Color = Aim.bBlocked ? FLinearColor(1.0f, 0.25f, 0.1f)
		: Aim.bTargetIsDamageable ? FLinearColor(0.2f, 1.0f, 0.5f) : FLinearColor::White;
	auto Line = [&](FVector2D A, FVector2D B)
	{
		TArray<FVector2D> Points{Center + A, Center + B};
		FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, FLinearColor::Black, true, 4.0f);
		FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color, true, 2.0f);
	};
	const float Gap = Pilot->bPrecisionAim ? 4.0f : 7.0f;
	Line(FVector2D(-Gap-7, 0), FVector2D(-Gap, 0));
	Line(FVector2D(Gap, 0), FVector2D(Gap+7, 0));
	Line(FVector2D(0, -Gap-7), FVector2D(0, -Gap));
	Line(FVector2D(0, Gap), FVector2D(0, Gap+7));
	Line(FVector2D(-1, 0), FVector2D(1, 0));
	if (Aim.bBlocked)
	{
		Line(FVector2D(-4, -4), FVector2D(4, 4));
		Line(FVector2D(-4, 4), FVector2D(4, -4));
	}
	return Layer + 1;
}
