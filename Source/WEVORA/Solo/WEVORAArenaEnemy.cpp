#include "Solo/WEVORAArenaEnemy.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AWEVORAArenaEnemy::AWEVORAArenaEnemy()
{
	bShowBurningEffect = false;
	LocalStatuses = CreateDefaultSubobject<UWEVORALocalizedStatusComponent>(TEXT("LocalStatuses"));
	LocalStatuses->bUsePrototypeProfile = true;
	static ConstructorHelpers::FObjectFinder<UWEVORAZoneProfile> Zones(TEXT("/Game/WEVORA/Localized/DA_PrototypeZones.DA_PrototypeZones"));
	static ConstructorHelpers::FObjectFinder<UWEVORALocalReactionProfile> Rules(TEXT("/Game/WEVORA/Localized/DA_FireWindReactions.DA_FireWindReactions"));
	LocalStatuses->Profile = Zones.Object; LocalStatuses->ReactionProfile = Rules.Object;
	static ConstructorHelpers::FClassFinder<AWEVORAEnemy> Authored(TEXT("/Game/WEVORA/AI/BP_WEVORAEnemy"));
	if (Authored.Class)
	{
		const AWEVORAEnemy* Defaults = Authored.Class->GetDefaultObject<AWEVORAEnemy>();
		SpellReactionComponent->ReactionRules = Defaults->SpellReactionComponent->ReactionRules;
		ProjectileClass = Defaults->ProjectileClass;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (const FWEVORAZoneDefinition& Zone : GetDefault<UWEVORAZoneProfile>()->Zones)
	{
		if (Zone.ID == TEXT("Torso")) { ZoneVisuals.Add(Zone.ID,Body); continue; }
		USphereComponent* Collider = CreateDefaultSubobject<USphereComponent>(*FString::Printf(TEXT("Zone_%s"),*Zone.ID.ToString()));
		Collider->SetupAttachment(Collision); Collider->InitSphereRadius(22); Collider->SetRelativeLocation(Zone.Offset);
		Collider->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Collider->SetCollisionObjectType(ECC_Pawn);
		Collider->SetCollisionResponseToAllChannels(ECR_Ignore);
		Collider->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
		Collider->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Collider->SetGenerateOverlapEvents(false);
		ZoneColliders.Add(Collider);
		UStaticMeshComponent* Visual = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ZoneVisual_%s"),*Zone.ID.ToString()));
		Visual->SetupAttachment(Collider); Visual->SetStaticMesh(Sphere.Object); Visual->SetRelativeScale3D(FVector(0.44f));
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetCastShadow(false); ZoneVisuals.Add(Zone.ID,Visual);
	}
	DangerMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DangerMarker"));
	DangerMarker->SetupAttachment(Collision); DangerMarker->SetAbsolute(true,true,true);
	DangerMarker->SetStaticMesh(Cylinder.Object); DangerMarker->SetWorldScale3D(FVector(3,3,0.04f));
	DangerMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision); DangerMarker->SetCastShadow(false); DangerMarker->SetVisibility(false);
	DangerLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DangerLine"));
	DangerLine->SetupAttachment(Collision); DangerLine->SetAbsolute(true,true,true);
	DangerLine->SetStaticMesh(Cube.Object); DangerLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DangerLine->SetCastShadow(false); DangerLine->SetVisibility(false);
}

void AWEVORAArenaEnemy::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WEVORA/Effects/M_AttackEffect.M_AttackEffect"));
	if (Material && GetNetMode() != NM_DedicatedServer)
	{
		for (const auto& Pair : ZoneVisuals)
		{
			UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(Material,this);
			Pair.Value->SetMaterial(0,Tint); Tints.Add(Pair.Key,Tint);
		}
		UMaterialInstanceDynamic* Danger = UMaterialInstanceDynamic::Create(Material,this);
		Danger->SetVectorParameterValue(TEXT("SpellColor"),FLinearColor(3,0.15f,0.01f));
		DangerMarker->SetMaterial(0,Danger); DangerLine->SetMaterial(0,Danger);
	}
	LocalStatuses->OnStatesChanged.AddUniqueDynamic(this,&ThisClass::RefreshZones); RefreshZones();
}

void AWEVORAArenaEnemy::RefreshZones()
{
	for (const auto& Pair : Tints)
	{
		Pair.Value->SetVectorParameterValue(TEXT("SpellColor"),LocalStatuses->HasStatus(Pair.Key,TEXT("Burning"))
			? FLinearColor(2,0.25f,0.01f) : (bBossStyle ? FLinearColor(0.6f,0.08f,1.2f) : FLinearColor(0.03f,0.3f,0.6f)));
	}
}

void AWEVORAArenaEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bTell = bBossStyle && bWindingUp && HealthComponent->IsAlive();
	DangerMarker->SetVisibility(bTell); DangerLine->SetVisibility(bTell);
	if (bTell)
	{
		DangerMarker->SetWorldLocation(TelegraphTarget);
		const FVector Start = Muzzle->GetComponentLocation(); const FVector Offset = TelegraphTarget-Start;
		DangerLine->SetWorldLocation((Start+TelegraphTarget)*0.5f); DangerLine->SetWorldRotation(Offset.Rotation());
		DangerLine->SetWorldScale3D(FVector(Offset.Size()/100,0.05f,0.05f));
	}
}

void AWEVORAArenaEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
	LocalStatuses->OnStatesChanged.RemoveDynamic(this,&ThisClass::RefreshZones);
	Super::EndPlay(Reason);
}
