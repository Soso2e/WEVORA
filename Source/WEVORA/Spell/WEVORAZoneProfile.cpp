#include "Spell/WEVORAZoneProfile.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "GameFramework/Actor.h"

UWEVORAZoneProfile::UWEVORAZoneProfile()
{
	auto Zone = [this](const TCHAR* ID, FVector Offset)
	{
		FWEVORAZoneDefinition D; D.ID = ID; D.Offset = Offset; Zones.Add(D);
		ColliderToZone.Add(FName(*FString::Printf(TEXT("Zone_%s"), ID)), ID);
	};
	Zone(TEXT("Head"), FVector(0,0,95));
	Zone(TEXT("Torso"), FVector::ZeroVector);
	Zone(TEXT("LeftArm"), FVector(0,-85,15));
	Zone(TEXT("RightArm"), FVector(0,85,15));
	Zone(TEXT("LeftHand"), FVector(0,-150,0));
	Zone(TEXT("RightHand"), FVector(0,150,0));
	Zone(TEXT("LeftLeg"), FVector(0,-35,-90));
	Zone(TEXT("RightLeg"), FVector(0,35,-90));
	ColliderToZone.Add(TEXT("Collision"), TEXT("Torso"));
	auto Link = [this](const TCHAR* A, const TCHAR* B)
	{
		FWEVORAZoneLink L; L.From = A; L.To = B; Links.Add(L);
	};
	Link(TEXT("RightHand"), TEXT("RightArm")); Link(TEXT("RightArm"), TEXT("Torso"));
	Link(TEXT("LeftHand"), TEXT("LeftArm")); Link(TEXT("LeftArm"), TEXT("Torso"));
	Link(TEXT("Head"), TEXT("Torso")); Link(TEXT("LeftLeg"), TEXT("Torso")); Link(TEXT("RightLeg"), TEXT("Torso"));
}

bool UWEVORAZoneProfile::ContainsZone(FName ID) const
{
	return !ID.IsNone() && Zones.ContainsByPredicate([ID](const FWEVORAZoneDefinition& D) { return D.ID == ID; });
}

FName UWEVORAZoneProfile::ResolveNames(FName Bone, FName PhysicsBody, FName Collider) const
{
	const FName* Found = nullptr;
	if (!Bone.IsNone()) { Found = BoneToZone.Find(Bone); }
	if (!Found && !PhysicsBody.IsNone()) { Found = BodyToZone.Find(PhysicsBody); }
	// An unregistered bone on a mesh must not silently become the mesh's default body zone.
	if (!Found && Bone.IsNone() && PhysicsBody.IsNone()) { Found = ColliderToZone.Find(Collider); }
	return Found && ContainsZone(*Found) ? *Found : NAME_None;
}

FName UWEVORAZoneProfile::ResolveHit(const FHitResult& Hit, const AActor* ExpectedOwner) const
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component || Component->GetOwner() != ExpectedOwner) { return NAME_None; }
	FName Body;
	if (const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Component))
	{
		const UPhysicsAsset* Physics = Mesh->GetPhysicsAsset();
		// Hit.BoneName normally identifies the physics body. Item is an index only when no bone is reported.
		const int32 Index = Physics ? (!Hit.BoneName.IsNone() ? Physics->FindBodyIndex(Hit.BoneName) : Hit.Item) : INDEX_NONE;
		if (Physics && Physics->SkeletalBodySetups.IsValidIndex(Index) && Physics->SkeletalBodySetups[Index])
		{
			Body = Physics->SkeletalBodySetups[Index]->BoneName;
		}
	}
	return ResolveNames(Hit.BoneName, Body, Component->GetFName());
}

FVector UWEVORAZoneProfile::GetZonePosition(FName ID, const AActor* Owner) const
{
	if (!Owner) { return FVector::ZeroVector; }
	const FWEVORAZoneDefinition* D = Zones.FindByPredicate([ID](const FWEVORAZoneDefinition& Z) { return Z.ID == ID; });
	if (!D) { return Owner->GetActorLocation(); }
	if (!D->AnchorBone.IsNone())
	{
		if (const USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (Mesh->DoesSocketExist(D->AnchorBone)) { return Mesh->GetSocketTransform(D->AnchorBone).TransformPosition(D->Offset); }
		}
	}
	return Owner->GetActorTransform().TransformPosition(D->Offset);
}

TArray<FName> UWEVORAZoneProfile::GetNeighbors(FName ID) const
{
	TArray<FName> Result;
	for (const FWEVORAZoneLink& L : Links)
	{
		if (L.From == ID && ContainsZone(L.To)) { Result.AddUnique(L.To); }
		if (L.bBidirectional && L.To == ID && ContainsZone(L.From)) { Result.AddUnique(L.From); }
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}
