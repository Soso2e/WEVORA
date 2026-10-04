#include "WEVORASpellSelectionEffectComponent.h"
#include "WEVORASpellWeavingComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"

UWEVORASpellSelectionEffectComponent::UWEVORASpellSelectionEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	FWEVORASpellSelectionEffect Fire;
	Fire.Color = FLinearColor(1.0f, 0.15f, 0.02f);
	ElementEffects.Add(EWEVORASpellElement::Fire, Fire);
	FWEVORASpellSelectionEffect Wind;
	Wind.Color = FLinearColor(0.1f, 0.8f, 1.0f);
	ElementEffects.Add(EWEVORASpellElement::Wind, Wind);
}

void UWEVORASpellSelectionEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer || !GetOwner()) { return; }
	Weaving = GetOwner()->FindComponentByClass<UWEVORASpellWeavingComponent>();
	if (!Weaving) { return; }
	Niagara = NewObject<UNiagaraComponent>(GetOwner());
	Niagara->SetAutoActivate(false);
	Niagara->SetupAttachment(this);
	Niagara->RegisterComponent();
	PreviewLight = NewObject<UPointLightComponent>(GetOwner());
	PreviewLight->SetupAttachment(this);
	PreviewLight->SetCastShadows(false);
	PreviewLight->SetVisibility(false);
	PreviewLight->RegisterComponent();
	LastElement = Weaving->Context.Element;
	Weaving->OnSpellPresentationChanged.AddUniqueDynamic(this, &ThisClass::HandlePresentationChanged);
	RefreshEffect();
}

void UWEVORASpellSelectionEffectComponent::HandlePresentationChanged(EWEVORAWeavingState State, const FWEVORASpellContext& Context)
{
	const bool bElementChanged = LastElement != Context.Element;
	LastElement = Context.Element;
	GetWorld()->GetTimerManager().ClearTimer(IdlePreviewTimer);
	if (State == EWEVORAWeavingState::Idle)
	{
		if (!bElementChanged || IdlePreviewDuration <= 0.0f)
		{
			StopPreview();
			return;
		}
		GetWorld()->GetTimerManager().SetTimer(IdlePreviewTimer, this, &ThisClass::StopPreview, IdlePreviewDuration, false);
	}
	ApplyEffect(State, Context);
}

void UWEVORASpellSelectionEffectComponent::RefreshEffect()
{
	if (!Weaving) { return; }
	if (Weaving->State != EWEVORAWeavingState::Idle || bPreviewActive)
	{
		ApplyEffect(Weaving->State, Weaving->Context);
	}
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
	if (Niagara) { Niagara->DeactivateImmediate(); }
	if (PreviewLight) { PreviewLight->SetVisibility(false); }
}

void UWEVORASpellSelectionEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(IdlePreviewTimer); }
	if (Weaving) { Weaving->OnSpellPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged); }
	StopPreview();
	if (Niagara) { Niagara->DestroyComponent(); Niagara = nullptr; }
	if (PreviewLight) { PreviewLight->DestroyComponent(); PreviewLight = nullptr; }
	Weaving = nullptr;
	Super::EndPlay(EndPlayReason);
}
