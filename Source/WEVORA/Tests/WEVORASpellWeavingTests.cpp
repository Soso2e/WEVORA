#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Spell/WEVORAGestureRecognizer.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellSelectionEffectComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAGestureTest, "WEVORA.Spell.Gestures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORAGestureTest::RunTest(const FString& Parameters)
{
	const FWEVORAGestureThresholds T;
	FWEVORAGestureRecognizer R;
	auto Flick = [&](FVector2D Delta, EWEVORAGesture Expected)
	{
		R.Reset(); R.Record(Delta, T.MinimumSampleDistance);
		TestEqual(TEXT("Flick classification"), R.Recognize(T, 0.2f).Gesture, Expected);
	};
	Flick(FVector2D(2, 15), EWEVORAGesture::Thrust);
	Flick(FVector2D(2, -15), EWEVORAGesture::Slam);
	Flick(FVector2D(15, 3), EWEVORAGesture::Sweep);
	Flick(FVector2D(-15, 3), EWEVORAGesture::Sweep);
	Flick(FVector2D(1, 1), EWEVORAGesture::None);
	for (const float Sign : {1.0f, -1.0f})
	{
		R.Reset();
		FVector2D Previous(10, 0);
		for (int32 I = 1; I <= 24; ++I)
		{
			const float Angle = Sign * I * 2.0f * PI / 24.0f;
			const FVector2D Point(10 * FMath::Cos(Angle), 10 * FMath::Sin(Angle));
			R.Record(Point - Previous, T.MinimumSampleDistance); Previous = Point;
		}
		const FWEVORASpellContext Result = R.Recognize(T, 0.5f);
		TestEqual(TEXT("Both circle directions"), Result.Gesture, EWEVORAGesture::Circle);
		TestTrue(TEXT("Circle path magnitude"), Result.GestureMagnitude > 50);
		TestEqual(TEXT("Duration preserved"), Result.GestureDuration, 0.5f);
	}
	R.Reset();
	for (int32 I = 0; I < 8; ++I) { R.Record(FVector2D(I % 2 ? -10 : 10, 0), T.MinimumSampleDistance); }
	TestEqual(TEXT("Backtracking is not a circle"), R.Recognize(T, 1).Gesture, EWEVORAGesture::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAWeaveTest, "WEVORA.Spell.StateFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORAWeaveTest::RunTest(const FString& Parameters)
{
	UWEVORASpellWeavingComponent* C = NewObject<UWEVORASpellWeavingComponent>();
	C->bLogEvents = false;
	C->BeginShape();
	TestEqual(TEXT("E outside weave ignored"), C->State, EWEVORAWeavingState::Idle);
	C->BeginWeave(); C->RecordLook(FVector2D(100, 0)); C->BeginShape(); C->EndShape();
	TestEqual(TEXT("Input outside E ignored; invalid returns to weave"), C->State, EWEVORAWeavingState::Weaving);
	C->BeginShape(); C->CycleElement(); C->RecordLook(FVector2D(0, -15)); C->EndShape();
	TestEqual(TEXT("Shape ready"), C->State, EWEVORAWeavingState::ReadyToCast);
	TestEqual(TEXT("Negative pitch maps to screen up"), C->Context.Gesture, EWEVORAGesture::Thrust);
	TestEqual(TEXT("Element switches while shaping"), C->Context.Element, EWEVORASpellElement::Wind);
	C->ReleaseWeave();
	TestEqual(TEXT("Cast resets to idle"), C->State, EWEVORAWeavingState::Idle);
	TestEqual(TEXT("Cast snapshot"), C->LastCastContext.Gesture, EWEVORAGesture::Thrust);
	TestEqual(TEXT("Element persists"), C->Context.Element, EWEVORASpellElement::Wind);
	C->BeginWeave(); C->BeginShape(); C->RecordLook(FVector2D(15, 0)); C->ReleaseWeave(); C->EndShape();
	TestEqual(TEXT("Early LMB release cancels safely"), C->State, EWEVORAWeavingState::Idle);
	TestEqual(TEXT("Canceled shape does not overwrite cast"), C->LastCastContext.Gesture, EWEVORAGesture::Thrust);
	C->BeginWeave(); C->BeginShape(); C->RecordLook(FVector2D(15, 0)); C->EndShape(); C->BeginShape(); C->EndShape();
	TestEqual(TEXT("Invalid replacement clears ready shape"), C->State, EWEVORAWeavingState::Weaving);
	C->ReleaseWeave();
	TestEqual(TEXT("Incomplete weave cancels"), C->State, EWEVORAWeavingState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORASelectionEffectTest, "WEVORA.Spell.SelectionEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORASelectionEffectTest::RunTest(const FString& Parameters)
{
	// This synchronous test drives a private world's timers across simulated frames.
	TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Owner = World->SpawnActor<AActor>();
	UWEVORASpellWeavingComponent* Weaving = NewObject<UWEVORASpellWeavingComponent>(Owner);
	Owner->AddInstanceComponent(Weaving);
	Weaving->bShowDebug = false;
	Weaving->bLogEvents = false;
	Weaving->RegisterComponent();
	UWEVORASpellSelectionEffectComponent* Effect = NewObject<UWEVORASpellSelectionEffectComponent>(Owner);
	Owner->AddInstanceComponent(Effect);
	Owner->SetRootComponent(Effect);
	Effect->RegisterComponent();
	Owner->PreInitializeComponents();
	Owner->InitializeComponents();
	Owner->PostInitializeComponents();
	Owner->DispatchBeginPlay();
	UPointLightComponent* Light = Owner->FindComponentByClass<UPointLightComponent>();
	if (!TestNotNull(TEXT("Placeholder light created without an asset"), Light))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestFalse(TEXT("Idle starts hidden"), Effect->IsPreviewActive());
	Weaving->CycleElement();
	TestTrue(TEXT("Idle selection starts preview"), Effect->IsPreviewActive());
	TestTrue(TEXT("Wind updates placeholder color"), Light->GetLightColor().Equals(Effect->ElementEffects[EWEVORASpellElement::Wind].Color, 0.01f));
	++GFrameCounter;
	World->GetTimerManager().Tick(1.0f);
	++GFrameCounter;
	World->GetTimerManager().Tick(1.0f);
	TestFalse(TEXT("Idle selection expires"), Effect->IsPreviewActive());
	Weaving->CycleElement();
	Weaving->CancelWeave();
	TestFalse(TEXT("Explicit idle cancellation hides preview"), Light->IsVisible());
	Weaving->BeginWeave();
	TestTrue(TEXT("Weaving shows preview"), Light->IsVisible());
	++GFrameCounter;
	World->GetTimerManager().Tick(1.0f);
	TestTrue(TEXT("Weaving is not limited by idle timer"), Effect->IsPreviewActive());
	Weaving->BeginShape();
	Weaving->RecordLook(FVector2D(15, 0));
	Weaving->EndShape();
	Weaving->CycleElement();
	TestEqual(TEXT("Selection does not discard ready shape"), Weaving->State, EWEVORAWeavingState::ReadyToCast);
	TestTrue(TEXT("Ready preview remains active"), Effect->IsPreviewActive());
	Weaving->ReleaseWeave();
	TestFalse(TEXT("Cast hides preview"), Light->IsVisible());
	Weaving->BeginWeave();
	Effect->ElementEffects.Remove(Weaving->Context.Element);
	Effect->RefreshEffect();
	TestFalse(TEXT("Missing profile clears old effect"), Effect->IsPreviewActive());
	Weaving->CancelWeave();
	Effect->IdlePreviewDuration = 0.0f;
	Weaving->CycleElement();
	TestFalse(TEXT("Zero duration disables idle preview"), Effect->IsPreviewActive());
	// The isolated world has no game mode; explicitly route its play lifecycle.
	Owner->RouteEndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("EndPlay unbinds presentation callback"), Weaving->OnSpellPresentationChanged.IsBound());
	World->DestroyWorld(false);
	return true;
}
#endif
