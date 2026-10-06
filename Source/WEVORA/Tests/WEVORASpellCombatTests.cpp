#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WEVORACharacter.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellProjectile.h"
#include "Spell/WEVORASpellReactionComponent.h"
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
		Pilot->DoLook(0.0f, -15.0f); // Existing inverted Look input recognizes Thrust -> Forward.
		if (bFinish) { Pilot->SpellWeavingComponent->EndShape(); }
		Pilot->SpellWeavingComponent->ReleaseWeave();
	}
	AWEVORASpellProjectile* Shot() const
	{
		TActorIterator<AWEVORASpellProjectile> It(World);
		return It ? *It : nullptr;
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
	TestEqual(TEXT("Actual recognized gesture retained"), Fire->Launch.Composition.Gesture, EWEVORAGesture::Thrust);
	TestEqual(TEXT("Thrust resolves to Forward"), Fire->Launch.Spell.Shape, EWEVORASpellShape::Forward);
	TestTrue(TEXT("World direction independent of flight-facing yaw"), Fire->Launch.Direction.X > 0.99f);
	TestEqual(TEXT("Owner is shooter"), Fire->GetOwner(), static_cast<AActor*>(Scene.Pilot));
	TestEqual(TEXT("Instigator is shooter"), Fire->GetInstigator(), static_cast<APawn*>(Scene.Pilot));
	TestTrue(TEXT("Flying shooter ignores its own bolt"), Scene.Pilot->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Fire));
	TestNotNull(TEXT("Placeholder mesh available without new VFX assets"), Fire->Visual->GetStaticMesh().Get());
	TWeakObjectPtr<AWEVORASpellProjectile> FireRef(Fire);
	Scene.Step(0.8f); // Real swept projectile hit; no direct ApplyDamage or hit callback.
	TestEqual(TEXT("Fire hit subtracts damage exactly once"), Scene.Enemy->HealthComponent->Health, 75.0f);
	TestTrue(TEXT("Fire adds Burning state"), Scene.Enemy->SpellReactionComponent->HasState(EWEVORASpellTargetState::Burning));
	TestFalse(TEXT("Hit consumes Fire projectile"), FireRef.IsValid());
	TestFalse(TEXT("Ignore entry cleaned up on impact"), Scene.Pilot->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Fire));
	TestEqual(TEXT("Self HP unchanged"), Scene.Pilot->HealthComponent->Health, 100.0f);
	Scene.Step(3.2f);
	TestTrue(TEXT("Fire periodic damage finishes after three seconds"), FMath::IsNearlyEqual(Scene.Enemy->HealthComponent->Health, 60.0f, 0.01f));
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
	const FVector EnemyBeforeWind = Scene.Enemy->GetActorLocation();
	const float HealthBeforeWind = Scene.Enemy->HealthComponent->Health;
	Scene.Step(0.8f);
	TestEqual(TEXT("Wind does not deal direct damage"), Scene.Enemy->HealthComponent->Health, HealthBeforeWind);
	TestTrue(TEXT("Wind pushes the enemy physically"), Scene.Enemy->GetActorLocation().X > EnemyBeforeWind.X + 50.0f);
	TestTrue(TEXT("Wind also lifts the enemy"), Scene.Enemy->GetActorLocation().Z > EnemyBeforeWind.Z + 20.0f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAReactionRuleTest, "WEVORA.Spell.Combat.ReactionRulesAndStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORAReactionRuleTest::RunTest(const FString& Parameters)
{
	FSpellCombatWorld Scene;
	if (!TestNotNull(TEXT("Enemy"), Scene.Enemy)) { return false; }
	UWEVORASpellReactionComponent* Receiver = Scene.Enemy->SpellReactionComponent;
	if (!TestNotNull(TEXT("Receiver inherited by saved enemy Blueprint"), Receiver)) { return false; }
	FWEVORASpellReactionRule Bonus;
	Bonus.Element = EWEVORASpellElement::Wind;
	Bonus.RequiredState = EWEVORASpellTargetState::Burning;
	Bonus.bMatchShape = true;
	Bonus.Shape = EWEVORASpellShape::Forward;
	FWEVORASpellReactionEffect Damage;
	Damage.Magnitude = 7.0f;
	Bonus.Effects.Add(Damage);
	Receiver->ReactionRules.Add(Bonus);
	FWEVORASpellData Spell;
	Spell.Element = EWEVORASpellElement::Wind;
	FHitResult Hit(Scene.Enemy, Scene.Enemy->Collision, Scene.Enemy->GetActorLocation(), -FVector::ForwardVector);
	TestTrue(TEXT("Wind reacts to an ordinary target"), Receiver->ReceiveSpell(Spell, Hit, Scene.Controller, Scene.Pilot));
	TestEqual(TEXT("Conditional rule requires Burning"), Scene.Enemy->HealthComponent->Health, 100.0f);
	Spell.Element = EWEVORASpellElement::Fire;
	TestTrue(TEXT("Fire accepted"), Receiver->ReceiveSpell(Spell, Hit, Scene.Controller, Scene.Pilot));
	TestEqual(TEXT("Fire direct damage"), Scene.Enemy->HealthComponent->Health, 75.0f);
	Spell.Element = EWEVORASpellElement::Wind;
	Spell.Shape = EWEVORASpellShape::Circle;
	Receiver->ReceiveSpell(Spell, Hit, Scene.Controller, Scene.Pilot);
	TestEqual(TEXT("Conditional shape mismatch"), Scene.Enemy->HealthComponent->Health, 75.0f);
	Spell.Shape = EWEVORASpellShape::Forward;
	Spell.Power = 2.0f;
	Receiver->ReceiveSpell(Spell, Hit, Scene.Controller, Scene.Pilot);
	TestEqual(TEXT("Wind + Burning added only through rule data; Power scales effect"), Scene.Enemy->HealthComponent->Health, 61.0f);
	Scene.Step(3.2f);
	TestTrue(TEXT("Burning dealt exactly its bounded duration damage"), FMath::IsNearlyEqual(Scene.Enemy->HealthComponent->Health, 46.0f, 0.01f));
	TestFalse(TEXT("Burn expires"), Receiver->HasState(EWEVORASpellTargetState::Burning));
	TestFalse(TEXT("No idle tick after reactions end"), Receiver->IsComponentTickEnabled());
	TestFalse(TEXT("No remaining displacement"), Receiver->IsDisplaced());
	const float AfterExpiry = Scene.Enemy->HealthComponent->Health;
	Scene.Step(1.1f);
	TestEqual(TEXT("Expired burn cannot continue damaging"), Scene.Enemy->HealthComponent->Health, AfterExpiry);
	Spell.Power = 0.0f;
	TestFalse(TEXT("Zero Power ignored"), Receiver->ReceiveSpell(Spell, Hit, Scene.Controller, Scene.Pilot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORADisplacementTest, "WEVORA.Spell.Combat.DisplacementWallsAndAI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORADisplacementTest::RunTest(const FString& Parameters)
{
	FSpellCombatWorld Scene;
	if (!TestNotNull(TEXT("Enemy"), Scene.Enemy)) { return false; }
	Scene.Enemy->DetectionRange = 5000.0f;
	Scene.Enemy->MoveSpeed = 450.0f;
	Scene.Enemy->PreferredDistance = 0.0f;
	Scene.Enemy->AttackRange = 0.0f;
	Scene.Enemy->SetActorLocation(FVector(1000, 0, 500));
	AActor* Wall = Scene.Wall(FVector(1160, 0, 500), FVector(5, 200, 300));
	FWEVORASpellData Wind;
	Wind.Element = EWEVORASpellElement::Wind;
	FHitResult Hit(Scene.Enemy, Scene.Enemy->Collision, Scene.Enemy->GetActorLocation(), -FVector::ForwardVector);
	Scene.Enemy->SpellReactionComponent->ReceiveSpell(Wind, Hit, Scene.Controller, Scene.Pilot);
	Scene.Step(0.1f);
	TestTrue(TEXT("AI does not cancel outward displacement"), Scene.Enemy->GetActorLocation().X > 1050.0f);
	Scene.Step(0.1f);
	TestTrue(TEXT("Swept knockback cannot cross the wall"), Scene.Enemy->GetActorLocation().X <= 1111.0f);
	TestFalse(TEXT("Blocking collision ends displacement"), Scene.Enemy->SpellReactionComponent->IsDisplaced());
	Wall->Destroy();
	const float AfterKnockbackX = Scene.Enemy->GetActorLocation().X;
	Scene.Step(0.5f);
	TestTrue(TEXT("AI resumes approach after displacement"), Scene.Enemy->GetActorLocation().X < AfterKnockbackX - 30.0f);
	TestEqual(TEXT("Wind leaves HP unchanged"), Scene.Enemy->HealthComponent->Health, 100.0f);
	// World object opts in using exactly the same component, with no enemy/HP class required.
	AActor* Object = Scene.Wall(FVector(2500, 0, 500), FVector(40));
	Object->GetRootComponent()->SetMobility(EComponentMobility::Movable);
	UWEVORASpellReactionComponent* Receiver = NewObject<UWEVORASpellReactionComponent>(Object);
	Object->AddInstanceComponent(Receiver);
	Receiver->RegisterComponent();
	FHitResult ObjectHit(Object, Cast<UPrimitiveComponent>(Object->GetRootComponent()), Object->GetActorLocation(), -FVector::ForwardVector);
	TestTrue(TEXT("World object accepts Wind without HP"), Receiver->ReceiveSpell(Wind, ObjectHit, Scene.Controller, Scene.Pilot));
	Scene.Step(0.2f);
	TestTrue(TEXT("World object moved through generic receiver"), Object->GetActorLocation().X > 2600.0f);
	Object->Destroy();
	TestFalse(TEXT("EndPlay clears active displacement"), Receiver->IsDisplaced());
	AActor* PhysicsObject = Scene.Wall(FVector(3000, 0, 500), FVector(40));
	UBoxComponent* PhysicsBody = CastChecked<UBoxComponent>(PhysicsObject->GetRootComponent());
	PhysicsBody->SetMobility(EComponentMobility::Movable);
	PhysicsBody->SetSimulatePhysics(true);
	PhysicsBody->SetEnableGravity(false);
	UWEVORASpellReactionComponent* PhysicsReceiver = NewObject<UWEVORASpellReactionComponent>(PhysicsObject);
	PhysicsObject->AddInstanceComponent(PhysicsReceiver);
	PhysicsReceiver->RegisterComponent();
	FHitResult PhysicsHit(PhysicsObject, PhysicsBody, PhysicsObject->GetActorLocation(), -FVector::ForwardVector);
	PhysicsReceiver->ReceiveSpell(Wind, PhysicsHit, Scene.Controller, Scene.Pilot);
	Scene.Step(0.2f);
	TestTrue(TEXT("World physics body receives Wind impulse"), PhysicsObject->GetActorLocation().X > 3100.0f);
	TestFalse(TEXT("Physics body does not also use fallback displacement"), PhysicsReceiver->IsDisplaced());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORASpellDataTest, "WEVORA.Spell.Combat.DataResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWEVORASpellDataTest::RunTest(const FString& Parameters)
{
	FSpellCombatWorld Scene;
	if (!TestNotNull(TEXT("Pilot"), Scene.Pilot)) { return false; }
	FWEVORASpellContext Context;
	Context.Gesture = EWEVORAGesture::Thrust;
	FWEVORASpellLaunch Launch;
	TestTrue(TEXT("Recognized Thrust resolves"), Scene.Pilot->SpellCastComponent->ResolveSpell(Context, Launch));
	TestEqual(TEXT("Gesture is mapped to world Forward"), Launch.Spell.Shape, EWEVORASpellShape::Forward);
	Scene.Pilot->SpellCastComponent->ShapeProfiles[EWEVORAGesture::Thrust].Delivery = EWEVORASpellDelivery::Area;
	Scene.Weave();
	TestNull(TEXT("Unimplemented delivery cannot silently fire a projectile"), Scene.Shot());
	Scene.Pilot->SpellCastComponent->ShapeProfiles.Remove(EWEVORAGesture::Thrust);
	TestFalse(TEXT("Missing shape rejected"), Scene.Pilot->SpellCastComponent->ResolveSpell(Context, Launch));
	Scene.Pilot->SpellWeavingComponent->AvailableElements = { EWEVORASpellElement::Wind };
	Scene.Pilot->SpellWeavingComponent->CycleElement();
	TestEqual(TEXT("Selection follows configured elements"), Scene.Pilot->SpellWeavingComponent->Context.Element, EWEVORASpellElement::Wind);
	return true;
}
#endif
