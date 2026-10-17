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
#include "EngineUtils.h"
#include "Playground/WEVORACombatPlayground.h"

UWEVORASpellCastComponent::UWEVORASpellCastComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ProjectileClass = AWEVORASpellProjectile::StaticClass();
	ElementParameters.Add(EWEVORASpellElement::Fire, FWEVORASpellProjectileParameters());
	FWEVORASpellProjectileParameters Wind;
	Wind.Speed = 2600.0f;
	Wind.Color = FLinearColor(0.1f, 0.8f, 1.0f);
	ElementParameters.Add(EWEVORASpellElement::Wind, Wind);
	FWEVORASpellProjectileParameters Lightning;
	Lightning.Speed = 3200.0f;
	Lightning.Color = FLinearColor(0.65f, 0.3f, 1.0f);
	ElementParameters.Add(EWEVORASpellElement::Lightning, Lightning);
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
	AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(Pawn);
	if (!ResolveCastLaunch(Context, Launch)) { return; }
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
	FWEVORAAimSolution Aim;
	if (!GetAimSolution(Launch.Parameters.Radius, Aim) || Aim.bMuzzleBlocked)
	{
		if (bLogEvents) { UE_LOG(LogWEVORA, Log, TEXT("Spell spawn blocked at muzzle")); }
		return;
	}
	Launch.Direction = Aim.Direction;
	Launch.Spell.Direction = Launch.Direction;
	const FVector Start = Aim.Start;
	FVector DeliveryStart = Start;
	if (Launch.Spell.Delivery == EWEVORASpellDelivery::Field || Launch.Spell.Delivery == EWEVORASpellDelivery::Area)
	{
		const FVector Desired = Launch.Spell.Delivery == EWEVORASpellDelivery::Field
			? Start + Launch.Direction * FMath::Min(Launch.Volume.Range, float(FVector::Dist(Start, Aim.Target)))
			: Start + Launch.Direction * FMath::Min(250.0f, Launch.Volume.Range) - FVector(0,0,180);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellVolumePlacement), false, Pawn);
		FHitResult Placement;
		DeliveryStart = GetWorld()->LineTraceSingleByChannel(Placement, Start, Desired, ECC_Visibility, Query)
			? Placement.ImpactPoint + Placement.ImpactNormal * 8 : Desired;
	}
	const FTransform Transform(Launch.Direction.Rotation(), DeliveryStart);
	AWEVORASpellDelivery* Shot = GetWorld()->SpawnActorDeferred<AWEVORASpellDelivery>(
		DeliveryClass, Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Shot) { return; }
	const FVector ShooterVelocity = Pawn->GetVelocity();
	Launch.InheritedVelocity = (bInheritShooterVelocity ? FVector(ShooterVelocity.X, ShooterVelocity.Y, 0.0f) : FVector::ZeroVector) *
		FMath::Clamp(HorizontalVelocityInheritance, 0.0f, 1.0f);
	Shot->bLogEvents = bLogEvents;
	Shot->InitializeSpell(Launch);
	UGameplayStatics::FinishSpawningActor(Shot, Transform);
	if (Pilot)
	{
		Pilot->ApplySpellLaunchFeedback(Launch.Direction, Pilot->GetSpellRecoilSpeed(Context));
	}
	if (bLogEvents)
	{
		UE_LOG(LogWEVORA, Log, TEXT("Spell spawn: %s element=%s shape=%s direction=%s power=%.1f speed=%.1f"),
			*GetNameSafe(Shot), *UEnum::GetValueAsString(Context.Element), *UEnum::GetValueAsString(Launch.Spell.Shape),
			*Launch.Direction.ToCompactString(), Launch.Spell.Power, Launch.Parameters.Speed);
	}
}

bool UWEVORASpellCastComponent::ResolveCastLaunch(const FWEVORASpellContext& Context, FWEVORASpellLaunch& Launch) const
{
	const AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(GetOwner());
	if (!(Pilot ? Pilot->ResolveSpellLaunch(Context, Launch) : ResolveSpell(Context, Launch))) { return false; }
	// Apply after Blueprint resolution as saved design graphs also supply base power.
	if (!FMath::IsFinite(Context.ChargeAlpha) || !FMath::IsFinite(MaxChargePowerMultiplier)) { return false; }
	Launch.Spell.Power *= FMath::Lerp(1.0f, FMath::Max(1.0f, MaxChargePowerMultiplier),
		FMath::Clamp(Context.ChargeAlpha, 0.0f, 1.0f));
	if (const AWEVORACombatPlayground* Trial = AWEVORACombatPlayground::Find(GetWorld()))
	{
		if (Context.Element == EWEVORASpellElement::Lightning) { Launch.Parameters.Speed = Trial->LightningSpeed; }
		Launch.Parameters.Speed *= Trial->ProjectileSpeedMultiplier;
		Launch.Parameters.Radius *= Trial->ProjectileRadiusMultiplier * FMath::Lerp(1.0f,
			FMath::Max(1.0f, Trial->ChargedRadiusMultiplier), FMath::Clamp(Context.ChargeAlpha, 0.0f, 1.0f));
		Launch.Parameters.MaxDistance = Trial->ProjectileRange;
		Launch.Spell.Power *= Trial->DamageMultiplier;
		Launch.bDebugTrajectory = Trial->bDebugOverlay;
	}
	if (const FWEVORASpellShapeProfile* Shape = ShapeProfiles.Find(Context.Gesture))
	{
		Launch.Volume = *Shape;
		Launch.Spell.Power *= Shape->PowerMultiplier;
		if (Context.GestureMagnitude > 0)
		{
			Launch.Parameters.Speed *= Shape->WovenSpeedMultiplier;
			Launch.Spell.Power *= Shape->WovenPowerMultiplier;
		}
	}
	// Blueprint rules may supply custom data; reject invalid physics inputs before a sweep or spawn.
	if (!FMath::IsFinite(Launch.Spell.Power) || Launch.Spell.Power <= 0.0f ||
		!FMath::IsFinite(Launch.Parameters.Speed) || Launch.Parameters.Speed < 1.0f ||
		!FMath::IsFinite(Launch.Parameters.Radius) || Launch.Parameters.Radius < 1.0f ||
		!FMath::IsFinite(Launch.Parameters.Lifetime) || Launch.Parameters.Lifetime < 0.1f ||
		!FMath::IsFinite(Launch.Parameters.MaxDistance) || Launch.Parameters.MaxDistance < 0)
	{
		return false;
	}
	return true;
}

bool UWEVORASpellCastComponent::GetAimSolution(float Radius, FWEVORAAimSolution& OutAim) const
{
	OutAim = FWEVORAAimSolution();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->GetController() || !GetWorld()) { return false; }
	FVector ViewLocation;
	FRotator ViewRotation;
	Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellAim), false, Pawn);
	// Existing bolts must not become the next shot's camera target or block its muzzle.
	for (TActorIterator<AWEVORASpellDelivery> It(GetWorld()); It; ++It) { Query.AddIgnoredActor(*It); }
	OutAim.Target = ViewLocation + ViewRotation.Vector() * FMath::Max(1.0f, AimDistance);
	FHitResult CameraHit;
	if (GetWorld()->LineTraceSingleByObjectType(CameraHit, ViewLocation, OutAim.Target, Objects, Query))
	{
		OutAim.Target = CameraHit.ImpactPoint;
		const UWEVORAHealthComponent* Health = CameraHit.GetActor()
			? CameraHit.GetActor()->FindComponentByClass<UWEVORAHealthComponent>() : nullptr;
		OutAim.bTargetIsDamageable = Health && Health->IsAlive();
	}
	const FVector Origin = Pawn->GetPawnViewLocation();
	OutAim.Direction = (OutAim.Target - Origin).GetSafeNormal();
	OutAim.Start = Origin + OutAim.Direction * FMath::Max(0.0f, SpawnDistance);
	if (OutAim.Direction.IsNearlyZero() || FVector::DotProduct(OutAim.Direction, ViewRotation.Vector()) <= 0.0f)
	{
		OutAim.bBlocked = OutAim.bMuzzleBlocked = true;
		OutAim.Obstruction = Origin;
		return true;
	}
	FHitResult Hit;
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(FMath::Max(1.0f, Radius));
	if (GetWorld()->SweepSingleByObjectType(Hit, Origin, OutAim.Start, FQuat::Identity, Objects, Sphere, Query))
	{
		OutAim.bBlocked = OutAim.bMuzzleBlocked = true;
		OutAim.Obstruction = Hit.ImpactPoint;
	}
	else if (GetWorld()->SweepSingleByObjectType(Hit, OutAim.Start, OutAim.Target, FQuat::Identity, Objects, Sphere, Query)
		&& (!CameraHit.GetActor() || Hit.GetActor() != CameraHit.GetActor()))
	{
		OutAim.bBlocked = true;
		OutAim.Obstruction = Hit.ImpactPoint;
	}
	// Optional narrow, cast-time Lightning correction. Both sight lines must be unobstructed.
	const AWEVORACombatPlayground* Trial = AWEVORACombatPlayground::Find(GetWorld());
	const UWEVORASpellWeavingComponent* Input = Pawn->FindComponentByClass<UWEVORASpellWeavingComponent>();
	if (Trial && Trial->bLightningAssist && Input && Input->Context.Element == EWEVORASpellElement::Lightning
		&& !OutAim.bMuzzleBlocked)
	{
		float BestDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Trial->LightningAssistDegrees, 0.0f, 3.0f)));
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			const APawn* Candidate = *It;
			const UWEVORAHealthComponent* Health = Candidate->FindComponentByClass<UWEVORAHealthComponent>();
			if (Candidate == Pawn || Candidate->IsHidden() || !Health || !Health->IsAlive()) { continue; }
			const FVector Target = Candidate->GetActorLocation();
			const FVector Offset = Target - ViewLocation;
			if (Offset.Size() > FMath::Min(AimDistance, Trial->LightningAssistRange)) { continue; }
			const float Dot = FVector::DotProduct(Offset.GetSafeNormal(), ViewRotation.Vector());
			if (Dot < BestDot) { continue; }
			FHitResult SightHit;
			if (GetWorld()->LineTraceSingleByObjectType(SightHit, ViewLocation, Target, Objects, Query)
				&& SightHit.GetActor() != Candidate) { continue; }
			const FVector Direction = (Target - Origin).GetSafeNormal();
			const FVector Start = Origin + Direction * FMath::Max(0.0f, SpawnDistance);
			if (GetWorld()->SweepSingleByObjectType(SightHit, Origin, Start, FQuat::Identity, Objects, Sphere, Query)) { continue; }
			if (GetWorld()->SweepSingleByObjectType(SightHit, Start, Target, FQuat::Identity, Objects, Sphere, Query)
				&& SightHit.GetActor() != Candidate) { continue; }
			BestDot = Dot;
			OutAim.Target = Target;
			OutAim.Start = Start;
			OutAim.Direction = Direction;
			OutAim.bBlocked = false;
			OutAim.bTargetIsDamageable = true;
			OutAim.bAssisted = true;
		}
	}
	return true;
}
