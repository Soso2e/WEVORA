#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Spell/WEVORAZoneProfile.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "Spell/WEVORASpellReactionComponent.h"
#include "Spell/WEVORASpellProjectile.h"
#include "Spell/WEVORASpellVolume.h"
#include "Playground/WEVORALocalizedPlaygroundTarget.h"
#include "Playground/WEVORACombatPlayground.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "WEVORACharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include <limits>
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
	for (bool Reverse : {false, true})
	{
		Health->ResetHealth(); Receiver->ReceiveSpell(Fire, Hand, nullptr, nullptr);
		for (int32 Index = 0; Index < 129; ++Index)
		{
			const int32 I = Reverse ? 128 - Index : Index;
			FWEVORASpellData Event = Wind; Event.Power = I == 128 ? 5 : 1;
			FHitResult Contact = Hand; Contact.ImpactPoint.X += I * 0.01;
			Local->QueueReaction(Event, Contact);
		}
		Local->CommitReactions();
		const FWEVORALocalStatus* Strongest = Local->States.FindByPredicate([](const FWEVORALocalStatus& S) { return S.Zone == TEXT("RightHand"); });
		if (TestNotNull(TEXT("Overload keeps hand"), Strongest))
		{
			TestTrue(TEXT("129 inputs keep strongest result in either order"), FMath::IsNearlyEqual(Strongest->Intensity, 5.75f, 0.0001f));
		}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalProjectileIntegrationTest, "WEVORA.Localized.RealProjectileFireWindAndRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalProjectileIntegrationTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene;
	AWEVORALocalizedPlaygroundTarget* Target = Scene.World->SpawnActor<AWEVORALocalizedPlaygroundTarget>(FVector(1000,0,500), FRotator::ZeroRotator);
	UWEVORALocalizedStatusComponent* Local = Target->LocalStatuses;
	TestEqual(TEXT("Seven additional colliders plus existing torso"), Target->ZoneColliders.Num(), 7);
	TestNotNull(TEXT("Sample uses editable saved Zone DataAsset"), Local->Profile.Get());
	TestNotNull(TEXT("Sample uses editable saved Reaction DataAsset"), Local->ReactionProfile.Get());
	auto Shoot = [&](EWEVORASpellElement Element, FName Zone)
	{
		const FVector Position = Local->GetProfile()->GetZonePosition(Zone, Target);
		const FTransform Transform(FRotator::ZeroRotator, FVector(Position.X - 700, Position.Y, Position.Z));
		AWEVORASpellProjectile* Shot = Scene.World->SpawnActorDeferred<AWEVORASpellProjectile>(AWEVORASpellProjectile::StaticClass(), Transform);
		FWEVORASpellLaunch Launch; Launch.Spell.Element = Element; Launch.Spell.Direction = Launch.Direction = FVector::ForwardVector;
		Launch.Parameters.Radius = 10; Launch.Parameters.Speed = 2000;
		Shot->bLogEvents = false; Shot->InitializeSpell(Launch); Shot->FinishSpawning(Transform);
		Scene.Step(0.4f);
		TestTrue(TEXT("Real swept projectile consumed by collider"), Shot->IsActorBeingDestroyed());
	};
	Shoot(EWEVORASpellElement::Fire, TEXT("RightHand"));
	TestEqual(TEXT("Real projectile resolves hand"), Local->LastHitZone, FName(TEXT("RightHand")));
	TestEqual(TEXT("Projectile ignites only hand"), Local->States.Num(), 1);
	TestTrue(TEXT("Fire direct plus bounded elapsed local DoT"), Target->HealthComponent->Health < 75 && Target->HealthComponent->Health > 73);
	const FVector BeforeWind = Target->GetActorLocation();
	Shoot(EWEVORASpellElement::Wind, TEXT("RightArm"));
	TestTrue(TEXT("Wind projectile at neighbor spreads to torso"), Local->HasStatus(TEXT("Torso"), TEXT("Burning")));
	TestTrue(TEXT("Wind simultaneously moves the target"), Target->GetActorLocation().X > BeforeWind.X);
	TestTrue(TEXT("Real integration records propagation"), Local->LastPropagationPaths.Contains(TEXT("RightHand -> RightArm -> Torso")));
	Target->RespawnDelay = 0.1f;
	UGameplayStatics::ApplyDamage(Target, 1000, nullptr, nullptr, UDamageType::StaticClass());
	TestTrue(TEXT("Death clears all local state immediately"), Local->States.IsEmpty());
	TestTrue(TEXT("Death clears propagation"), Local->LastPropagationPaths.IsEmpty());
	TestFalse(TEXT("Death stops local tick"), Local->IsComponentTickEnabled());
	Scene.Step(0.2f);
	TestTrue(TEXT("Target respawns"), Target->HealthComponent->IsAlive() && !Target->IsHidden());
	TestTrue(TEXT("Respawn has no old states"), Local->States.IsEmpty());
	FWEVORASpellData Fire, Wind; Wind.Element = EWEVORASpellElement::Wind;
	Target->SpellReactionComponent->ReceiveSpell(Fire, Scene.Hit(Local, TEXT("RightHand")), nullptr, nullptr);
	Target->SpellReactionComponent->ReceiveSpell(Wind, Scene.Hit(Local, TEXT("RightHand")), nullptr, nullptr);
	AWEVORACombatPlayground* Trial = Scene.World->SpawnActor<AWEVORACombatPlayground>();
	Trial->ResetTrial(); Local->CommitReactions();
	TestTrue(TEXT("Global reset discards pending reactions and states"), Local->States.IsEmpty() && Local->LastPropagationPaths.IsEmpty());
	TestTrue(TEXT("Global reset clears last hit"), Local->LastHitZone.IsNone());
	TestEqual(TEXT("Global reset restores configured HP"), Target->HealthComponent->Health, Trial->TargetHealth);
	Scene.Step(0.5f); TestEqual(TEXT("Reset has no delayed DoT"), Target->HealthComponent->Health, Trial->TargetHealth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalVolumeIntegrationTest, "WEVORA.Localized.RealVolumeMultiZoneAndDebugInvariance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalVolumeIntegrationTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene;
	AWEVORALocalizedPlaygroundTarget* Target = Scene.World->SpawnActor<AWEVORALocalizedPlaygroundTarget>(FVector(1000,0,500), FRotator::ZeroRotator);
	UWEVORALocalizedStatusComponent* Local = Target->LocalStatuses;
	FWEVORASpellLaunch Launch; Launch.Spell.Delivery = EWEVORASpellDelivery::Area;
	Launch.Spell.Direction = Launch.Direction = FVector::ForwardVector; Launch.Volume.Width = 28;
	auto Deliver = [&](FVector Position)
	{
		FTransform Transform(FRotator::ZeroRotator, Position);
		AWEVORASpellVolume* Volume = Scene.World->SpawnActorDeferred<AWEVORASpellVolume>(AWEVORASpellVolume::StaticClass(), Transform);
		Volume->InitializeSpell(Launch); Volume->FinishSpawning(Transform); return Volume;
	};
	const FVector Hand = Local->GetProfile()->GetZonePosition(TEXT("RightHand"), Target);
	AWEVORASpellVolume* Small = Deliver(Hand);
	TestEqual(TEXT("Small volume hits only local hand"), Local->States.Num(), 1);
	TestTrue(TEXT("Hand selected with actor origin outside volume"), Local->HasStatus(TEXT("RightHand"), TEXT("Burning")));
	TestEqual(TEXT("Single direct hit"), Target->HealthComponent->Health, 75.0f);
	TestFalse(TEXT("Same event callback cannot damage again"), Small->DeliverToTarget(Target, Scene.Hit(Local, TEXT("RightHand"))));
	Small->Destroy(); Target->ResetTarget();
	Launch.Spell.Delivery = EWEVORASpellDelivery::Sweep; Launch.Volume.Range = 1000; Launch.Volume.Width = 650; Launch.Volume.Height = 200;
	AWEVORASpellVolume* Wide = Deliver(Target->GetActorLocation() - FVector(500,0,0));
	TestEqual(TEXT("Wide real volume hits all eight zones"), Local->States.Num(), 8);
	TestEqual(TEXT("Eight colliders do one direct hit"), Target->HealthComponent->Health, 75.0f);
	Local->AdvanceStatuses(1);
	TestEqual(TEXT("Eight simultaneous burns still only five DPS"), Target->HealthComponent->Health, 70.0f);
	Wide->Destroy(); Target->ResetTarget();
	AWEVORACombatPlayground* Trial = Scene.World->SpawnActor<AWEVORACombatPlayground>();
	FHitResult Before, After;
	const FVector Start = Hand - FVector(300,0,0), End = Hand + FVector(300,0,0);
	Scene.World->LineTraceSingleByChannel(Before, Start, End, ECC_Visibility);
	Trial->ToggleDebug();
	Scene.World->LineTraceSingleByChannel(After, Start, End, ECC_Visibility);
	TestEqual(TEXT("F2 cannot change resolved hit"), Local->GetProfile()->ResolveHit(Before, Target), Local->GetProfile()->ResolveHit(After, Target));
	TestEqual(TEXT("F2 leaves HP unchanged"), Target->HealthComponent->Health, 100.0f);
	for (const USphereComponent* Collider : Target->ZoneColliders)
	{
		TestEqual(TEXT("Gameplay colliders remain query-only"), Collider->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalPhysicsBodyResolverTest, "WEVORA.Localized.PhysicsAssetHitNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalPhysicsBodyResolverTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene;
	AActor* Owner = Scene.World->SpawnActor<AActor>();
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner, TEXT("TestRig"));
	UPhysicsAsset* Physics = NewObject<UPhysicsAsset>();
	USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(Physics); Body->BoneName = TEXT("mgear_hand_R");
	Physics->SkeletalBodySetups.Add(Body); Physics->UpdateBodySetupIndexMap();
	Mesh->SetPhysicsAsset(Physics); // A resolver test needs no model or physics simulation.
	UWEVORAZoneProfile* Profile = NewObject<UWEVORAZoneProfile>();
	Profile->BodyToZone.Add(Body->BoneName, TEXT("RightHand"));
	FHitResult Hit(Owner, Mesh, FVector::ZeroVector, FVector::UpVector); Hit.BoneName = Body->BoneName;
	TestEqual(TEXT("PhysicsAsset BoneName normalized via body mapping"), Profile->ResolveHit(Hit, Owner), FName(TEXT("RightHand")));
	Hit.BoneName = NAME_None; Hit.Item = 0;
	TestEqual(TEXT("Overlap body index normalized"), Profile->ResolveHit(Hit, Owner), FName(TEXT("RightHand")));
	Hit.Item = 500;
	TestEqual(TEXT("Invalid body index safely ignored"), Profile->ResolveHit(Hit, Owner), NAME_None);
	Hit.Item = 0; Hit.BoneName = TEXT("not_in_physics");
	TestEqual(TEXT("Unknown explicit bone must not reuse unrelated body index"), Profile->ResolveHit(Hit, Owner), NAME_None);
	Profile->BoneToZone.Add(Body->BoneName, TEXT("LeftHand")); Hit.BoneName = Body->BoneName;
	TestEqual(TEXT("Explicit bone mapping wins over body mapping"), Profile->ResolveHit(Hit, Owner), FName(TEXT("LeftHand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalSavedBlueprintTest, "WEVORA.Localized.SavedBlueprintNormalChargeAndAllDeliveries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalSavedBlueprintTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene;
	UClass* PlayerClass = LoadClass<AWEVORACharacter>(nullptr, TEXT("/Game/WEVORA/Playground/BP_PlaygroundPlayer.BP_PlaygroundPlayer_C"));
	if (!TestNotNull(TEXT("Saved Playground Blueprint"), PlayerClass)) { return false; }
	AWEVORACharacter* Player = Scene.World->SpawnActor<AWEVORACharacter>(PlayerClass, FVector(-2000,0,2000), FRotator::ZeroRotator);
	AWEVORALocalizedPlaygroundTarget* Target = Scene.World->SpawnActor<AWEVORALocalizedPlaygroundTarget>(FVector(1000,0,500), FRotator::ZeroRotator);
	Target->HealthComponent->MaxHealth = 1000;
	const TArray<EWEVORAGesture> Gestures{EWEVORAGesture::Thrust, EWEVORAGesture::Sweep, EWEVORAGesture::Circle, EWEVORAGesture::Slam};
	for (EWEVORASpellElement Element : {EWEVORASpellElement::Fire, EWEVORASpellElement::Wind, EWEVORASpellElement::Lightning})
	{
		for (EWEVORAGesture Gesture : Gestures)
		{
			for (float Charge : {0.0f, 1.0f})
			{
				Target->ResetTarget();
				FWEVORASpellContext Context; Context.Element = Element; Context.Gesture = Gesture;
				Context.ChargeAlpha = Charge; Context.GestureMagnitude = Gesture == EWEVORAGesture::Thrust ? 0 : 20;
				FWEVORASpellLaunch Launch;
				if (!TestTrue(TEXT("Blueprint resolves normal/charge and 4 shapes"), Player->SpellCastComponent->ResolveCastLaunch(Context, Launch))) { return false; }
				const FVector Hand = Target->LocalStatuses->GetProfile()->GetZonePosition(TEXT("RightHand"), Target);
				if (Element == EWEVORASpellElement::Wind)
				{
					FWEVORASpellData Fire;
					Target->SpellReactionComponent->ReceiveSpell(Fire, Scene.Hit(Target->LocalStatuses, TEXT("RightHand")), nullptr, Player);
				}
				Launch.Spell.Direction = Launch.Direction = FVector::ForwardVector;
				const bool Projectile = Launch.Spell.Delivery == EWEVORASpellDelivery::Projectile;
				UClass* DeliveryClass = Projectile ? Player->SpellCastComponent->ProjectileClass.Get()
					: Player->SpellCastComponent->DeliveryClasses.FindRef(Launch.Spell.Delivery).Get();
				if (!TestNotNull(TEXT("Concrete delivery exists"), DeliveryClass)) { return false; }
				const FVector Position = Projectile ? Hand - FVector(300,0,0)
					: Launch.Spell.Delivery == EWEVORASpellDelivery::Sweep ? Target->GetActorLocation() - FVector(300,0,0) : Target->GetActorLocation();
				const FTransform Transform(FRotator::ZeroRotator, Position);
				AWEVORASpellDelivery* Delivery = Scene.World->SpawnActorDeferred<AWEVORASpellDelivery>(DeliveryClass, Transform, Player, Player,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				Delivery->bLogEvents = false; Delivery->InitializeSpell(Launch); Delivery->FinishSpawning(Transform);
				Scene.Step(Projectile ? 0.3f : 0.05f);
				TestTrue(TEXT("Actual delivery reached localized target"), Player->SpellCastComponent->LastImpact.Target == Target);
				const float ExpectedDamage = Element == EWEVORASpellElement::Wind ? 0 : 25 * Launch.Spell.Power;
				TestTrue(TEXT("Exactly one direct damage contribution per target"), FMath::IsNearlyEqual(Player->SpellCastComponent->LastImpact.DirectDamage, ExpectedDamage, 0.005f));
				if (Element == EWEVORASpellElement::Fire)
				{
					TestTrue(TEXT("Fire delivery applies local state"), !Target->LocalStatuses->States.IsEmpty());
					TestEqual(TEXT("No legacy duplicate burn"), Target->SpellReactionComponent->BurningRemaining, 0.0f);
				}
				else if (Element == EWEVORASpellElement::Wind)
				{
					TestTrue(TEXT("Every Wind shape can spread hand fire"), Target->LocalStatuses->HasStatus(TEXT("Torso"), TEXT("Burning")));
					TestTrue(TEXT("Every Wind shape keeps its displacement"), Target->SpellReactionComponent->IsDisplaced() || Target->GetActorLocation().X > 1000);
				}
				else { TestTrue(TEXT("Lightning adds no new status"), Target->LocalStatuses->States.IsEmpty()); }
				if (IsValid(Delivery)) { Delivery->Destroy(); }
				Player->SpellCastComponent->ResetFeedback();
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalRuleOrderTest, "WEVORA.Localized.RuleOrderAttributionAndInvalidValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLocalRuleOrderTest::RunTest(const FString& Parameters)
{
	FLocalizedWorld Scene; UWEVORALocalizedStatusComponent* Local = Scene.Receiver();
	Local->ReactionProfile = NewObject<UWEVORALocalReactionProfile>();
	FWEVORALocalReactionRule Additional; Additional.IntensityMultiplier = 1.4f; Additional.DurationMultiplier = 1.8f; Additional.MaxHops = 1;
	Local->ReactionProfile->Rules.Add(Additional);
	FWEVORASpellData Wind; Wind.Element = EWEVORASpellElement::Wind;
	TArray<FWEVORALocalStatus> First;
	for (bool Reverse : {false, true})
	{
		Local->ResetStatuses();
		if (Reverse) { Swap(Local->ReactionProfile->Rules[0], Local->ReactionProfile->Rules[1]); }
		FWEVORALocalStatus S; S.Zone = TEXT("RightHand"); Local->ApplyStatus(S);
		Local->QueueReaction(Wind, Scene.Hit(Local, TEXT("RightHand"))); Local->CommitReactions();
		if (!Reverse) { First = Local->States; }
		TestEqual(TEXT("Rule reordering has equal state count"), Local->States.Num(), First.Num());
		for (int32 I = 0; I < First.Num(); ++I)
		{
			TestEqual(TEXT("Rule reordering has equal intensity"), Local->States[I].Intensity, First[I].Intensity);
			TestEqual(TEXT("Rule reordering has equal duration"), Local->States[I].Remaining, First[I].Remaining);
			TestEqual(TEXT("Rule reordering has equal DPS"), Local->States[I].DamagePerSecond, First[I].DamagePerSecond);
		}
	}
	Local->ResetStatuses();
	FWEVORALocalStatus Strong; Strong.Zone = TEXT("RightHand"); Strong.DamagePerSecond = 10;
	AActor* StrongSource = Scene.World->SpawnActor<AActor>(); Strong.Source = StrongSource;
	Local->ApplyStatus(Strong);
	FWEVORALocalStatus Weak = Strong; Weak.Source = Scene.World->SpawnActor<AActor>(); Weak.DamagePerSecond = 1;
	Local->ApplyStatus(Weak); TestTrue(TEXT("Weak refresh retains strong DoT attribution"), Local->States[0].Source == StrongSource);
	Local->ResetStatuses(); Local->ReactionProfile->Rules = {FWEVORALocalReactionRule()};
	Local->ApplyStatus(Strong);
	Weak.Zone = TEXT("Torso"); Weak.Intensity = 5; Local->ApplyStatus(Weak);
	Local->QueueReaction(Wind, Scene.Hit(Local, TEXT("RightArm"))); Local->CommitReactions();
	const FWEVORALocalStatus* Arm = Local->States.FindByPredicate([](const FWEVORALocalStatus& S) { return S.Zone == TEXT("RightArm"); });
	if (TestNotNull(TEXT("Both neighboring seeds reach arm"), Arm))
	{
		TestTrue(TEXT("Merged source follows highest DPS, independent of intensity"), Arm->Source == StrongSource);
	}
	Local->ResetStatuses(); Local->ApplyStatus(Strong);
	FWEVORALocalStatus Invalid = Strong; Invalid.Intensity = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN state rejected"), Local->ApplyStatus(Invalid));
	Invalid = Strong; Invalid.Remaining = -1; TestFalse(TEXT("Negative lifetime rejected"), Local->ApplyStatus(Invalid));
	const float HealthBefore = Local->GetOwner()->FindComponentByClass<UWEVORAHealthComponent>()->Health;
	Local->AdvanceStatuses(std::numeric_limits<float>::quiet_NaN());
	TestEqual(TEXT("Invalid delta cannot damage HP"), Local->GetOwner()->FindComponentByClass<UWEVORAHealthComponent>()->Health, HealthBefore);
	Local->ReactionProfile->Rules.SetNum(1); Local->ReactionProfile->Rules[0].EffectRadius = std::numeric_limits<float>::infinity();
	Local->QueueReaction(Wind, Scene.Hit(Local, TEXT("RightHand"))); Local->CommitReactions();
	TestEqual(TEXT("Invalid rule cannot propagate"), Local->States.Num(), 1);
	Invalid = Strong; Invalid.Intensity = 1000; Invalid.Accumulation = 1000; Invalid.Remaining = 1000; Invalid.DamagePerSecond = 1000;
	Invalid.Policy = EWEVORAStatusPolicy::Replace; Local->ApplyStatus(Invalid);
	TestEqual(TEXT("Intensity capped"), Local->States[0].Intensity, Local->MaxIntensity);
	TestEqual(TEXT("Accumulation capped"), Local->States[0].Accumulation, Local->MaxAccumulation);
	TestEqual(TEXT("Lifetime capped"), Local->States[0].Remaining, Local->MaxDuration);
	TestEqual(TEXT("DPS capped"), Local->States[0].DamagePerSecond, Local->MaxDamagePerSecond);
	return true;
}
#endif
