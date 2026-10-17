#include "Playground/WEVORAPlaygroundTarget.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "TimerManager.h"
#include "Engine/World.h"

AWEVORAPlaygroundTarget::AWEVORAPlaygroundTarget()
{
	HealthComponent->bDestroyOwnerOnDeath = false;
}

void AWEVORAPlaygroundTarget::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	HealthComponent->bDestroyOwnerOnDeath = false;
	HealthComponent->OnDeath.AddUniqueDynamic(this, &ThisClass::OnDefeated);
	SpellReactionComponent->OnSpellReceived.AddUniqueDynamic(this, &ThisClass::OnReceivedSpell);
	if (!bCombatAI) { DetectionRange = 0; }
}

void AWEVORAPlaygroundTarget::Tick(float DeltaSeconds)
{
	const FVector Previous = GetActorLocation();
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !HealthComponent->IsAlive()) { return; }
	if (!bCombatAI && !SpellReactionComponent->IsDisplaced() && MotionAmplitude > 0 && MotionSpeed > 0)
	{
		const float Omega = MotionSpeed / FMath::Max(1.0f, MotionAmplitude);
		const float PreviousOffset = FMath::Sin(MotionTime * Omega) * MotionAmplitude;
		MotionTime += DeltaSeconds;
		// Incremental swept motion preserves Wind displacement rather than snapping to a rail.
		AddActorWorldOffset(MotionAxis.GetSafeNormal() *
			(FMath::Sin(MotionTime * Omega) * MotionAmplitude - PreviousOffset), true);
	}
	MeasuredVelocity = DeltaSeconds > SMALL_NUMBER ? (GetActorLocation() - Previous) / DeltaSeconds : FVector::ZeroVector;
}

void AWEVORAPlaygroundTarget::OnReceivedSpell(const FWEVORASpellData& Spell)
{
	LastHitTime = GetWorld()->GetTimeSeconds();
}

void AWEVORAPlaygroundTarget::OnDefeated()
{
	SpellReactionComponent->ResetReactions();
	Movement->StopMovementImmediately();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::ResetTarget, FMath::Max(0.1f, RespawnDelay));
}

void AWEVORAPlaygroundTarget::ResetTarget()
{
	if (!HasAuthority()) { return; }
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	ResetCombatState();
	SpellReactionComponent->ResetReactions();
	HealthComponent->ResetHealth();
	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	MotionTime = 0;
	MeasuredVelocity = FVector::ZeroVector;
	LastHitTime = -100;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void AWEVORAPlaygroundTarget::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	HealthComponent->OnDeath.RemoveDynamic(this, &ThisClass::OnDefeated);
	SpellReactionComponent->OnSpellReceived.RemoveDynamic(this, &ThisClass::OnReceivedSpell);
	Super::EndPlay(Reason);
}
