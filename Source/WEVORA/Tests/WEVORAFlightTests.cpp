#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WEVORACharacter.h"
#include "Movement/WEVORAFlightMovementComponent.h"
#include "Movement/WEVORAManaComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
/** Saved project Blueprint, real tick ordering, swept movement and a physical landing surface. */
struct FFlightWorld
{
	TGuardValue<uint64> FrameGuard{GFrameCounter, GFrameCounter};
	UWorld* World = nullptr;
	AWEVORACharacter* Pilot = nullptr;
	APlayerController* Controller = nullptr;

	FFlightWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		AActor* Floor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
		Floor->AddInstanceComponent(Box);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(10000.0f, 10000.0f, 50.0f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0.0f, 0.0f, -50.0f));
		World->BeginPlay();
		UClass* PilotClass = LoadClass<AWEVORACharacter>(nullptr,
			TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
		if (!PilotClass) { return; }
		Pilot = World->SpawnActor<AWEVORACharacter>(PilotClass, FVector(0.0f, 0.0f, 110.0f), FRotator::ZeroRotator);
		if (!Pilot) { return; }
		Pilot->SpellWeavingComponent->bShowDebug = false;
		Pilot->SpellWeavingComponent->bLogEvents = false;
		Controller = World->SpawnActor<APlayerController>();
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Pilot);
		// Tests call the same public controls; they do not synthesize real keyboard/viewport input.
		Controller->SetActorTickEnabled(false);
		Step(0.2f);
	}

	~FFlightWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	void Step(float Seconds, float Delta = 1.0f / 60.0f)
	{
		for (float Remaining = Seconds; Remaining > KINDA_SMALL_NUMBER; Remaining -= Delta)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, FMath::Min(Remaining, Delta));
		}
	}

	void PutInAir(float Height = 5000.0f)
	{
		Pilot->SetActorLocation(FVector(0.0f, 0.0f, Height));
		Pilot->GetCharacterMovement()->StopMovementImmediately();
		Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAJumpTest, "WEVORA.Movement.JumpAndCoast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORAJumpTest::RunTest(const FString& Parameters)
{
	FFlightWorld Scene;
	if (!TestNotNull(TEXT("Saved project pilot spawns"), Scene.Pilot)) { return false; }
	AWEVORACharacter* Pilot = Scene.Pilot;
	UCharacterMovementComponent* Movement = Pilot->GetCharacterMovement();
	TestNotNull(TEXT("Saved Blueprint inherits pre-physics flight movement"), Cast<UWEVORAFlightMovementComponent>(Movement));
	TestTrue(TEXT("Test pilot has local control"), Pilot->IsLocallyControlled());
	TestTrue(TEXT("Unpowered pilot settles on physical ground"), Movement->IsMovingOnGround());
	const float StartMana = Pilot->ManaComponent->Mana;
	Pilot->DoJumpStart();
	Scene.Step(1.0f / 60.0f);
	TestTrue(TEXT("Tap immediately launches upward"), Movement->Velocity.Z > 800.0f);
	Pilot->DoJumpEnd();
	const double BeforeAirTap = Movement->Velocity.Z;
	Pilot->DoJumpStart(); Pilot->DoJumpEnd();
	TestEqual(TEXT("A second air tap gives no free jump impulse"), Movement->Velocity.Z, BeforeAirTap);
	float CoastTime = 0.0f;
	bool bSawFall = false;
	for (int32 Frame = 0; Frame < 210; ++Frame)
	{
		Scene.Step(1.0f / 60.0f);
		if (Pilot->FlightState == EWEVORAFlightState::Coasting) { CoastTime += 1.0f / 60.0f; }
		bSawFall |= Pilot->FlightState == EWEVORAFlightState::Falling;
	}
	TestTrue(TEXT("Apex float and gravity transition last about one second"), CoastTime > 1.2f && CoastTime < 1.5f);
	TestTrue(TEXT("No input eventually restores falling"), bSawFall);
	TestEqual(TEXT("Tap and passive float use no mana"), Pilot->ManaComponent->Mana, StartMana);
	TestTrue(TEXT("Actual swept fall lands"), Movement->IsMovingOnGround());
	TestTrue(TEXT("Landing rests above floor instead of floating"), Pilot->GetActorLocation().Z < 110.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAPoweredFlightTest, "WEVORA.Movement.PowerAndPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORAPoweredFlightTest::RunTest(const FString& Parameters)
{
	FFlightWorld Scene;
	if (!TestNotNull(TEXT("Pilot"), Scene.Pilot)) { return false; }
	AWEVORACharacter* Pilot = Scene.Pilot;
	UCharacterMovementComponent* Movement = Pilot->GetCharacterMovement();
	Scene.PutInAir();
	Pilot->DoMove(0.0f, 1.0f);
	Scene.Step(0.5f);
	TestEqual(TEXT("Steering supports altitude"), Pilot->FlightState, EWEVORAFlightState::Hovering);
	TestTrue(TEXT("Powered hover spends mana per elapsed second"), FMath::IsNearlyEqual(Pilot->ManaComponent->Mana, 96.0f, 0.05f));
	TestTrue(TEXT("Hover keeps altitude"), FMath::IsNearlyEqual(Pilot->GetActorLocation().Z, 5000.0f, 1.0f));
	Pilot->DoMove(0.0f, 0.0f);
	Scene.Step(0.1f);
	TestEqual(TEXT("Brief direction change retains paid support"), Pilot->FlightState, EWEVORAFlightState::Hovering);
	Scene.Step(0.25f);
	TestEqual(TEXT("Release enters unpowered coast"), Pilot->FlightState, EWEVORAFlightState::Coasting);
	const float CoastMana = Pilot->ManaComponent->Mana;
	Scene.Step(0.5f);
	TestEqual(TEXT("Residual velocity does not spend mana"), Pilot->ManaComponent->Mana, CoastMana);
	TestTrue(TEXT("Release preserves planar inertia"), Movement->Velocity.Size2D() > 500.0f);
	Scene.Step(1.0f);
	TestEqual(TEXT("Passive support expires"), Pilot->FlightState, EWEVORAFlightState::Falling);
	Pilot->DoMove(1.0f, 0.0f);
	Pilot->DoAscendStart(); Pilot->DoBrakeStart(); Pilot->DoDescendStart();
	const float DiveMana = Pilot->ManaComponent->Mana;
	TestTrue(TEXT("Ctrl immediately cancels upward velocity"), Movement->Velocity.Z <= -300.0f);
	Scene.Step(0.3f);
	TestEqual(TEXT("Ctrl takes priority over all support controls"), Pilot->FlightState, EWEVORAFlightState::Diving);
	TestEqual(TEXT("Dive spends no mana"), Pilot->ManaComponent->Mana, DiveMana);
	TestTrue(TEXT("Alt cannot stop dive gravity"), Movement->Velocity.Z < -500.0f);
	Pilot->DoDescendEnd(); Pilot->DoBrakeEnd();
	Scene.Step(1.0f);
	TestEqual(TEXT("Held Space resumes paid climb"), Pilot->FlightState, EWEVORAFlightState::Ascending);
	TestTrue(TEXT("Climb catches fall and travels upward"), Movement->Velocity.Z > 500.0f);
	TestTrue(TEXT("Ascent has its own mana rate"), FMath::IsNearlyEqual(DiveMana - Pilot->ManaComponent->Mana, 18.0f, 0.1f));
	Pilot->DoAscendEnd();
	Pilot->ManaComponent->ConsumeMana(Pilot->ManaComponent->MaxMana);
	Pilot->DoBrakeStart(); Pilot->DoAscendStart();
	Scene.Step(0.5f);
	TestEqual(TEXT("Empty mana cannot hover or ascend"), Pilot->FlightState, EWEVORAFlightState::Falling);
	TestEqual(TEXT("Mana does not recover in air"), Pilot->ManaComponent->Mana, 0.0f);
	TestTrue(TEXT("Empty mana has normal gravity"), Movement->GravityScale > 1.0f);
	Scene.Controller->UnPossess();
	TestTrue(TEXT("Unpossess cancels altitude support"), Movement->GravityScale > 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORABurstTest, "WEVORA.Movement.BurstAndBrake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORABurstTest::RunTest(const FString& Parameters)
{
	FFlightWorld Scene;
	if (!TestNotNull(TEXT("Pilot"), Scene.Pilot)) { return false; }
	AWEVORACharacter* Pilot = Scene.Pilot;
	UCharacterMovementComponent* Movement = Pilot->GetCharacterMovement();
	Scene.PutInAir();
	Pilot->DoMove(0.0f, 1.0f);
	Scene.Step(0.6f);
	const float PreBurstMana = Pilot->ManaComponent->Mana;
	Pilot->DoBurst();
	TestTrue(TEXT("Stronger burst adds substantial speed"), Movement->Velocity.Size2D() > 2900.0f);
	TestTrue(TEXT("Burst spends one mana cost"), FMath::IsNearlyEqual(PreBurstMana - Pilot->ManaComponent->Mana, 12.0f, 0.01f));
	const float BurstMana = Pilot->ManaComponent->Mana;
	Pilot->DoBurst();
	TestEqual(TEXT("Cooldown blocks both burst and repeat cost"), Pilot->ManaComponent->Mana, BurstMana);
	Scene.Step(1.0f / 60.0f);
	TestTrue(TEXT("Next movement tick does not clamp boost to cruise"), Movement->Velocity.Size2D() > 2800.0f);
	Scene.Step(0.5f);
	Pilot->DoBurst();
	TestTrue(TEXT("Repeated boost respects speed cap"), Movement->Velocity.Size2D() <= 3200.1f);
	Pilot->DoBrakeStart();
	Scene.Step(1.0f);
	TestTrue(TEXT("Alt brakes even while steering is held"), Movement->Velocity.Size2D() < 50.0f);
	TestEqual(TEXT("Alt enables stationary paid hover"), Pilot->FlightState, EWEVORAFlightState::Hovering);
	TestTrue(TEXT("Stationary hover supports altitude"), FMath::Abs(Movement->Velocity.Z) < 1.0f);
	Pilot->ManaComponent->ConsumeMana(100.0f);
	const double BeforeBlockedBurst = Movement->Velocity.Size2D();
	Pilot->DoBrakeEnd(); Pilot->DoBurst();
	TestEqual(TEXT("Empty mana cannot boost"), Movement->Velocity.Size2D(), BeforeBlockedBurst);
	Pilot->DoMove(0.0f, 0.0f);
	Pilot->DoBrakeStart();
	Scene.Step(0.4f);
	TestTrue(TEXT("Empty-mana brake cannot freeze vertical falling"), Movement->Velocity.Z < -300.0f);
	Scene.Step(4.0f);
	TestTrue(TEXT("Empty-mana flight lands on floor"), Movement->IsMovingOnGround());
	TestTrue(TEXT("Mana recovers after landing"), Pilot->ManaComponent->Mana > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAManaTest, "WEVORA.Movement.ManaRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORAManaTest::RunTest(const FString& Parameters)
{
	UWEVORAManaComponent* Mana = NewObject<UWEVORAManaComponent>();
	TestFalse(TEXT("Invalid negative cost cannot grant mana"), Mana->ConsumeMana(-1.0f));
	TestTrue(TEXT("Valid cost"), Mana->ConsumeMana(90.0f));
	Mana->UpdateRecovery(0.5f, true);
	TestEqual(TEXT("Recovery waits after spending"), Mana->Mana, 10.0f);
	Mana->UpdateRecovery(0.5f, true);
	TestEqual(TEXT("Only time after recovery delay counts"), Mana->Mana, 15.0f);
	Mana->UpdateRecovery(2.0f, false);
	TestEqual(TEXT("Airborne recovery disabled"), Mana->Mana, 15.0f);
	TestFalse(TEXT("Insufficient cost reports failure"), Mana->ConsumeMana(20.0f));
	TestEqual(TEXT("Insufficient cost clamps to empty"), Mana->Mana, 0.0f);
	Mana->RestoreMana(1000.0f);
	TestEqual(TEXT("Recovery cannot exceed maximum"), Mana->Mana, 100.0f);
	return true;
}
#endif
