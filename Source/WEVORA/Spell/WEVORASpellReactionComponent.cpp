#include "Spell/WEVORASpellReactionComponent.h"
#include "AI/WEVORAHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "WEVORA.h"

UWEVORASpellReactionComponent::UWEVORASpellReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	FWEVORASpellReactionRule Fire;
	FWEVORASpellReactionEffect Damage;
	Fire.Effects.Add(Damage);
	FWEVORASpellReactionEffect Burn;
	Burn.Reaction = EWEVORASpellReaction::Burning;
	Burn.Magnitude = 5.0f;
	Fire.Effects.Add(Burn);
	ReactionRules.Add(Fire);
	FWEVORASpellReactionRule Wind;
	Wind.Element = EWEVORASpellElement::Wind;
	FWEVORASpellReactionEffect Push;
	Push.Reaction = EWEVORASpellReaction::Knockback;
	Push.Magnitude = 900.0f;
	Wind.Effects.Add(Push);
	FWEVORASpellReactionEffect Lift;
	Lift.Reaction = EWEVORASpellReaction::Lift;
	Lift.Magnitude = 220.0f;
	Wind.Effects.Add(Lift);
	ReactionRules.Add(Wind);
}

bool UWEVORASpellReactionComponent::HasState(EWEVORASpellTargetState State) const
{
	return State == EWEVORASpellTargetState::Burning && BurningRemaining > 0.0f;
}

bool UWEVORASpellReactionComponent::ApplyDisplacement(const FVector& Velocity, const FHitResult& Hit)
{
	if (Velocity.IsNearlyZero()) { return false; }
	UPrimitiveComponent* Body = Hit.GetComponent();
	if (!Body || Body->GetOwner() != GetOwner()) { Body = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()); }
	if (Body && Body->IsSimulatingPhysics(Hit.BoneName))
	{
		Body->AddImpulse(Velocity, Hit.BoneName, true);
		return true;
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LaunchCharacter(Velocity, false, false);
		return true;
	}
	USceneComponent* Root = GetOwner()->GetRootComponent();
	if (!Root || Root->Mobility != EComponentMobility::Movable) { return false; }
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) { Movement->StopMovementImmediately(); }
	}
	ExternalVelocity += Velocity;
	SetComponentTickEnabled(true);
	return true;
}

bool UWEVORASpellReactionComponent::ReceiveSpell(const FWEVORASpellData& Spell, const FHitResult& Hit,
	AController* Instigator, AActor* Causer)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || Spell.Power <= 0.0f || !FMath::IsFinite(Spell.Power)
		|| Spell.Direction.ContainsNaN()) { return false; }
	const APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn && Pawn->IsPlayerControlled() && !bAffectPlayers) { return false; }
	const UWEVORAHealthComponent* Health = Owner->FindComponentByClass<UWEVORAHealthComponent>();
	if (Health && !Health->IsAlive()) { return false; }
	// Resolve against an immutable pre-hit state. Rule order cannot enable another rule on this same hit.
	TArray<FWEVORASpellReactionEffect> Effects;
	for (const FWEVORASpellReactionRule& Rule : ReactionRules)
	{
		if (Rule.Element == Spell.Element && (!Rule.bMatchShape || Rule.Shape == Spell.Shape)
			&& (Rule.RequiredState == EWEVORASpellTargetState::None || HasState(Rule.RequiredState)))
		{
			Effects.Append(Rule.Effects);
		}
	}
	bool bApplied = false;
	FVector Displacement = FVector::ZeroVector;
	for (const FWEVORASpellReactionEffect& Effect : Effects)
	{
		if (!IsValid(Owner) || (Health && !Health->IsAlive())) { break; }
		const float Magnitude = FMath::Max(0.0f, Effect.Magnitude) * Spell.Power;
		if (Magnitude <= 0.0f) { continue; }
		switch (Effect.Reaction)
		{
		case EWEVORASpellReaction::Damage:
			bApplied |= UGameplayStatics::ApplyPointDamage(Owner, Magnitude, Spell.Direction.GetSafeNormal(),
				Hit, Instigator, Causer, UDamageType::StaticClass()) > 0.0f;
			break;
		case EWEVORASpellReaction::Burning:
			if (Effect.Duration > 0.0f)
			{
				const bool bWasBurning = HasState(EWEVORASpellTargetState::Burning);
				// Refresh duration; no stacking or additional periodic damage from rapid hits.
				BurningRemaining = Effect.Duration;
				BurningDamagePerSecond = Magnitude;
				BurningInstigator = Instigator;
				BurningSource = Causer ? Causer->GetOwner() : nullptr;
				SetComponentTickEnabled(true);
				if (!bWasBurning) { OnStateChanged.Broadcast(EWEVORASpellTargetState::Burning, true); }
				bApplied = true;
			}
			break;
		case EWEVORASpellReaction::Knockback:
			Displacement += Spell.Direction.GetSafeNormal() * Magnitude;
			break;
		case EWEVORASpellReaction::Lift:
			Displacement += FVector::UpVector * Magnitude;
			break;
		}
	}
	if (IsValid(Owner) && (!Health || Health->IsAlive())) { bApplied |= ApplyDisplacement(Displacement, Hit); }
	if (bApplied)
	{
		OnSpellReceived.Broadcast(Spell);
		UE_LOG(LogWEVORA, Verbose, TEXT("Spell reaction: %s element=%s shape=%s burning=%.2f velocity=%s"),
			*GetNameSafe(Owner), *UEnum::GetValueAsString(Spell.Element), *UEnum::GetValueAsString(Spell.Shape),
			BurningRemaining, *ExternalVelocity.ToCompactString());
	}
	return bApplied;
}

void UWEVORASpellReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!Owner->HasAuthority()) { SetComponentTickEnabled(false); return; }
	const UWEVORAHealthComponent* Health = Owner->FindComponentByClass<UWEVORAHealthComponent>();
	if (Health && !Health->IsAlive())
	{
		const bool bWasBurning = HasState(EWEVORASpellTargetState::Burning);
		BurningRemaining = BurningAccumulator = 0.0f;
		ExternalVelocity = FVector::ZeroVector;
		BurningInstigator.Reset();
		BurningSource.Reset();
		if (bWasBurning) { OnStateChanged.Broadcast(EWEVORASpellTargetState::Burning, false); }
		SetComponentTickEnabled(false);
		return;
	}
	if (BurningRemaining > 0.0f)
	{
		BurningAccumulator += FMath::Min(DeltaTime, BurningRemaining);
		BurningRemaining = FMath::Max(0.0f, BurningRemaining - DeltaTime);
		if (BurningAccumulator >= 1.0f || BurningRemaining <= 0.0f)
		{
			const float Damage = BurningDamagePerSecond * BurningAccumulator;
			BurningAccumulator = 0.0f;
			UGameplayStatics::ApplyDamage(Owner, Damage, BurningInstigator.Get(), BurningSource.Get(), UDamageType::StaticClass());
			if (!IsValid(Owner)) { return; }
		}
		if (BurningRemaining <= 0.0f)
		{
			BurningInstigator.Reset();
			BurningSource.Reset();
			OnStateChanged.Broadcast(EWEVORASpellTargetState::Burning, false);
		}
	}
	if (IsDisplaced())
	{
		USceneComponent* Root = Owner->GetRootComponent();
		FHitResult MotionHit;
		const FVector NextVelocity = FMath::VInterpConstantTo(ExternalVelocity, FVector::ZeroVector, DeltaTime,
			FMath::Max(1.0f, DisplacementDeceleration));
		if (Root) { Root->MoveComponent((ExternalVelocity + NextVelocity) * 0.5f * DeltaTime,
			Root->GetComponentQuat(), true, &MotionHit); }
		ExternalVelocity = !Root || MotionHit.bBlockingHit ? FVector::ZeroVector : NextVelocity;
	}
	SetComponentTickEnabled(BurningRemaining > 0.0f || IsDisplaced());
}

void UWEVORASpellReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BurningRemaining = BurningAccumulator = 0.0f;
	ExternalVelocity = FVector::ZeroVector;
	BurningInstigator.Reset();
	BurningSource.Reset();
	Super::EndPlay(EndPlayReason);
}
