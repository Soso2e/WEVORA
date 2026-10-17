#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Spell/WEVORALocalReactionSolver.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "AI/WEVORAHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

UWEVORALocalizedStatusComponent::UWEVORALocalizedStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

const UWEVORAZoneProfile* UWEVORALocalizedStatusComponent::GetProfile() const
{
	return Profile ? Profile.Get() : bUsePrototypeProfile ? GetDefault<UWEVORAZoneProfile>() : nullptr;
}

bool UWEVORALocalizedStatusComponent::CanMutate() const
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsLocalized()) { return false; }
	const UWEVORAHealthComponent* Health = GetOwner()->FindComponentByClass<UWEVORAHealthComponent>();
	return !Health || Health->IsAlive();
}

void UWEVORALocalizedStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWEVORAHealthComponent* Health = GetOwner()->FindComponentByClass<UWEVORAHealthComponent>())
	{
		Health->OnDeath.AddUniqueDynamic(this, &ThisClass::ResetStatuses);
		Health->OnReset.AddUniqueDynamic(this, &ThisClass::ResetStatuses);
	}
}

void UWEVORALocalizedStatusComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWEVORAHealthComponent* Health = GetOwner()->FindComponentByClass<UWEVORAHealthComponent>())
	{
		Health->OnDeath.RemoveDynamic(this, &ThisClass::ResetStatuses);
		Health->OnReset.RemoveDynamic(this, &ThisClass::ResetStatuses);
	}
	ResetStatuses();
	Super::EndPlay(Reason);
}

bool UWEVORALocalizedStatusComponent::HasStatus(FName Zone, FName Type) const
{
	return States.ContainsByPredicate([Zone, Type](const FWEVORALocalStatus& S)
	{ return (Zone.IsNone() || S.Zone == Zone) && S.Type == Type && S.Remaining > 0; });
}

float UWEVORALocalizedStatusComponent::GetRemaining(FName Type) const
{
	float Result = 0;
	for (const FWEVORALocalStatus& S : States) { if (S.Type == Type) { Result = FMath::Max(Result, S.Remaining); } }
	return Result;
}

bool UWEVORALocalizedStatusComponent::ApplyStatus(const FWEVORALocalStatus& Application)
{
	if (!CanMutate() || !GetProfile()->ContainsZone(Application.Zone) || Application.Type.IsNone()
		|| !FMath::IsFinite(Application.Intensity) || !FMath::IsFinite(Application.Accumulation)
		|| !FMath::IsFinite(Application.Remaining) || !FMath::IsFinite(Application.DamagePerSecond)
		|| Application.Intensity <= 0 || Application.Remaining <= 0) { return false; }
	FWEVORALocalStatus Next = Application;
	if (UWEVORASpellReactionComponent* Legacy = GetOwner()->FindComponentByClass<UWEVORASpellReactionComponent>())
	{
		Legacy->ClearLegacyBurning();
	}
	FWEVORALocalStatus* Previous = States.FindByPredicate([&](const FWEVORALocalStatus& S)
	{ return S.Zone == Next.Zone && S.Type == Next.Type; });
	if (Previous)
	{
		// Refreshing with a weaker hit must not reattribute the stronger retained DoT to that hit.
		if (Next.Policy != EWEVORAStatusPolicy::Replace && (Previous->DamagePerSecond > Next.DamagePerSecond
			|| (Previous->DamagePerSecond == Next.DamagePerSecond
				&& GetPathNameSafe(Previous->Source.Get()) < GetPathNameSafe(Next.Source.Get()))))
		{
			Next.Source = Previous->Source; Next.Instigator = Previous->Instigator;
		}
		switch (Next.Policy)
		{
		case EWEVORAStatusPolicy::Replace: break;
		case EWEVORAStatusPolicy::Refresh:
			Next.Intensity = FMath::Max(Previous->Intensity, Next.Intensity);
			Next.Accumulation = FMath::Max(Previous->Accumulation, Next.Accumulation);
			Next.DamagePerSecond = FMath::Max(Previous->DamagePerSecond, Next.DamagePerSecond);
			Next.Remaining = FMath::Max(Previous->Remaining, Next.Remaining);
			break;
		case EWEVORAStatusPolicy::Accumulate:
			Next.Intensity += Previous->Intensity; Next.Accumulation += Previous->Accumulation;
			Next.DamagePerSecond += Previous->DamagePerSecond;
			Next.Remaining = FMath::Max(Previous->Remaining, Next.Remaining);
			break;
		case EWEVORAStatusPolicy::Extend:
			Next.Remaining += Previous->Remaining;
			Next.Intensity = FMath::Max(Previous->Intensity, Next.Intensity);
			Next.Accumulation = FMath::Max(Previous->Accumulation, Next.Accumulation);
			Next.DamagePerSecond = FMath::Max(Previous->DamagePerSecond, Next.DamagePerSecond);
			break;
		}
	}
	Next.Intensity = FMath::Clamp(Next.Intensity, 0.0f, MaxIntensity);
	Next.Accumulation = FMath::Clamp(Next.Accumulation, 0.0f, MaxAccumulation);
	Next.Remaining = FMath::Clamp(Next.Remaining, 0.0f, MaxDuration);
	Next.DamagePerSecond = FMath::Clamp(Next.DamagePerSecond, 0.0f, MaxDamagePerSecond);
	Next.AppliedAt = GetWorld()->GetTimeSeconds();
	if (Previous) { *Previous = Next; } else { States.Add(Next); }
	States.Sort([](const FWEVORALocalStatus& A, const FWEVORALocalStatus& B)
	{ return A.Zone == B.Zone ? A.Type.LexicalLess(B.Type) : A.Zone.LexicalLess(B.Zone); });
	SetComponentTickEnabled(true);
	if (!bApplyingBatch) { OnStatesChanged.Broadcast(); }
	return true;
}

bool UWEVORALocalizedStatusComponent::ApplyStatusBatch(const TArray<FWEVORALocalStatus>& Applications)
{
	bool bApplied = false;
	{
		TGuardValue<bool> Guard(bApplyingBatch, true);
		for (const FWEVORALocalStatus& S : Applications) { bApplied |= ApplyStatus(S); }
	}
	if (bApplied && !bApplyingBatch) { OnStatesChanged.Broadcast(); }
	return bApplied;
}

void UWEVORALocalizedStatusComponent::RemoveStatus(FName Zone, FName Type)
{
	if (!GetOwner()->HasAuthority()) { return; }
	States.RemoveAll([Zone, Type](const FWEVORALocalStatus& S) { return S.Zone == Zone && S.Type == Type; });
	SetComponentTickEnabled(!States.IsEmpty() || !PendingInputs.IsEmpty()); OnStatesChanged.Broadcast();
}

void UWEVORALocalizedStatusComponent::ResetStatuses()
{
	if (!GetOwner()->HasAuthority()) { return; }
	States.Reset(); LastHitZone = NAME_None; LastPeriodicDamage = 0;
	PendingInputs.Reset(); LastPropagationPaths.Reset();
	SetComponentTickEnabled(false); OnStatesChanged.Broadcast();
}

void UWEVORALocalizedStatusComponent::AdvanceStatuses(float Seconds)
{
	if (!CanMutate()) { ResetStatuses(); return; }
	if (!FMath::IsFinite(Seconds) || Seconds <= 0) { return; }
	// Integrate the maximum DPS envelope exactly across expiration boundaries.
	TArray<float> Boundaries{0, Seconds};
	for (const FWEVORALocalStatus& S : States) { Boundaries.AddUnique(FMath::Min(S.Remaining, Seconds)); }
	Boundaries.Sort();
	double Damage = 0;
	TWeakObjectPtr<AActor> Source;
	TWeakObjectPtr<AController> Instigator;
	float Strongest = -1;
	for (int32 I = 1; I < Boundaries.Num(); ++I)
	{
		float Rate = 0;
		for (const FWEVORALocalStatus& S : States)
		{
			if (S.Remaining <= Boundaries[I-1]) { continue; }
			Rate = FMath::Max(Rate, S.DamagePerSecond);
			if (S.DamagePerSecond > Strongest) { Strongest = S.DamagePerSecond; Source = S.Source; Instigator = S.Instigator; }
		}
		Damage += FMath::Min(Rate, MaxDamagePerSecond) * double(Boundaries[I] - Boundaries[I-1]);
	}
	for (FWEVORALocalStatus& S : States) { S.Remaining = FMath::Max(0.0f, S.Remaining - Seconds); }
	States.RemoveAll([](const FWEVORALocalStatus& S) { return S.Remaining <= KINDA_SMALL_NUMBER; });
	LastPeriodicDamage = float(Damage);
	// Final HP mutation goes through the same Health receiver as direct damage.
	if (Damage > 0) { UGameplayStatics::ApplyDamage(GetOwner(), float(Damage), Instigator.Get(), Source.Get(), UDamageType::StaticClass()); }
	SetComponentTickEnabled(!States.IsEmpty() || !PendingInputs.IsEmpty());
	OnStatesChanged.Broadcast();
}

void UWEVORALocalizedStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CommitReactions();
	AdvanceStatuses(DeltaTime);
}

bool UWEVORALocalizedStatusComponent::QueueReaction(const FWEVORASpellData& Spell, const FHitResult& Hit)
{
	if (!CanMutate() || Spell.Power <= 0 || !FMath::IsFinite(Spell.Power) || Hit.ImpactPoint.ContainsNaN()) { return false; }
	LastHitZone = GetProfile()->ResolveHit(Hit, GetOwner());
	if (LastHitZone.IsNone()) { return false; }
	const UWEVORALocalReactionProfile* Rules = ReactionProfile ? ReactionProfile.Get() : GetDefault<UWEVORALocalReactionProfile>();
	if (!Rules->Rules.ContainsByPredicate([&](const FWEVORALocalReactionRule& R) { return R.Trigger == Spell.Element; })) { return false; }
	FWEVORALocalReactionInput Input;
	Input.Zone = LastHitZone; Input.Element = Spell.Element; Input.Power = Spell.Power;
	Input.LocalImpact = GetOwner()->GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
	// Reject exact duplicate contacts inside a batch, rather than multiplying propagation.
	const bool Duplicate = PendingInputs.ContainsByPredicate([&](const FWEVORALocalReactionInput& P)
	{ return P.Zone == Input.Zone && P.Element == Input.Element && P.Power == Input.Power && P.LocalImpact == Input.LocalImpact; });
	if (!Duplicate)
	{
		PendingInputs.Add(Input);
		if (PendingInputs.Num() > 128)
		{
			// Even overload must be order-independent. Keep a canonical strongest 128, not the first 128 callbacks.
			PendingInputs.Sort([](const FWEVORALocalReactionInput& A, const FWEVORALocalReactionInput& B)
			{
				if (A.Power != B.Power) { return A.Power > B.Power; }
				if (A.Zone != B.Zone) { return A.Zone.LexicalLess(B.Zone); }
				if (A.Element != B.Element) { return uint8(A.Element) < uint8(B.Element); }
				if (A.LocalImpact.X != B.LocalImpact.X) { return A.LocalImpact.X < B.LocalImpact.X; }
				if (A.LocalImpact.Y != B.LocalImpact.Y) { return A.LocalImpact.Y < B.LocalImpact.Y; }
				return A.LocalImpact.Z < B.LocalImpact.Z;
			});
			PendingInputs.SetNum(128);
		}
	}
	SetComponentTickEnabled(true);
	return true;
}

void UWEVORALocalizedStatusComponent::CommitReactions()
{
	if (!CanMutate()) { ResetStatuses(); return; }
	if (PendingInputs.IsEmpty()) { return; }
	TArray<FWEVORALocalReactionInput> Batch = MoveTemp(PendingInputs);
	PendingInputs.Reset();
	const UWEVORALocalReactionProfile* Rules = ReactionProfile ? ReactionProfile.Get() : GetDefault<UWEVORALocalReactionProfile>();
	// Every event/rule reads the same immutable snapshot. New propagation never becomes a seed in this batch.
	const FWEVORALocalReactionResult Result = WEVORALocalReactions::Solve(*GetProfile(), *Rules, *GetOwner(), States, Batch);
	LastPropagationPaths = Result.Paths; LastPropagationPaths.Sort();
	if (!ApplyStatusBatch(Result.Applications)) { OnStatesChanged.Broadcast(); }
}
