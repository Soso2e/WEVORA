#include "Spell/WEVORASpellEffectComponent.h"
#include "Spell/WEVORAImpactEffect.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "AI/WEVORAHealthComponent.h"
#include "EngineUtils.h"

UWEVORASpellEffectComponent::UWEVORASpellEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/WEVORA/Effects/M_AttackEffect.M_AttackEffect"));
	SphereMesh = Sphere.Object;
	SegmentMesh = Cube.Object;
	DefaultMaterial = Material.Object;
	FWEVORAAttackEffectProfile Fire;
	ElementEffects.Add(EWEVORASpellElement::Fire, Fire);
	FWEVORAAttackEffectProfile Wind;
	Wind.Color = FLinearColor(0.1f, 0.8f, 1.0f);
	ElementEffects.Add(EWEVORASpellElement::Wind, Wind);
	FWEVORAAttackEffectProfile Lightning;
	Lightning.Color = FLinearColor(0.65f, 0.3f, 1.0f);
	ElementEffects.Add(EWEVORASpellElement::Lightning, Lightning);
}

void UWEVORASpellEffectComponent::StartEffect(const FWEVORASpellLaunch& Launch)
{
	StopEffect();
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer || !GetOwner()) { return; }
	const FWEVORAAttackEffectProfile* Profile = ElementEffects.Find(Launch.Spell.Element);
	if (!Profile) { return; }
	ActiveProfile = *Profile;
	ActiveElement = Launch.Spell.Element;
	Radius = FMath::Clamp(Launch.Parameters.Radius * ActiveProfile.Scale, 1.0f, 500.0f);
	TrailLength = FMath::Clamp(Launch.Parameters.Speed * 0.07f, Radius * 5, 1200.0f);
	ImpactPower = FMath::Clamp(Launch.Spell.Power, 0.25f, 3.0f);
	ImpactColor = Launch.Parameters.Color;
	if (bUseLaunchColor) { ActiveProfile.Color = Launch.Parameters.Color; }
	UMaterialInterface* Material = ActiveProfile.Material ? ActiveProfile.Material.Get() : DefaultMaterial.Get();
	if (Material)
	{
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, this);
		Dynamic->SetVectorParameterValue(TEXT("SpellColor"), ActiveProfile.Color);
		Material = Dynamic;
	}
	if (ActiveProfile.System)
	{
		UNiagaraComponent* Niagara = UNiagaraFunctionLibrary::SpawnSystemAttached(ActiveProfile.System, this,
			NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, false);
		if (Niagara)
		{
			NiagaraEffect = Niagara;
			Niagara->SetVariableLinearColor(TEXT("User.SpellColor"), ActiveProfile.Color);
			Niagara->SetVariableFloat(TEXT("User.SpellRadius"), Radius);
			if (Material) { Niagara->SetVariableMaterial(TEXT("User.SpellMaterial"), Material); }
			Niagara->Activate(true);
		}
		return;
	}
	Pieces = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Pieces->SetupAttachment(this);
	Pieces->SetStaticMesh(ActiveElement == EWEVORASpellElement::Fire ? SphereMesh : SegmentMesh);
	Pieces->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pieces->SetGenerateOverlapEvents(false);
	Pieces->SetCastShadow(false);
	Pieces->SetCanEverAffectNavigation(false);
	if (Material) { Pieces->SetMaterial(0, Material); }
	Pieces->RegisterComponent();
	const int32 Count = ActiveElement == EWEVORASpellElement::Fire ? 18 : 48;
	for (int32 Index = 0; Index <= Count; ++Index) { Pieces->AddInstance(FTransform::Identity); }
	SetComponentTickEnabled(true);
	TickComponent(0, LEVELTICK_All, nullptr);
}

void UWEVORASpellEffectComponent::StopEffect()
{
	SetComponentTickEnabled(false);
	if (Pieces) { Pieces->DestroyComponent(); Pieces = nullptr; }
	if (NiagaraEffect) { NiagaraEffect->DestroyComponent(); NiagaraEffect = nullptr; }
	Age = 0;
}

void UWEVORASpellEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	if (!Pieces) { return; }
	Age += DeltaTime * FMath::Max(0.0f, ActiveProfile.AnimationSpeed);
	for (int32 Index = 0; Index < Pieces->GetInstanceCount(); ++Index)
	{
		FTransform Transform;
		if (Index == Pieces->GetInstanceCount() - 1)
		{
			// A stable visible nucleus independent of animated trailing pieces.
			const float Core = FMath::Max(22.0f, Radius) / 50;
			Transform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(Core));
		}
		else if (ActiveElement == EWEVORASpellElement::Fire)
		{
			const float Phase = FMath::Frac(Age * 2.0f + Index / 18.0f);
			const float Angle = Index * 2.4f + Age * 4;
			const FVector Position(-Phase * TrailLength, FMath::Cos(Angle) * Radius * Phase, FMath::Sin(Angle) * Radius * Phase + Phase * Radius);
			const float Size = Radius / 50.0f * (1 - Phase) * 0.7f;
			Transform = FTransform(FQuat::Identity, Position, FVector(Size * 1.8f, Size, Size));
		}
		else
		{
			const int32 Strand = Index / 16;
			const int32 Segment = Index % 16;
			auto Point = [&](int32 Step)
			{
				const float Along = Step / 16.0f;
				const float Angle = Along * PI * 4 + Age * 9 + Strand * PI * 2 / 3;
				if (ActiveElement == EWEVORASpellElement::Wind)
				{
					const float Width = Radius * (0.6f + Along * 0.8f);
					return FVector(-Along * TrailLength, FMath::Cos(Angle) * Width, FMath::Sin(Angle) * Width);
				}
				// Discrete flicker keeps the lightning angular rather than a smooth wave.
				FRandomStream Random(Step * 7919 + Strand * 104729 + FMath::FloorToInt(Age * 18) * 101);
				return FVector(-Along * TrailLength, Random.FRandRange(-Radius, Radius), Random.FRandRange(-Radius, Radius));
			};
			const FVector Start = Point(Segment), End = Point(Segment + 1);
			const FVector Delta = End - Start;
			const float Thickness = Radius * (ActiveElement == EWEVORASpellElement::Wind ? 0.08f : 0.055f);
			Transform = FTransform(Delta.Rotation(), (Start + End) * 0.5f, FVector(Delta.Size() / 100, Thickness / 100, Thickness / 100));
		}
		Pieces->UpdateInstanceTransform(Index, Transform, false, false);
	}
	Pieces->MarkRenderStateDirty();
}

UMaterialInterface* UWEVORASpellEffectComponent::ResolveHitMaterial(const FHitResult& Hit)
{
	UMeshComponent* Mesh = Cast<UMeshComponent>(Hit.GetComponent());
	if (!Mesh && IsValid(Hit.GetActor()))
	{
		TArray<UMeshComponent*> Meshes;
		Hit.GetActor()->GetComponents(Meshes);
		float BestDistance = TNumericLimits<float>::Max();
		for (UMeshComponent* Candidate : Meshes)
		{
			if (!Candidate->IsVisible() || Candidate->GetNumMaterials() == 0) { continue; }
			const float Distance = Candidate->Bounds.GetBox().ComputeSquaredDistanceToPoint(Hit.ImpactPoint);
			if (Distance < BestDistance) { Mesh = Candidate; BestDistance = Distance; }
		}
	}
	if (!Mesh) { return nullptr; }
	int32 Slot = 0;
	if (Mesh == Hit.GetComponent() && Hit.FaceIndex != INDEX_NONE)
	{
		if (UMaterialInterface* Material = Mesh->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Slot)) { return Material; }
	}
	FHitResult Surface;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellImpactMaterial), true);
	Query.bReturnFaceIndex = true;
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	if (Mesh->LineTraceComponent(Surface, Hit.ImpactPoint + Normal * 30, Hit.ImpactPoint - Normal * 100, Query)
		&& Surface.FaceIndex != INDEX_NONE)
	{
		if (UMaterialInterface* Material = Mesh->GetMaterialFromCollisionFaceIndex(Surface.FaceIndex, Slot)) { return Material; }
	}
	return Mesh->GetMaterial(0);
}

void UWEVORASpellEffectComponent::SpawnImpact(const FHitResult& Hit)
{
	if (!Impact.bEnabled || Impact.FragmentCount <= 0 || !GetWorld() || GetNetMode() == NM_DedicatedServer) { return; }
	int32 Active = 0;
	for (TActorIterator<AWEVORAImpactEffect> It(GetWorld()); It; ++It) { ++Active; }
	if (Active >= 32) { return; }
	UMaterialInterface* Material = ResolveHitMaterial(Hit);
	if (!Material) { Material = Impact.FallbackMaterial; }
	AWEVORAImpactEffect* Effect = GetWorld()->SpawnActor<AWEVORAImpactEffect>(
		Hit.ImpactPoint + Hit.ImpactNormal * 2, FRotator::ZeroRotator);
	if (Effect)
	{
		Effect->Initialize(Impact, Material, Hit.ImpactNormal);
		const bool bTarget = Hit.GetActor() && Hit.GetActor()->FindComponentByClass<UWEVORAHealthComponent>();
		Effect->InitializeFlash(DefaultMaterial, bTarget ? ImpactColor : FLinearColor(0.5f,0.55f,0.6f),
			(bTarget ? 70.0f : 35.0f) * FMath::Sqrt(ImpactPower));
	}
}
