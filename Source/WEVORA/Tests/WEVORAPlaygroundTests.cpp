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
#include "Movement/WEVORAManaComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
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
#endif
