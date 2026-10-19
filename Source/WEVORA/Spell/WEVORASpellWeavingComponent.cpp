#include "WEVORASpellWeavingComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "WEVORA.h"

UWEVORASpellWeavingComponent::UWEVORASpellWeavingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWEVORASpellWeavingComponent::ClearGesture()
{
	const EWEVORASpellElement Element = Context.Element;
	Context = FWEVORASpellContext();
	Context.Element = Element;
	Recorder.Reset();
}

void UWEVORASpellWeavingComponent::SetState(EWEVORAWeavingState NewState)
{
	if (State == NewState) { return; }
	State = NewState;
	if (bLogEvents) { UE_LOG(LogWEVORA, Log, TEXT("Spell state: %s"), *UEnum::GetValueAsString(State)); }
	OnSpellPresentationChanged.Broadcast(State, Context);
}

void UWEVORASpellWeavingComponent::BeginWeave()
{
	if (!bCombatInputEnabled) { return; }
	if (State != EWEVORAWeavingState::Idle || GetCooldownRemaining() > 0.0f) { return; }
	ClearGesture();
	SetState(EWEVORAWeavingState::Weaving);
}

float UWEVORASpellWeavingComponent::GetCooldownRemaining() const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return float(FMath::Max(0.0, NextAttackTime - Now));
}

float UWEVORASpellWeavingComponent::GetChargeAlpha() const
{
	if (!bAttackHeld) { return 0.0f; }
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return FMath::Clamp(float(Now - AttackStartTime) / FMath::Max(0.01f, MaxChargeTime), 0.0f, 1.0f);
}

void UWEVORASpellWeavingComponent::BeginAttack()
{
	if (!bCombatInputEnabled) { return; }
	if (State != EWEVORAWeavingState::Idle || GetCooldownRemaining() > 0.0f) { return; }
	AttackStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bAttackHeld = true;
	BeginWeave();
}

void UWEVORASpellWeavingComponent::ReleaseAttack()
{
	if (!bAttackHeld) { return; }
	if (State != EWEVORAWeavingState::Weaving) { CancelWeave(); return; }
	Context.Gesture = EWEVORAGesture::Thrust;
	Context.ChargeAlpha = GetChargeAlpha();
	Context.ChargeDuration = Context.ChargeAlpha * FMath::Max(0.01f, MaxChargeTime);
	SetState(EWEVORAWeavingState::ReadyToCast);
	ReleaseWeave();
}

void UWEVORASpellWeavingComponent::BeginQuickShape()
{
	if (!bCombatInputEnabled) { return; }
	if (State != EWEVORAWeavingState::Idle || GetCooldownRemaining() > 0.0f) { return; }
	bQuickShapeHeld = true;
	BeginWeave();
	BeginShape();
}

void UWEVORASpellWeavingComponent::EndQuickShape()
{
	if (!bQuickShapeHeld) { return; }
	EndShape();
	ReleaseWeave();
}

void UWEVORASpellWeavingComponent::CycleElement()
{
	if (!bCombatInputEnabled) { return; }
	if (AvailableElements.IsEmpty()) { return; }
	const int32 CurrentIndex = AvailableElements.IndexOfByKey(Context.Element);
	Context.Element = AvailableElements[(CurrentIndex + 1) % AvailableElements.Num()];
	if (bLogEvents) { UE_LOG(LogWEVORA, Log, TEXT("Spell element: %s"), *UEnum::GetValueAsString(Context.Element)); }
	OnSpellPresentationChanged.Broadcast(State, Context);
}

void UWEVORASpellWeavingComponent::BeginShape()
{
	if (State != EWEVORAWeavingState::Weaving && State != EWEVORAWeavingState::ReadyToCast) { return; }
	ClearGesture();
	ShapeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	SetState(EWEVORAWeavingState::Shaping);
}

void UWEVORASpellWeavingComponent::RecordLook(FVector2D LookDelta)
{
	if (State != EWEVORAWeavingState::Shaping) { return; }
	if (!FMath::IsFinite(LookDelta.X) || !FMath::IsFinite(LookDelta.Y)) { return; }
	if (bInvertLookY) { LookDelta.Y *= -1.0f; }
	Recorder.Record(LookDelta, Thresholds.MinimumSampleDistance);
	OnGestureSample.Broadcast(LookDelta);
}

void UWEVORASpellWeavingComponent::EndShape()
{
	if (State != EWEVORAWeavingState::Shaping) { return; }
	const EWEVORASpellElement Element = Context.Element;
	const float Duration = GetWorld() ? float(GetWorld()->GetTimeSeconds() - ShapeStartTime) : 0.0f;
	Context = Recorder.Recognize(Thresholds, Duration);
	Context.Element = Element;
	if (bLogEvents) { UE_LOG(LogWEVORA, Log, TEXT("Shape: %s, magnitude=%.2f duration=%.3f"), *UEnum::GetValueAsString(Context.Gesture), Context.GestureMagnitude, Context.GestureDuration); }
	SetState(Context.Gesture == EWEVORAGesture::None ? EWEVORAWeavingState::Weaving : EWEVORAWeavingState::ReadyToCast);
}

void UWEVORASpellWeavingComponent::ReleaseWeave()
{
	if (!bCombatInputEnabled || (bRestrictElements && !AvailableElements.Contains(Context.Element))) { CancelWeave(); return; }
	if (State != EWEVORAWeavingState::ReadyToCast || Context.Gesture == EWEVORAGesture::None || GetCooldownRemaining() > 0.0f)
	{
		CancelWeave();
		return;
	}
	const FWEVORASpellContext CastContext = Context;
	LastCastContext = CastContext;
	bHasLastCast = true;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	NextAttackTime = Now + FMath::Max(0.0f, AttackCooldown);
	// Reset before broadcasting so Blueprint callbacks can safely begin a new weave.
	CancelWeave();
	if (bLogEvents)
	{
		UE_LOG(LogWEVORA, Log, TEXT("Cast: %s + %s direction=(%.3f, %.3f) magnitude=%.2f duration=%.3f"),
			*UEnum::GetValueAsString(CastContext.Element), *UEnum::GetValueAsString(CastContext.Gesture),
			CastContext.GestureDirection.X, CastContext.GestureDirection.Y, CastContext.GestureMagnitude, CastContext.GestureDuration);
	}
	OnSpellCast.Broadcast(CastContext);
}

void UWEVORASpellWeavingComponent::CancelWeave()
{
	const bool bWasIdle = State == EWEVORAWeavingState::Idle;
	bAttackHeld = false;
	bQuickShapeHeld = false;
	ClearGesture();
	SetState(EWEVORAWeavingState::Idle);
	// Explicit cancellation also stops an idle selection preview.
	if (bWasIdle) { OnSpellPresentationChanged.Broadcast(State, Context); }
}

void UWEVORASpellWeavingComponent::ResetCombat()
{
	CancelWeave();
	NextAttackTime = AttackStartTime = ShapeStartTime = 0;
	bHasLastCast = false;
	Context.Element = AvailableElements.IsEmpty() ? EWEVORASpellElement::Fire : AvailableElements[0];
	LastCastContext = FWEVORASpellContext();
	OnSpellPresentationChanged.Broadcast(State, Context);
}

uint64 UWEVORASpellWeavingComponent::DebugKey() const
{
	return uint64(GetUniqueID()) | (uint64(1) << 32);
}

void UWEVORASpellWeavingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!GEngine) { return; }
	if (!bShowDebug || !Pawn || !Pawn->IsLocallyControlled())
	{
		GEngine->RemoveOnScreenDebugMessage(DebugKey());
		return;
	}
	const FString Result = bHasLastCast
		? UEnum::GetValueAsString(LastCastContext.Element) + TEXT(" + ") + UEnum::GetValueAsString(LastCastContext.Gesture)
		: TEXT("None");
	GEngine->AddOnScreenDebugMessage(DebugKey(), 0.15f, FColor::Cyan,
		FString::Printf(TEXT("Spell: %s\nElement: %s\nGesture: %s\nLast Cast: %s"),
			*UEnum::GetValueAsString(State), *UEnum::GetValueAsString(Context.Element), *UEnum::GetValueAsString(Context.Gesture), *Result));
}

void UWEVORASpellWeavingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine) { GEngine->RemoveOnScreenDebugMessage(DebugKey()); }
	CancelWeave();
	Super::EndPlay(EndPlayReason);
}
