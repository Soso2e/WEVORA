#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WEVORAFlightMovementComponent.generated.h"

/** Updates flight forces before Unreal integrates movement and resolves landings. */
UCLASS()
class WEVORA_API UWEVORAFlightMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
