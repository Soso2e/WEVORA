#include "Movement/WEVORAFlightMovementComponent.h"
#include "WEVORACharacter.h"

void UWEVORAFlightMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	if (!ShouldSkipUpdate(DeltaTime) && HasValidData() && CharacterOwner->IsLocallyControlled())
	{
		if (AWEVORACharacter* Pilot = Cast<AWEVORACharacter>(CharacterOwner))
		{
			Pilot->UpdateFlightBeforeMovement(DeltaTime);
		}
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
