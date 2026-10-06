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
	FWEVORASpellShapeProfile Forward;
	Forward.SpeedMultiplier = 1.2f;
	ShapeProfiles.Add(EWEVORAGesture::Thrust, Forward);
	FWEVORASpellShapeProfile Sweep;
	Sweep.Shape = EWEVORASpellShape::Sweep;
	ShapeProfiles.Add(EWEVORAGesture::Sweep, Sweep);
	FWEVORASpellShapeProfile Circle;
	Circle.Shape = EWEVORASpellShape::Circle;
	Circle.RadiusMultiplier = 1.5f;
	ShapeProfiles.Add(EWEVORAGesture::Circle, Circle);
	FWEVORASpellShapeProfile Slam;
	Slam.Shape = EWEVORASpellShape::Slam;
	ShapeProfiles.Add(EWEVORAGesture::Slam, Slam);
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
	FWEVORASpellLaunch& OutLaunch) const
{
	const FWEVORASpellProjectileParameters* Profile = ElementParameters.Find(Context.Element);
	const FWEVORASpellShapeProfile* Shape = ShapeProfiles.Find(Context.Gesture);
	if (!Profile || !Shape || SpellPower <= 0.0f || !FMath::IsFinite(SpellPower)) { return false; }
	OutLaunch = FWEVORASpellLaunch();
	OutLaunch.Composition = Context;
	OutLaunch.Spell.Element = Context.Element;
	OutLaunch.Spell.Shape = Shape->Shape;
	OutLaunch.Spell.Delivery = Shape->Delivery;
	OutLaunch.Spell.Power = SpellPower;
	OutLaunch.Parameters = *Profile;
	OutLaunch.Parameters.Speed = FMath::Max(1.0f, Profile->Speed * Shape->SpeedMultiplier);
	OutLaunch.Parameters.Radius = FMath::Max(1.0f, Profile->Radius * Shape->RadiusMultiplier);
	OutLaunch.Parameters.Lifetime = FMath::Max(0.1f, Profile->Lifetime);
	return true;
}

void UWEVORASpellCastComponent::HandleCast(const FWEVORASpellContext& Context)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || !Pawn->GetController()) { return; }
	const UWEVORAHealthComponent* Health = Pawn->FindComponentByClass<UWEVORAHealthComponent>();
	if (Health && !Health->IsAlive()) { return; }
	FWEVORASpellLaunch Launch;
	if (!ResolveSpell(Context, Launch)) { return; }
	TSubclassOf<AWEVORASpellDelivery> DeliveryClass;
	if (const TSubclassOf<AWEVORASpellDelivery>* Configured = DeliveryClasses.Find(Launch.Spell.Delivery))
	{
		DeliveryClass = *Configured;
	}
	else if (Launch.Spell.Delivery == EWEVORASpellDelivery::Projectile)
	{
		DeliveryClass = ProjectileClass.Get();
	}
	if (!DeliveryClass || DeliveryClass->HasAnyClassFlags(CLASS_Abstract))
	{
		if (bLogEvents) { UE_LOG(LogWEVORA, Log, TEXT("Spell delivery unavailable: %s"), *UEnum::GetValueAsString(Launch.Spell.Delivery)); }
		return;
	}
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
	Launch.Spell.Direction = Launch.Direction;
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
	AWEVORASpellDelivery* Shot = GetWorld()->SpawnActorDeferred<AWEVORASpellDelivery>(
		DeliveryClass, Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
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
		UE_LOG(LogWEVORA, Log, TEXT("Spell spawn: %s element=%s shape=%s direction=%s power=%.1f speed=%.1f"),
			*GetNameSafe(Shot), *UEnum::GetValueAsString(Context.Element), *UEnum::GetValueAsString(Launch.Spell.Shape),
			*Launch.Direction.ToCompactString(), Launch.Spell.Power, Launch.Parameters.Speed);
	}
}
