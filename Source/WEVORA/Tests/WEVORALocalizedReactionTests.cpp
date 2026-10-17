#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Spell/WEVORAZoneProfile.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "AI/WEVORAHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
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
	UWEVORALocalizedStatusComponent* Receiver()
	{
		AActor* Actor = World->SpawnActor<AActor>();
		USphereComponent* Root = NewObject<USphereComponent>(Actor, TEXT("Collision"));
		Actor->SetRootComponent(Root); Root->RegisterComponent();
		UWEVORAHealthComponent* Health = NewObject<UWEVORAHealthComponent>(Actor);
		Actor->AddInstanceComponent(Health); Health->RegisterComponent();
		UWEVORALocalizedStatusComponent* Local = NewObject<UWEVORALocalizedStatusComponent>(Actor);
		Local->bUsePrototypeProfile = true;
		Actor->AddInstanceComponent(Local); Local->RegisterComponent();
		return Local;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalStatusLifecycleTest, "WEVORA.Localized.StatusPoliciesAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalStatusLifecycleTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene;
	UWEVORALocalizedStatusComponent* Local = Scene.Receiver();
	UWEVORAHealthComponent* Health = Local->GetOwner()->FindComponentByClass<UWEVORAHealthComponent>();
	FWEVORALocalStatus Status; Status.Zone = TEXT("RightHand");
	TestTrue(TEXT("Hand applies"), Local->ApplyStatus(Status));
	TestFalse(TEXT("Torso unaffected"), Local->HasStatus(TEXT("Torso"), TEXT("Burning")));
	TestEqual(TEXT("Status application has no direct HP cost"), Health->Health, 100.0f);
	Status.Zone = TEXT("Missing"); TestFalse(TEXT("Unknown zone rejected"), Local->ApplyStatus(Status));
	Status.Zone = TEXT("RightHand"); Status.Policy = EWEVORAStatusPolicy::Replace; Status.Intensity = 2; Status.Remaining = 1;
	Local->ApplyStatus(Status); TestEqual(TEXT("Replace duration"), Local->States[0].Remaining, 1.0f);
	Status.Policy = EWEVORAStatusPolicy::Refresh; Status.Intensity = 1; Status.Remaining = 0.5f;
	Local->ApplyStatus(Status); TestEqual(TEXT("Refresh cannot weaken"), Local->States[0].Intensity, 2.0f);
	TestEqual(TEXT("Refresh cannot shorten"), Local->States[0].Remaining, 1.0f);
	Status.Policy = EWEVORAStatusPolicy::Accumulate; Local->ApplyStatus(Status);
	TestEqual(TEXT("Accumulate intensity"), Local->States[0].Intensity, 3.0f);
	TestEqual(TEXT("Accumulate value"), Local->States[0].Accumulation, 2.0f);
	Status.Policy = EWEVORAStatusPolicy::Extend; Status.Remaining = 2; Local->ApplyStatus(Status);
	TestEqual(TEXT("Extend duration"), Local->States[0].Remaining, 3.0f);
	Local->RemoveStatus(Status.Zone, Status.Type); TestTrue(TEXT("Explicit removal"), Local->States.IsEmpty());
	Local->ApplyStatus(Status); Health->ResetHealth();
	TestTrue(TEXT("Health reset clears local state"), Local->States.IsEmpty());
	Local->ApplyStatus(Status);
	UGameplayStatics::ApplyDamage(Local->GetOwner(), 1000, nullptr, nullptr, UDamageType::StaticClass());
	TestTrue(TEXT("Death immediately clears states"), Local->States.IsEmpty());
	TestFalse(TEXT("Death disables tick"), Local->IsComponentTickEnabled());
	TestFalse(TEXT("Dead target rejects application"), Local->ApplyStatus(Status));
	Health->ResetHealth(); Local->AdvanceStatuses(5);
	TestEqual(TEXT("New life has no delayed damage"), Health->Health, 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalDamageIntegralTest, "WEVORA.Localized.DamageIntegralAndFrameRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalDamageIntegralTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene;
	for (float Frame : {1.0f/30, 1.0f/60, 1.0f/120, 4.0f})
	{
		UWEVORALocalizedStatusComponent* Local = Scene.Receiver();
		FWEVORALocalStatus S; S.Zone = TEXT("RightHand"); S.DamagePerSecond = 10; S.Remaining = 1;
		Local->ApplyStatus(S);
		S.Zone = TEXT("Torso"); S.DamagePerSecond = 5; S.Remaining = 3; Local->ApplyStatus(S);
		Local->SetComponentTickEnabled(false);
		for (double Left = 4; Left > 0.000001; Left -= Frame) { Local->AdvanceStatuses(FMath::Min(Frame, float(Left))); }
		const UWEVORAHealthComponent* Health = Local->GetOwner()->FindComponentByClass<UWEVORAHealthComponent>();
		TestTrue(TEXT("Max envelope integrates 10*1 + 5*2 at every frame rate/hitch"), FMath::IsNearlyEqual(Health->Health, 80.0f, 0.002f));
		TestTrue(TEXT("All states expire"), Local->States.IsEmpty());
		Local->AdvanceStatuses(10); TestTrue(TEXT("Expired states do no extra damage"), FMath::IsNearlyEqual(Health->Health, 80.0f, 0.002f));
	}
	return true;
}
#endif
