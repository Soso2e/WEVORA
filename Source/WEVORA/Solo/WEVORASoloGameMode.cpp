#include "Solo/WEVORASoloGameMode.h"
#include "Solo/WEVORARunDirector.h"
#include "WEVORACharacter.h"
#include "AI/WEVORAHealthComponent.h"
#include "Movement/WEVORAManaComponent.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AWEVORASoloGameMode::AWEVORASoloGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> Player(TEXT("/Game/WEVORA/Playground/BP_PlaygroundPlayer"));
	static ConstructorHelpers::FClassFinder<APlayerController> Controller(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));
	DefaultPawnClass = Player.Class;
	PlayerControllerClass = Controller.Class;
	HUDClass = AWEVORASoloHUD::StaticClass();
}

void AWEVORASoloGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!TActorIterator<AWEVORARunDirector>(GetWorld())) { GetWorld()->SpawnActor<AWEVORARunDirector>(); }
}

void AWEVORASoloHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }
	for (TActorIterator<AWEVORARunDirector> It(GetWorld()); It; ++It)
	{
		const AWEVORARunDirector* Run = *It;
		const AWEVORACharacter* Player = Run->Pilot;
		DrawText(FString::Printf(TEXT("SOLO ARENA   Wave %d / %d   %s\nStart %.1fs   Spawned %d   Alive %d   Defeated %d\nHP %.0f / %.0f   Mana %.0f / %.0f\n%s"),
			Run->WaveIndex + 1, Run->Config->Waves.Num(), *UEnum::GetValueAsString(Run->State), Run->Countdown,
			Run->SpawnedCount, Run->Enemies.Num(), Run->KilledCount,
			Player ? Player->HealthComponent->Health : 0, Player ? Player->HealthComponent->MaxHealth : 0,
			Player ? Player->ManaComponent->Mana : 0, Player ? Player->ManaComponent->MaxMana : 0, *Run->ResultReason),
			FLinearColor::White, 30, 30, nullptr, 1.25f);
		break;
	}
}
