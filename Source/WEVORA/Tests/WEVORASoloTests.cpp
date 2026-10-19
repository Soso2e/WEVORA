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
	S.Run->ContinueAfterReward(); S.Step(); S.Step();
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
#endif
