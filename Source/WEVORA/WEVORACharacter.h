// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "WEVORACharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * WEVORA player character.
 * Built around a hover / glide movement model rather than conventional ground locomotion.
 */
UCLASS(abstract)
class AWEVORACharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

protected:

	/** Existing jump action is reused as Ascend for the movement prototype. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Optional Descend action. Falls back to Left Ctrl when not assigned. */
	UPROPERTY(EditAnywhere, Category="Input|WEVORA Movement")
	UInputAction* DescendAction;

	/** Optional Burst action. Falls back to Left Shift when not assigned. */
	UPROPERTY(EditAnywhere, Category="Input|WEVORA Movement")
	UInputAction* BurstAction;

	/** Optional Brake action. Falls back to Left Alt when not assigned. */
	UPROPERTY(EditAnywhere, Category="Input|WEVORA Movement")
	UInputAction* BrakeAction;

	/** Enable the ground-aware hover spring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover")
	bool bHoverEnabled = true;

	/** Desired gap between the capsule bottom and the surface below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(ClampMin="0.0"))
	float HoverHeight = 55.0f;

	/** Extra distance below the desired hover height used to search for ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(ClampMin="0.0"))
	float HoverTraceExtraDistance = 180.0f;

	/** Spring response toward HoverHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(ClampMin="0.0"))
	float HoverSpringStrength = 18.0f;

	/** Vertical damping applied by the hover spring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(ClampMin="0.0"))
	float HoverSpringDamping = 6.5f;

	/** Maximum vertical acceleration the hover spring may apply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(ClampMin="0.0"))
	float MaxHoverAcceleration = 2200.0f;

	/** Base free-flight speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Glide", meta=(ClampMin="0.0"))
	float CruiseSpeed = 1200.0f;

	/** Acceleration while steering. Higher values make input feel more immediate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Glide", meta=(ClampMin="0.0"))
	float GlideAcceleration = 2100.0f;

	/** Deceleration when no planar input is held. Low values preserve inertia. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Glide", meta=(ClampMin="0.0"))
	float GlideBrakingDeceleration = 220.0f;

	/** Vertical input strength while ascending / descending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float VerticalInputScale = 0.85f;

	/** How quickly free vertical motion settles after releasing vertical input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float VerticalVelocityDamping = 2.2f;

	/** Instant planar speed added by Burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstImpulse = 950.0f;

	/** Maximum planar speed immediately after Burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstMaxSpeed = 2200.0f;

	/** Seconds before another Burst may be triggered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstCooldown = 0.45f;

	/** Velocity interpolation strength while Brake is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Brake", meta=(ClampMin="0.0"))
	float BrakeInterpSpeed = 4.5f;

	/** Camera FOV at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Camera", meta=(ClampMin="5.0", ClampMax="170.0"))
	float BaseCameraFOV = 90.0f;

	/** Additional FOV at BurstMaxSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Camera", meta=(ClampMin="0.0"))
	float SpeedFOVBoost = 12.0f;

	/** FOV interpolation speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Camera", meta=(ClampMin="0.0"))
	float CameraFOVInterpSpeed = 5.0f;

	/** True while ascend input is held. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	bool bAscending = false;

	/** True while descend input is held. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	bool bDescending = false;

	/** True while brake input is held. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	bool bBraking = false;

	/** Last requested planar direction, used by Burst. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	FVector LastPlanarInputDirection = FVector::ForwardVector;

	/** Time of the most recent burst. */
	float LastBurstTime = -1000.0f;

public:

	/** Constructor */
	AWEVORACharacter();

	/** Update hover, vertical control, braking and camera feel. */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Ground-aware hover force. */
	void UpdateHover(float DeltaSeconds);

	/** Manual up/down movement and vertical damping. */
	void UpdateVerticalMovement(float DeltaSeconds);

	/** Strong deceleration while Brake is held. */
	void UpdateBrake(float DeltaSeconds);

	/** Speed-reactive camera FOV. */
	void UpdateCameraFeel(float DeltaSeconds);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Begin ascending. Existing Jump input routes here. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoAscendStart();

	/** Stop ascending. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoAscendEnd();

	/** Begin descending. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoDescendStart();

	/** Stop descending. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoDescendEnd();

	/** Trigger an impulse in the current steering / travel direction. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoBurst();

	/** Begin rapid braking. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoBrakeStart();

	/** Stop rapid braking. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoBrakeEnd();

	/** Compatibility helpers retained for existing Blueprint/UI callers. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
