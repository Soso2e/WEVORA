#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORALocalReactionProfile.generated.h"

/** All positions are actor-local so a Wind displacement cannot move the impact away from its limb. */
struct FWEVORALocalReactionInput
{
	FName Zone;
	EWEVORASpellElement Element = EWEVORASpellElement::Wind;
	float Power = 1;
	FVector LocalImpact = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORALocalReactionRule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	EWEVORASpellElement Trigger = EWEVORASpellElement::Wind;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	FName RequiredStatus = TEXT("Burning");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	FName OutputStatus = TEXT("Burning");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	EWEVORASpellElement OutputElement = EWEVORASpellElement::Fire;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0"))
	float MinimumIntensity = 0.1f;
	/** Trigger must hit seed zone or one of its graph neighbors AND be within this radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0", Units="cm"))
	float SeedRadius = 110;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0", Units="cm"))
	float EffectRadius = 240;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0", Units="cm"))
	float MaxEdgeDistance = 110;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0", Units="cm"))
	float MaxPathDistance = 220;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0", ClampMax="16"))
	int32 MaxHops = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0"))
	float IntensityMultiplier = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0", ClampMax="1"))
	float AttenuationPerHop = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction", meta=(ClampMin="0"))
	float DurationMultiplier = 1.3f;
};

/** Add trigger/status/output rules without adding element-specific code paths. */
UCLASS(BlueprintType)
class WEVORA_API UWEVORALocalReactionProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UWEVORALocalReactionProfile() { Rules.Add(FWEVORALocalReactionRule()); }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	TArray<FWEVORALocalReactionRule> Rules;
};
