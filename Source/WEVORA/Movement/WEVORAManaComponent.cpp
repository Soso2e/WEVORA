#include "Movement/WEVORAManaComponent.h"

UWEVORAManaComponent::UWEVORAManaComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWEVORAManaComponent::BeginPlay()
{
	Super::BeginPlay();
	MaxMana = FMath::Max(1.0f, MaxMana);
	SetMana(MaxMana);
}

void UWEVORAManaComponent::SetMana(float Value)
{
	const float NewMana = FMath::Clamp(Value, 0.0f, FMath::Max(1.0f, MaxMana));
	if (Mana != NewMana)
	{
		Mana = NewMana;
		OnManaChanged.Broadcast(Mana, MaxMana);
	}
}

bool UWEVORAManaComponent::ConsumeMana(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount < 0.0f) { return false; }
	if (Amount == 0.0f) { return true; }
	const bool bEnough = Mana >= Amount;
	TimeSinceConsumption = 0.0f;
	SetMana(Mana - Amount);
	return bEnough;
}

void UWEVORAManaComponent::RestoreMana(float Amount)
{
	if (FMath::IsFinite(Amount) && Amount > 0.0f) { SetMana(Mana + Amount); }
}

float UWEVORAManaComponent::GetManaRatio() const
{
	return FMath::Clamp(Mana / FMath::Max(1.0f, MaxMana), 0.0f, 1.0f);
}

void UWEVORAManaComponent::UpdateRecovery(float DeltaSeconds, bool bGrounded)
{
	if (DeltaSeconds <= 0.0f) { return; }
	SetMana(Mana);
	const float PreviousTime = TimeSinceConsumption;
	TimeSinceConsumption += DeltaSeconds;
	if (bGrounded)
	{
		const float RecoveryTime = FMath::Max(0.0f, TimeSinceConsumption - FMath::Max(PreviousTime, RecoveryDelay));
		RestoreMana(FMath::Max(0.0f, GroundRecoveryPerSecond) * RecoveryTime);
	}
}
