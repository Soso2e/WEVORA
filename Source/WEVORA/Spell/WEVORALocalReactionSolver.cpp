#include "Spell/WEVORALocalReactionSolver.h"
#include "GameFramework/Actor.h"

FWEVORALocalReactionResult WEVORALocalReactions::Solve(const UWEVORAZoneProfile& Zones,
	const UWEVORALocalReactionProfile& Rules, const AActor& Owner,
	const TArray<FWEVORALocalStatus>& Snapshot, const TArray<FWEVORALocalReactionInput>& Inputs)
{
	FWEVORALocalReactionResult Result;
	for (const FWEVORALocalReactionInput& Input : Inputs)
	{
		if (!Zones.ContainsZone(Input.Zone)) { continue; }
		const FVector Impact = Owner.GetActorTransform().TransformPosition(Input.LocalImpact);
		for (const FWEVORALocalReactionRule& Rule : Rules.Rules)
		{
			if (Rule.Trigger != Input.Element || !Rule.IsUsable()) { continue; }
			for (const FWEVORALocalStatus& Seed : Snapshot)
			{
				const FVector SeedPosition = Zones.GetZonePosition(Seed.Zone, &Owner);
				if (Seed.Type != Rule.RequiredStatus || Seed.Intensity < Rule.MinimumIntensity || Seed.Remaining <= 0
					|| (Seed.Zone != Input.Zone && !Zones.GetNeighbors(Input.Zone).Contains(Seed.Zone))
					|| FVector::Distance(Impact, SeedPosition) > Rule.SeedRadius) { continue; }
				struct FNode { FName Zone; float Distance; TArray<FName> Path; };
				TArray<FNode> Frontier; Frontier.Add({Seed.Zone, 0, {Seed.Zone}});
				// At most Zones * (MaxHops+1) frontier states; no recursive reaction and no unbounded walk.
				for (int32 Hop = 0; Hop <= FMath::Clamp(Rule.MaxHops, 0, 16) && !Frontier.IsEmpty(); ++Hop)
				{
					TArray<FNode> Next;
					for (const FNode& Node : Frontier)
					{
						const FVector Position = Zones.GetZonePosition(Node.Zone, &Owner);
						if (FVector::Distance(Impact, Position) > Rule.EffectRadius) { continue; }
						const float Scale = Rule.IntensityMultiplier * FMath::Clamp(Input.Power, 0.0f, 5.0f)
							* FMath::Pow(FMath::Clamp(Rule.AttenuationPerHop, 0.0f, 1.0f), Hop);
						FWEVORALocalStatus Proposal = Seed;
						Proposal.Zone = Node.Zone; Proposal.Type = Rule.OutputStatus; Proposal.Element = Rule.OutputElement;
						Proposal.Intensity *= Scale; Proposal.DamagePerSecond *= Scale; Proposal.Accumulation *= Scale;
						Proposal.Remaining *= Rule.DurationMultiplier; Proposal.Policy = EWEVORAStatusPolicy::Refresh;
						if (Proposal.Intensity < Rule.MinimumIntensity || Proposal.Remaining <= 0) { continue; }
						FString Path;
						for (FName ID : Node.Path) { if (!Path.IsEmpty()) { Path += TEXT(" -> "); } Path += ID.ToString(); }
						const int32 Existing = Result.Applications.IndexOfByPredicate([&](const FWEVORALocalStatus& S)
						{ return S.Zone == Proposal.Zone && S.Type == Proposal.Type; });
						if (Existing == INDEX_NONE) { Result.Applications.Add(Proposal); Result.Paths.Add(Path); }
						else
						{
							FWEVORALocalStatus& Previous = Result.Applications[Existing];
							// Numeric fields merge independently; attribution follows the retained strongest DPS.
							const bool bPrefer = Proposal.DamagePerSecond > Previous.DamagePerSecond || (Proposal.DamagePerSecond == Previous.DamagePerSecond
								&& (Path < Result.Paths[Existing] || (Path == Result.Paths[Existing]
								&& GetPathNameSafe(Proposal.Source.Get()) < GetPathNameSafe(Previous.Source.Get()))));
							const float Intensity = FMath::Max(Previous.Intensity, Proposal.Intensity);
							const float Duration = FMath::Max(Previous.Remaining, Proposal.Remaining);
							const float DPS = FMath::Max(Previous.DamagePerSecond, Proposal.DamagePerSecond);
							const float Accumulation = FMath::Max(Previous.Accumulation, Proposal.Accumulation);
							if (bPrefer) { Previous = Proposal; Result.Paths[Existing] = Path; }
							Previous.Intensity = Intensity; Previous.Remaining = Duration;
							Previous.DamagePerSecond = DPS; Previous.Accumulation = Accumulation;
						}
						for (FName Neighbor : Zones.GetNeighbors(Node.Zone))
						{
							if (Node.Path.Contains(Neighbor)) { continue; }
							const float Edge = FVector::Distance(Position, Zones.GetZonePosition(Neighbor, &Owner));
							const float Distance = Node.Distance + Edge;
							if (Edge > Rule.MaxEdgeDistance || Distance > Rule.MaxPathDistance) { continue; }
							FNode Candidate{Neighbor, Distance, Node.Path}; Candidate.Path.Add(Neighbor);
							FNode* Found = Next.FindByPredicate([Neighbor](const FNode& N) { return N.Zone == Neighbor; });
							if (!Found) { Next.Add(Candidate); }
							else if (Distance < Found->Distance) { *Found = Candidate; }
						}
					}
					Frontier = MoveTemp(Next);
				}
			}
		}
	}
	return Result;
}
