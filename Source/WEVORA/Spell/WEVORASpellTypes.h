#pragma once

#include "CoreMinimal.h"
#include "WEVORASpellTypes.generated.h"

UENUM(BlueprintType)
enum class EWEVORASpellElement : uint8 { Fire, Wind, Lightning };

UENUM(BlueprintType)
enum class EWEVORAGesture : uint8 { None, Thrust, Sweep, Circle, Slam };

UENUM(BlueprintType)
enum class EWEVORAWeavingState : uint8 { Idle, Weaving, Shaping, ReadyToCast };

/** Gesture describes input; Shape describes the spell in world space. */
UENUM(BlueprintType)
enum class EWEVORASpellShape : uint8 { Forward, Sweep, Circle, Slam };

UENUM(BlueprintType, meta=(ScriptName="WEVORASpellDeliveryKind"))
enum class EWEVORASpellDelivery : uint8 { Projectile, Sweep, Area, Field };

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EWEVORASpellElement Element = EWEVORASpellElement::Fire;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EWEVORASpellShape Shape = EWEVORASpellShape::Forward;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EWEVORASpellDelivery Delivery = EWEVORASpellDelivery::Projectile;
	/** Dimensionless multiplier for target reaction magnitudes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="0"))
	float Power = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	FVector Direction = FVector::ForwardVector;
};

USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellShapeProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EWEVORASpellShape Shape = EWEVORASpellShape::Forward;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EWEVORASpellDelivery Delivery = EWEVORASpellDelivery::Projectile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="0.01"))
	float SpeedMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="0.01"))
	float RadiusMultiplier = 1.0f;
	/** Sweeps use Range/Width/Height; Field/Area use Width as their radius. cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Volume", meta=(ClampMin="1"))
	float Range = 900;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Volume", meta=(ClampMin="1"))
	float Width = 600;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Volume", meta=(ClampMin="1"))
	float Height = 180;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Volume", meta=(ClampMin="0.05"))
	float Duration = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Volume", meta=(ClampMin="0.1"))
	float PulseInterval = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Volume", meta=(ClampMin="0.01"))
	float PowerMultiplier = 1;
	/** Extra reward for a drawn gesture; normal LMB has zero gesture magnitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Weaving", meta=(ClampMin="0.01"))
	float WovenSpeedMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell|Weaving", meta=(ClampMin="0.01"))
	float WovenPowerMultiplier = 1;
};

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
	/** Normal attack charge, independent of gesture recording duration. */
	UPROPERTY(BlueprintReadOnly, Category="Spell")
	float ChargeDuration = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Spell")
	float ChargeAlpha = 0.0f;
};

/** Resolved runtime behaviour, separate from gesture recognition and named spells. */
USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellProjectileParameters
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="1"))
	float Speed = 2200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="1"))
	float Radius = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="0.1"))
	float Lifetime = 4.0f;
	/** Zero preserves legacy lifetime-only range; positive values cap straight travel distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell", meta=(ClampMin="0", Units="cm"))
	float MaxDistance = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	FLinearColor Color = FLinearColor(1.0f, 0.15f, 0.02f);
};

/** Immutable cast snapshot: composition + world-space direction + resolved behaviour. */
USTRUCT(BlueprintType)
struct WEVORA_API FWEVORASpellLaunch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="Spell")
	FWEVORASpellContext Composition;
	UPROPERTY(BlueprintReadWrite, Category="Spell")
	FWEVORASpellData Spell;
	UPROPERTY(BlueprintReadWrite, Category="Spell")
	FVector Direction = FVector::ForwardVector;
	/** Shooter planar velocity contribution captured before recoil. */
	UPROPERTY(BlueprintReadWrite, Category="Spell")
	FVector InheritedVelocity = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category="Spell")
	FWEVORASpellProjectileParameters Parameters;
	UPROPERTY(BlueprintReadWrite, Category="Spell|Debug")
	bool bDebugTrajectory = false;
	/** Captured geometry/timing, used only by non-projectile delivery implementations. */
	UPROPERTY(BlueprintReadWrite, Category="Spell")
	FWEVORASpellShapeProfile Volume;
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
