#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Spell/WEVORAZoneProfile.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Spell/WEVORASpellProjectile.h"
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
		UWEVORASpellReactionComponent* Reaction = NewObject<UWEVORASpellReactionComponent>(Actor);
		Actor->AddInstanceComponent(Reaction); Reaction->RegisterComponent();
		return Local;
	}
	FHitResult Hit(UWEVORALocalizedStatusComponent* Local, FName Zone)
	{
		AActor* Actor = Local->GetOwner();
		const FName Name(*FString::Printf(TEXT("Zone_%s"), *Zone.ToString()));
		USphereComponent* Collider = FindObject<USphereComponent>(Actor, *Name.ToString());
		if (!Collider)
		{
			Collider = NewObject<USphereComponent>(Actor, Name);
			Actor->AddInstanceComponent(Collider); Collider->SetupAttachment(Actor->GetRootComponent());
			Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision); Collider->RegisterComponent();
		}
		return FHitResult(Actor, Collider, Local->GetProfile()->GetZonePosition(Zone, Actor), FVector::UpVector);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalFireWindTest, "WEVORA.Localized.FireWindCombatLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalFireWindTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene; UWEVORALocalizedStatusComponent* Local = Scene.Receiver();
	AActor* Actor = Local->GetOwner();
	UWEVORASpellReactionComponent* Receiver = Actor->FindComponentByClass<UWEVORASpellReactionComponent>();
	UWEVORAHealthComponent* Health = Actor->FindComponentByClass<UWEVORAHealthComponent>();
	FWEVORASpellData Spell;
	const FHitResult Hit = Scene.Hit(Local, TEXT("RightHand"));
	TestTrue(TEXT("Fire accepted on generic actor"), Receiver->ReceiveSpell(Spell, Hit, nullptr, nullptr));
	TestEqual(TEXT("One direct hit"), Health->Health, 75.0f);
	TestEqual(TEXT("Only one local state"), Local->States.Num(), 1);
	TestTrue(TEXT("Right hand burns"), Local->HasStatus(TEXT("RightHand"), TEXT("Burning")));
	TestFalse(TEXT("Arm not yet burning"), Local->HasStatus(TEXT("RightArm"), TEXT("Burning")));
	TestEqual(TEXT("No legacy burn timer"), Receiver->BurningRemaining, 0.0f);
	Spell.Element = EWEVORASpellElement::Wind;
	TestTrue(TEXT("Wind accepted"), Receiver->ReceiveSpell(Spell, Hit, nullptr, nullptr));
	TestTrue(TEXT("Wind displacement remains"), Receiver->IsDisplaced());
	Local->CommitReactions();
	TestEqual(TEXT("Exactly hand, arm, torso"), Local->States.Num(), 3);
	TestTrue(TEXT("Arm burns"), Local->HasStatus(TEXT("RightArm"), TEXT("Burning")));
	TestTrue(TEXT("Torso burns"), Local->HasStatus(TEXT("Torso"), TEXT("Burning")));
	TestFalse(TEXT("Opposite hand is not reached"), Local->HasStatus(TEXT("LeftHand"), TEXT("Burning")));
	TestTrue(TEXT("Trace records two-hop path"), Local->LastPropagationPaths.Contains(TEXT("RightHand -> RightArm -> Torso")));
	TestTrue(TEXT("Duration updated"), FMath::IsNearlyEqual(Local->GetRemaining(TEXT("Burning")), 3.9f, 0.0001f));
	Local->AdvanceStatuses(1);
	TestTrue(TEXT("One strongest 5.75 DPS stream"), FMath::IsNearlyEqual(Health->Health, 69.25f));
	Local->AdvanceStatuses(4);
	TestTrue(TEXT("All effects expire"), Local->States.IsEmpty());
	const float FinalHP = Health->Health;
	Local->AdvanceStatuses(4); TestEqual(TEXT("No expired damage"), Health->Health, FinalHP);
	Receiver->ResetReactions(); Health->ResetHealth();
	TestTrue(TEXT("Reset clears path"), Local->LastPropagationPaths.IsEmpty());
	TestTrue(TEXT("Reset clears last zone"), Local->LastHitZone.IsNone());
	TestFalse(TEXT("Reset clears wind"), Receiver->IsDisplaced());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalPropagationLimitsTest, "WEVORA.Localized.TopologyDistanceAndCycles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalPropagationLimitsTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene; UWEVORALocalizedStatusComponent* Local = Scene.Receiver();
	Local->ReactionProfile = NewObject<UWEVORALocalReactionProfile>();
	Local->Profile = NewObject<UWEVORAZoneProfile>();
	UWEVORASpellReactionComponent* Receiver = Local->GetOwner()->FindComponentByClass<UWEVORASpellReactionComponent>();
	UWEVORAHealthComponent* Health = Local->GetOwner()->FindComponentByClass<UWEVORAHealthComponent>();
	FWEVORASpellData Fire, Wind; Wind.Element = EWEVORASpellElement::Wind;
	auto Run = [&](FName TriggerZone)
	{
		Health->ResetHealth(); Receiver->ReceiveSpell(Fire, Scene.Hit(Local, TEXT("RightHand")), nullptr, nullptr);
		Receiver->ReceiveSpell(Wind, Scene.Hit(Local, TriggerZone), nullptr, nullptr); Local->CommitReactions();
	};
	FWEVORALocalReactionRule& Rule = Local->ReactionProfile->Rules[0];
	Rule.MaxHops = 1; Run(TEXT("RightHand"));
	TestEqual(TEXT("One hop reaches only arm"), Local->States.Num(), 2);
	Rule.MaxHops = 2; Rule.MaxEdgeDistance = 20; Run(TEXT("RightHand"));
	TestEqual(TEXT("Long edge blocked"), Local->States.Num(), 1);
	Rule.MaxEdgeDistance = 110; Rule.MaxPathDistance = 100; Run(TEXT("RightHand"));
	TestEqual(TEXT("Total path blocks torso"), Local->States.Num(), 2);
	Rule.MaxPathDistance = 220; Rule.EffectRadius = 80; Run(TEXT("RightHand"));
	TestEqual(TEXT("World effect radius blocks torso"), Local->States.Num(), 2);
	Rule.EffectRadius = 240; Run(TEXT("RightArm"));
	TestTrue(TEXT("Adjacent wind can find burning hand"), Local->HasStatus(TEXT("Torso"), TEXT("Burning")));
	Run(TEXT("LeftHand")); TestEqual(TEXT("Unconnected distant hit cannot seed"), Local->States.Num(), 1);
	Rule.MinimumIntensity = 2; Run(TEXT("RightHand")); TestEqual(TEXT("Weak seed rejected"), Local->States.Num(), 1);
	Rule.MinimumIntensity = 0.1f; Rule.MaxHops = 16; Rule.EffectRadius = 2000; Rule.MaxEdgeDistance = 1000; Rule.MaxPathDistance = 2000;
	FWEVORAZoneLink Cycle; Cycle.From = TEXT("Torso"); Cycle.To = TEXT("RightHand"); Local->Profile->Links.Add(Cycle);
	Run(TEXT("RightHand"));
	TestEqual(TEXT("Cyclic graph applies each zone once"), Local->States.Num(), 8);
	TestEqual(TEXT("One trace per resulting status"), Local->LastPropagationPaths.Num(), 8);
	TestTrue(TEXT("Cycle cannot compound duration"), FMath::IsNearlyEqual(Local->GetRemaining(TEXT("Burning")), 3.9f, 0.0001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalOrderingTest, "WEVORA.Localized.FrameBatchOrderAndDuplicateDelivery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalOrderingTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene; UWEVORALocalizedStatusComponent* Local = Scene.Receiver();
	UWEVORASpellReactionComponent* Receiver = Local->GetOwner()->FindComponentByClass<UWEVORASpellReactionComponent>();
	UWEVORAHealthComponent* Health = Local->GetOwner()->FindComponentByClass<UWEVORAHealthComponent>();
	FWEVORASpellData Fire, Wind; Wind.Element = EWEVORASpellElement::Wind;
	const FHitResult Hand = Scene.Hit(Local, TEXT("RightHand"));
	const FHitResult Arm = Scene.Hit(Local, TEXT("RightArm"));
	TArray<FWEVORALocalStatus> First;
	for (bool Reverse : {false, true})
	{
		Health->ResetHealth();
		if (Reverse) { Receiver->ReceiveSpell(Wind, Arm, nullptr, nullptr); }
		Receiver->ReceiveSpell(Fire, Hand, nullptr, nullptr);
		if (!Reverse) { Receiver->ReceiveSpell(Wind, Arm, nullptr, nullptr); }
		Receiver->ReceiveSpell(Wind, Hand, nullptr, nullptr);
		Receiver->ReceiveSpell(Wind, Hand, nullptr, nullptr);
		Local->CommitReactions();
		if (!Reverse) { First = Local->States; }
		TestEqual(TEXT("Order-independent state count"), Local->States.Num(), First.Num());
		for (int32 I = 0; I < First.Num() && I < Local->States.Num(); ++I)
		{
			TestEqual(TEXT("Stable zones"), Local->States[I].Zone, First[I].Zone);
			TestEqual(TEXT("Stable intensity"), Local->States[I].Intensity, First[I].Intensity);
			TestEqual(TEXT("Stable duration"), Local->States[I].Remaining, First[I].Remaining);
		}
		TestEqual(TEXT("Wind never adds direct damage"), Health->Health, 75.0f);
		TestTrue(TEXT("Multiple reactions use one snapshot, no amplification chain"), FMath::IsNearlyEqual(Local->GetRemaining(TEXT("Burning")), 3.9f, 0.0001f));
	}
	Health->ResetHealth();
	AWEVORASpellProjectile* Delivery = Scene.World->SpawnActor<AWEVORASpellProjectile>();
	FWEVORASpellLaunch Launch; Delivery->InitializeSpell(Launch);
	TestTrue(TEXT("Multi-zone delivery"), Delivery->DeliverToTargetZones(Local->GetOwner(), {Hand, Arm, Hand}));
	TestEqual(TEXT("One direct damage across overlapping limbs"), Health->Health, 75.0f);
	TestEqual(TEXT("Only unique hit zones get statuses"), Local->States.Num(), 2);
	TestFalse(TEXT("Duplicate callback rejected"), Delivery->DeliverToTarget(Local->GetOwner(), Hand));
	TestEqual(TEXT("Duplicate callback cannot reduce HP"), Health->Health, 75.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalLegacyCompatibilityTest, "WEVORA.Localized.LegacyBurningCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalLegacyCompatibilityTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene; UWEVORALocalizedStatusComponent* Local = Scene.Receiver();
	UWEVORASpellReactionComponent* Receiver = Local->GetOwner()->FindComponentByClass<UWEVORASpellReactionComponent>();
	UWEVORAHealthComponent* Health = Local->GetOwner()->FindComponentByClass<UWEVORAHealthComponent>();
	FWEVORASpellData Fire;
	const FHitResult Hand = Scene.Hit(Local, TEXT("RightHand"));
	Local->bUsePrototypeProfile = false;
	Receiver->ReceiveSpell(Fire, Hand, nullptr, nullptr);
	TestEqual(TEXT("Opt-out uses existing whole-body burning"), Receiver->BurningRemaining, 3.0f);
	Local->bUsePrototypeProfile = true;
	Receiver->ReceiveSpell(Fire, Hand, nullptr, nullptr);
	TestEqual(TEXT("Localized application cancels old whole-body burn"), Receiver->BurningRemaining, 0.0f);
	Local->AdvanceStatuses(3);
	Receiver->TickComponent(3, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Two direct hits and one 15HP burn, no duplicate DoT"), Health->Health, 35.0f);
	Health->ResetHealth();
	FHitResult Unknown = Hand; Unknown.BoneName = TEXT("not_registered");
	Receiver->ReceiveSpell(Fire, Unknown, nullptr, nullptr);
	TestEqual(TEXT("Unknown hit still has normal direct damage"), Health->Health, 75.0f);
	TestTrue(TEXT("Unknown hit cannot burn a guessed zone"), Local->States.IsEmpty());
	TestEqual(TEXT("Unknown hit cannot fall back to whole-body burn"), Receiver->BurningRemaining, 0.0f);
	return true;
}
#endif
