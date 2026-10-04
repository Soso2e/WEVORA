#include "Spell/WEVORASpellProjectile.h"
#include "AI/WEVORAHealthComponent.h"
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

void AWEVORASpellProjectile::InitializeSpell(const FWEVORASpellLaunch& InLaunch)
{
	Launch = InLaunch;
	Launch.Direction = Launch.Direction.GetSafeNormal();
	Launch.Parameters.Speed = FMath::Max(1.0f, Launch.Parameters.Speed);
	Launch.Parameters.Damage = FMath::Max(0.0f, Launch.Parameters.Damage);
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
	Movement->MaxSpeed = Launch.Parameters.Speed;
	Movement->Velocity = Launch.Direction * Launch.Parameters.Speed;
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
	UWEVORAHealthComponent* Health = IsValid(OtherActor) ? OtherActor->FindComponentByClass<UWEVORAHealthComponent>() : nullptr;
	const APawn* OtherPawn = Cast<APawn>(OtherActor);
	float AppliedDamage = 0.0f;
	// Existing health is the interaction contract. Never damage another player in this single-player phase.
	if (HasAuthority() && Health && Health->IsAlive() && (!OtherPawn || !OtherPawn->IsPlayerControlled()))
	{
		const float Before = Health->Health;
		UGameplayStatics::ApplyPointDamage(OtherActor, Launch.Parameters.Damage, Launch.Direction,
			Hit, GetInstigatorController(), this, UDamageType::StaticClass());
		AppliedDamage = Before - Health->Health;
	}
	if (bLogEvents)
	{
		UE_LOG(LogWEVORA, Log, TEXT("Spell hit: actor=%s element=%s gesture=%s appliedDamage=%.1f HP=%.1f"),
			*GetNameSafe(OtherActor), *UEnum::GetValueAsString(Launch.Composition.Element),
			*UEnum::GetValueAsString(Launch.Composition.Gesture), AppliedDamage, Health ? Health->Health : -1.0f);
	}
	Destroy();
}
