// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "WEVORACharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UWEVORASpellWeavingComponent;
class UWEVORASpellSelectionEffectComponent;
class UWEVORASpellCastComponent;
class UWEVORAHealthComponent;
class UWEVORAManaComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UENUM(BlueprintType)
enum class EWEVORAFlightState : uint8
{
	Grounded,
	Jumping,
	Coasting,
	Hovering,
	Ascending,
	Diving,
	Falling
};

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

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	UWEVORAHealthComponent* HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spell")
	UWEVORASpellWeavingComponent* SpellWeavingComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spell")
	UWEVORASpellSelectionEffectComponent* SpellSelectionEffectComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spell")
	UWEVORASpellCastComponent* SpellCastComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mana")
	UWEVORAManaComponent* ManaComponent;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	EWEVORAFlightState FlightState = EWEVORAFlightState::Grounded;

	/** Called by the movement component before physics, never from the actor's post-movement Tick. */
	void UpdateFlightBeforeMovement(float DeltaSeconds);

	/** Called only after the cast component successfully launches a projectile. */
	void ApplySpellLaunchFeedback(const FVector& Direction, float RecoilSpeed);

protected:

	/** Tap to jump; hold to ascend using mana. */
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

	/** Allow mana-powered altitude support; gravity still applies when support is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover")
	bool bHoverEnabled = true;

	/** Maximum acceleration when catching a fall or changing powered climb speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(ClampMin="0.0"))
	float MaxHoverAcceleration = 2200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(ClampMin="0.0"))
	float HoverManaPerSecond = 8.0f;
	/** Legacy Blueprint fields retained for load compatibility; input release now restores gravity immediately. */
	UPROPERTY(BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(DeprecatedProperty, DeprecationMessage="Input release now restores normal gravity immediately."))
	float SteeringGraceDuration = 0.2f;
	UPROPERTY(BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(DeprecatedProperty, DeprecationMessage="Passive float has been replaced by input-driven hover."))
	float PassiveFloatDuration = 1.0f;
	UPROPERTY(BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(DeprecatedProperty, DeprecationMessage="Passive float has been replaced by input-driven hover."))
	float PassiveGravityScale = 0.12f;
	UPROPERTY(BlueprintReadWrite, Category="WEVORA Movement|Hover", meta=(DeprecatedProperty, DeprecationMessage="Input release now restores normal gravity immediately."))
	float GravityReturnDuration = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Ground", meta=(ClampMin="0.0"))
	float GroundSpeed = 500.0f;

	/** Base free-flight speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Glide", meta=(ClampMin="0.0"))
	float CruiseSpeed = 1200.0f;

	/** Ground steering acceleration. Retained for existing Blueprint settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Glide", meta=(ClampMin="0.0"))
	float GlideAcceleration = 2100.0f;
	/** Air steering acceleration in cm/s^2, independent of ground acceleration and dash impulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Glide", meta=(ClampMin="0.0"))
	float AirSteeringAcceleration = 550.0f;

	/** Deceleration when no planar input is held. Low values preserve inertia. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Glide", meta=(ClampMin="0.0"))
	float GlideBrakingDeceleration = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float JumpLaunchSpeed = 900.0f;
	/** Separates a free tap jump from a paid held ascent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float AscendHoldDelay = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float AscendSpeed = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float AscendManaPerSecond = 18.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float FallGravityScale = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float DiveGravityScale = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float DiveStartSpeed = 300.0f;

	/** Damping response while powered flight approaches its target vertical speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Vertical", meta=(ClampMin="0.0"))
	float VerticalVelocityDamping = 4.5f;

	/** Instant planar speed added by Burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstImpulse = 1800.0f;

	/** Maximum planar speed immediately after Burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstMaxSpeed = 3200.0f;
	/** Air speed ceiling relaxes toward cruise at this rate; no instant post-burst clamp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstSpeedDecay = 650.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstManaCost = 12.0f;

	/** Seconds before another Burst may be triggered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Burst", meta=(ClampMin="0.0"))
	float BurstCooldown = 0.45f;

	/** Velocity interpolation strength while Brake is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Brake", meta=(ClampMin="0.0"))
	float BrakeInterpSpeed = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Spell", meta=(ClampMin="0", ClampMax="1"))
	float WeavingMovementMultiplier = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Spell", meta=(ClampMin="0", ClampMax="1"))
	float ShapingMovementMultiplier = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Spell", meta=(ClampMin="0", ClampMax="1"))
	float ReadyMovementMultiplier = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Spell", meta=(ClampMin="0.01"))
	float SpellMovementBlendDuration = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Spell", meta=(ClampMin="0"))
	float CastRecoveryDuration = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Spell", meta=(ClampMin="0", ClampMax="1"))
	float CastRecoveryMultiplier = 0.35f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	float SpellMovementMultiplier = 1.0f;
	float CastRecoveryRemaining = 0.0f;
	float SpellMovementTarget = 1.0f;
	float SpellMovementBlendStart = 1.0f;
	float SpellMovementBlendElapsed = 0.0f;
	void UpdateSpellMovement(float DeltaSeconds);
	void CancelSpellForEvasion();

	/** Camera FOV at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Camera", meta=(ClampMin="5.0", ClampMax="170.0"))
	float BaseCameraFOV = 90.0f;

	/** Additional FOV at BurstMaxSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Camera", meta=(ClampMin="0.0"))
	float SpeedFOVBoost = 12.0f;

	/** FOV interpolation speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Camera", meta=(ClampMin="0.0"))
	float CameraFOVInterpSpeed = 5.0f;
	/** Temporary resource/state feedback, replaceable by a Blueprint HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WEVORA Movement|Feedback")
	bool bShowFlightFeedback = true;

	/** True while ascend input is held. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	bool bAscending = false;

	/** True while descend input is held. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	bool bDescending = false;

	/** True while brake input is held. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	bool bBraking = false;
	/** Shift hold supports altitude without Alt's planar braking. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	bool bBurstHeld = false;

	/** Last steering direction, retained for Blueprint feedback. Burst uses current input or velocity. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WEVORA Movement|State")
	FVector LastPlanarInputDirection = FVector::ForwardVector;

	/** Time of the most recent burst. */
	float LastBurstTime = -1000.0f;
	FVector PlanarInputDirection = FVector::ZeroVector;
	float PlanarInputMagnitude = 0.0f;
	float AscendHeldTime = 0.0f;
	float AirSpeedLimit = 1200.0f;
	bool bJumpLaunchPhase = false;

public:

	/** Constructor */
	AWEVORACharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Cancel unfinished spell input when control leaves this pawn. */
	virtual void UnPossessed() override;

	/** Update camera and feedback after movement. */
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnJumped_Implementation() override;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);
	void MoveEnded(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Planar braking never suppresses gravity or a Ctrl dive. */
	void UpdateBrake(float DeltaSeconds);
	void ResetFlightInput();

	/** Speed-reactive camera FOV. */
	void UpdateCameraFeel(float DeltaSeconds);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Recenter the camera behind the character, looking horizontally forward. */
	UFUNCTION(BlueprintCallable, Category="Input|Camera")
	virtual void DoRecenterView();

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

	/** Dash once on press, then support altitude for the duration of the hold. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoBurstStart();

	/** Release dash hover without clearing Alt, Space or steering input. */
	UFUNCTION(BlueprintCallable, Category="WEVORA Movement")
	virtual void DoBurstEnd();

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
