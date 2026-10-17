#include "Spell/WEVORASpellVolume.h"
#include "Spell/WEVORASpellEffectComponent.h"
#include "AI/WEVORAHealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

AWEVORASpellVolume::AWEVORASpellVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Origin")));
	AttackEffect = CreateDefaultSubobject<UWEVORASpellEffectComponent>(TEXT("AttackEffect"));
	AttackEffect->SetupAttachment(RootComponent);
}

void AWEVORASpellVolume::InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch)
{
	Super::InitializeSpell_Implementation(InLaunch);
	Launch.Volume.Range = FMath::Clamp(Launch.Volume.Range, 1.0f, 5000.0f);
	Launch.Volume.Width = FMath::Clamp(Launch.Volume.Width, 1.0f, 1500.0f);
	Launch.Volume.Height = FMath::Clamp(Launch.Volume.Height, 1.0f, 1500.0f);
	Launch.Volume.Duration = FMath::Clamp(Launch.Volume.Duration, 0.05f, 5.0f);
	Launch.Volume.PulseInterval = FMath::Clamp(Launch.Volume.PulseInterval, 0.1f, 5.0f);
}

void AWEVORASpellVolume::BeginPlay()
{
	Super::BeginPlay();
	FWEVORASpellLaunch Cosmetic = Launch;
	Cosmetic.Parameters.Radius = FMath::Clamp(Launch.Volume.Width * 0.18f, 25.0f, 100.0f);
	AttackEffect->StartEffect(Cosmetic);
	SetLifeSpan(Launch.Spell.Delivery == EWEVORASpellDelivery::Field ? Launch.Volume.Duration : 0.25f);
	if (!HasAuthority()) { return; }
	Pulse();
	if (Launch.Spell.Delivery == EWEVORASpellDelivery::Field)
	{
		GetWorldTimerManager().SetTimer(PulseTimer, this, &ThisClass::Pulse, Launch.Volume.PulseInterval, true);
	}
}

void AWEVORASpellVolume::Pulse()
{
	if (!HasAuthority()) { return; }
	HitInPulse.Reset();
	const bool bSweep = Launch.Spell.Delivery == EWEVORASpellDelivery::Sweep;
	const float Radius = bSweep ? FMath::Sqrt(FMath::Square(Launch.Volume.Range) +
		FMath::Square(Launch.Volume.Width) + FMath::Square(Launch.Volume.Height)) : Launch.Volume.Width;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellVolume), false, GetOwner());
	Query.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams::AllDynamicObjects, FCollisionShape::MakeSphere(Radius), Query);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Target == GetInstigator() || HitInPulse.Contains(Target)) { continue; }
		HitInPulse.Add(Target);
		UWEVORAHealthComponent* Health = Target->FindComponentByClass<UWEVORAHealthComponent>();
		if (!Health || !Health->IsAlive()) { continue; }
		const FVector Position = Target->GetActorLocation();
		const FVector Relative = GetActorTransform().InverseTransformPosition(Position);
		if (bSweep && (Relative.X < 0 || Relative.X > Launch.Volume.Range ||
			FMath::Abs(Relative.Y) > Launch.Volume.Width || FMath::Abs(Relative.Z) > Launch.Volume.Height)) { continue; }
		if (!bSweep && FVector::DistSquared(Position, GetActorLocation()) > FMath::Square(Radius)) { continue; }
		FHitResult Sight;
		// Covers both world geometry and intervening pawns. Volumes never damage through walls.
		if (GetWorld()->LineTraceSingleByChannel(Sight, GetActorLocation(), Position, ECC_Visibility, Query)
			&& Sight.GetActor() != Target) { continue; }
		FHitResult Hit(Target, Overlap.GetComponent(), Position, -Launch.Direction);
		if (DeliverToTarget(Target, Hit)) { AttackEffect->SpawnImpact(Hit); }
	}
	if (Launch.bDebugTrajectory)
	{
		if (bSweep) { DrawDebugBox(GetWorld(), GetActorLocation() + GetActorForwardVector() * Launch.Volume.Range * 0.5f,
			FVector(Launch.Volume.Range * 0.5f, Launch.Volume.Width, Launch.Volume.Height), GetActorQuat(), FColor::Cyan, false, 0.3f); }
		else { DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 24, FColor::Cyan, false, 0.3f); }
	}
}

void AWEVORASpellVolume::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(PulseTimer);
	Super::EndPlay(Reason);
}
