#include "Spell/WEVORASpellVolume.h"
#include "Spell/WEVORASpellEffectComponent.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "AI/WEVORAHealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

AWEVORASpellVolume::AWEVORASpellVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Origin")));
	AttackEffect = CreateDefaultSubobject<UWEVORASpellEffectComponent>(TEXT("AttackEffect"));
	AttackEffect->SetupAttachment(RootComponent);
	Outline = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VolumeOutline"));
	Outline->SetupAttachment(RootComponent);
	Outline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Outline->SetGenerateOverlapEvents(false);
	Outline->SetCastShadow(false);
	Outline->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/WEVORA/Effects/M_AttackEffect.M_AttackEffect"));
	Outline->SetStaticMesh(Cube.Object);
	OutlineMaterial = Glow.Object;
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
	if (GetNetMode() != NM_DedicatedServer)
	{
		UMaterialInstanceDynamic* Material = OutlineMaterial ? UMaterialInstanceDynamic::Create(OutlineMaterial, this) : nullptr;
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("SpellColor"), Launch.Parameters.Color * 0.65f);
			Outline->SetMaterial(0, Material);
		}
		const bool bSweep = Launch.Spell.Delivery == EWEVORASpellDelivery::Sweep;
		// World-horizontal ring for area denial; a forward fan for the directional sweep.
		if (!bSweep) { Outline->SetWorldRotation(FRotator::ZeroRotator); }
		for (int32 Index = 0; Index < 40; ++Index)
		{
			auto Point = [&](int32 I)
			{
				const float Angle = bSweep ? -PI * 0.5f + I * PI / 40 : I * 2 * PI / 40;
				return FVector(FMath::Cos(Angle) * (bSweep ? Launch.Volume.Range : Launch.Volume.Width),
					FMath::Sin(Angle) * Launch.Volume.Width, 0);
			};
			const FVector A = Point(Index), B = Point(Index + 1), Delta = B - A;
			Outline->AddInstance(FTransform(Delta.Rotation(), (A+B)*0.5f, FVector(Delta.Size()/100, 0.045f, 0.045f)));
		}
	}
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
	BeginDeliveryEvent();
	const bool bSweep = Launch.Spell.Delivery == EWEVORASpellDelivery::Sweep;
	const float Radius = bSweep ? FMath::Sqrt(FMath::Square(Launch.Volume.Range) +
		FMath::Square(Launch.Volume.Width) + FMath::Square(Launch.Volume.Height)) : Launch.Volume.Width;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellVolume), false, GetOwner());
	Query.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams::AllDynamicObjects, FCollisionShape::MakeSphere(Radius), Query);
	TMap<AActor*, TArray<FHitResult>> HitsByTarget;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Target == GetInstigator()) { continue; }
		UWEVORAHealthComponent* Health = Target->FindComponentByClass<UWEVORAHealthComponent>();
		if (!Health || !Health->IsAlive()) { continue; }
		FVector Position = Target->GetActorLocation();
		FHitResult ZoneHit(Target, Overlap.GetComponent(), Position, -Launch.Direction);
		ZoneHit.Item = Overlap.ItemIndex;
		if (const UWEVORALocalizedStatusComponent* Local = Target->FindComponentByClass<UWEVORALocalizedStatusComponent>())
		{
			if (const UWEVORAZoneProfile* Profile = Local->GetProfile())
			{
				const FName Zone = Profile->ResolveHit(ZoneHit, Target);
				if (Zone.IsNone()) { continue; }
				Position = Profile->GetZonePosition(Zone, Target);
			}
		}
		const FVector Relative = GetActorTransform().InverseTransformPosition(Position);
		if (bSweep && (Relative.X < 0 || Relative.X > Launch.Volume.Range ||
			FMath::Abs(Relative.Y) > Launch.Volume.Width || FMath::Abs(Relative.Z) > Launch.Volume.Height)) { continue; }
		if (bSweep && FMath::Square(Relative.X / Launch.Volume.Range) +
			FMath::Square(Relative.Y / Launch.Volume.Width) > 1) { continue; }
		if (!bSweep && FVector::DistSquared(Position, GetActorLocation()) > FMath::Square(Radius)) { continue; }
		FHitResult Sight;
		// Covers both world geometry and intervening pawns. Volumes never damage through walls.
		if (GetWorld()->LineTraceSingleByChannel(Sight, GetActorLocation(), Position, ECC_Visibility, Query)
			&& Sight.GetActor() != Target) { continue; }
		ZoneHit.ImpactPoint = ZoneHit.Location = Position;
		HitsByTarget.FindOrAdd(Target).Add(ZoneHit);
	}
	TArray<AActor*> Targets; HitsByTarget.GetKeys(Targets);
	Targets.Sort([](const AActor& A, const AActor& B) { return A.GetPathName() < B.GetPathName(); });
	for (AActor* Target : Targets)
	{
		TArray<FHitResult>& Hits = HitsByTarget[Target];
		Hits.Sort([](const FHitResult& A, const FHitResult& B)
		{ return GetNameSafe(A.GetComponent()) < GetNameSafe(B.GetComponent()); });
		if (DeliverToTargetZones(Target, Hits)) { AttackEffect->SpawnImpact(Hits[0]); }
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
