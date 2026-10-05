#include "Spell/WEVORASpellProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "WEVORA.h"

AWEVORASpellProjectile::AWEVORASpellProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 4.0f;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(16.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnHit);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeScale3D(FVector(0.32f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) { Visual->SetStaticMesh(Sphere.Object); }
	PreviewLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PreviewLight"));
	PreviewLight->SetupAttachment(Collision);
	PreviewLight->SetCastShadows(false);
	PreviewLight->SetIntensity(3000.0f);
	PreviewLight->SetAttenuationRadius(150.0f);
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 0.0f; // Initialized explicitly from the cast snapshot.
	Movement->MaxSpeed = 2200.0f;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
}

void AWEVORASpellProjectile::InitializeSpell_Implementation(const FWEVORASpellLaunch& InLaunch)
{
	Super::InitializeSpell_Implementation(InLaunch);
	Launch.Direction = Launch.Direction.GetSafeNormal();
	Launch.Parameters.Speed = FMath::Max(1.0f, Launch.Parameters.Speed);
	Launch.Parameters.Radius = FMath::Max(1.0f, Launch.Parameters.Radius);
	Launch.Parameters.Lifetime = FMath::Max(0.1f, Launch.Parameters.Lifetime);
	Collision->SetSphereRadius(Launch.Parameters.Radius);
	Collision->IgnoreActorWhenMoving(GetOwner(), true);
	Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	Visual->SetRelativeScale3D(FVector(Launch.Parameters.Radius / 50.0f));
	PreviewLight->SetLightColor(Launch.Parameters.Color);
}

void AWEVORASpellProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Shooter = GetInstigator()) { Shooter->MoveIgnoreActorAdd(this); }
	Movement->Velocity = Launch.Direction * Launch.Parameters.Speed +
		FVector(Launch.InheritedVelocity.X, Launch.InheritedVelocity.Y, 0.0f);
	// Do not clamp away the shooter velocity contribution on the first movement tick.
	Movement->MaxSpeed = FMath::Max(Launch.Parameters.Speed, float(Movement->Velocity.Size()));
	SetLifeSpan(Launch.Parameters.Lifetime);
}

void AWEVORASpellProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* Shooter = GetInstigator()) { Shooter->MoveIgnoreActorRemove(this); }
	Super::EndPlay(EndPlayReason);
}

void AWEVORASpellProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHitConsumed || OtherActor == GetOwner() || OtherActor == GetInstigator()) { return; }
	bHitConsumed = true;
	Movement->StopMovementImmediately();
	const bool bApplied = DeliverToTarget(OtherActor, Hit);
	if (bLogEvents)
	{
		UE_LOG(LogWEVORA, Log, TEXT("Spell hit: actor=%s element=%s shape=%s reaction=%s"),
			*GetNameSafe(OtherActor), *UEnum::GetValueAsString(Launch.Spell.Element),
			*UEnum::GetValueAsString(Launch.Spell.Shape), bApplied ? TEXT("applied") : TEXT("none"));
	}
	Destroy();
}
