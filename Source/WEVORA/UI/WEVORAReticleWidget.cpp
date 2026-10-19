#include "UI/WEVORAReticleWidget.h"
#include "WEVORACharacter.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Rendering/DrawElements.h"
#include "AI/WEVORAEnemy.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"
#include "Playground/WEVORACombatPlayground.h"
#include "Playground/WEVORAPlaygroundTarget.h"
#include "Movement/WEVORAManaComponent.h"

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
		const AWEVORAPlaygroundTarget* Practice = Cast<AWEVORAPlaygroundTarget>(Enemy);
		const FVector2D PanelPosition = Position - FVector2D(80, 40);
		const FVector2D PanelSize(Practice ? 220 : 160, Practice ? (bBurning ? 78 : 58) : (bBurning ? 50 : 28));
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
		if (Practice)
		{
			FSlateDrawElement::MakeText(Elements, Layer + 1,
				Geometry.ToPaintGeometry(PanelSize, FSlateLayoutTransform(PanelPosition + FVector2D(8, bBurning ? 48 : 27))),
				Practice->TargetName, FCoreStyle::GetDefaultFontStyle("Regular", 11), ESlateDrawEffect::None, FLinearColor(0.5f,0.8f,1));
			if (GetWorld()->GetTimeSeconds() - Practice->LastDamageTime < 0.75f)
			{
				FSlateDrawElement::MakeText(Elements, Layer + 2,
					Geometry.ToPaintGeometry(PanelSize, FSlateLayoutTransform(PanelPosition + FVector2D(145, 3))),
					FString::Printf(TEXT("-%.0f"), Practice->LastDamage), FCoreStyle::GetDefaultFontStyle("Bold", 15),
					ESlateDrawEffect::None, FLinearColor(1,0.6f,0.2f));
			}
		}
	}
	return Layer + 1;
}

void UWEVORAReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateTrailSource();
}

int32 UWEVORAReticleWidget::DrawPlaygroundHUD(const FGeometry& Geometry,
	FSlateWindowElementList& Elements, int32 Layer) const
{
	const AWEVORACombatPlayground* Trial = AWEVORACombatPlayground::Find(GetWorld());
	const AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(GetOwningPlayerPawn());
	if (!Trial || !Pilot) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	auto Box = [&](FVector2D Position, FVector2D Extent, FLinearColor Tint)
	{
		FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(Position)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Tint);
	};
	auto Text = [&](FVector2D Position, const FString& Value, int32 FontSize, FLinearColor Tint = FLinearColor::White)
	{
		FSlateDrawElement::MakeText(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2D(450,30), FSlateLayoutTransform(Position)),
			Value, FCoreStyle::GetDefaultFontStyle("Regular", FontSize), ESlateDrawEffect::None, Tint);
	};
	const UWEVORASpellWeavingComponent* Weave = Pilot->SpellWeavingComponent;
	const UWEVORASpellCastComponent* Cast = Pilot->SpellCastComponent;
	const EWEVORASpellElement Element = Weave->Context.Element;
	const FLinearColor Color = Element == EWEVORASpellElement::Fire ? FLinearColor(1,0.4f,0.1f)
		: Element == EWEVORASpellElement::Wind ? FLinearColor(0.1f,0.85f,1) : FLinearColor(0.75f,0.45f,1);
	Box(FVector2D(22,20), FVector2D(315,116), FLinearColor(0.01f,0.02f,0.035f,0.8f));
	Text(FVector2D(34,27), TEXT("WEVORA  /  COMBAT PLAYGROUND v1"), 12, FLinearColor(0.55f,0.7f,0.85f));
	Text(FVector2D(34,48), FString::Printf(TEXT("HP %.0f/%.0f      MANA %.0f/%.0f"), Pilot->HealthComponent->Health,
		Pilot->HealthComponent->MaxHealth, Pilot->ManaComponent->Mana, Pilot->ManaComponent->MaxMana), 14);
	Box(FVector2D(34,72), FVector2D(130,4), FLinearColor(0.15f,0.15f,0.15f));
	Box(FVector2D(184,72), FVector2D(130,4), FLinearColor(0.15f,0.15f,0.15f));
	Box(FVector2D(34,72), FVector2D(130 * FMath::Clamp(Pilot->HealthComponent->Health /
		FMath::Max(1.0f, Pilot->HealthComponent->MaxHealth), 0.0f, 1.0f),4), FLinearColor(0.4f,1,0.5f));
	Box(FVector2D(184,72), FVector2D(130 * Pilot->ManaComponent->GetManaRatio(),4), FLinearColor(0.15f,0.55f,1));
	Text(FVector2D(34,84), FString::Printf(TEXT("%s    %s"), *StaticEnum<EWEVORASpellElement>()->GetNameStringByValue(int64(Element)),
		Weave->GetCooldownRemaining() > 0 ? *FString::Printf(TEXT("CT %.1fs"), Weave->GetCooldownRemaining())
		: Weave->State != EWEVORAWeavingState::Idle ? *FString::Printf(TEXT("PREPARING %.0f%%"), Weave->GetChargeAlpha()*100) : TEXT("READY")), 15, Color);
	Text(FVector2D(34,111), FString::Printf(TEXT("Lightning assist: %s  [F3]"), Trial->bLightningAssist ? TEXT("ON (cast only)") : TEXT("OFF / straight")), 11);
	if (Trial->bShowGuide)
	{
		const FVector2D Origin(22, Size.Y - 221);
		Box(Origin, FVector2D(430,174), FLinearColor(0.01f,0.02f,0.035f,0.78f));
		const TArray<FString> Guide = {
			TEXT("WASD move  |  Space jump / ascend  |  Ctrl dive"),
			TEXT("Shift dash + hold altitude  |  Alt brake / hover"),
			TEXT("LMB tap: quick shot  |  hold + release: charge"),
			TEXT("Q: Fire / Wind / Lightning  |  RMB: precision aim"),
			TEXT("Hold E + draw, release E: cast your shape"),
			TEXT("UP thrust  |  SIDE sweep  |  LOOP field  |  DOWN slam"),
			TEXT("Lead moving targets. The circle is an aiming guide."),
			TEXT("Fire burns  |  Wind pushes  |  Lightning is fast")};
		for (int32 I = 0; I < Guide.Num(); ++I) { Text(Origin + FVector2D(12, 8 + I*20), Guide[I], 12); }
	}
	Text(FVector2D(24,Size.Y-31), FString::Printf(TEXT("R  RESET TRIAL     F1  GUIDE     F2  DEBUG     Trial %d"), Trial->ResetCount), 12, FLinearColor(0.7f,0.8f,0.9f));
	const FVector Position = Pilot->GetActorLocation() - Trial->GetActorLocation();
	if (FMath::Abs(Position.X) > Trial->BoundsExtent.X * 0.85f || FMath::Abs(Position.Y) > Trial->BoundsExtent.Y * 0.85f
		|| Position.Z > Trial->BoundsExtent.Z * 0.85f)
	{
		Text(Center + FVector2D(-130,-110), TEXT("FIELD LIMIT - return to practice lanes"), 14, FLinearColor(1,0.65f,0.15f));
	}
	if (!Pilot->HealthComponent->IsAlive()) { Text(Center + FVector2D(-135,100), TEXT("DEFEATED - restarting trial..."), 18, FLinearColor(1,0.4f,0.2f)); }
	if (Trial->bDebugOverlay)
	{
		const FVector2D Origin(FMath::Max(360.0, Size.X - 354), 22);
		Box(Origin, FVector2D(332, 510), FLinearColor(0.01f,0.02f,0.035f,0.85f));
		Text(Origin + FVector2D(12,8), FString::Printf(TEXT("%.0f FPS  |  speed %.0f cm/s"), SmoothedFPS, Pilot->GetVelocity().Size()),14);
		Text(Origin + FVector2D(12,32), FString::Printf(TEXT("Casts %d  |  reaction contacts %d"), Cast->CastCount, Cast->HitCount),12);
		Text(Origin + FVector2D(12,54), Trial->LastResetReason, 12);
		int32 Index = 0;
		for (const AWEVORAPlaygroundTarget* Target : Trial->Targets)
		{
			if (!IsValid(Target)) { continue; }
			Text(Origin + FVector2D(12, 80 + Index++ * 27), FString::Printf(TEXT("%s: HP %.0f  v%.0f %s"),
				*Target->TargetName.Left(12), Target->HealthComponent->Health, Target->MeasuredVelocity.Size(),
				!Target->HealthComponent->IsAlive() ? TEXT("RESPAWN") : Target->SpellReactionComponent->IsDisplaced()
				? TEXT("PUSH") : Target->SpellReactionComponent->BurningRemaining > 0 ? TEXT("BURN") : TEXT("")),11);
		}
		const AWEVORAPlaygroundTarget* Focus = ::Cast<AWEVORAPlaygroundTarget>(Cast->LastImpact.Target.Get());
		if (!Focus || !Focus->FindComponentByClass<UWEVORALocalizedStatusComponent>())
		{
			for (const AWEVORAPlaygroundTarget* Target : Trial->Targets)
			{
				if (IsValid(Target) && Target->FindComponentByClass<UWEVORALocalizedStatusComponent>()) { Focus = Target; break; }
			}
		}
		const UWEVORALocalizedStatusComponent* Local = Focus ? Focus->FindComponentByClass<UWEVORALocalizedStatusComponent>() : nullptr;
		if (Local && Local->GetProfile())
		{
			Text(Origin + FVector2D(12,248), FString::Printf(TEXT("LOCAL last=%s  HP %.1f  delta -%.2f"),
				*Local->LastHitZone.ToString(), Focus->HealthComponent->Health, Focus->LastDamage), 10, FLinearColor(1,0.65f,0.2f));
			int32 Row = 0;
			for (const FWEVORAZoneDefinition& Zone : Local->GetProfile()->Zones)
			{
				FString StatusText = Zone.ID.ToString() + TEXT(": --");
				for (const FWEVORALocalStatus& State : Local->States)
				{
					if (State.Zone != Zone.ID) { continue; }
					StatusText = FString::Printf(TEXT("%s %s %.2fs I%.2f A%.2f"), *Zone.ID.ToString(),
						*State.Type.ToString(), State.Remaining, State.Intensity, State.Accumulation);
					break;
				}
				Text(Origin + FVector2D(12,270 + Row++ * 18), StatusText, 10);
			}
			Text(Origin + FVector2D(12,420), TEXT("Propagation (last frame batch):"), 10);
			for (int32 I = 0; I < FMath::Min(3, Local->LastPropagationPaths.Num()); ++I)
			{
				Text(Origin + FVector2D(12,440 + I*18), Local->LastPropagationPaths[I], 10, FLinearColor(1,0.6f,0.2f));
			}
		}
	}
	const float SinceImpact = GetWorld()->GetTimeSeconds() - Cast->LastImpactTime;
	if (SinceImpact < 0.6f)
	{
		const FWEVORASpellImpact& Impact = Cast->LastImpact;
		Text(Center + FVector2D(67,-10), Impact.bApplied ? Impact.DirectDamage > 0
			? FString::Printf(TEXT("HIT -%.0f"), Impact.DirectDamage) : TEXT("PUSH / LIFT") : TEXT("WORLD"),14,
			Impact.bApplied ? FLinearColor(0.5f,1,0.7f) : FLinearColor(0.6f,0.65f,0.7f));
		if (Impact.bApplied && SinceImpact < 0.2f)
		{
			for (int32 SignX : {-1,1}) { for (int32 SignY : {-1,1})
			{
				TArray<FVector2D> Points{Center + FVector2D(SignX*11,SignY*11), Center + FVector2D(SignX*19,SignY*19)};
				FSlateDrawElement::MakeLines(Elements, Layer+2, Geometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, FLinearColor::White, true, 2);
			} }
		}
	}
	return Layer + 2;
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
	if (DeltaTime > SMALL_NUMBER) { SmoothedFPS = FMath::FInterpTo(SmoothedFPS, 1.0f / DeltaTime, DeltaTime, 3); }
	UpdateTrailSource();
	while (!Trail.IsEmpty() && TrailTime - Trail[0].Time > FMath::Max(0.05f, TrailLifetime))
	{
		Trail.RemoveAt(0);
	}
	if (const AWEVORACombatPlayground* Trial = AWEVORACombatPlayground::Find(GetWorld()))
	{
		if (SeenResetCount != Trial->ResetCount) { Trail.Reset(); SeenResetCount = Trial->ResetCount; }
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
	const int32 Layer = DrawPlaygroundHUD(Geometry, Elements, DrawEnemyStatus(Geometry, Elements,
		Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled) + 1) + 1) + 1;
	const AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(GetOwningPlayerPawn());
	if (!Pilot || !Pilot->SpellCastComponent || !Pilot->SpellWeavingComponent) { return Layer; }
	const FVector2D Center = Geometry.GetLocalSize() * 0.5f;
	const float Charge = Pilot->SpellWeavingComponent->GetChargeAlpha();
	const float Cooldown = Pilot->SpellWeavingComponent->GetCooldownRemaining();
	if (bShowGestureTrail)
	{
		const FLinearColor Tint = TrailElement == EWEVORASpellElement::Fire
			? FLinearColor(1.0f, 0.3f, 0.04f) : TrailElement == EWEVORASpellElement::Lightning
			? FLinearColor(0.7f, 0.35f, 1.0f) : FLinearColor(0.05f, 0.8f, 1.0f);
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
	Context.ChargeAlpha = Charge;
	if (!Pilot->SpellCastComponent->ResolveCastLaunch(Context, Launch)) { return Layer + 2; }
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
	// Slate local coordinates inherit viewport DPI scaling; no raw-pixel offsets or camera motion.
	const float RingRadius = Pilot->bPrecisionAim ? PrecisionRadius : TravelRadius;
	TArray<FVector2D> Ring;
	TArray<FVector2D> ProgressArc;
	const float Fill = Cooldown > 0 ? 1.0f - FMath::Clamp(Cooldown /
		FMath::Max(0.01f, Pilot->SpellWeavingComponent->AttackCooldown), 0.0f, 1.0f) : Charge;
	for (int32 Index = 0; Index <= 64; ++Index)
	{
		const float Angle = Index * 2 * PI / 64 - PI * 0.5f;
		const FVector2D Unit(FMath::Cos(Angle), FMath::Sin(Angle));
		Ring.Add(Center + Unit * RingRadius);
		if (Index <= FMath::FloorToInt(Fill * 64)) { ProgressArc.Add(Center + Unit * (RingRadius + 5)); }
	}
	FSlateDrawElement::MakeLines(Elements, Layer + 3, Geometry.ToPaintGeometry(), Ring,
		ESlateDrawEffect::None, FLinearColor(0,0,0,0.75f), true, 4);
	FSlateDrawElement::MakeLines(Elements, Layer + 4, Geometry.ToPaintGeometry(), Ring,
		ESlateDrawEffect::None, Color.CopyWithNewOpacity(Cooldown > 0 ? 0.35f : 0.8f), true, 1.5f);
	if (ProgressArc.Num() > 1)
	{
		FSlateDrawElement::MakeLines(Elements, Layer + 4, Geometry.ToPaintGeometry(), ProgressArc,
			ESlateDrawEffect::None, Cooldown > 0 ? FLinearColor(0.5f,0.6f,0.7f) : Launch.Parameters.Color, true, 3);
	}
	Line(FVector2D(-1, 0), FVector2D(1, 0));
	if (Aim.bBlocked)
	{
		Line(FVector2D(-4, -4), FVector2D(4, 4));
		Line(FVector2D(-4, 4), FVector2D(4, -4));
		FSlateDrawElement::MakeText(Elements, Layer + 4,
			Geometry.ToPaintGeometry(FVector2D(220,22), FSlateLayoutTransform(Center + FVector2D(-55,RingRadius+13))),
			FString(Aim.bMuzzleBlocked ? TEXT("MUZZLE BLOCKED") : TEXT("PATH BLOCKED")),
			FCoreStyle::GetDefaultFontStyle("Bold", 11), ESlateDrawEffect::None, Color);
	}
	return Layer + 4;
}
