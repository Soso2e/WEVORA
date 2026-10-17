#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Playground/WEVORACombatPlayground.h"
#include "Playground/WEVORAPlaygroundTarget.h"
#include "WEVORACharacter.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellProjectile.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellVolume.h"
#include "Movement/WEVORAManaComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include <limits>

namespace
{
struct FPlaygroundWorld
{
	TGuardValue<uint64> FrameGuard{GFrameCounter, GFrameCounter};
	UWorld* World;
	FPlaygroundWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
	}
	~FPlaygroundWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	void Step(float Seconds)
	{
		for (float Left = Seconds; Left > KINDA_SMALL_NUMBER; Left -= 1.0f / 60)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, FMath::Min(Left, 1.0f / 60));
		}
	}
	AWEVORAPlaygroundTarget* Target(FVector Position = FVector(1000, 0, 500))
	{
		return World->SpawnActor<AWEVORAPlaygroundTarget>(Position, FRotator::ZeroRotator);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaygroundResetTest, "WEVORA.Playground.ResetAndRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlaygroundResetTest::RunTest(const FString& Parameters)
{
	FPlaygroundWorld Scene;
	AWEVORAPlaygroundTarget* Target = Scene.Target();
	const FVector Initial = Target->GetActorLocation();
	Target->RespawnDelay = 0.2f;
	FWEVORASpellData Fire;
	FHitResult Hit(Target, Target->Collision, Initial, FVector::UpVector);
	TestTrue(TEXT("Uses existing reaction pipeline"), Target->SpellReactionComponent->ReceiveSpell(Fire, Hit, nullptr, nullptr));
	TestTrue(TEXT("Burning set"), Target->SpellReactionComponent->HasState(EWEVORASpellTargetState::Burning));
	Target->SetActorLocation(Initial + FVector(200, 0, 0));
	Target->ResetTarget();
	TestEqual(TEXT("Reset restores transform"), Target->GetActorLocation(), Initial);
	TestEqual(TEXT("Reset restores health"), Target->HealthComponent->Health, Target->HealthComponent->MaxHealth);
	Scene.Step(1.1f);
	TestEqual(TEXT("Reset prevents delayed burn damage"), Target->HealthComponent->Health, Target->HealthComponent->MaxHealth);
	UGameplayStatics::ApplyDamage(Target, 1000, nullptr, nullptr, UDamageType::StaticClass());
	TestTrue(TEXT("Defeat hides retained actor"), Target->IsHidden());
	TestFalse(TEXT("Defeat disables collision"), Target->GetActorEnableCollision());
	Scene.Step(0.35f);
	TestTrue(TEXT("Timer respawns without actor replacement"), IsValid(Target) && Target->HealthComponent->IsAlive() && !Target->IsHidden());
	TestTrue(TEXT("Respawn restores collision"), Target->GetActorEnableCollision());
	// Explicit reset must cancel a pending respawn, even if the target is hit again immediately.
	UGameplayStatics::ApplyDamage(Target, 1000, nullptr, nullptr, UDamageType::StaticClass());
	Target->ResetTarget();
	UGameplayStatics::ApplyDamage(Target, 10, nullptr, nullptr, UDamageType::StaticClass());
	Scene.Step(0.35f);
	TestEqual(TEXT("No old timer heals a new trial"), Target->HealthComponent->Health, Target->HealthComponent->MaxHealth - 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaygroundMotionTest, "WEVORA.Playground.MotionAndWind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlaygroundMotionTest::RunTest(const FString& Parameters)
{
	FPlaygroundWorld Scene;
	AWEVORAPlaygroundTarget* Target = Scene.Target();
	Target->MotionAmplitude = 300;
	Target->MotionSpeed = 300;
	const FVector Initial = Target->GetActorLocation();
	Scene.Step(0.5f);
	TestTrue(TEXT("Strafing has measurable lead velocity"), Target->GetActorLocation().Y > Initial.Y + 100 && Target->MeasuredVelocity.Y > 200);
	FWEVORASpellData Wind;
	Wind.Element = EWEVORASpellElement::Wind;
	FHitResult Hit(Target, Target->Collision, Target->GetActorLocation(), FVector::UpVector);
	Target->SpellReactionComponent->ReceiveSpell(Wind, Hit, nullptr, nullptr);
	Scene.Step(0.7f);
	TestTrue(TEXT("Wind displacement persists after motion resumes"), Target->GetActorLocation().X > Initial.X + 100);
	Target->ResetTarget();
	TestEqual(TEXT("Reset clears displacement and phase"), Target->GetActorLocation(), Initial);
	Target->MotionAxis = FVector::UpVector;
	Scene.Step(0.5f);
	TestTrue(TEXT("Vertical path moves upward"), Target->GetActorLocation().Z > Initial.Z + 100);
	TestTrue(TEXT("Vertical path leaves lateral location unchanged"), FMath::IsNearlyEqual(Target->GetActorLocation().Y, Initial.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaygroundTrialTest, "WEVORA.Playground.FullTrialReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlaygroundTrialTest::RunTest(const FString& Parameters)
{
	FPlaygroundWorld Scene;
	AWEVORAPlaygroundTarget* Target = Scene.Target();
	UClass* Class = LoadClass<AWEVORACharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	if (!TestNotNull(TEXT("Saved player class"), Class)) { return false; }
	AWEVORACharacter* Player = Scene.World->SpawnActor<AWEVORACharacter>(Class, FVector(0, 0, 500), FRotator::ZeroRotator);
	APlayerController* PC = Scene.World->SpawnActor<APlayerController>();
	PC->Possess(Player);
	AWEVORACombatPlayground* Trial = Scene.World->SpawnActor<AWEVORACombatPlayground>();
	Scene.Step(0.15f);
	Player->ManaComponent->ConsumeMana(50);
	Player->SpellWeavingComponent->BeginAttack();
	Scene.World->SpawnActor<AWEVORASpellProjectile>(FVector(0, 800, 500), FRotator::ZeroRotator);
	Trial->ResetTrial();
	TestEqual(TEXT("Full mana"), Player->ManaComponent->Mana, Player->ManaComponent->MaxMana);
	TestEqual(TEXT("Held charge cancelled"), Player->SpellWeavingComponent->State, EWEVORAWeavingState::Idle);
	TestEqual(TEXT("Cooldown cleared"), Player->SpellWeavingComponent->GetCooldownRemaining(), 0.0f);
	TestEqual(TEXT("All target HP restored with trial tuning"), Target->HealthComponent->Health, Trial->TargetHealth);
	TestTrue(TEXT("Bounds detect roof and floor"), Trial->IsOutsideBounds(FVector(0,0,7000)) && Trial->IsOutsideBounds(FVector(0,0,-500)));
	Player->SetActorLocation(FVector(11000, 0, 100));
	Scene.Step(0.15f);
	TestTrue(TEXT("Boundary returns player to start"), Player->GetActorLocation().X < 100);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaygroundBallisticsTest, "WEVORA.Playground.BallisticsAndCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlaygroundBallisticsTest::RunTest(const FString& Parameters)
{
	FPlaygroundWorld Scene;
	UClass* Class = LoadClass<AWEVORACharacter>(nullptr, TEXT("/Game/WEVORA/Playground/BP_PlaygroundPlayer.BP_PlaygroundPlayer_C"));
	if (!TestNotNull(TEXT("Saved independent player"), Class)) { return false; }
	AWEVORACharacter* Player = Scene.World->SpawnActor<AWEVORACharacter>(Class, FVector(0,0,500), FRotator::ZeroRotator);
	AWEVORACombatPlayground* Trial = Scene.World->SpawnActor<AWEVORACombatPlayground>();
	FWEVORASpellContext Context;
	Context.Gesture = EWEVORAGesture::Thrust;
	FWEVORASpellLaunch Normal, Charged, Lightning;
	TestTrue(TEXT("Saved Blueprint plus trial resolves"), Player->SpellCastComponent->ResolveCastLaunch(Context, Normal));
	Context.ChargeAlpha = 1;
	Player->SpellCastComponent->ResolveCastLaunch(Context, Charged);
	TestTrue(TEXT("Charge trades preparation for larger collision"), Charged.Parameters.Radius > Normal.Parameters.Radius * 1.5f);
	TestTrue(TEXT("Charge retains existing power reward"), Charged.Spell.Power >= Normal.Spell.Power * 2);
	Context.Element = EWEVORASpellElement::Lightning;
	Player->SpellCastComponent->ResolveCastLaunch(Context, Lightning);
	TestTrue(TEXT("Lightning has distinct high speed"), Lightning.Parameters.Speed > Normal.Parameters.Speed * 3);
	Trial->ProjectileSpeedMultiplier = 0.5f;
	Player->SpellCastComponent->ResolveCastLaunch(Context, Lightning);
	TestEqual(TEXT("Trial speed multiplier adjusts resolved Blueprint data"), Lightning.Parameters.Speed, Trial->LightningSpeed * 0.5f);
	Normal.Direction = FVector::ForwardVector;
	Normal.Parameters.Speed = 1000;
	Normal.Parameters.MaxDistance = 300;
	Normal.Parameters.Lifetime = 4;
	const FTransform Transform(FVector(0,4000,2000));
	AWEVORASpellProjectile* Shot = Scene.World->SpawnActorDeferred<AWEVORASpellProjectile>(AWEVORASpellProjectile::StaticClass(), Transform);
	Shot->InitializeSpell(Normal);
	Shot->FinishSpawning(Transform);
	TestTrue(TEXT("Projectile lifetime respects distance cap"), Shot->GetLifeSpan() <= 0.301f);
	TestFalse(TEXT("Default projectile has no homing"), Shot->Movement->bIsHomingProjectile);
	Scene.Step(0.4f);
	TestTrue(TEXT("Distance cap expires unhit bolt"), Shot->IsActorBeingDestroyed());
	Trial->ProjectileRadiusMultiplier = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Invalid tuning rejected before physics"), Player->SpellCastComponent->ResolveCastLaunch(Context, Lightning));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaygroundShapeTest, "WEVORA.Playground.SweepFieldAndOcclusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlaygroundShapeTest::RunTest(const FString& Parameters)
{
	FPlaygroundWorld Scene;
	AWEVORAPlaygroundTarget* Center = Scene.Target(FVector(700,0,500));
	AWEVORAPlaygroundTarget* Side = Scene.Target(FVector(700,450,500));
	AWEVORAPlaygroundTarget* Far = Scene.Target(FVector(2000,0,500));
	FWEVORASpellLaunch Launch;
	Launch.Spell.Element = EWEVORASpellElement::Lightning; // Isolate direct damage from burn.
	Launch.Spell.Shape = EWEVORASpellShape::Sweep;
	Launch.Spell.Delivery = EWEVORASpellDelivery::Sweep;
	Launch.Direction = FVector::ForwardVector;
	Launch.Volume.Range = 1000;
	Launch.Volume.Width = 650;
	Launch.Volume.Height = 200;
	auto Deliver = [&](FVector Location)
	{
		const FTransform Transform(FRotator::ZeroRotator, Location);
		AWEVORASpellVolume* Volume = Scene.World->SpawnActorDeferred<AWEVORASpellVolume>(AWEVORASpellVolume::StaticClass(), Transform);
		Volume->InitializeSpell(Launch);
		Volume->FinishSpawning(Transform);
		return Volume;
	};
	Deliver(FVector(0,0,500));
	TestEqual(TEXT("Sweep reaches center"), Center->HealthComponent->Health, 75.0f);
	TestEqual(TEXT("Sweep reaches off-axis target in one cast"), Side->HealthComponent->Health, 75.0f);
	TestEqual(TEXT("Sweep cannot reach distant target"), Far->HealthComponent->Health, 100.0f);
	Scene.Step(0.3f);
	TestEqual(TEXT("Single sweep never ticks repeat damage"), Side->HealthComponent->Health, 75.0f);
	AActor* Wall = Scene.World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->AddInstanceComponent(Box);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(20,1000,500));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Wall->SetActorLocation(FVector(350,0,500));
	Deliver(FVector(0,0,500));
	TestEqual(TEXT("Wall rejects wide attack damage"), Side->HealthComponent->Health, 75.0f);
	Wall->Destroy();
	Center->ResetTarget();
	Side->ResetTarget();
	Launch.Spell.Shape = EWEVORASpellShape::Circle;
	Launch.Spell.Delivery = EWEVORASpellDelivery::Field;
	Launch.Spell.Power = 0.2f;
	Launch.Volume.Width = 150;
	Launch.Volume.PulseInterval = 0.2f;
	Launch.Volume.Duration = 0.55f;
	AWEVORASpellVolume* Field = Deliver(Center->GetActorLocation());
	TestEqual(TEXT("Circle begins with bounded pulse"), Center->HealthComponent->Health, 95.0f);
	Scene.Step(0.45f);
	TestEqual(TEXT("Circle sustains control in one area"), Center->HealthComponent->Health, 85.0f);
	TestEqual(TEXT("Field is local"), Side->HealthComponent->Health, 100.0f);
	Scene.Step(0.3f);
	TestTrue(TEXT("Field expires"), Field->IsActorBeingDestroyed());
	const float EndHealth = Center->HealthComponent->Health;
	Scene.Step(0.4f);
	TestEqual(TEXT("Expired field has no lingering timer damage"), Center->HealthComponent->Health, EndHealth);
	return true;
}
#endif
