#pragma once
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Spell/WEVORALocalReactionProfile.h"

struct FWEVORALocalReactionResult
{
	TArray<FWEVORALocalStatus> Applications;
	TArray<FString> Paths;
};

/** Pure proposal calculation: no HP, status mutation, presentation or UObject allocation. */
namespace WEVORALocalReactions
{
	FWEVORALocalReactionResult Solve(const UWEVORAZoneProfile& Zones, const UWEVORALocalReactionProfile& Rules,
		const AActor& Owner, const TArray<FWEVORALocalStatus>& Snapshot, const TArray<FWEVORALocalReactionInput>& Inputs);
}
