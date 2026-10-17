#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/HitResult.h"
#include "WEVORAZoneProfile.generated.h"

/** Stable game IDs are names, not mesh/bone enums. Non-humanoids can add their own IDs. */
USTRUCT(BlueprintType)
struct WEVORA_API FWEVORAZoneDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
	FName ID;
	/** Optional animated anchor; otherwise Offset is in actor space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
	FName AnchorBone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
	FVector Offset = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORAZoneLink
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
	FName From;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
	FName To;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
	bool bBidirectional = true;
};

/** Rig-specific lookup and topology. Physics body names are normalized independently of IDs. */
UCLASS(BlueprintType)
class WEVORA_API UWEVORAZoneProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UWEVORAZoneProfile();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zones")
	TArray<FWEVORAZoneDefinition> Zones;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zones")
	TMap<FName, FName> BoneToZone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zones")
	TMap<FName, FName> BodyToZone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zones")
	TMap<FName, FName> ColliderToZone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zones")
	TArray<FWEVORAZoneLink> Links;
	UFUNCTION(BlueprintPure, Category="Zones")
	bool ContainsZone(FName ID) const;
	/** Unknown or invalid mappings return None. Never infer a different limb by proximity. */
	UFUNCTION(BlueprintPure, Category="Zones")
	FName ResolveNames(FName Bone, FName PhysicsBody, FName Collider) const;
	FName ResolveHit(const FHitResult& Hit, const AActor* ExpectedOwner) const;
	FVector GetZonePosition(FName ID, const AActor* Owner) const;
	TArray<FName> GetNeighbors(FName ID) const;
};
