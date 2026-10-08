#include "AI/WEVORAEnemy.h"
#include "AI/WEVORAEnemyProjectile.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "AIController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AWEVORAEnemy::AWEVORAEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(45.0f);
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.9f));
	Muzzle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Collision);
	Muzzle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Muzzle->SetRelativeLocation(FVector(60.0f, 0.0f, 0.0f));
	Muzzle->SetRelativeScale3D(FVector(0.25f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) { Body->SetStaticMesh(Sphere.Object); Muzzle->SetStaticMesh(Sphere.Object); }
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->MaxSpeed = MoveSpeed;
	Movement->Acceleration = 1600.0f;
	Movement->Deceleration = 2400.0f;
	HealthComponent = CreateDefaultSubobject<UWEVORAHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->bDestroyOwnerOnDeath = true;
	SpellReactionComponent = CreateDefaultSubobject<UWEVORASpellReactionComponent>(TEXT("SpellReactionComponent"));
	BurningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BurningLight"));
	BurningLight->SetupAttachment(Collision);
	BurningLight->SetLightColor(FLinearColor(1.0f, 0.2f, 0.02f));
	BurningLight->SetIntensity(1800.0f);
	BurningLight->SetAttenuationRadius(200.0f);
	BurningLight->SetCastShadows(false);
	BurningLight->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/LevelPrototyping/Interactable/JumpPad/Assets/Materials/M_SimpleGlow.M_SimpleGlow"));
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UStaticMeshComponent* Flame = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("BurningFlame%d"), Index));
		Flame->SetupAttachment(Collision);
		Flame->SetAbsolute(false, true, false);
		Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flame->SetCastShadow(false);
		Flame->SetVisibility(false);
		if (Cone.Succeeded()) { Flame->SetStaticMesh(Cone.Object); }
		if (Glow.Succeeded()) { Flame->SetMaterial(0, Glow.Object); }
		BurningFlames.Add(Flame);
	}
	ProjectileClass = AWEVORAEnemyProjectile::StaticClass();
}

void AWEVORAEnemy::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) { return; }
	// Reuse the project's glow material; no new shader or Niagara assets are required.
	if (!BurningFlames.IsEmpty())
	{
		UMaterialInstanceDynamic* Glow = BurningFlames[0]->CreateDynamicMaterialInstance(0);
		if (Glow)
		{
			TArray<FMaterialParameterInfo> Parameters;
			TArray<FGuid> IDs;
			Glow->GetAllVectorParameterInfo(Parameters, IDs);
			for (const FMaterialParameterInfo& Parameter : Parameters)
			{
				Glow->SetVectorParameterValue(Parameter.Name, FLinearColor(3.0f, 0.35f, 0.01f));
			}
			for (UStaticMeshComponent* Flame : BurningFlames) { Flame->SetMaterial(0, Glow); }
		}
	}
	UpdateBurningVisual();
}

void AWEVORAEnemy::UpdateBurningVisual()
{
	const bool bBurning = GetNetMode() != NM_DedicatedServer && bShowBurningEffect
		&& HealthComponent->IsAlive() && SpellReactionComponent->HasState(EWEVORASpellTargetState::Burning);
	BurningLight->SetVisibility(bBurning);
	const float Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < BurningFlames.Num(); ++Index)
	{
		UStaticMeshComponent* Flame = BurningFlames[Index];
		Flame->SetVisibility(bBurning);
		if (!bBurning) { continue; }
		const float Phase = FMath::Frac(Time * 1.3f + Index / 6.0f);
		const float Angle = Index * PI / 3.0f + Time * 0.3f;
		const float Pulse = 0.8f + 0.2f * FMath::Sin(Time * 17.0f + Index * 2.0f);
		Flame->SetWorldLocation(GetActorLocation() + FVector(FMath::Cos(Angle) * 32.0f, FMath::Sin(Angle) * 32.0f, -20.0f + Phase * 85.0f));
		Flame->SetRelativeScale3D(FVector(0.16f * (1.0f - Phase) * Pulse, 0.16f * (1.0f - Phase) * Pulse, 0.45f * Pulse));
	}
	if (bBurning) { BurningLight->SetIntensity(1800.0f + 400.0f * FMath::Sin(Time * 19.0f)); }
}

bool AWEVORAEnemy::IsValidTarget(const APawn* Candidate) const
{
	if (!IsValid(Candidate) || Candidate == this || !Candidate->IsPlayerControlled()) { return false; }
	const UWEVORAHealthComponent* Health = Candidate->FindComponentByClass<UWEVORAHealthComponent>();
	return (!Health || Health->IsAlive()) && FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation())
		<= FMath::Square(FMath::Max(0.0f, DetectionRange));
}

bool AWEVORAEnemy::CanSeeTarget() const
{
	if (!IsValidTarget(Target)) { return false; }
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemySight), false, this);
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(),
		Target->GetActorLocation(), ECC_Visibility, Query);
	return !bBlocked || Hit.GetActor() == Target;
}

void AWEVORAEnemy::SetWindupVisual(bool bActive)
{
	bWindingUp = bActive;
	Muzzle->SetRelativeScale3D(FVector(bActive ? 0.5f : 0.25f));
}

void AWEVORAEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBurningVisual();
	if (!HasAuthority() || !HealthComponent->IsAlive()) { return; }
	// Yield steering and attacks to swept reaction displacement; resume normal AI afterward.
	if (SpellReactionComponent->IsDisplaced())
	{
		Movement->StopMovementImmediately();
		ConsumeMovementInputVector();
		SetWindupVisual(false);
		return;
	}
	Movement->MaxSpeed = FMath::Max(0.0f, MoveSpeed);
	if (!IsValidTarget(Target) || !CanSeeTarget())
	{
		Target = nullptr;
		SetWindupVisual(false);
		float Nearest = TNumericLimits<float>::Max();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Candidate = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (!IsValidTarget(Candidate)) { continue; }
			const float Distance = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
			if (Distance >= Nearest) { continue; }
			APawn* PreviousTarget = Target;
			Target = Candidate;
			if (CanSeeTarget()) { Nearest = Distance; }
			else { Target = PreviousTarget; }
		}
	}
	if (!Target) { Movement->StopMovementImmediately(); return; }
	const FVector Offset = Target->GetActorLocation() - GetActorLocation();
	const float Distance = Offset.Size();
	SetActorRotation(Offset.Rotation());
	const float Now = GetWorld()->GetTimeSeconds();
	if (bWindingUp)
	{
		Movement->StopMovementImmediately();
		if (Distance > FMath::Max(0.0f, AttackRange)) { SetWindupVisual(false); }
		else if (Now >= FireTime) { Fire(); }
		return;
	}
	const float DesiredDistance = FMath::Min(FMath::Max(0.0f, PreferredDistance), FMath::Max(0.0f, AttackRange));
	if (Distance > DesiredDistance + 50.0f) { AddMovementInput(Offset.GetSafeNormal()); }
	else { Movement->StopMovementImmediately(); }
	if (Distance <= FMath::Max(0.0f, AttackRange) && Now >= NextAttackTime && ProjectileClass)
	{
		SetWindupVisual(true);
		FireTime = Now + FMath::Max(0.0f, WindupTime);
		OnAttackStarted();
	}
}

void AWEVORAEnemy::Fire()
{
	SetWindupVisual(false);
	NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, AttackInterval);
	if (!IsValidTarget(Target) || !CanSeeTarget() || !ProjectileClass) { return; }
	// Start just outside the body. Check this short segment too, so a muzzle inside a wall cannot fire through it.
	const FVector Start = Muzzle->GetComponentLocation();
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyMuzzle), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Start, ECC_Visibility, Query)) { return; }
	const FTransform ShotTransform((Target->GetActorLocation() - Start).Rotation(), Start);
	AWEVORAEnemyProjectile* Shot = GetWorld()->SpawnActorDeferred<AWEVORAEnemyProjectile>(
		ProjectileClass, ShotTransform, this, this, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (Shot)
	{
		Shot->Damage = FMath::Max(0.0f, AttackDamage);
		Shot->Speed = FMath::Max(1.0f, ProjectileSpeed);
		UGameplayStatics::FinishSpawningActor(Shot, ShotTransform);
		OnAttackFired();
	}
}
