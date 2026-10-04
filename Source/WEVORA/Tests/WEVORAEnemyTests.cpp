#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AI/WEVORAEnemy.h"
#include "AI/WEVORAEnemyProjectile.h"
#include "AI/WEVORAHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWEVORAEnemyTest, "WEVORA.AI.TargetAndAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWEVORAEnemyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	FURL URL;
	URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
	World->SetGameMode(URL);
	World->InitializeActorsForPlay(URL);
	auto StartActor = [](AActor* Actor)
	{
		if (!Actor->IsActorInitialized())
		{
			Actor->PreInitializeComponents();
			Actor->InitializeComponents();
			Actor->PostInitializeComponents();
		}
		Actor->DispatchBeginPlay();
	};
	APlayerController* PlayerController = World->SpawnActor<APlayerController>();
	ADefaultPawn* Player = World->SpawnActor<ADefaultPawn>(FVector(1000, 0, 200), FRotator::ZeroRotator);
	UWEVORAHealthComponent* Health = NewObject<UWEVORAHealthComponent>(Player);
	Player->AddInstanceComponent(Health);
	Health->RegisterComponent();
	StartActor(Player);
	PlayerController->Possess(Player);
	UClass* EnemyClass = LoadClass<AWEVORAEnemy>(nullptr, TEXT("/Game/WEVORA/AI/BP_WEVORAEnemy.BP_WEVORAEnemy_C"));
	if (!TestNotNull(TEXT("Saved enemy Blueprint loads"), EnemyClass))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	AWEVORAEnemy* Enemy = World->SpawnActor<AWEVORAEnemy>(EnemyClass, FVector(0, 0, 200), FRotator::ZeroRotator);
	Enemy->WindupTime = 0.0f;
	StartActor(Enemy);
	Enemy->AttackRange = 200.0f;
	Enemy->Tick(0.016f);
	Enemy->Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Floating movement approaches player outside attack range"), Enemy->GetActorLocation().X > 0.0f);
	TestFalse(TEXT("Outside attack range does not start windup"), Enemy->bWindingUp);
	Enemy->SetActorLocation(FVector(0, 0, 200));
	Enemy->Movement->StopMovementImmediately();
	Enemy->AttackRange = 1500.0f;
	auto CountShots = [&]()
	{
		int32 Count = 0;
		for (TActorIterator<AWEVORAEnemyProjectile> It(World); It; ++It) { ++Count; }
		return Count;
	};
	Enemy->Tick(0.016f);
	TestEqual(TEXT("Acquires player without a Player tag"), Enemy->Target.Get(), static_cast<APawn*>(Player));
	TestTrue(TEXT("Attack starts with a windup"), Enemy->bWindingUp);
	Enemy->Tick(0.016f);
	TestEqual(TEXT("One aimed projectile spawned"), CountShots(), 1);
	Enemy->Tick(0.016f);
	Enemy->Tick(0.016f);
	TestEqual(TEXT("Cooldown prevents repeated shots"), CountShots(), 1);
	TestEqual(TEXT("Own projectile does not obstruct target visibility"), Enemy->Target.Get(), static_cast<APawn*>(Player));
	AWEVORAEnemyProjectile* Shot = nullptr;
	for (TActorIterator<AWEVORAEnemyProjectile> It(World); It; ++It) { Shot = *It; break; }
	if (TestNotNull(TEXT("Projectile"), Shot))
	{
		StartActor(Shot);
		TestTrue(TEXT("Shot aimed at player"), Shot->GetActorForwardVector().Equals(
			(Player->GetActorLocation() - Shot->GetActorLocation()).GetSafeNormal(), 0.001f));
		// Drive a real swept projectile collision; no direct invocation of the damage handler.
		Shot->Movement->TickComponent(0.8f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Projectile hit reduces player HP once"), Health->Health, 90.0f);
		TestTrue(TEXT("Hit consumes projectile"), Shot->IsActorBeingDestroyed());
	}
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->AddInstanceComponent(Box);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(30, 300, 300));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Wall->SetActorLocation(FVector(500, 0, 200));
	Enemy->Tick(0.016f);
	TestNull(TEXT("Wall prevents target acquisition"), Enemy->Target.Get());
	TestFalse(TEXT("Occlusion cancels windup"), Enemy->bWindingUp);
	Wall->Destroy();
	Player->SetActorLocation(FVector(5000, 0, 200));
	Enemy->Tick(0.016f);
	TestNull(TEXT("Out of detection range is ignored"), Enemy->Target.Get());
	Player->SetActorLocation(FVector(1000, 0, 200));
	UGameplayStatics::ApplyDamage(Player, -20.0f, nullptr, Enemy, UDamageType::StaticClass());
	TestEqual(TEXT("Negative damage cannot heal"), Health->Health, 90.0f);
	UGameplayStatics::ApplyDamage(Player, 200.0f, nullptr, Enemy, UDamageType::StaticClass());
	TestEqual(TEXT("Health clamps at zero"), Health->Health, 0.0f);
	Enemy->Tick(0.016f);
	TestNull(TEXT("Defeated player is ignored"), Enemy->Target.Get());
	UGameplayStatics::ApplyDamage(Enemy, 200.0f, PlayerController, Player, UDamageType::StaticClass());
	TestTrue(TEXT("Enemy destroyed on death"), Enemy->IsActorBeingDestroyed());
	Player->RouteEndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("Damage subscription removed on end play"), Player->OnTakeAnyDamage.IsBound());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
