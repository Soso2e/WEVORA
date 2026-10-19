#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Solo/WEVORARunDirector.h"
#include "WEVORACharacter.h"
#include "AI/WEVORAHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Movement/WEVORAManaComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
struct FSoloWorld
{
	TGuardValue<uint64> FrameGuard{GFrameCounter, GFrameCounter};
	UWorld* World;
	AWEVORACharacter* Player;
	AWEVORARunDirector* Run;
	FSoloWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL); World->BeginPlay();
		UClass* Class = LoadClass<AWEVORACharacter>(nullptr, TEXT("/Game/WEVORA/Playground/BP_PlaygroundPlayer.BP_PlaygroundPlayer_C"));
		Player = World->SpawnActor<AWEVORACharacter>(Class, FVector(0,0,500), FRotator::ZeroRotator);
		APlayerController* PC = World->SpawnActor<APlayerController>(); PC->Possess(Player);
		Run = World->SpawnActor<AWEVORARunDirector>(); Run->bAutoStart = false;
		Run->Config = NewObject<UWEVORAArenaConfig>(Run);
		Run->Config->Waves.SetNum(2);
		Run->Config->Waves[0].Count = 3; Run->Config->Waves[0].AliveCap = 2;
		Run->Config->Waves[0].SpawnInterval = 0.1f;
		Run->Config->Waves[1].Count = 1; Run->Config->Waves[1].bBoss = true;
		Run->Config->PreparationSeconds = 0;
	}
	~FSoloWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	void Step(float Seconds = 0.1f) { ++GFrameCounter; Run->Tick(Seconds); }
	void KillEnemies() { for (AWEVORAEnemy* Enemy : Run->Enemies) { UGameplayStatics::ApplyDamage(Enemy,100000,nullptr,Player,UDamageType::StaticClass()); } }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloWavesTest, "WEVORA.Solo.WavesCapAndProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloWavesTest::RunTest(const FString& Parameters)
{
	FSoloWorld S;
	AWEVORAEnemy* Practice = S.World->SpawnActor<AWEVORAEnemy>(FVector(8000,0,500), FRotator::ZeroRotator);
	TestTrue(TEXT("Start"), S.Run->StartRun(S.Player));
	TestFalse(TEXT("Cannot start twice"), S.Run->StartRun(S.Player));
	S.Step(); S.Step(); TestEqual(TEXT("Incremental spawn"), S.Run->Enemies.Num(), 1);
	S.Step(); S.Step(5); TestEqual(TEXT("Cap including hitch"), S.Run->Enemies.Num(), 2);
	S.KillEnemies(); S.Step(); TestEqual(TEXT("Two real kills"), S.Run->KilledCount, 2);
	S.KillEnemies(); S.Step(); S.Step();
	TestEqual(TEXT("Reward exactly once"), S.Run->State, EWEVORARunState::UpgradeChoice);
	S.Step(); TestEqual(TEXT("Waits for selection"), S.Run->WaveIndex, 0);
	TestTrue(TEXT("Choose one card"), S.Run->SelectUpgrade(0)); S.Step(); S.Step();
	TestEqual(TEXT("Boss encounter"), S.Run->State, EWEVORARunState::BossFight);
	S.KillEnemies(); S.Step(); S.Step();
	TestEqual(TEXT("Victory uses real health"), S.Run->State, EWEVORARunState::Victory);
	TestTrue(TEXT("Non-run actor retained"), IsValid(Practice));
	S.Run->Retry(); TestEqual(TEXT("Retry starts at wave zero"), S.Run->WaveIndex, 0);
	TestEqual(TEXT("Retry counters reset"), S.Run->KilledCount, 0);
	TestEqual(TEXT("Player restored"), S.Player->HealthComponent->Health, S.Player->HealthComponent->MaxHealth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloDefeatTest, "WEVORA.Solo.DefeatPrecedenceAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloDefeatTest::RunTest(const FString& Parameters)
{
	FSoloWorld S;
	S.Run->Config->Waves.SetNum(1); S.Run->Config->Waves[0].Count = 1;
	S.Run->StartRun(S.Player); S.Step(); S.Step(); S.KillEnemies(); S.Step();
	UGameplayStatics::ApplyDamage(S.Player,100000,nullptr,nullptr,UDamageType::StaticClass()); S.Step();
	TestEqual(TEXT("Simultaneous final kill loses"), S.Run->State, EWEVORARunState::Defeat);
	TestEqual(TEXT("All combatants disposed"), S.Run->Enemies.Num(), 0);
	S.Step(30); TestEqual(TEXT("No delayed victory or spawn"), S.Run->State, EWEVORARunState::Defeat);
	S.Run->Retry(); S.Player->SetActorLocation(FVector(12000,0,500)); S.Step();
	TestEqual(TEXT("Bounds lose once"), S.Run->State, EWEVORARunState::Defeat);
	S.Run->Retry(); S.Step(); S.Step(); S.Run->Enemies[0]->Destroy(); S.Step();
	TestEqual(TEXT("External destruction is not a kill"), S.Run->State, EWEVORARunState::Defeat);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloUpgradesTest, "WEVORA.Solo.ThreeFamiliesAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloUpgradesTest::RunTest(const FString& Parameters)
{
	FSoloWorld S;
	const float BaseHP = S.Player->HealthComponent->MaxHealth;
	const float AuthoredRadius = S.Player->SpellCastComponent->ElementParameters[EWEVORASpellElement::Fire].Radius;
	S.Run->StartRun(S.Player);
	FWEVORASpellContext Context; Context.Gesture = EWEVORAGesture::Thrust;
	FWEVORASpellLaunch Before, After;
	TestTrue(TEXT("Base Fire resolution"), S.Player->SpellCastComponent->ResolveCastLaunch(Context,Before));
	for (const FWEVORARunUpgrade& Upgrade : S.Run->Config->Upgrades) { TestTrue(TEXT("Each family applies"),S.Run->Upgrades->Apply(Upgrade)); }
	S.Player->SpellCastComponent->ResolveCastLaunch(Context,After);
	TestTrue(TEXT("Radius flows into final launch"),FMath::IsNearlyEqual(After.Parameters.Radius,Before.Parameters.Radius*1.2f));
	TestEqual(TEXT("Blueprint element data unmodified"),S.Player->SpellCastComponent->ElementParameters[EWEVORASpellElement::Fire].Radius,AuthoredRadius);
	TestTrue(TEXT("Mobility speed"),S.Player->RunSpeedMultiplier > 1);
	TestEqual(TEXT("Mana recovery"),S.Player->ManaComponent->RunRecoveryMultiplier,1.25f);
	TestEqual(TEXT("Max health"),S.Player->HealthComponent->MaxHealth,BaseHP+30);
	S.Player->ManaComponent->ConsumeMana(80); S.Player->ManaComponent->UpdateRecovery(2,true);
	TestTrue(TEXT("Recovery uses upgraded rate"),S.Player->ManaComponent->Mana > 45);
	FWEVORARunUpgrade Limited = S.Run->Config->Upgrades[0]; Limited.MaxStacks=1;
	TestFalse(TEXT("Stack cap enforced"),S.Run->Upgrades->Apply(Limited));
	S.Step(); // Activate combat, then fail.
	UGameplayStatics::ApplyDamage(S.Player,100000,nullptr,nullptr,UDamageType::StaticClass());
	TestEqual(TEXT("Defeat resets max HP"),S.Player->HealthComponent->MaxHealth,BaseHP);
	TestEqual(TEXT("Reset must not revive"),S.Player->HealthComponent->Health,0.0f);
	S.Player->HealthComponent->Heal(50); TestEqual(TEXT("Healing cannot revive"),S.Player->HealthComponent->Health,0.0f);
	TestEqual(TEXT("Radius reset"),S.Player->SpellCastComponent->RunRadiusMultiplier,1.0f);
	TestEqual(TEXT("Movement reset"),S.Player->RunSpeedMultiplier,1.0f);
	TestEqual(TEXT("Recovery reset"),S.Player->ManaComponent->RunRecoveryMultiplier,1.0f);
	S.Run->Retry(); TestEqual(TEXT("Fresh run has no stacks"),S.Run->Upgrades->Stacks.Num(),0);
	TestEqual(TEXT("Retry full baseline health"),S.Player->HealthComponent->Health,BaseHP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloChoiceTest, "WEVORA.Solo.ChoiceAndElementGates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloChoiceTest::RunTest(const FString& Parameters)
{
	FSoloWorld S; S.Run->Config->Waves[0].Count=1;
	TestTrue(TEXT("Practice initially keeps Lightning"),S.Player->SpellWeavingComponent->AvailableElements.Contains(EWEVORASpellElement::Lightning));
	S.Run->StartRun(S.Player); S.Step(); S.Step();
	FWEVORASpellContext Context; Context.Gesture=EWEVORAGesture::Thrust; Context.Element=EWEVORASpellElement::Lightning;
	FWEVORASpellLaunch Launch;
	TestFalse(TEXT("Solo cast rejects locked Lightning even if called directly"),S.Player->SpellCastComponent->ResolveCastLaunch(Context,Launch));
	S.Player->SpellWeavingComponent->Context.Element=EWEVORASpellElement::Wind;
	S.Player->SpellWeavingComponent->CycleElement(); TestEqual(TEXT("Cycle only accesses base elements"),S.Player->SpellWeavingComponent->Context.Element,EWEVORASpellElement::Fire);
	S.KillEnemies(); S.Step(); S.Step();
	TestEqual(TEXT("Three distinct families offered"),S.Run->Choices.Num(),3);
	TestTrue(TEXT("Choice suspends flight controls"),S.Player->AreRunControlsSuspended());
	S.Player->SpellWeavingComponent->BeginAttack(); S.Player->SpellWeavingComponent->ReleaseAttack();
	TestEqual(TEXT("Modal blocks casts"),S.Player->SpellCastComponent->CastCount,0);
	TestFalse(TEXT("Bad selection ignored"),S.Run->SelectUpgrade(-1));
	TestTrue(TEXT("Choose survival"),S.Run->SelectUpgrade(2));
	TestFalse(TEXT("Double selection ignored"),S.Run->SelectUpgrade(2));
	S.Step(); TestFalse(TEXT("Controls restored after countdown"),S.Player->AreRunControlsSuspended());
	TestEqual(TEXT("Held spell discarded"),S.Player->SpellWeavingComponent->State,EWEVORAWeavingState::Idle);
	return true;
}
#endif
