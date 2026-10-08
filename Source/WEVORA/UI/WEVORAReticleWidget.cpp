#include "UI/WEVORAReticleWidget.h"
#include "WEVORACharacter.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Rendering/DrawElements.h"
#include "AI/WEVORAEnemy.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"

int32 UWEVORAReticleWidget::DrawEnemyStatus(const FGeometry& Geometry,
	FSlateWindowElementList& Elements, int32 Layer) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!bShowEnemyStatus || !PC || !GetWorld()) { return Layer; }
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	for (TActorIterator<AWEVORAEnemy> It(GetWorld()); It; ++It)
	{
		const AWEVORAEnemy* Enemy = *It;
		if (!Enemy->HealthComponent->IsAlive() || Enemy->IsHidden()) { continue; }
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyStatusSight), false, PC->GetPawn());
		Query.AddIgnoredActor(Enemy);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, Enemy->GetActorLocation(), ECC_Visibility, Query)) { continue; }
		FVector2D Position;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,
			Enemy->GetActorLocation() + FVector(0, 0, 100), Position, true)) { continue; }
		const FVector2D Size = Geometry.GetLocalSize();
		if (Position.X < 0 || Position.Y < 0 || Position.X > Size.X || Position.Y > Size.Y) { continue; }
		const bool bBurning = Enemy->SpellReactionComponent->HasState(EWEVORASpellTargetState::Burning);
		const FVector2D PanelPosition = Position - FVector2D(80, 40);
		const FVector2D PanelSize(160, bBurning ? 50 : 28);
		FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(PanelSize, FSlateLayoutTransform(PanelPosition)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.01f, 0.01f, 0.015f, 0.8f));
		const FString HP = FString::Printf(TEXT("HP %.0f / %.0f"), Enemy->HealthComponent->Health, Enemy->HealthComponent->MaxHealth);
		FSlateDrawElement::MakeText(Elements, Layer + 1,
			Geometry.ToPaintGeometry(PanelSize, FSlateLayoutTransform(PanelPosition + FVector2D(10, 4))),
			HP, FCoreStyle::GetDefaultFontStyle("Bold", 14), ESlateDrawEffect::None, FLinearColor::White);
		if (bBurning)
		{
			FSlateDrawElement::MakeText(Elements, Layer + 1,
				Geometry.ToPaintGeometry(PanelSize, FSlateLayoutTransform(PanelPosition + FVector2D(10, 26))),
				FString(TEXT("燃焼中")), FCoreStyle::GetDefaultFontStyle("Bold", 13),
				ESlateDrawEffect::None, FLinearColor(1.0f, 0.45f, 0.08f));
		}
	}
	return Layer + 1;
}

void UWEVORAReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateTrailSource();
}

void UWEVORAReticleWidget::UnbindTrailSource()
{
	if (UWEVORASpellWeavingComponent* Source = TrailSource.Get())
	{
		Source->OnGestureSample.Remove(SampleHandle);
		Source->OnSpellPresentationChanged.RemoveDynamic(this, &ThisClass::UpdateTrailState);
	}
	TrailSource.Reset();
}

void UWEVORAReticleWidget::NativeDestruct()
{
	UnbindTrailSource();
	Trail.Reset();
	Super::NativeDestruct();
}

void UWEVORAReticleWidget::UpdateTrailSource()
{
	const AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(GetOwningPlayerPawn());
	UWEVORASpellWeavingComponent* Source = Pilot ? Pilot->SpellWeavingComponent : nullptr;
	if (Source == TrailSource.Get()) { return; }
	UnbindTrailSource();
	Trail.Reset();
	PreviousState = EWEVORAWeavingState::Idle;
	TrailSource = Source;
	if (Source)
	{
		SampleHandle = Source->OnGestureSample.AddUObject(this, &ThisClass::RecordTrailSample);
		Source->OnSpellPresentationChanged.AddUniqueDynamic(this, &ThisClass::UpdateTrailState);
		UpdateTrailState(Source->State, Source->Context);
	}
}

void UWEVORAReticleWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	TrailTime += FMath::Max(0.0f, DeltaTime);
	UpdateTrailSource();
	while (!Trail.IsEmpty() && TrailTime - Trail[0].Time > FMath::Max(0.05f, TrailLifetime))
	{
		Trail.RemoveAt(0);
	}
}

void UWEVORAReticleWidget::UpdateTrailState(EWEVORAWeavingState State, const FWEVORASpellContext& Context)
{
	// Attribute changes while drawing recolor the line without restarting the gesture.
	TrailElement = Context.Element;
	if (State == EWEVORAWeavingState::Shaping && PreviousState != State)
	{
		Trail.Reset();
		TrailCursor = FVector2D::ZeroVector;
		Trail.Add({TrailCursor, TrailTime});
	}
	PreviousState = State;
}

void UWEVORAReticleWidget::RecordTrailSample(FVector2D Delta)
{
	if (!bShowGestureTrail || Delta.IsNearlyZero()) { return; }
	// Recognition uses +Y up; Slate uses +Y down. No gameplay input is modified.
	TrailCursor += FVector2D(Delta.X, -Delta.Y) * FMath::Max(1.0f, TrailInputScale);
	if (Trail.IsEmpty()) { Trail.Add({TrailCursor, TrailTime}); }
	else
	{
		if (Trail.Num() >= 128) { Trail.RemoveAt(0); }
		Trail.Add({TrailCursor, TrailTime});
	}
}

int32 UWEVORAReticleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 Layer = DrawEnemyStatus(Geometry, Elements,
		Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled) + 1) + 1;
	const AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(GetOwningPlayerPawn());
	if (!Pilot || !Pilot->SpellCastComponent || !Pilot->SpellWeavingComponent) { return Layer; }
	const FVector2D Center = Geometry.GetLocalSize() * 0.5f;
	if (bShowGestureTrail)
	{
		const FLinearColor Tint = TrailElement == EWEVORASpellElement::Fire
			? FLinearColor(1.0f, 0.3f, 0.04f) : FLinearColor(0.05f, 0.8f, 1.0f);
		for (int32 Index = 1; Index < Trail.Num(); ++Index)
		{
			const float Alpha = FMath::Clamp(1.0f - (TrailTime - Trail[Index-1].Time)
				/ FMath::Max(0.05f, TrailLifetime), 0.0f, 1.0f);
			TArray<FVector2D> Points{Center + Trail[Index-1].Position, Center + Trail[Index].Position};
			FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, Tint.CopyWithNewOpacity(Alpha * 0.12f), true, 14.0f);
			FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, Tint.CopyWithNewOpacity(Alpha * 0.45f), true, 7.0f);
			FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, FLinearColor(1.0f, 0.95f, 0.85f, Alpha), true, 2.0f);
		}
	}
	FWEVORASpellContext Context = Pilot->SpellWeavingComponent->Context;
	if (Context.Gesture == EWEVORAGesture::None) { Context.Gesture = EWEVORAGesture::Thrust; }
	FWEVORASpellLaunch Launch;
	if (!Pilot->ResolveSpellLaunch(Context, Launch)) { return Layer + 2; }
	FWEVORAAimSolution Aim;
	if (!Pilot->SpellCastComponent->GetAimSolution(Launch.Parameters.Radius, Aim)) { return Layer + 2; }
	const FLinearColor Color = Aim.bBlocked ? FLinearColor(1.0f, 0.25f, 0.1f)
		: Aim.bTargetIsDamageable ? FLinearColor(0.2f, 1.0f, 0.5f) : FLinearColor::White;
	auto Line = [&](FVector2D A, FVector2D B)
	{
		TArray<FVector2D> Points{Center + A, Center + B};
		FSlateDrawElement::MakeLines(Elements, Layer + 3, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, FLinearColor::Black, true, 4.0f);
		FSlateDrawElement::MakeLines(Elements, Layer + 4, Geometry.ToPaintGeometry(), Points,
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
	return Layer + 4;
}
