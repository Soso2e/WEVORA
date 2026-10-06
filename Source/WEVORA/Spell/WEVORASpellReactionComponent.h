#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASpellReactionComponent.generated.h"

UENUM(BlueprintType)
enum class EWEVORASpellTargetState : uint8 { None, Burning };
UENUM(BlueprintType)
enum class EWEVORASpellReaction : uint8 { Damage, Burning, Knockback, Lift };

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellReactionEffect
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	EWEVORASpellReaction Reaction = EWEVORASpellReaction::Damage;
	/** HP for Damage, HP/s for Burning, cm/s velocity change for Knockback/Lift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0"))
	float Magnitude = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0"))
	float Duration = 3.0f;
};

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellReactionRule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	EWEVORASpellElement Element = EWEVORASpellElement::Fire;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	bool bMatchShape = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(EditCondition="bMatchShape"))
	EWEVORASpellShape Shape = EWEVORASpellShape::Forward;
	/** None means unconditional. All rules inspect the state BEFORE this hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	EWEVORASpellTargetState RequiredState = EWEVORASpellTargetState::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	TArray<FWEVORASpellReactionEffect> Effects;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWEVORASpellReceived, const FWEVORASpellData&, Spell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWEVORASpellStateChanged, EWEVORASpellTargetState, State, bool, bActive);

/** Opt-in receiver for enemies and world actors; no dependency on an enemy class. */
UCLASS(ClassGroup=(WEVORA), meta=(BlueprintSpawnableComponent))
class WEVORA_API UWEVORASpellReactionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWEVORASpellReactionComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Reaction")
	TArray<FWEVORASpellReactionRule> ReactionRules;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Reaction")
	bool bAffectPlayers = false;
	/** Swept motion fallback for non-physics pawns/actors. AI must yield while displaced. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Reaction", meta=(ClampMin="1"))
	float DisplacementDeceleration = 1800.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Spell|State")
	float BurningRemaining = 0.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Spell|State")
	FVector ExternalVelocity = FVector::ZeroVector;
	UPROPERTY(BlueprintAssignable, Category="Spell|Reaction")
	FWEVORASpellReceived OnSpellReceived;
	UPROPERTY(BlueprintAssignable, Category="Spell|State")
	FWEVORASpellStateChanged OnStateChanged;
	UFUNCTION(BlueprintCallable, Category="Spell|Reaction")
	bool ReceiveSpell(const FWEVORASpellData& Spell, const FHitResult& Hit, AController* Instigator, AActor* Causer);
	UFUNCTION(BlueprintPure, Category="Spell|State")
	bool HasState(EWEVORASpellTargetState State) const;
	UFUNCTION(BlueprintPure, Category="Spell|State")
	bool IsDisplaced() const { return !ExternalVelocity.IsNearlyZero(); }
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	float BurningDamagePerSecond = 0.0f;
	float BurningAccumulator = 0.0f;
	TWeakObjectPtr<AController> BurningInstigator;
	TWeakObjectPtr<AActor> BurningSource;
	bool ApplyDisplacement(const FVector& Velocity, const FHitResult& Hit);
};
