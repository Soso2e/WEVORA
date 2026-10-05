#include "Spell/WEVORASpellCastComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellProjectile.h"
#include "AI/WEVORAHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "WEVORA.h"
#include "WEVORACharacter.h"

UWEVORASpellCastComponent::UWEVORASpellCastComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ProjectileClass = AWEVORASpellProjectile::StaticClass();
	ElementParameters.Add(EWEVORASpellElement::Fire, FWEVORASpellProjectileParameters());
	FWEVORASpellProjectileParameters Wind;
	Wind.Speed = 2600.0f;
	Wind.Color = FLinearColor(0.1f, 0.8f, 1.0f);
	ElementParameters.Add(EWEVORASpellElement::Wind, Wind);
}

void UWEVORASpellCastComponent::BeginPlay()
{
	Super::BeginPlay();
	Weaving = GetOwner()->FindComponentByClass<UWEVORASpellWeavingComponent>();
	if (Weaving) { Weaving->OnSpellCast.AddUniqueDynamic(this, &ThisClass::HandleCast); }
}

void UWEVORASpellCastComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Weaving) { Weaving->OnSpellCast.RemoveDynamic(this, &ThisClass::HandleCast); }
	Super::EndPlay(EndPlayReason);
}

bool UWEVORASpellCastComponent::ResolveSpell(const FWEVORASpellContext& Context,
	FWEVORASpellProjectileParameters& OutParameters) const
{
	const FWEVORASpellProjectileParameters* Profile = ElementParameters.Find(Context.Element);
	if (!Profile || Context.Gesture == EWEVORAGesture::None) { return false; }
	OutParameters = *Profile;
	// All shapes currently produce one bolt. Minimal shape influence, without named spell classes.
	if (Context.Gesture == EWEVORAGesture::Thrust) { OutParameters.Speed *= 1.2f; }
	if (Context.Gesture == EWEVORAGesture::Circle) { OutParameters.Radius *= 1.5f; }
	OutParameters.Speed = FMath::Max(1.0f, OutParameters.Speed);
	OutParameters.Damage = FMath::Max(0.0f, OutParameters.Damage);
	OutParameters.Radius = FMath::Max(1.0f, OutParameters.Radius);
	OutParameters.Lifetime = FMath::Max(0.1f, OutParameters.Lifetime);
	return true;
}

void UWEVORASpellCastComponent::HandleCast(const FWEVORASpellContext& Context)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || !Pawn->GetController() || !ProjectileClass) { return; }
	const UWEVORAHealthComponent* Health = Pawn->FindComponentByClass<UWEVORAHealthComponent>();
	if (Health && !Health->IsAlive()) { return; }
	FWEVORASpellLaunch Launch;
	Launch.Composition = Context;
	if (!ResolveSpell(Context, Launch.Parameters)) { return; }
	FVector ViewLocation;
	FRotator ViewRotation;
	Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	if (APlayerController* ViewController = Cast<APlayerController>(Pawn->GetController()))
	{
		ViewController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	// Aim at the camera centre, but launch from the player: walls at the player still block the shot.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellAim), false, Pawn);
	const FVector AimEnd = ViewLocation + ViewRotation.Vector() * FMath::Max(1.0f, AimDistance);
	FHitResult AimHit;
	const bool bAimHit = GetWorld()->LineTraceSingleByObjectType(AimHit, ViewLocation, AimEnd, Objects, Query);
	const FVector Origin = Pawn->GetPawnViewLocation();
	Launch.Direction = ((bAimHit ? AimHit.ImpactPoint : AimEnd) - Origin).GetSafeNormal();
	if (Launch.Direction.IsNearlyZero() || FVector::DotProduct(Launch.Direction, ViewRotation.Vector()) <= 0.0f)
	{
		if (bLogEvents) { UE_LOG(LogWEVORA, Log, TEXT("Spell spawn blocked: camera aim is behind launch origin")); }
		return;
	}
	const FVector Start = Origin + Launch.Direction * FMath::Max(0.0f, SpawnDistance);
	// Sweep the entire launch segment, including the final sphere. Never spawn through a nearby wall.
	FHitResult MuzzleHit;
	if (GetWorld()->SweepSingleByObjectType(MuzzleHit, Origin, Start, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(Launch.Parameters.Radius), Query))
	{
		if (bLogEvents) { UE_LOG(LogWEVORA, Log, TEXT("Spell spawn blocked: %s"), *GetNameSafe(MuzzleHit.GetActor())); }
		return;
	}
	const FTransform Transform(Launch.Direction.Rotation(), Start);
	AWEVORASpellProjectile* Shot = GetWorld()->SpawnActorDeferred<AWEVORASpellProjectile>(
		ProjectileClass, Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Shot) { return; }
	const FVector ShooterVelocity = Pawn->GetVelocity();
	Launch.InheritedVelocity = FVector(ShooterVelocity.X, ShooterVelocity.Y, 0.0f) *
		FMath::Clamp(HorizontalVelocityInheritance, 0.0f, 1.0f);
	Shot->bLogEvents = bLogEvents;
	Shot->InitializeSpell(Launch);
	UGameplayStatics::FinishSpawningActor(Shot, Transform);
	if (AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(Pawn))
	{
		Pilot->ApplySpellLaunchFeedback(Launch.Direction,
			Context.Gesture == EWEVORAGesture::Thrust ? ThrustRecoilSpeed : RecoilSpeed);
	}
	if (bLogEvents)
	{
		UE_LOG(LogWEVORA, Log, TEXT("Spell spawn: %s element=%s gesture=%s direction=%s damage=%.1f speed=%.1f"),
			*GetNameSafe(Shot), *UEnum::GetValueAsString(Context.Element), *UEnum::GetValueAsString(Context.Gesture),
			*Launch.Direction.ToCompactString(), Launch.Parameters.Damage, Launch.Parameters.Speed);
	}
}
