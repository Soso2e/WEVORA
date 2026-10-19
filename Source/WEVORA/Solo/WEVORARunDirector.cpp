#include "Solo/WEVORARunDirector.h"
#include "WEVORACharacter.h"
#include "AI/WEVORAHealthComponent.h"
#include "AI/WEVORAEnemyProjectile.h"
#include "Spell/WEVORASpellDelivery.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"
#include "Solo/WEVORASoloWidget.h"
#include "Solo/WEVORAArenaEnemy.h"

AWEVORARunDirector::AWEVORARunDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Config = CreateDefaultSubobject<UWEVORAArenaConfig>(TEXT("PrototypeConfig"));
	Upgrades = CreateDefaultSubobject<UWEVORARunUpgrades>(TEXT("RunUpgrades"));
	Progression = CreateDefaultSubobject<UWEVORASoloProgression>(TEXT("PermanentProgress"));
}

bool AWEVORARunDirector::IsCombatActive() const
{
	return State == EWEVORARunState::WaveActive || State == EWEVORARunState::BossFight;
}

bool AWEVORARunDirector::StartRun(AWEVORACharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player)) { return false; }
	if (State != EWEVORARunState::Idle && State != EWEVORARunState::Victory && State != EWEVORARunState::Defeat) { return false; }
	const FString ConfigurationError = Config ? Config->Validate() : TEXT("Arena config is missing");
	if (!ConfigurationError.IsEmpty())
	{
		Pilot=Player; State=EWEVORARunState::Defeat; ResultReason=ConfigurationError; SetControls(false); return false;
	}
	CleanupCombat();
	Upgrades->Reset();
	if (Pilot) { Pilot->HealthComponent->OnDeath.RemoveDynamic(this, &ThisClass::PlayerDied); }
	if (Pilot != Player)
	{
		PlayerStart = Player->GetActorTransform();
		bOriginalSpellDebug=Player->SpellWeavingComponent->bShowDebug;
		bOriginalDamageFeedback=Player->HealthComponent->bShowDamageFeedback;
	}
	Pilot = Player;
	Pilot->bUseSoloHUD=true;
	Pilot->SpellWeavingComponent->bShowDebug=false;
	Pilot->HealthComponent->bShowDamageFeedback=false;
	if (APlayerController* PC = Cast<APlayerController>(Pilot->GetController()); PC && !InputComponent)
	{
		EnableInput(PC);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::Retry);
	}
	Pilot->HealthComponent->bDestroyOwnerOnDeath = false;
	Pilot->HealthComponent->OnDeath.AddUniqueDynamic(this, &ThisClass::PlayerDied);
	Upgrades->BeginRun(Pilot);
	if (bPersistProgress && !bProgressLoaded) { Progression->LoadProgress(); bProgressLoaded = true; }
	Pilot->SpellWeavingComponent->AvailableElements = Progression->AllowedElements();
	Pilot->SpellWeavingComponent->bRestrictElements = true;
	Pilot->ResetForPlayground(PlayerStart);
	Choices.Empty();
	WaveIndex = 0; SpawnedCount = KilledCount = 0;
	WaveElapsed = SpawnElapsed = 0;
	bCompletionPending = false;
	ResultReason.Empty();
	Countdown = FMath::Max(0.0f, Config->PreparationSeconds);
	State = EWEVORARunState::Preparing;
	SetControls(false);
	return true;
}

void AWEVORARunDirector::Retry()
{
	if (!HasAuthority()) { return; }
	if (State != EWEVORARunState::Idle && State != EWEVORARunState::Victory && State != EWEVORARunState::Defeat)
	{
		FinishRun(false,TEXT("Run restarted"));
	}
	StartRun(Pilot);
}

void AWEVORARunDirector::PlayerDied()
{
	if (State != EWEVORARunState::Idle && State != EWEVORARunState::Victory && State != EWEVORARunState::Defeat)
	{
		FinishRun(false, TEXT("Defeated"));
	}
}

void AWEVORARunDirector::BeginWave()
{
	if (!Pilot || !Pilot->HealthComponent->IsAlive()) { PlayerDied(); return; }
	const FWEVORAArenaWave& Wave = Config->Waves[WaveIndex];
	if (!Wave.EnemyClass || Wave.EnemyClass->HasAnyClassFlags(CLASS_Abstract))
	{
		FinishRun(false, TEXT("Wave enemy class is unavailable")); return;
	}
	State = Wave.bBoss ? EWEVORARunState::BossFight : EWEVORARunState::WaveActive;
	WaveElapsed = 0; SpawnElapsed = FMath::Max(0.1f, Wave.SpawnInterval);
	SpawnedCount = KilledCount = 0; bCompletionPending = false;
	SetControls(true);
}

void AWEVORARunDirector::SetControls(bool bCombat)
{
	if (!Pilot) { return; }
	Pilot->SpellWeavingComponent->ResetCombat();
	Pilot->SpellWeavingComponent->bCombatInputEnabled = bCombat;
	Pilot->SuspendRunControls(!bCombat);
	APlayerController* PC = Cast<APlayerController>(Pilot->GetController());
	if (!PC || !PC->GetLocalPlayer()) { return; }
	const bool bShowModal = State == EWEVORARunState::UpgradeChoice || State == EWEVORARunState::Victory || State == EWEVORARunState::Defeat;
	if (bShowModal)
	{
		if (!Modal)
		{
			Modal = CreateWidget<UWEVORASoloWidget>(PC); Modal->AddToPlayerScreen(100);
		}
		Modal->Show(this);
		FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Modal->TakeWidget()); PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	else
	{
		if (Modal) { Modal->SetVisibility(ESlateVisibility::Collapsed); }
		PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor = false;
	}
}

void AWEVORARunDirector::OfferUpgrades()
{
	Choices.Empty();
	for (EWEVORAUpgradeFamily Family : { EWEVORAUpgradeFamily::Element, EWEVORAUpgradeFamily::Mobility, EWEVORAUpgradeFamily::Survival })
	{
		for (const FWEVORARunUpgrade& Upgrade : Config->Upgrades)
		{
			if (Upgrade.Family == Family && !Upgrade.ID.IsNone() && Upgrades->Stacks.FindRef(Upgrade.ID) < FMath::Clamp(Upgrade.MaxStacks,1,10))
			{
				Choices.Add(Upgrade); break;
			}
		}
	}
	if (Choices.IsEmpty()) { FinishRun(false,TEXT("Upgrade pool exhausted; adjust trial data")); return; }
	State = EWEVORARunState::UpgradeChoice; SetControls(false);
}

bool AWEVORARunDirector::SelectUpgrade(int32 Index)
{
	if (!HasAuthority() || State != EWEVORARunState::UpgradeChoice || !Choices.IsValidIndex(Index)) { return false; }
	if (!Upgrades->Apply(Choices[Index])) { return false; }
	Choices.Empty(); ContinueAfterReward(); return true;
}

void AWEVORARunDirector::ContinueAfterReward()
{
	if (!HasAuthority() || State != EWEVORARunState::UpgradeChoice || !Pilot || !Pilot->HealthComponent->IsAlive()) { return; }
	++WaveIndex;
	Countdown = FMath::Max(0.0f, Config->PreparationSeconds);
	State = EWEVORARunState::Preparing;
	SetControls(false);
}

void AWEVORARunDirector::SpawnEnemy()
{
	const FWEVORAArenaWave& Wave = Config->Waves[WaveIndex];
	const FVector Offset = Config->SpawnOffsets[SpawnedCount % Config->SpawnOffsets.Num()];
	const FTransform Transform(FRotator::ZeroRotator, GetActorLocation() + Offset);
	AWEVORAEnemy* Enemy = GetWorld()->SpawnActorDeferred<AWEVORAEnemy>(Wave.EnemyClass, Transform,
		this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Enemy) { FinishRun(false, TEXT("Spawn blocked: retry or adjust spawn positions")); return; }
	Enemy->HealthComponent->MaxHealth = FMath::Clamp(Wave.Health, 1.0f, 100000.0f);
	Enemy->HealthComponent->bDestroyOwnerOnDeath = false; // Poll real HP before disposing the corpse.
	Enemy->MoveSpeed *= FMath::Clamp(Wave.SpeedMultiplier, 0.1f, 4.0f);
	Enemy->AttackDamage *= FMath::Clamp(Wave.DamageMultiplier, 0.0f, 10.0f);
	Enemy->DetectionRange = 25000; Enemy->AttackRange = 4500;
	Enemy->WindupTime = Wave.bBoss ? FMath::Clamp(Wave.BossWindup,0.1f,10.0f) : 0.65f;
	Enemy->AttackInterval = Wave.bBoss ? 3.0f : 2.0f;
	Enemy->bLockWindupAim = Wave.bBoss;
	Enemy->RecoveryTime = Wave.bBoss ? FMath::Clamp(Wave.BossRecovery,0.1f,10.0f) : 0;
	if (AWEVORAArenaEnemy* ArenaEnemy = Cast<AWEVORAArenaEnemy>(Enemy)) { ArenaEnemy->bBossStyle = Wave.bBoss; }
	UGameplayStatics::FinishSpawningActor(Enemy, Transform);
	Enemies.Add(Enemy); ++SpawnedCount;
}

void AWEVORARunDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) { return; }
	if (State == EWEVORARunState::Idle && bAutoStart)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		StartRun(PC ? Cast<AWEVORACharacter>(PC->GetPawn()) : nullptr);
	}
	if (State == EWEVORARunState::Idle || State == EWEVORARunState::Victory || State == EWEVORARunState::Defeat) { return; }
	if (!IsValid(Pilot) || !Pilot->HealthComponent->IsAlive()) { FinishRun(false, TEXT("Defeated")); return; }
	const FVector Position = Pilot->GetActorLocation() - GetActorLocation();
	if (FMath::Abs(Position.X) > Config->BoundsExtent.X || FMath::Abs(Position.Y) > Config->BoundsExtent.Y
		|| Position.Z > Config->BoundsExtent.Z || Position.Z < Config->MinimumHeight)
	{
		FinishRun(false, TEXT("Left the arena")); return;
	}
	if (State == EWEVORARunState::Preparing)
	{
		Countdown = FMath::Max(0.0f, Countdown - DeltaSeconds);
		if (Countdown <= 0) { BeginWave(); }
		return;
	}
	if (!IsCombatActive()) { return; }
	WaveElapsed += DeltaSeconds;
	if (WaveElapsed >= FMath::Max(10.0f, Config->WaveTimeout)) { FinishRun(false, TEXT("Wave timed out")); return; }
	for (int32 Index = Enemies.Num() - 1; Index >= 0; --Index)
	{
		AWEVORAEnemy* Enemy = Enemies[Index];
		if (!IsValid(Enemy)) { FinishRun(false, TEXT("Combatant removed without a defeat")); return; }
		if (!Enemy->HealthComponent->IsAlive())
		{
			Enemy->Destroy(); Enemies.RemoveAtSwap(Index); ++KilledCount;
		}
	}
	const FWEVORAArenaWave& Wave = Config->Waves[WaveIndex];
	if (SpawnedCount >= FMath::Clamp(Wave.Count,1,100) && Enemies.IsEmpty())
	{
		// Settle one tick before a reward: a final kill and player death in one frame must lose.
		if (!bCompletionPending) { bCompletionPending = true; return; }
		if (WaveIndex == Config->Waves.Num() - 1) { FinishRun(true, TEXT("Trial cleared")); }
		else { CleanupCombat(); OfferUpgrades(); }
		return;
	}
	SpawnElapsed += DeltaSeconds;
	if (SpawnedCount < FMath::Clamp(Wave.Count,1,100) && Enemies.Num() < FMath::Clamp(Wave.AliveCap,1,16)
		&& SpawnElapsed >= FMath::Max(0.1f, Wave.SpawnInterval))
	{
		SpawnElapsed = 0; SpawnEnemy(); // At most one spawn per tick, including hitches.
	}
}

void AWEVORARunDirector::CleanupCombat()
{
	// Restrict cleanup to this director and its pawn; practice actors are not run-owned.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsA<AWEVORASpellDelivery>() && !It->IsA<AWEVORAEnemyProjectile>()) { continue; }
		AActor* DeliveryOwner = It->GetOwner();
		const AWEVORAEnemyProjectile* EnemyShot = Cast<AWEVORAEnemyProjectile>(*It);
		if ((EnemyShot && EnemyShot->CombatScope == this) ||
			(DeliveryOwner && (DeliveryOwner == Pilot || DeliveryOwner == this || DeliveryOwner->GetOwner() == this))) { It->Destroy(); }
	}
	for (AWEVORAEnemy* Enemy : Enemies) { if (IsValid(Enemy)) { Enemy->Destroy(); } }
	Enemies.Empty();
	bCompletionPending = false;
	if (Pilot) { Pilot->SpellWeavingComponent->ResetCombat(); }
}

void AWEVORARunDirector::FinishRun(bool bWon, const FString& Reason)
{
	if (State == EWEVORARunState::Victory || State == EWEVORARunState::Defeat) { return; }
	State = bWon ? EWEVORARunState::Victory : EWEVORARunState::Defeat;
	ResultReason = Reason;
	CleanupCombat();
	if (bWon && bPersistProgress)
	{
		Progression->CompleteTrial(Config->Trial);
		ResultReason += TEXT("\n") + Progression->Status;
	}
	Upgrades->Reset(); Choices.Empty(); SetControls(false);
}

void AWEVORARunDirector::RetrySave()
{
	if (!HasAuthority() || (State != EWEVORARunState::Victory && State != EWEVORARunState::Defeat) || !Progression->bSavePending) { return; }
	Progression->RetrySave(); ResultReason += TEXT("\n") + Progression->Status; SetControls(false);
}

void AWEVORARunDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	CleanupCombat();
	Upgrades->Reset();
	if (Pilot) { Pilot->HealthComponent->OnDeath.RemoveDynamic(this, &ThisClass::PlayerDied); }
	if (Modal) { Modal->RemoveFromParent(); }
	if (IsValid(Pilot))
	{
		Pilot->bUseSoloHUD=false;
		Pilot->SpellWeavingComponent->bShowDebug=bOriginalSpellDebug;
		Pilot->HealthComponent->bShowDamageFeedback=bOriginalDamageFeedback;
		Pilot->SpellWeavingComponent->bCombatInputEnabled = true; Pilot->SuspendRunControls(false);
		if (APlayerController* PC=Cast<APlayerController>(Pilot->GetController()); PC && PC->GetLocalPlayer())
		{
			PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=false;
		}
	}
	Super::EndPlay(Reason);
}
