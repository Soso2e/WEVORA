#include "WEVORAGestureRecognizer.h"

void FWEVORAGestureRecognizer::Reset()
{
	*this = FWEVORAGestureRecognizer();
}

void FWEVORAGestureRecognizer::Record(const FVector2D& Delta, float MinimumSampleDistance)
{
	if (!FMath::IsFinite(Delta.X) || !FMath::IsFinite(Delta.Y)) { return; }
	Displacement += Delta;
	PathLength += Delta.Size();
	PendingDelta += Delta;
	if (PendingDelta.Size() < FMath::Max(MinimumSampleDistance, KINDA_SMALL_NUMBER)) { return; }
	const FVector2D Direction = PendingDelta.GetSafeNormal();
	if (!PreviousDirection.IsNearlyZero())
	{
		const float Cross = PreviousDirection.X * Direction.Y - PreviousDirection.Y * Direction.X;
		const float Turn = FMath::Atan2(Cross, FVector2D::DotProduct(PreviousDirection, Direction));
		SignedTurn += Turn;
		AbsoluteTurn += FMath::Abs(Turn);
	}
	PreviousDirection = Direction;
	PendingDelta = FVector2D::ZeroVector;
}

FWEVORASpellContext FWEVORAGestureRecognizer::Recognize(const FWEVORAGestureThresholds& T, float Duration) const
{
	FWEVORASpellContext Result;
	Result.GestureDuration = FMath::Max(0.0f, Duration);
	Result.GestureMagnitude = PathLength;
	Result.GestureDirection = Displacement.GetSafeNormal();
	if (PathLength < FMath::Max(T.MinimumPathLength, KINDA_SMALL_NUMBER)) { return Result; }
	const float Distance = Displacement.Size();
	const float TurnDegrees = FMath::RadiansToDegrees(FMath::Abs(SignedTurn));
	if (PathLength >= T.MinimumCirclePathLength && Distance / PathLength <= T.MaximumCircleClosureRatio
		&& TurnDegrees >= T.MinimumCircleTurnDegrees
		&& AbsoluteTurn > KINDA_SMALL_NUMBER && FMath::Abs(SignedTurn) / AbsoluteTurn >= T.MinimumCircleTurnConsistency)
	{
		Result.Gesture = EWEVORAGesture::Circle;
	}
	else if (Distance >= T.MinimumFlickDisplacement && Distance / PathLength >= T.MinimumFlickStraightness)
	{
		if (FMath::Abs(Displacement.X) >= FMath::Abs(Displacement.Y)) { Result.Gesture = EWEVORAGesture::Sweep; }
		else { Result.Gesture = Displacement.Y > 0.0f ? EWEVORAGesture::Thrust : EWEVORAGesture::Slam; }
	}
	return Result;
}
