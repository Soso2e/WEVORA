#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Spell/WEVORASpellTypes.h"
#include "Spell/WEVORAZoneProfile.h"
#include "Spell/WEVORALocalReactionProfile.h"
#include "WEVORALocalizedStatusComponent.generated.h"

class AController;

UENUM(BlueprintType)
enum class EWEVORAStatusPolicy : uint8 { Replace, Refresh, Accumulate, Extend };

/** One status per (Zone, Type). Intensity is dimensionless; DPS is explicit. */
USTRUCT(BlueprintType)
struct WEVORA_API FWEVORALocalStatus
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	FName Zone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	FName Type = TEXT("Burning");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	EWEVORASpellElement Element = EWEVORASpellElement::Fire;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	float Intensity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	float Accumulation = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	float Remaining = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	float DamagePerSecond = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	EWEVORAStatusPolicy Policy = EWEVORAStatusPolicy::Refresh;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Status")
	double AppliedAt = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	TWeakObjectPtr<AActor> Source;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status")
	TWeakObjectPtr<AController> Instigator;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWEVORALocalStatesChanged);

/** Reusable authoritative state store. No enemy, rig, UI or timer dependency. */
UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORALocalizedStatusComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWEVORALocalizedStatusComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localized")
	TObjectPtr<UWEVORAZoneProfile> Profile;
	/** Opt-in prototype profile. Leave off when supplying a production rig profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localized")
	bool bUsePrototypeProfile = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localized")
	TObjectPtr<UWEVORALocalReactionProfile> ReactionProfile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localized", meta=(ClampMin="0"))
	float MaxIntensity = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localized", meta=(ClampMin="0"))
	float MaxAccumulation = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localized", meta=(ClampMin="0"))
	float MaxDuration = 30;
	/** Shared maximum DPS across all zones/statuses, never a sum of limb damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localized", meta=(ClampMin="0"))
	float MaxDamagePerSecond = 25;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Localized")
	TArray<FWEVORALocalStatus> States;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Localized")
	FName LastHitZone;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Localized")
	float LastPeriodicDamage = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Localized")
	TArray<FString> LastPropagationPaths;
	UPROPERTY(BlueprintAssignable, Category="Localized")
	FWEVORALocalStatesChanged OnStatesChanged;
	const UWEVORAZoneProfile* GetProfile() const;
	bool IsLocalized() const { return GetProfile() != nullptr; }
	UFUNCTION(BlueprintPure, Category="Localized")
	bool HasStatus(FName Zone, FName Type) const;
	UFUNCTION(BlueprintPure, Category="Localized")
	float GetRemaining(FName Type) const;
	UFUNCTION(BlueprintCallable, Category="Localized")
	bool ApplyStatus(const FWEVORALocalStatus& Application);
	/** Observers receive one notification after the complete batch is committed. */
	UFUNCTION(BlueprintCallable, Category="Localized")
	bool ApplyStatusBatch(const TArray<FWEVORALocalStatus>& Applications);
	UFUNCTION(BlueprintCallable, Category="Localized")
	void RemoveStatus(FName Zone, FName Type);
	UFUNCTION(BlueprintCallable, Category="Localized")
	void ResetStatuses();
	/** Deterministic simulation seam. Runtime calls once per tick; tests may disable tick and step explicitly. */
	UFUNCTION(BlueprintCallable, Category="Localized")
	void AdvanceStatuses(float Seconds);
	/** Direct statuses are immediately visible; reactions commit together after all frame hits. */
	bool QueueReaction(const FWEVORASpellData& Spell, const FHitResult& Hit);
	UFUNCTION(BlueprintCallable, Category="Localized")
	void CommitReactions();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool CanMutate() const;
private:
	TArray<FWEVORALocalReactionInput> PendingInputs;
	bool bApplyingBatch = false;
};
