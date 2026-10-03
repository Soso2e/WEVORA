#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Spell/WEVORAGestureRecognizer.h"
#include "Spell/WEVORASpellWeavingComponent.h"

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
#endif
