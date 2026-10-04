#pragma once

#include "CoreMinimal.h"
#include "WEVORASpellTypes.generated.h"

UENUM(BlueprintType)
enum class EWEVORASpellElement : uint8 { Fire, Wind };

UENUM(BlueprintType)
enum class EWEVORAGesture : uint8 { None, Thrust, Sweep, Circle, Slam };

UENUM(BlueprintType)
enum class EWEVORAWeavingState : uint8 { Idle, Weaving, Shaping, ReadyToCast };

/** Input-space measurements, independent of movement and eventual spell effects. */
USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Spell")
	EWEVORASpellElement Element = EWEVORASpellElement::Fire;
	UPROPERTY(BlueprintReadOnly, Category="Spell")
	EWEVORAGesture Gesture = EWEVORAGesture::None;
	/** Normalized screen-space displacement: X right, Y up. Circle may have zero displacement. */
	UPROPERTY(BlueprintReadOnly, Category="Spell")
	FVector2D GestureDirection = FVector2D::ZeroVector;
	/** Total path length in Look input units. */
	UPROPERTY(BlueprintReadOnly, Category="Spell")
	float GestureMagnitude = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Spell")
	float GestureDuration = 0.0f;
};

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORAGestureThresholds
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.001"))
	float MinimumPathLength = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.001"))
	float MinimumFlickDisplacement = 6.0f;
	/** Displacement / path length. Allows imperfect flicks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumFlickStraightness = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.001"))
	float MinimumCirclePathLength = 20.0f;
	/** Endpoint distance / path length; a loose loop need not close perfectly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaximumCircleClosureRatio = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.0", ClampMax="360.0"))
	float MinimumCircleTurnDegrees = 220.0f;
	/** Net signed turn / total absolute turn, rejecting back-and-forth scribbles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumCircleTurnConsistency = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gesture", meta=(ClampMin="0.001"))
	float MinimumSampleDistance = 0.1f;
};
