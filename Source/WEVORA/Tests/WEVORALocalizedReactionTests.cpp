#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Spell/WEVORAZoneProfile.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

namespace
{
struct FLocalizedWorld
{
	TGuardValue<uint64> FrameGuard{GFrameCounter, GFrameCounter};
	UWorld* World;
	FLocalizedWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL); World->BeginPlay();
	}
	~FLocalizedWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	void Step(float Seconds, float Frame = 1.0f/60)
	{
		for (double Left = Seconds; Left > 0.000001; Left -= Frame)
		{
			++GFrameCounter; World->Tick(LEVELTICK_All, FMath::Min(float(Left), Frame));
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoneResolverTest, "WEVORA.Localized.Resolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FZoneResolverTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene;
	UWEVORAZoneProfile* Profile = NewObject<UWEVORAZoneProfile>();
	Profile->BoneToZone.Add(TEXT("rig_hand_r"), TEXT("RightHand"));
	Profile->BodyToZone.Add(TEXT("physics_r"), TEXT("RightArm"));
	TestEqual(TEXT("Bone maps to independent ID"), Profile->ResolveNames(TEXT("rig_hand_r"), NAME_None, NAME_None), FName(TEXT("RightHand")));
	TestEqual(TEXT("Body maps to independent ID"), Profile->ResolveNames(NAME_None, TEXT("physics_r"), NAME_None), FName(TEXT("RightArm")));
	TestEqual(TEXT("Unknown bone cannot fall back to torso"), Profile->ResolveNames(TEXT("unknown"), NAME_None, TEXT("Collision")), NAME_None);
	Profile->BoneToZone.Add(TEXT("bad"), TEXT("MissingZone"));
	TestEqual(TEXT("Invalid mapping is safe"), Profile->ResolveNames(TEXT("bad"), NAME_None, NAME_None), NAME_None);
	AActor* Owner = Scene.World->SpawnActor<AActor>();
	USphereComponent* Collider = NewObject<USphereComponent>(Owner, TEXT("Zone_RightHand"));
	Owner->SetRootComponent(Collider); Collider->RegisterComponent();
	FHitResult Hit(Owner, Collider, FVector::ZeroVector, FVector::UpVector);
	TestEqual(TEXT("Hit collider resolves hand"), Profile->ResolveHit(Hit, Owner), FName(TEXT("RightHand")));
	TestEqual(TEXT("Other actor component rejected"), Profile->ResolveHit(Hit, Scene.World->SpawnActor<AActor>()), NAME_None);
	TestTrue(TEXT("Hand has adjacent arm"), Profile->GetNeighbors(TEXT("RightHand")).Contains(TEXT("RightArm")));
	TestFalse(TEXT("Hand has no direct torso edge"), Profile->GetNeighbors(TEXT("RightHand")).Contains(TEXT("Torso")));
	TestEqual(TEXT("Standard profile has eight zones"), Profile->Zones.Num(), 8);
	return true;
}
#endif
