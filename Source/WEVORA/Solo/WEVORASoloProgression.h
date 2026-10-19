#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Components/ActorComponent.h"
#include "Spell/WEVORASpellTypes.h"
#include "WEVORASoloProgression.generated.h"

USTRUCT(BlueprintType)
struct FWEVORAArenaTrial
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID = TEXT("LightningTrial");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title = FText::FromString(TEXT("Trial of Lightning"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EWEVORASpellElement Reward = EWEVORASpellElement::Lightning;
};

/** Permanent data only: no temporary card, health, mana or input state is serialized. */
UCLASS()
class WEVORA_API UWEVORASoloSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) TArray<FName> CompletedTrials;
	UPROPERTY(SaveGame) TArray<EWEVORASpellElement> UnlockedElements;
};

UCLASS()
class WEVORA_API UWEVORASoloProgression : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SlotName = TEXT("WEVORA_Solo_Progress_v1");
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) FString Status;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) bool bSavePending = false;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) bool bLastUnlockNew = false;
	UFUNCTION(BlueprintCallable) bool LoadProgress();
	UFUNCTION(BlueprintCallable) bool CompleteTrial(const FWEVORAArenaTrial& Trial);
	UFUNCTION(BlueprintCallable) bool RetrySave();
	TArray<EWEVORASpellElement> AllowedElements() const;
	bool HasCompleted(FName ID) const;
	static bool IsSupported(EWEVORASpellElement Element);
private:
	UPROPERTY(Transient) TObjectPtr<UWEVORASoloSave> Data;
	FWEVORAArenaTrial PendingTrial;
	bool bWritable = true;
	bool bLoaded = false;
};
