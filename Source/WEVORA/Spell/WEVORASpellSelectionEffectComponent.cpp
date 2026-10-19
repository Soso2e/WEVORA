#include "WEVORASpellSelectionEffectComponent.h"
#include "WEVORASpellWeavingComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "WEVORA.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UWEVORASpellSelectionEffectComponent::UWEVORASpellSelectionEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/WEVORA/Effects/M_AttackEffect.M_AttackEffect"));
	CoreMesh = Sphere.Object;
	CoreMaterial = Glow.Object;
	FWEVORASpellSelectionEffect Fire;
	Fire.Color = FLinearColor(1.0f, 0.15f, 0.02f);
	ElementEffects.Add(EWEVORASpellElement::Fire, Fire);
	FWEVORASpellSelectionEffect Wind;
	Wind.Color = FLinearColor(0.1f, 0.8f, 1.0f);
	ElementEffects.Add(EWEVORASpellElement::Wind, Wind);
	FWEVORASpellSelectionEffect Lightning;
	Lightning.Color = FLinearColor(0.65f, 0.3f, 1.0f);
	ElementEffects.Add(EWEVORASpellElement::Lightning, Lightning);
}

void UWEVORASpellSelectionEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer || !GetOwner()) { return; }
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (Mesh && !HandSocketName.IsNone() && Mesh->DoesSocketExist(HandSocketName))
		{
			AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, HandSocketName);
			SetRelativeTransform(HandOffset);
		}
		else
		{
			UE_LOG(LogWEVORA, Warning, TEXT("Spell selection effect: hand socket '%s' missing on %s; retaining current attachment."), *HandSocketName.ToString(), *GetOwner()->GetName());
		}
	}
	Weaving = GetOwner()->FindComponentByClass<UWEVORASpellWeavingComponent>();
	if (!Weaving) { return; }
	ChargeCore = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("ChargeCore"));
	ChargeCore->SetupAttachment(this);
	ChargeCore->SetStaticMesh(CoreMesh);
	ChargeCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargeCore->SetGenerateOverlapEvents(false);
	ChargeCore->SetCastShadow(false);
	ChargeCore->SetCanEverAffectNavigation(false);
	ChargeCore->SetVisibility(false);
	ChargeCore->RegisterComponent();
	if (CoreMaterial)
	{
		CoreDynamic = UMaterialInstanceDynamic::Create(CoreMaterial, this);
		ChargeCore->SetMaterial(0, CoreDynamic);
	}
	Niagara = NewObject<UNiagaraComponent>(GetOwner());
	Niagara->SetAutoActivate(false);
	Niagara->SetupAttachment(this);
	Niagara->RegisterComponent();
	PreviewLight = NewObject<UPointLightComponent>(GetOwner());
	PreviewLight->SetupAttachment(this);
	PreviewLight->SetCastShadows(false);
	PreviewLight->SetVisibility(false);
	PreviewLight->RegisterComponent();
	USceneComponent* BodyParent = this;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner())) { BodyParent = Character->GetMesh(); }
	BodyNiagara = NewObject<UNiagaraComponent>(GetOwner(), TEXT("SpellBodySelectionNiagara"));
	BodyNiagara->SetAutoActivate(false);
	BodyNiagara->SetupAttachment(BodyParent);
	BodyNiagara->SetRelativeTransform(BodyOffset);
	BodyNiagara->RegisterComponent();
	BodyLight = NewObject<UPointLightComponent>(GetOwner(), TEXT("SpellBodySelectionLight"));
	BodyLight->SetupAttachment(BodyParent);
	BodyLight->SetRelativeTransform(BodyOffset);
	BodyLight->SetCastShadows(false);
	BodyLight->SetVisibility(false);
	BodyLight->RegisterComponent();
	LastElement = Weaving->Context.Element;
	Weaving->OnSpellPresentationChanged.AddUniqueDynamic(this, &ThisClass::HandlePresentationChanged);
	RefreshEffect();
}

void UWEVORASpellSelectionEffectComponent::HandlePresentationChanged(EWEVORAWeavingState State, const FWEVORASpellContext& Context)
{
	const bool bElementChanged = LastElement != Context.Element;
	LastElement = Context.Element;
	if (bElementChanged) { ShowBodyPreview(Context); }
	if (State == EWEVORAWeavingState::Idle)
	{
		StopPreview();
		if (!bElementChanged) { StopBodyPreview(); }
		return;
	}
	ApplyEffect(State, Context);
}

void UWEVORASpellSelectionEffectComponent::RefreshEffect()
{
	if (!Weaving) { return; }
	if (Weaving->State != EWEVORAWeavingState::Idle)
	{
		ApplyEffect(Weaving->State, Weaving->Context);
	}
}

void UWEVORASpellSelectionEffectComponent::ShowBodyPreview(const FWEVORASpellContext& Context)
{
	StopBodyPreview();
	const FWEVORASpellSelectionEffect* Profile = ElementEffects.Find(Context.Element);
	if (!Profile || IdlePreviewDuration <= 0.0f || !BodyNiagara || !BodyLight) { return; }
	bBodyPreviewActive = true;
	BodyNiagara->SetRelativeTransform(BodyOffset);
	BodyLight->SetRelativeTransform(BodyOffset);
	BodyLight->SetLightColor(Profile->Color);
	BodyLight->SetIntensity(FMath::Max(0.0f, PreviewLightIntensity));
	BodyLight->SetAttenuationRadius(FMath::Max(0.0f, PreviewLightRadius));
	BodyLight->SetVisibility(bEnablePreviewLight);
	BodyNiagara->SetAsset(Profile->SelectionSystem ? Profile->SelectionSystem : Profile->System);
	if (BodyNiagara->GetAsset())
	{
		BodyNiagara->SetVariableLinearColor(TEXT("User.SpellColor"), Profile->Color);
		BodyNiagara->SetVariableInt(TEXT("User.SpellElement"), static_cast<int32>(Context.Element));
		BodyNiagara->SetVariableInt(TEXT("User.WeavingState"), static_cast<int32>(Weaving->State));
		BodyNiagara->SetVariableInt(TEXT("User.SpellGesture"), static_cast<int32>(Context.Gesture));
		BodyNiagara->Activate(true);
	}
	GetWorld()->GetTimerManager().SetTimer(IdlePreviewTimer, this, &ThisClass::StopBodyPreview, IdlePreviewDuration, false);
}

void UWEVORASpellSelectionEffectComponent::StopBodyPreview()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(IdlePreviewTimer); }
	bBodyPreviewActive = false;
	if (BodyNiagara) { BodyNiagara->DeactivateImmediate(); }
	if (BodyLight) { BodyLight->SetVisibility(false); }
}

void UWEVORASpellSelectionEffectComponent::ApplyEffect(EWEVORAWeavingState State, const FWEVORASpellContext& Context)
{
	if (!Niagara || !PreviewLight) { return; }
	const FWEVORASpellSelectionEffect* Profile = ElementEffects.Find(Context.Element);
	if (!Profile)
	{
		StopPreview();
		return;
	}
	bPreviewActive = true;
	SetComponentTickEnabled(true);
	if (CoreDynamic) { CoreDynamic->SetVectorParameterValue(TEXT("SpellColor"), Profile->Color); }
	if (ChargeCore) { ChargeCore->SetVisibility(!Profile->System); }
	PreviewLight->SetLightColor(Profile->Color);
	PreviewLight->SetIntensity(FMath::Max(0.0f, PreviewLightIntensity));
	PreviewLight->SetAttenuationRadius(FMath::Max(0.0f, PreviewLightRadius));
	PreviewLight->SetVisibility(bEnablePreviewLight);
	if (Niagara->GetAsset() != Profile->System)
	{
		Niagara->DeactivateImmediate();
		Niagara->SetAsset(Profile->System);
	}
	if (!Profile->System) { return; }
	Niagara->SetVariableLinearColor(TEXT("User.SpellColor"), Profile->Color);
	Niagara->SetVariableInt(TEXT("User.SpellElement"), static_cast<int32>(Context.Element));
	Niagara->SetVariableInt(TEXT("User.WeavingState"), static_cast<int32>(State));
	Niagara->SetVariableInt(TEXT("User.SpellGesture"), static_cast<int32>(Context.Gesture));
	if (!Niagara->IsActive()) { Niagara->Activate(true); }
}

void UWEVORASpellSelectionEffectComponent::StopPreview()
{
	bPreviewActive = false;
	SetComponentTickEnabled(false);
	if (ChargeCore) { ChargeCore->SetVisibility(false); }
	if (Niagara) { Niagara->DeactivateImmediate(); }
	if (PreviewLight) { PreviewLight->SetVisibility(false); }
}

void UWEVORASpellSelectionEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	if (!bPreviewActive || !Weaving) { return; }
	const float Charge = Weaving->GetChargeAlpha();
	const float Time = GetWorld()->GetTimeSeconds();
	const float Pulse = 1 + FMath::Sin(Time * (LastElement == EWEVORASpellElement::Lightning ? 28 : 9)) * 0.07f;
	if (ChargeCore)
	{
		const float Scale = FMath::Lerp(0.075f, 0.28f, Charge) * Pulse;
		ChargeCore->SetRelativeScale3D(LastElement == EWEVORASpellElement::Wind ? FVector(Scale, Scale, Scale * 0.65f) : FVector(Scale));
		ChargeCore->SetRelativeRotation(FRotator(0, Time * 150, Time * 90));
	}
	if (PreviewLight)
	{
		PreviewLight->SetIntensity(FMath::Max(0.0f, PreviewLightIntensity) * FMath::Lerp(0.25f, 1.0f, Charge) * Pulse);
		PreviewLight->SetAttenuationRadius(PreviewLightRadius * FMath::Lerp(0.55f, 1.15f, Charge));
	}
	if (Niagara && Niagara->GetAsset()) { Niagara->SetVariableFloat(TEXT("User.ChargeAlpha"), Charge); }
}

void UWEVORASpellSelectionEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(IdlePreviewTimer); }
	if (Weaving) { Weaving->OnSpellPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged); }
	StopPreview();
	StopBodyPreview();
	if (Niagara) { Niagara->DestroyComponent(); Niagara = nullptr; }
	if (PreviewLight) { PreviewLight->DestroyComponent(); PreviewLight = nullptr; }
	if (BodyNiagara) { BodyNiagara->DestroyComponent(); BodyNiagara = nullptr; }
	if (BodyLight) { BodyLight->DestroyComponent(); BodyLight = nullptr; }
	if (ChargeCore) { ChargeCore->DestroyComponent(); ChargeCore = nullptr; }
	CoreDynamic = nullptr;
	Weaving = nullptr;
	Super::EndPlay(EndPlayReason);
}
