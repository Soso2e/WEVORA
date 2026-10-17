#include "Playground/WEVORALocalizedPlaygroundTarget.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AWEVORALocalizedPlaygroundTarget::AWEVORALocalizedPlaygroundTarget()
{
	TargetName = TEXT("06 Localized: Fire hand -> Wind");
	bShowBurningEffect = false; // Whole-body flame presentation would misrepresent a local state.
	LocalStatuses = CreateDefaultSubobject<UWEVORALocalizedStatusComponent>(TEXT("LocalStatuses"));
	LocalStatuses->bUsePrototypeProfile = true;
	static ConstructorHelpers::FObjectFinder<UWEVORAZoneProfile> Zones(TEXT("/Game/WEVORA/Localized/DA_PrototypeZones.DA_PrototypeZones"));
	static ConstructorHelpers::FObjectFinder<UWEVORALocalReactionProfile> Reactions(TEXT("/Game/WEVORA/Localized/DA_FireWindReactions.DA_FireWindReactions"));
	LocalStatuses->Profile = Zones.Object;
	LocalStatuses->ReactionProfile = Reactions.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/WEVORA/Effects/M_AttackEffect.M_AttackEffect"));
	ZoneMaterial = Glow.Object;
	for (const FWEVORAZoneDefinition& Zone : GetDefault<UWEVORAZoneProfile>()->Zones)
	{
		if (Zone.ID == TEXT("Torso")) { ZoneVisuals.Add(Zone.ID, Body); continue; }
		USphereComponent* Collider = CreateDefaultSubobject<USphereComponent>(*FString::Printf(TEXT("Zone_%s"), *Zone.ID.ToString()));
		Collider->SetupAttachment(Collision);
		Collider->InitSphereRadius(22);
		Collider->SetRelativeLocation(Zone.Offset);
		Collider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collider->SetCollisionObjectType(ECC_Pawn);
		Collider->SetCollisionResponseToAllChannels(ECR_Ignore);
		Collider->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Collider->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Collider->SetGenerateOverlapEvents(false);
		ZoneColliders.Add(Collider);
		UStaticMeshComponent* Visual = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ZoneVisual_%s"), *Zone.ID.ToString()));
		Visual->SetupAttachment(Collider); Visual->SetStaticMesh(Sphere.Object);
		Visual->SetRelativeScale3D(FVector(0.44f));
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetGenerateOverlapEvents(false); Visual->SetCastShadow(false);
		ZoneVisuals.Add(Zone.ID, Visual);
	}
}

void AWEVORALocalizedPlaygroundTarget::BeginPlay()
{
	Super::BeginPlay();
	if (ZoneMaterial && GetNetMode() != NM_DedicatedServer)
	{
		for (const auto& Pair : ZoneVisuals)
		{
			UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(ZoneMaterial, this);
			Pair.Value->SetMaterial(0, Tint); ZoneTints.Add(Pair.Key, Tint);
		}
	}
	LocalStatuses->OnStatesChanged.AddUniqueDynamic(this, &ThisClass::RefreshZoneVisuals);
	RefreshZoneVisuals();
}

void AWEVORALocalizedPlaygroundTarget::RefreshZoneVisuals()
{
	for (const auto& Pair : ZoneTints)
	{
		Pair.Value->SetVectorParameterValue(TEXT("SpellColor"), LocalStatuses->HasStatus(Pair.Key, TEXT("Burning"))
			? FLinearColor(1.8f,0.22f,0.015f) : FLinearColor(0.03f,0.32f,0.55f));
	}
}

void AWEVORALocalizedPlaygroundTarget::EndPlay(const EEndPlayReason::Type Reason)
{
	LocalStatuses->OnStatesChanged.RemoveDynamic(this, &ThisClass::RefreshZoneVisuals);
	Super::EndPlay(Reason);
}
