#include "Spell/WEVORAImpactEffect.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AWEVORAImpactEffect::AWEVORAImpactEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	Fragments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Fragments"));
	SetRootComponent(Fragments);
	Fragments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fragments->SetGenerateOverlapEvents(false);
	Fragments->SetCastShadow(false);
	Fragments->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Mesh.Succeeded()) { Fragments->SetStaticMesh(Mesh.Object); }
}

void AWEVORAImpactEffect::Initialize(const FWEVORAImpactEffectSettings& Settings, UMaterialInterface* Material, FVector Normal)
{
	Config = Settings;
	Config.Lifetime = FMath::Clamp(Config.Lifetime, 0.05f, 5.0f);
	Config.Size = FMath::Clamp(Config.Size, 0.1f, 100.0f);
	Config.Speed = FMath::Clamp(Config.Speed, 0.0f, 3000.0f);
	Config.Gravity = FMath::Clamp(Config.Gravity, 0.0f, 3000.0f);
	if (Config.FragmentMesh) { Fragments->SetStaticMesh(Config.FragmentMesh); }
	if (Material)
	{
		for (int32 Slot = 0; Slot < Fragments->GetNumMaterials(); ++Slot) { Fragments->SetMaterial(Slot, Material); }
	}
	Normal = Normal.GetSafeNormal();
	if (Normal.IsNearlyZero()) { Normal = FVector::UpVector; }
	for (int32 Index = 0; Index < FMath::Clamp(Config.FragmentCount, 0, 64); ++Index)
	{
		Velocities.Add(FMath::VRandCone(Normal, PI * 0.38f) * Config.Speed * FMath::FRandRange(0.5f, 1.3f));
		Spins.Add(FMath::VRand() * FMath::FRandRange(180.0f, 540.0f));
		Scales.Add(FVector(FMath::FRandRange(0.5f, 1.3f), FMath::FRandRange(0.3f, 0.8f), FMath::FRandRange(0.2f, 0.6f)) * Config.Size / 100.0f);
		Fragments->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, Scales.Last()));
	}
	SetLifeSpan(Config.Lifetime);
}

void AWEVORAImpactEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Fade = 1.0f - FMath::Clamp((Age / Config.Lifetime - 0.65f) / 0.35f, 0.0f, 1.0f);
	for (int32 Index = 0; Index < Velocities.Num(); ++Index)
	{
		const FVector Position = Velocities[Index] * Age - FVector(0, 0, 0.5f * Config.Gravity * Age * Age);
		Fragments->UpdateInstanceTransform(Index, FTransform(FRotator::MakeFromEuler(Spins[Index] * Age), Position, Scales[Index] * Fade), false, false);
	}
	Fragments->MarkRenderStateDirty();
}
