#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "WEVORASoloGameMode.generated.h"

UCLASS()
class WEVORA_API AWEVORASoloGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AWEVORASoloGameMode();
	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
};

UCLASS()
class WEVORA_API AWEVORASoloHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
