#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WEVORACharacter.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellProjectile.h"
#include "AI/WEVORAEnemy.h"
#include "AI/WEVORAHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"

namespace
{
/** Actual saved character/enemy Blueprints and world ticks; no synthesized keyboard or rendered play. */
struct FSpellCombatWorld
{
	TGuardValue<uint64> FrameGuard{GFrameCounter, GFrameCounter};
	UWorld* World = nullptr;
	AWEVORACharacter* Pilot = nullptr;
	AWEVORAEnemy* Enemy = nullptr;
	APlayerController* Controller = nullptr;

	FSpellCombatWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		UClass* PilotClass = LoadClass<AWEVORACharacter>(nullptr,
			TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
		UClass* EnemyClass = LoadClass<AWEVORAEnemy>(nullptr,
			TEXT("/Game/WEVORA/AI/BP_WEVORAEnemy.BP_WEVORAEnemy_C"));
		if (!PilotClass || !EnemyClass) { return; }
		Pilot = World->SpawnActor<AWEVORACharacter>(PilotClass, FVector(0, 0, 500), FRotator(0, 90, 0));
		if (!Pilot) { return; }
		Pilot->SpellWeavingComponent->bShowDebug = false;
		Pilot->SpellWeavingComponent->bLogEvents = false;
		Pilot->SpellCastComponent->bLogEvents = false;
		Controller = World->SpawnActor<APlayerController>();
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Pilot);
		Controller->SetActorTickEnabled(false);
		// No viewport/ULocalPlayer exists in this fixture. Explicitly allow camera calculation on the server.
		Controller->PlayerCameraManager->bUseClientSideCameraUpdates = false;
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Pilot->DoBrakeStart();
		Enemy = World->SpawnActor<AWEVORAEnemy>(EnemyClass, FVector(1200, 0, 500), FRotator::ZeroRotator);
		if (Enemy) { Enemy->DetectionRange = 0; Enemy->MoveSpeed = 0; }
		Step(0.05f);
		UpdateView();
	}
	~FSpellCombatWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	void Step(float Seconds)
	{
		for (float Remaining = Seconds; Remaining > KINDA_SMALL_NUMBER; Remaining -= 1.0f / 60.0f)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, FMath::Min(Remaining, 1.0f / 60.0f));
		}
	}
	void UpdateView()
	{
		Controller->SetViewTarget(Pilot);
		Controller->PlayerCameraManager->UpdateCamera(0.016f);
	}
	void AimTarget(FRotator Rotation)
	{
		Controller->SetControlRotation(Rotation);
		Step(0.5f); // Let the existing spring-arm camera rotation lag settle.
		UpdateView();
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		Enemy->SetActorLocation(ViewLocation + ViewRotation.Vector() * 1600.0f);
	}
	void Weave(bool bFinish = true)
	{
		Pilot->SpellWeavingComponent->BeginWeave();
		Pilot->SpellWeavingComponent->BeginShape();
		Pilot->DoLook(15.0f, 0.0f); // Existing gesture input path; a Sweep.
		if (bFinish) { Pilot->SpellWeavingComponent->EndShape(); }
		Pilot->SpellWeavingComponent->ReleaseWeave();
	}
	AWEVORASpellProjectile* Shot() const
	{
		for (TActorIterator<AWEVORASpellProjectile> It(World); It; ++It) { return *It; }
		return nullptr;
	}
	AActor* Wall(FVector Location, FVector Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Box);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORASpellAirCombatTest, "WEVORA.Spell.Combat.AirborneCastToDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORASpellAirCombatTest::RunTest(const FString& Parameters)
{
	FSpellCombatWorld Scene;
	if (!TestNotNull(TEXT("Saved character with cast component"), Scene.Pilot)
		|| !TestNotNull(TEXT("Saved enemy Blueprint"), Scene.Enemy)) { return false; }
	TestNotNull(TEXT("Native cast component inherited by existing Blueprint"), Scene.Pilot->SpellCastComponent);
	Scene.AimTarget(FRotator::ZeroRotator);
	TestTrue(TEXT("Pilot is airborne"), Scene.Pilot->GetCharacterMovement()->IsFalling());
	TestEqual(TEXT("Pilot maintains paid hover while weaving"), Scene.Pilot->FlightState, EWEVORAFlightState::Hovering);
	Scene.Weave();
	AWEVORASpellProjectile* Fire = Scene.Shot();
	if (!TestNotNull(TEXT("Weaving and release spawn Fire"), Fire)) { return false; }
	const float FireSpeed = Fire->Launch.Parameters.Speed;
	UClass* FireClass = Fire->GetClass();
	TestEqual(TEXT("Fire context retained"), Fire->Launch.Composition.Element, EWEVORASpellElement::Fire);
	TestEqual(TEXT("Actual recognized gesture retained"), Fire->Launch.Composition.Gesture, EWEVORAGesture::Sweep);
	TestTrue(TEXT("World direction independent of flight-facing yaw"), Fire->Launch.Direction.X > 0.99f);
	TestEqual(TEXT("Owner is shooter"), Fire->GetOwner(), static_cast<AActor*>(Scene.Pilot));
	TestEqual(TEXT("Instigator is shooter"), Fire->GetInstigator(), static_cast<APawn*>(Scene.Pilot));
	TestTrue(TEXT("Flying shooter ignores its own bolt"), Scene.Pilot->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Fire));
	TestNotNull(TEXT("Placeholder mesh available without new VFX assets"), Fire->Visual->GetStaticMesh().Get());
	TWeakObjectPtr<AWEVORASpellProjectile> FireRef(Fire);
	Scene.Step(0.8f); // Real swept projectile hit; no direct ApplyDamage or hit callback.
	TestEqual(TEXT("Fire hit subtracts damage exactly once"), Scene.Enemy->HealthComponent->Health, 75.0f);
	TestFalse(TEXT("Hit consumes Fire projectile"), FireRef.IsValid());
	TestFalse(TEXT("Ignore entry cleaned up on impact"), Scene.Pilot->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Fire));
	TestEqual(TEXT("Self HP unchanged"), Scene.Pilot->HealthComponent->Health, 100.0f);
	Scene.Pilot->SpellWeavingComponent->CycleElement();
	Scene.AimTarget(FRotator(20.0f, 35.0f, 0));
	Scene.Weave();
	AWEVORASpellProjectile* Wind = Scene.Shot();
	if (!TestNotNull(TEXT("Wind uses same projectile class"), Wind)) { return false; }
	TestEqual(TEXT("Shared runtime class"), Wind->GetClass(), FireClass);
	TestEqual(TEXT("Wind selection preserved"), Wind->Launch.Composition.Element, EWEVORASpellElement::Wind);
	TestTrue(TEXT("Wind parameters resolved from data"), Wind->Launch.Parameters.Speed > FireSpeed);
	TestTrue(TEXT("Camera pitch directs airborne shot upward"), Wind->Launch.Direction.Z > 0.2f);
	Scene.Pilot->SpellWeavingComponent->CycleElement();
	TestEqual(TEXT("In-flight composition is a snapshot"), Wind->Launch.Composition.Element, EWEVORASpellElement::Wind);
	Scene.Step(0.8f);
	TestEqual(TEXT("Wind swept hit also reduces enemy HP"), Scene.Enemy->HealthComponent->Health, 50.0f);
	TestEqual(TEXT("Casting never resets flight support"), Scene.Pilot->FlightState, EWEVORAFlightState::Hovering);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORASpellSafetyTest, "WEVORA.Spell.Combat.CancelWallsAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORASpellSafetyTest::RunTest(const FString& Parameters)
{
	FSpellCombatWorld Scene;
	if (!TestNotNull(TEXT("Pilot"), Scene.Pilot) || !TestNotNull(TEXT("Enemy"), Scene.Enemy)) { return false; }
	Scene.AimTarget(FRotator::ZeroRotator);
	Scene.Weave(false);
	TestNull(TEXT("Release before EndShape cancels without projectile"), Scene.Shot());
	Scene.Pilot->SpellWeavingComponent->BeginWeave();
	Scene.Pilot->SpellWeavingComponent->BeginShape();
	Scene.Pilot->SpellWeavingComponent->EndShape();
	Scene.Pilot->SpellWeavingComponent->ReleaseWeave();
	TestNull(TEXT("Invalid gesture never spawns"), Scene.Shot());
	AActor* NearWall = Scene.Wall(Scene.Pilot->GetPawnViewLocation() + FVector(40, 0, 0), FVector(5, 300, 300));
	Scene.Weave();
	TestNull(TEXT("Short muzzle sweep prevents spawning through nearby wall"), Scene.Shot());
	NearWall->Destroy();
	AActor* FarWall = Scene.Wall(FVector(500, 0, 500), FVector(5, 300, 300));
	Scene.Weave();
	AWEVORASpellProjectile* WallShot = Scene.Shot();
	if (!TestNotNull(TEXT("Clear muzzle spawns towards wall"), WallShot)) { return false; }
	TWeakObjectPtr<AWEVORASpellProjectile> WallShotRef(WallShot);
	Scene.Step(0.5f);
	TestFalse(TEXT("Swept wall impact consumes projectile"), WallShotRef.IsValid());
	TestEqual(TEXT("Enemy behind wall takes no damage"), Scene.Enemy->HealthComponent->Health, 100.0f);
	FarWall->Destroy();
	Scene.Enemy->SetActorLocation(FVector(20000, 0, 500));
	Scene.Pilot->SpellCastComponent->ElementParameters[EWEVORASpellElement::Fire].Lifetime = 0.1f;
	Scene.Weave();
	AWEVORASpellProjectile* Miss = Scene.Shot();
	if (!TestNotNull(TEXT("Missed projectile spawns"), Miss)) { return false; }
	TWeakObjectPtr<AWEVORASpellProjectile> MissRef(Miss);
	Scene.Step(0.2f);
	TestFalse(TEXT("Missed projectile expires"), MissRef.IsValid());
	TestFalse(TEXT("Ignore entry removed on expiration"), Scene.Pilot->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Miss));
	Scene.Controller->UnPossess();
	Scene.Weave();
	TestNull(TEXT("Unpossessed character cannot attack"), Scene.Shot());
	Scene.Pilot->RouteEndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("Cast subscription removed on end play"), Scene.Pilot->SpellWeavingComponent->OnSpellCast.IsBound());
	return true;
}
#endif
