#include "Playground/WEVORAPlaygroundTarget.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AWEVORAPlaygroundTarget::AWEVORAPlaygroundTarget()
{
	HealthComponent->bDestroyOwnerOnDeath = false;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/WEVORA/Effects/M_AttackEffect.M_AttackEffect"));
	TargetMaterial = Glow.Object;
}

void AWEVORAPlaygroundTarget::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	HealthComponent->bDestroyOwnerOnDeath = false;
	HealthComponent->OnDeath.AddUniqueDynamic(this, &ThisClass::OnDefeated);
	PreviousHealth = HealthComponent->Health;
	HealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::OnHPChanged);
	if (TargetMaterial && GetNetMode() != NM_DedicatedServer)
	{
		BodyTint = UMaterialInstanceDynamic::Create(TargetMaterial, this);
		Body->SetMaterial(0, BodyTint);
	}
	SpellReactionComponent->OnSpellReceived.AddUniqueDynamic(this, &ThisClass::OnReceivedSpell);
	if (!bCombatAI) { DetectionRange = 0; }
}

void AWEVORAPlaygroundTarget::Tick(float DeltaSeconds)
{
	const FVector Previous = GetActorLocation();
	Super::Tick(DeltaSeconds);
	if (BodyTint)
	{
		const FLinearColor Base = bCombatAI ? FLinearColor(0.4f,0.06f,0.05f) : FLinearColor(0.035f,0.16f,0.24f);
		const float Flash = FMath::Clamp(1 - (GetWorld()->GetTimeSeconds() - LastHitTime) / 0.18f, 0.0f, 1.0f);
		BodyTint->SetVectorParameterValue(TEXT("SpellColor"), FMath::Lerp(Base, HitColor * 1.5f, Flash));
	}
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
	HitColor = Spell.Element == EWEVORASpellElement::Fire ? FLinearColor(1,0.2f,0.02f)
		: Spell.Element == EWEVORASpellElement::Wind ? FLinearColor(0.1f,0.8f,1) : FLinearColor(0.65f,0.3f,1);
}

void AWEVORAPlaygroundTarget::OnHPChanged(float HP, float Maximum)
{
	if (HP < PreviousHealth)
	{
		LastDamage = PreviousHealth - HP;
		LastDamageTime = GetWorld()->GetTimeSeconds();
	}
	PreviousHealth = HP;
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
	LastDamage = 0;
	LastDamageTime = -100;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void AWEVORAPlaygroundTarget::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	HealthComponent->OnDeath.RemoveDynamic(this, &ThisClass::OnDefeated);
	HealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::OnHPChanged);
	SpellReactionComponent->OnSpellReceived.RemoveDynamic(this, &ThisClass::OnReceivedSpell);
	Super::EndPlay(Reason);
}
