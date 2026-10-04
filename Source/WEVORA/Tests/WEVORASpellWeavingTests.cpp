#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Spell/WEVORAGestureRecognizer.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellSelectionEffectComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WEVORACharacter.h"
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
	TArray<UPointLightComponent*> Lights;
	Owner->GetComponents(Lights);
	UPointLightComponent* BodyLight = nullptr;
	UPointLightComponent* Light = nullptr;
	for (UPointLightComponent* Candidate : Lights)
	{
		if (Candidate->GetFName() == TEXT("SpellBodySelectionLight")) { BodyLight = Candidate; }
		else { Light = Candidate; }
	}
	TestNotNull(TEXT("Separate body pulse light created"), BodyLight);
	if (!TestNotNull(TEXT("Placeholder light created without an asset"), Light))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestFalse(TEXT("Idle starts hidden"), Effect->IsPreviewActive());
	Weaving->CycleElement();
	TestTrue(TEXT("Idle selection starts preview"), Effect->IsPreviewActive());
	if (BodyLight)
	{
		TestTrue(TEXT("Wind updates body pulse color"), BodyLight->GetLightColor().Equals(Effect->ElementEffects[EWEVORASpellElement::Wind].Color, 0.01f));
		TestTrue(TEXT("Idle pulse appears at body"), BodyLight->IsVisible());
	}
	TestFalse(TEXT("Idle does not show hand effect"), Light->IsVisible());
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
	if (BodyLight) { TestTrue(TEXT("Q while weaving also pulses from body"), BodyLight->IsVisible()); }
	TestTrue(TEXT("Q while weaving retains hand display"), Light->IsVisible());
	TestTrue(TEXT("Ready preview remains active"), Effect->IsPreviewActive());
	Weaving->ReleaseWeave();
	TestFalse(TEXT("Cast hides preview"), Light->IsVisible());
	if (BodyLight) { TestFalse(TEXT("Cast also clears body pulse"), BodyLight->IsVisible()); }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAHandAttachmentTest, "WEVORA.Spell.HandAttachment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORAHandAttachmentTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<AWEVORACharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	if (!TestNotNull(TEXT("Project character Blueprint loads"), CharacterClass)) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AWEVORACharacter* Character = World->SpawnActor<AWEVORACharacter>(CharacterClass);
	if (!TestNotNull(TEXT("Project character spawns"), Character)) { World->DestroyWorld(false); return false; }
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	UWEVORASpellSelectionEffectComponent* Effect = Character->SpellSelectionEffectComponent;
	TestTrue(TEXT("Actual character mesh contains right hand"), Mesh->DoesSocketExist(TEXT("hand_r")));
	Effect->SetRelativeLocation(FVector(30, 0, 110));
	Character->PreInitializeComponents();
	Character->InitializeComponents();
	Character->PostInitializeComponents();
	Character->DispatchBeginPlay();
	TestTrue(TEXT("Effect attaches to character mesh"), Effect->GetAttachParent() == Mesh);
	TestEqual(TEXT("Effect follows right hand bone"), Effect->GetAttachSocketName(), FName(TEXT("hand_r")));
	TestTrue(TEXT("Legacy body offset is replaced"), Effect->GetRelativeTransform().Equals(Effect->HandOffset));
	TestTrue(TEXT("Effect world location matches actual right hand"), Effect->GetComponentLocation().Equals(Mesh->GetSocketLocation(TEXT("hand_r")), 0.1f));
	Character->SpellWeavingComponent->CycleElement();
	TArray<UPointLightComponent*> Lights;
	Character->GetComponents(Lights);
	for (UPointLightComponent* Light : Lights)
	{
		if (Light->GetFName() == TEXT("SpellBodySelectionLight"))
		{
			TestTrue(TEXT("Body pulse attaches to mesh rather than hand"), Light->GetAttachParent() == Mesh);
			TestTrue(TEXT("Body pulse has no hand socket"), Light->GetAttachSocketName().IsNone());
			TestTrue(TEXT("Body pulse uses body offset"), Light->GetRelativeTransform().Equals(Effect->BodyOffset));
		}
	}
	Character->RouteEndPlay(EEndPlayReason::Destroyed);
	World->DestroyWorld(false);
	return true;
}
#endif
