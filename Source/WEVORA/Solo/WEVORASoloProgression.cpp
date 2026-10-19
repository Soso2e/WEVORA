#include "Solo/WEVORASoloProgression.h"
#include "Kismet/GameplayStatics.h"

bool UWEVORASoloProgression::IsSupported(EWEVORASpellElement Element)
{
	return Element == EWEVORASpellElement::Fire || Element == EWEVORASpellElement::Wind || Element == EWEVORASpellElement::Lightning;
}

bool UWEVORASoloProgression::LoadProgress()
{
	bLoaded = true; bSavePending = bLastUnlockNew = false; bWritable = true;
	Data = NewObject<UWEVORASoloSave>(this);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName,0)) { Status = TEXT("New save: Fire and Wind available"); return true; }
	UWEVORASoloSave* Loaded = Cast<UWEVORASoloSave>(UGameplayStatics::LoadGameFromSlot(SlotName,0));
	if (!Loaded || Loaded->Version != 1)
	{
		bWritable = false;
		Status = TEXT("Save could not be loaded or has a different version. Original file preserved; progress cannot be saved.");
		return false;
	}
	for (EWEVORASpellElement Element : Loaded->UnlockedElements)
	{
		if (!IsSupported(Element))
		{
			bWritable = false; Status = TEXT("Save contains unsupported elements. Original file preserved."); return false;
		}
	}
	Data = Loaded; Status = TEXT("Progress loaded"); return true;
}

TArray<EWEVORASpellElement> UWEVORASoloProgression::AllowedElements() const
{
	TArray<EWEVORASpellElement> Result = { EWEVORASpellElement::Fire, EWEVORASpellElement::Wind };
	if (Data) { for (EWEVORASpellElement Element : Data->UnlockedElements) { if (IsSupported(Element)) { Result.AddUnique(Element); } } }
	return Result;
}

bool UWEVORASoloProgression::HasCompleted(FName ID) const { return Data && Data->CompletedTrials.Contains(ID); }

bool UWEVORASoloProgression::CompleteTrial(const FWEVORAArenaTrial& Trial)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) { return false; }
	if (!bLoaded) { LoadProgress(); }
	bLastUnlockNew = false;
	if (!bWritable) { return false; }
	if (Trial.ID.IsNone() || !IsSupported(Trial.Reward)) { Status = TEXT("Trial reward is not implemented"); return false; }
	if (HasCompleted(Trial.ID)) { Status = TEXT("Trial already completed. Permanent unlock retained."); return true; }
	UWEVORASoloSave* Candidate = NewObject<UWEVORASoloSave>(this);
	Candidate->CompletedTrials = Data->CompletedTrials; Candidate->UnlockedElements = Data->UnlockedElements;
	Candidate->CompletedTrials.AddUnique(Trial.ID); Candidate->UnlockedElements.AddUnique(Trial.Reward);
	if (!UGameplayStatics::SaveGameToSlot(Candidate, SlotName, 0))
	{
		PendingTrial = Trial; bSavePending = true;
		Status = TEXT("Progress NOT saved. Use Save again before retrying or quitting."); return false;
	}
	bLastUnlockNew = !AllowedElements().Contains(Trial.Reward);
	Data = Candidate; bSavePending = false;
	Status = FString::Printf(TEXT("%s permanently unlocked. Available from the NEXT run."), *UEnum::GetValueAsString(Trial.Reward));
	return true;
}

bool UWEVORASoloProgression::RetrySave() { return bSavePending && CompleteTrial(PendingTrial); }
