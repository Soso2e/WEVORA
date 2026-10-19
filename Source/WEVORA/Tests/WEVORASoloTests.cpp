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
#include "Solo/WEVORAArenaEnemy.h"
#include "Solo/WEVORASoloProgression.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Spell/WEVORASpellProjectile.h"
#include "AI/WEVORAEnemyProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
		Run->bPersistProgress = false;
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloSaveTest, "WEVORA.Solo.SaveRoundTripAndDuplicate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloSaveTest::RunTest(const FString& Parameters)
{
	FSoloWorld S;
	UWEVORASoloProgression* P = S.Run->Progression;
	P->SlotName = TEXT("WEVORA_Automation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TestTrue(TEXT("Fresh load"),P->LoadProgress());
	TestEqual(TEXT("Fresh has two elements"),P->AllowedElements().Num(),2);
	FWEVORAArenaTrial Trial;
	TestTrue(TEXT("First trial saves"),P->CompleteTrial(Trial)); TestTrue(TEXT("First unlock new"),P->bLastUnlockNew);
	UWEVORASoloProgression* Reload = NewObject<UWEVORASoloProgression>(S.Run); Reload->SlotName=P->SlotName;
	TestTrue(TEXT("Deserialize from disk"),Reload->LoadProgress());
	TestTrue(TEXT("Trial preserved"),Reload->HasCompleted(Trial.ID));
	TestTrue(TEXT("Lightning preserved"),Reload->AllowedElements().Contains(EWEVORASpellElement::Lightning));
	TestTrue(TEXT("Trial can be replayed"),Reload->CompleteTrial(Trial));
	TestFalse(TEXT("No duplicate reward"),Reload->bLastUnlockNew);
	TestEqual(TEXT("No duplicate element"),Reload->AllowedElements().Num(),3);
	Trial.ID=TEXT("AnotherTrial"); Trial.Reward=EWEVORASpellElement::Wind;
	TestTrue(TEXT("Trial ID to reward data, not clear count"),Reload->CompleteTrial(Trial));
	TestFalse(TEXT("Base element was already available"),Reload->bLastUnlockNew);
	UGameplayStatics::DeleteGameInSlot(P->SlotName,0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloSaveGuardTest, "WEVORA.Solo.SaveFailureAndVersionGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloSaveGuardTest::RunTest(const FString& Parameters)
{
	FSoloWorld S; UWEVORASoloProgression* P=S.Run->Progression;
	P->SlotName=TEXT(""); P->LoadProgress(); FWEVORAArenaTrial Trial;
	TestFalse(TEXT("Save failure reported"),P->CompleteTrial(Trial));
	TestTrue(TEXT("Failure retry pending"),P->bSavePending);
	TestFalse(TEXT("Failure cannot claim permanent unlock"),P->AllowedElements().Contains(EWEVORASpellElement::Lightning));
	P->SlotName=TEXT("WEVORA_Automation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TestTrue(TEXT("Retry actually saves"),P->RetrySave());
	UWEVORASoloSave* Future=NewObject<UWEVORASoloSave>(); Future->Version=999;
	UGameplayStatics::SaveGameToSlot(Future,P->SlotName,0);
	TestFalse(TEXT("Future version refused"),P->LoadProgress());
	TestFalse(TEXT("Future version never overwritten"),P->CompleteTrial(Trial));
	TestEqual(TEXT("Future version still on disk"),CastChecked<UWEVORASoloSave>(UGameplayStatics::LoadGameFromSlot(P->SlotName,0))->Version,999);
	UGameplayStatics::DeleteGameInSlot(P->SlotName,0);
	P->SlotName=TEXT("WEVORA_Automation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits); P->LoadProgress();
	Trial.Reward=static_cast<EWEVORASpellElement>(255);
	TestFalse(TEXT("Unimplemented reward refused"),P->CompleteTrial(Trial));
	TestFalse(TEXT("No file for fake reward"),UGameplayStatics::DoesSaveGameExist(P->SlotName,0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloFullRunTest, "WEVORA.Solo.FullRunUnlockAndReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloFullRunTest::RunTest(const FString& Parameters)
{
	FSoloWorld S; S.Run->bPersistProgress=true;
	S.Run->Progression->SlotName=TEXT("WEVORA_Automation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	S.Run->Config->Waves.SetNum(3);
	for (FWEVORAArenaWave& Wave:S.Run->Config->Waves) { Wave.EnemyClass=AWEVORAArenaEnemy::StaticClass(); Wave.Count=1; }
	S.Run->Config->Waves[1].bBoss=false; S.Run->Config->Waves[2].bBoss=true;
	S.Run->StartRun(S.Player);
	for (int32 Index=0; Index<3; ++Index)
	{
		S.Step(); S.Step(); S.KillEnemies(); S.Step(); S.Step();
		if (Index<2) { TestTrue(TEXT("Inter-wave selection"),S.Run->SelectUpgrade(Index)); }
	}
	TestEqual(TEXT("Full run victory"),S.Run->State,EWEVORARunState::Victory);
	TestTrue(TEXT("Trial completed from boss victory"),S.Run->Progression->HasCompleted(S.Run->Config->Trial.ID));
	TestEqual(TEXT("Victory resets upgrades immediately"),S.Run->Upgrades->Stacks.Num(),0);
	TestFalse(TEXT("Unlock not inserted into finished run"),S.Player->SpellWeavingComponent->AvailableElements.Contains(EWEVORASpellElement::Lightning));
	S.Run->Retry();
	TestTrue(TEXT("Next run gets Lightning"),S.Player->SpellWeavingComponent->AvailableElements.Contains(EWEVORASpellElement::Lightning));
	S.Step();
	FWEVORASpellContext Context; Context.Element=EWEVORASpellElement::Lightning; Context.Gesture=EWEVORAGesture::Thrust; FWEVORASpellLaunch Launch;
	TestTrue(TEXT("Unlocked element has real cast behavior"),S.Player->SpellCastComponent->ResolveCastLaunch(Context,Launch));
	S.Player->SpellWeavingComponent->Context.Element=EWEVORASpellElement::Lightning;
	S.Player->SpellWeavingComponent->BeginAttack(); S.Player->SpellWeavingComponent->ReleaseAttack();
	TestEqual(TEXT("Real Lightning delivery launched"),S.Player->SpellCastComponent->CastCount,1);
	S.Run->Retry(); TestEqual(TEXT("Active restart allowed"),S.Run->State,EWEVORARunState::Preparing);
	TestEqual(TEXT("Active restart clears casts"),S.Player->SpellCastComponent->CastCount,0);
	UGameplayStatics::DeleteGameInSlot(S.Run->Progression->SlotName,0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloBossTest, "WEVORA.Solo.BossTellRecoveryAndSharedDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloBossTest::RunTest(const FString& Parameters)
{
	FSoloWorld S; S.Player->GetCharacterMovement()->DisableMovement();
	S.Player->SetActorLocation(FVector(1500,0,500));
	AWEVORAArenaEnemy* Boss=S.World->SpawnActor<AWEVORAArenaEnemy>(FVector(0,0,500),FRotator::ZeroRotator);
	Boss->bBossStyle=true; Boss->bLockWindupAim=true; Boss->WindupTime=0.2f; Boss->RecoveryTime=1;
	Boss->AttackRange=4000; Boss->Tick(0.016f);
	TestTrue(TEXT("Visible windup"),Boss->bWindingUp && Boss->DangerMarker->IsVisible() && Boss->DangerLine->IsVisible());
	const FVector Aim=Boss->TelegraphTarget;
	S.Player->SetActorLocation(FVector(1500,700,500)); ++GFrameCounter; S.World->Tick(LEVELTICK_All,0.25f);
	TestTrue(TEXT("Stationary counterattack window"),Boss->GetRecoveryRemaining()>0);
	AWEVORAEnemyProjectile* Shot=nullptr;
	for (TActorIterator<AWEVORAEnemyProjectile> It(S.World); It; ++It) { Shot=*It; break; }
	if (TestNotNull(TEXT("Shared enemy projectile"),Shot))
	{
		TestTrue(TEXT("Shot keeps telegraphed aim after dodge"),Shot->GetActorForwardVector().Equals((Aim-Shot->GetActorLocation()).GetSafeNormal(),0.01f));
		const float HP=S.Player->HealthComponent->Health;
		S.Player->SetActorLocation(Aim); Shot->Movement->TickComponent(1,LEVELTICK_All,nullptr);
		TestTrue(TEXT("Existing projectile and Health damage path"),S.Player->HealthComponent->Health < HP);
	}
	Boss->ResetCombatState(); TestEqual(TEXT("Reset clears recovery"),Boss->GetRecoveryRemaining(),0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloLocalTest, "WEVORA.Solo.LocalReactionsWithoutCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloLocalTest::RunTest(const FString& Parameters)
{
	FSoloWorld S; S.Run->StartRun(S.Player);
	AWEVORAArenaEnemy* Enemy=S.World->SpawnActor<AWEVORAArenaEnemy>(FVector(1500,0,500),FRotator::ZeroRotator);
	USphereComponent* Hand=nullptr; USphereComponent* Arm=nullptr;
	for (USphereComponent* Zone:Enemy->ZoneColliders)
	{
		if (Zone->GetFName()==TEXT("Zone_RightHand")) { Hand=Zone; }
		if (Zone->GetFName()==TEXT("Zone_RightArm")) { Arm=Zone; }
	}
	if (!TestNotNull(TEXT("Hand collider"),Hand) || !TestNotNull(TEXT("Arm collider"),Arm)) { return false; }
	FWEVORASpellData Fire; FWEVORASpellData Wind; Wind.Element=EWEVORASpellElement::Wind;
	FHitResult HandHit(Enemy,Hand,Hand->GetComponentLocation(),FVector::UpVector);
	FHitResult ArmHit(Enemy,Arm,Arm->GetComponentLocation(),FVector::UpVector);
	const float HP=Enemy->HealthComponent->Health;
	TestTrue(TEXT("Fire accepted without cards"),Enemy->SpellReactionComponent->ReceiveSpell(Fire,HandHit,nullptr,S.Player));
	Enemy->LocalStatuses->CommitReactions();
	TestTrue(TEXT("Local hand burns"),Enemy->LocalStatuses->HasStatus(TEXT("RightHand"),TEXT("Burning")));
	Enemy->SpellReactionComponent->ReceiveSpell(Wind,ArmHit,nullptr,S.Player); Enemy->LocalStatuses->CommitReactions();
	TestTrue(TEXT("Wind spreads to torso without upgrade"),Enemy->LocalStatuses->HasStatus(TEXT("Torso"),TEXT("Burning")));
	Enemy->LocalStatuses->AdvanceStatuses(0.5f);
	TestTrue(TEXT("Shared health receives damage"),Enemy->HealthComponent->Health<HP);
	TestEqual(TEXT("No card required"),S.Run->Upgrades->Stacks.Num(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoloProcessSaveTest, "WEVORA.Solo.ProcessPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoloProcessSaveTest::RunTest(const FString& Parameters)
{
	FString Mode,Slot;
	if (!FParse::Value(FCommandLine::Get(),TEXT("SoloPersistenceMode="),Mode)) { AddInfo(TEXT("Cross-process write/read is driven separately by validation script.")); return true; }
	if (!FParse::Value(FCommandLine::Get(),TEXT("SoloPersistenceSlot="),Slot) || !Slot.StartsWith(TEXT("WEVORA_Automation_"))) { AddError(TEXT("Dedicated automation slot required")); return false; }
	FSoloWorld S; UWEVORASoloProgression* P=S.Run->Progression; P->SlotName=Slot;
	if (Mode==TEXT("Write"))
	{
		TestTrue(TEXT("Fresh process loads"),P->LoadProgress());
		TestTrue(TEXT("Write persistent trial"),P->CompleteTrial(FWEVORAArenaTrial()));
	}
	else if (Mode==TEXT("Read"))
	{
		TestTrue(TEXT("Separate process reloads"),P->LoadProgress());
		TestTrue(TEXT("Unlock survived process restart"),P->AllowedElements().Contains(EWEVORASpellElement::Lightning));
		TestTrue(TEXT("Trial survived process restart"),P->HasCompleted(TEXT("LightningTrial")));
		UGameplayStatics::DeleteGameInSlot(Slot,0);
	}
	else { AddError(TEXT("Unknown persistence mode")); }
	return true;
}
#endif
