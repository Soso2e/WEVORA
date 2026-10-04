#include "AI/WEVORAHealthComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

UWEVORAHealthComponent::UWEVORAHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWEVORAHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	Health = MaxHealth;
	GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UWEVORAHealthComponent::ReceiveDamage);
}

void UWEVORAHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetOwner()->OnTakeAnyDamage.RemoveDynamic(this, &UWEVORAHealthComponent::ReceiveDamage);
	Super::EndPlay(EndPlayReason);
}

void UWEVORAHealthComponent::ReceiveDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (!GetOwner()->HasAuthority() || !IsAlive() || Damage <= 0.0f) { return; }
	Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
	if (bShowDamageFeedback && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.0f, FColor::Red,
			FString::Printf(TEXT("HP: %.0f / %.0f%s"), Health, MaxHealth, IsAlive() ? TEXT("") : TEXT("  Defeated (restart PIE)")));
	}
	if (!IsAlive())
	{
		OnDeath.Broadcast();
		if (bDestroyOwnerOnDeath) { GetOwner()->Destroy(); }
	}
}
