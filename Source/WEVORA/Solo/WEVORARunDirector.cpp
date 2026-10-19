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

AWEVORARunDirector::AWEVORARunDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Config = CreateDefaultSubobject<UWEVORAArenaConfig>(TEXT("PrototypeConfig"));
}

bool AWEVORARunDirector::IsCombatActive() const
{
	return State == EWEVORARunState::WaveActive || State == EWEVORARunState::BossFight;
}

bool AWEVORARunDirector::StartRun(AWEVORACharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !Config || Config->Waves.IsEmpty() || Config->SpawnOffsets.IsEmpty()) { return false; }
	if (State != EWEVORARunState::Idle && State != EWEVORARunState::Victory && State != EWEVORARunState::Defeat) { return false; }
	CleanupCombat();
	if (Pilot) { Pilot->HealthComponent->OnDeath.RemoveDynamic(this, &ThisClass::PlayerDied); }
	if (Pilot != Player) { PlayerStart = Player->GetActorTransform(); }
	Pilot = Player;
	if (APlayerController* PC = Cast<APlayerController>(Pilot->GetController()); PC && !InputComponent)
	{
		EnableInput(PC);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::Retry);
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::ContinueAfterReward);
	}
	Pilot->HealthComponent->bDestroyOwnerOnDeath = false;
	Pilot->HealthComponent->OnDeath.AddUniqueDynamic(this, &ThisClass::PlayerDied);
	Pilot->ResetForPlayground(PlayerStart);
	WaveIndex = 0; SpawnedCount = KilledCount = 0;
	WaveElapsed = SpawnElapsed = 0;
	bCompletionPending = false;
	ResultReason.Empty();
	Countdown = FMath::Max(0.0f, Config->PreparationSeconds);
	State = EWEVORARunState::Preparing;
	return true;
}

void AWEVORARunDirector::Retry() { StartRun(Pilot); }

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
}

void AWEVORARunDirector::ContinueAfterReward()
{
	if (!HasAuthority() || State != EWEVORARunState::UpgradeChoice || !Pilot || !Pilot->HealthComponent->IsAlive()) { return; }
	++WaveIndex;
	Countdown = FMath::Max(0.0f, Config->PreparationSeconds);
	State = EWEVORARunState::Preparing;
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
	Enemy->WindupTime = Wave.bBoss ? 1.4f : 0.65f;
	Enemy->AttackInterval = Wave.bBoss ? 3.0f : 2.0f;
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
		else { CleanupCombat(); State = EWEVORARunState::UpgradeChoice; }
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
		if (DeliveryOwner && (DeliveryOwner == Pilot || DeliveryOwner == this || DeliveryOwner->GetOwner() == this)) { It->Destroy(); }
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
}

void AWEVORARunDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	CleanupCombat();
	if (Pilot) { Pilot->HealthComponent->OnDeath.RemoveDynamic(this, &ThisClass::PlayerDied); }
	Super::EndPlay(Reason);
}
