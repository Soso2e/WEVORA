#pragma once
#include "CoreMinimal.h"
#include "WEVORASpellTypes.h"

/** Streaming recorder: constant memory regardless of how long E is held. */
class WEVORA_API FWEVORAGestureRecognizer
{
public:
	void Reset();
	void Record(const FVector2D& Delta, float MinimumSampleDistance);
	FWEVORASpellContext Recognize(const FWEVORAGestureThresholds& Thresholds, float Duration) const;
private:
	FVector2D Displacement = FVector2D::ZeroVector;
	FVector2D PendingDelta = FVector2D::ZeroVector;
	FVector2D PreviousDirection = FVector2D::ZeroVector;
	float PathLength = 0.0f;
	float SignedTurn = 0.0f;
	float AbsoluteTurn = 0.0f;
};
