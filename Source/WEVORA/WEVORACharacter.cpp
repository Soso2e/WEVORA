// Copyright Epic Games, Inc. All Rights Reserved.

#include "WEVORACharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "WEVORA.h"
#include "AI/WEVORAHealthComponent.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellSelectionEffectComponent.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Movement/WEVORAFlightMovementComponent.h"
#include "Movement/WEVORAManaComponent.h"
#include "Engine/Engine.h"

AWEVORACharacter::AWEVORACharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UWEVORAFlightMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	HealthComponent = CreateDefaultSubobject<UWEVORAHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->bShowDamageFeedback = true;
	ManaComponent = CreateDefaultSubobject<UWEVORAManaComponent>(TEXT("ManaComponent"));
	SpellWeavingComponent = CreateDefaultSubobject<UWEVORASpellWeavingComponent>(TEXT("SpellWeavingComponent"));
	SpellCastComponent = CreateDefaultSubobject<UWEVORASpellCastComponent>(TEXT("SpellCastComponent"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	SpellSelectionEffectComponent = CreateDefaultSubobject<UWEVORASpellSelectionEffectComponent>(TEXT("SpellSelectionEffectComponent"));
	SpellSelectionEffectComponent->SetupAttachment(GetMesh(), TEXT("hand_r"));

	// Keep the character upright. Rotation follows planar travel while the camera remains independent.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 620.0f, 0.0f);

	// WEVORA movement intentionally keeps a little inertia.
	Movement->GravityScale = FallGravityScale;
	Movement->AirControl = 1.0f;
	Movement->AirControlBoostMultiplier = 0.0f;
	Movement->MaxAcceleration = GlideAcceleration;
	Movement->MaxFlySpeed = CruiseSpeed;
	Movement->BrakingDecelerationFlying = GlideBrakingDeceleration;
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = 0.25f;
	Movement->BrakingFrictionFactor = 1.0f;

	// Retain sensible walking values in case a future gameplay state temporarily returns to ground mode.
	Movement->JumpZVelocity = JumpLaunchSpeed;
	Movement->MaxWalkSpeed = GroundSpeed;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = GlideBrakingDeceleration;
	Movement->FallingLateralFriction = 0.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// A small amount of camera lag sells the sense that the body is gliding through space.
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.0f;
	CameraBoom->CameraLagMaxDistance = 80.0f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 13.0f;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AWEVORACharacter::UnPossessed()
{
	SpellWeavingComponent->CancelWeave();
	ResetFlightInput();
	Super::UnPossessed();
}

void AWEVORACharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = FallGravityScale;
		Movement->AirControl = 1.0f;
		Movement->AirControlBoostMultiplier = 0.0f;
		Movement->FallingLateralFriction = 0.0f;
		Movement->BrakingFriction = 0.0f;
		Movement->MaxAcceleration = GlideAcceleration;
		Movement->MaxFlySpeed = CruiseSpeed;
		Movement->MaxWalkSpeed = GroundSpeed;
		Movement->JumpZVelocity = JumpLaunchSpeed;
		Movement->BrakingDecelerationFalling = GlideBrakingDeceleration;
		Movement->SetMovementMode(MOVE_Walking);
	}
	AirSpeedLimit = CruiseSpeed;

	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(BaseCameraFOV);
	}
}

void AWEVORACharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCameraFeel(DeltaSeconds);
	if (bShowFlightFeedback && IsLocallyControlled() && GEngine)
	{
		const UEnum* StateEnum = StaticEnum<EWEVORAFlightState>();
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.1f,
			ManaComponent->Mana > 0.0f ? FColor::Cyan : FColor::Orange,
			FString::Printf(TEXT("Mana: %.0f / %.0f | %s"), ManaComponent->Mana, ManaComponent->MaxMana,
				*StateEnum->GetNameStringByValue(static_cast<int64>(FlightState))));
	}
}

void AWEVORACharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AWEVORACharacter::DoRecenterView).bConsumeInput = false;
	// Asset-free fallback; Blueprint/Enhanced Input may also call the component API.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, SpellWeavingComponent, &UWEVORASpellWeavingComponent::BeginWeave).bConsumeInput = false;
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, SpellWeavingComponent, &UWEVORASpellWeavingComponent::ReleaseWeave).bConsumeInput = false;
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, SpellWeavingComponent, &UWEVORASpellWeavingComponent::CycleElement).bConsumeInput = false;
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, SpellWeavingComponent, &UWEVORASpellWeavingComponent::BeginShape).bConsumeInput = false;
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, SpellWeavingComponent, &UWEVORASpellWeavingComponent::EndShape).bConsumeInput = false;
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Tap jumps; a held input takes over as paid ascent after a short delay.
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoAscendStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AWEVORACharacter::DoAscendEnd);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AWEVORACharacter::DoAscendEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWEVORACharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AWEVORACharacter::MoveEnded);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AWEVORACharacter::MoveEnded);

		// Looking
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AWEVORACharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWEVORACharacter::Look);

		// Optional Enhanced Input actions. These can be assigned later in the Character Blueprint.
		if (DescendAction)
		{
			EnhancedInputComponent->BindAction(DescendAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoDescendStart);
			EnhancedInputComponent->BindAction(DescendAction, ETriggerEvent::Completed, this, &AWEVORACharacter::DoDescendEnd);
			EnhancedInputComponent->BindAction(DescendAction, ETriggerEvent::Canceled, this, &AWEVORACharacter::DoDescendEnd);
		}
		else
		{
			PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed, this, &AWEVORACharacter::DoDescendStart);
			PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Released, this, &AWEVORACharacter::DoDescendEnd);
		}

		if (BurstAction)
		{
			EnhancedInputComponent->BindAction(BurstAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoBurstStart);
			EnhancedInputComponent->BindAction(BurstAction, ETriggerEvent::Completed, this, &AWEVORACharacter::DoBurstEnd);
			EnhancedInputComponent->BindAction(BurstAction, ETriggerEvent::Canceled, this, &AWEVORACharacter::DoBurstEnd);
		}
		else
		{
			PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AWEVORACharacter::DoBurstStart);
			PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AWEVORACharacter::DoBurstEnd);
		}

		if (BrakeAction)
		{
			EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoBrakeStart);
			EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &AWEVORACharacter::DoBrakeEnd);
			EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Canceled, this, &AWEVORACharacter::DoBrakeEnd);
		}
		else
		{
			PlayerInputComponent->BindKey(EKeys::LeftAlt, IE_Pressed, this, &AWEVORACharacter::DoBrakeStart);
			PlayerInputComponent->BindKey(EKeys::LeftAlt, IE_Released, this, &AWEVORACharacter::DoBrakeEnd);
		}
	}
	else
	{
		UE_LOG(LogWEVORA, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system."), *GetNameSafe(this));
	}
}

void AWEVORACharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AWEVORACharacter::MoveEnded(const FInputActionValue& Value)
{
	DoMove(0.0f, 0.0f);
}

void AWEVORACharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AWEVORACharacter::DoMove(float Right, float Forward)
{
	PlanarInputMagnitude = 0.0f;
	PlanarInputDirection = FVector::ZeroVector;
	if (!GetController())
	{
		return;
	}

	const FRotator Rotation = GetController()->GetControlRotation();
	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	FVector DesiredDirection = ForwardDirection * Forward + RightDirection * Right;
	const float InputMagnitude = FMath::Clamp(FVector2D(Right, Forward).Size(), 0.0f, 1.0f);

	if (!DesiredDirection.IsNearlyZero())
	{
		DesiredDirection.Normalize();
		LastPlanarInputDirection = DesiredDirection;
		PlanarInputDirection = DesiredDirection;
		PlanarInputMagnitude = InputMagnitude;
	}
}

void AWEVORACharacter::DoLook(float Yaw, float Pitch)
{
	SpellWeavingComponent->RecordLook(FVector2D(Yaw, Pitch));
	if (GetController())
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AWEVORACharacter::DoRecenterView()
{
	if (AController* ViewController = GetController())
	{
		// Set the view directly: recentering is not mouse input for spell gestures.
		ViewController->SetControlRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	}
}

void AWEVORACharacter::DoAscendStart()
{
	bAscending = true;
	AscendHeldTime = 0.0f;
	if (!bDescending && GetCharacterMovement()->IsMovingOnGround())
	{
		Jump();
	}
}

void AWEVORACharacter::DoAscendEnd()
{
	bAscending = false;
	AscendHeldTime = 0.0f;
	StopJumping();
}

void AWEVORACharacter::DoDescendStart()
{
	CancelSpellForEvasion();
	bDescending = true;
	bJumpLaunchPhase = false;
	StopJumping();
	if (GetCharacterMovement()->IsFalling())
	{
		GetCharacterMovement()->GravityScale = DiveGravityScale;
		GetCharacterMovement()->Velocity.Z = FMath::Min(GetCharacterMovement()->Velocity.Z, -DiveStartSpeed);
	}
}

void AWEVORACharacter::DoDescendEnd()
{
	bDescending = false;
}

void AWEVORACharacter::DoBurstStart()
{
	if (bBurstHeld) { return; }
	bBurstHeld = true;
	DoBurst();
}

void AWEVORACharacter::DoBurstEnd()
{
	bBurstHeld = false;
}

void AWEVORACharacter::DoBurst()
{
	CancelSpellForEvasion();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || !GetWorld() || !Movement->IsFalling() || bDescending || bBraking)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastBurstTime < BurstCooldown)
	{
		return;
	}
	if (ManaComponent->Mana <= 0.0f || !ManaComponent->ConsumeMana(FMath::Max(0.0f, BurstManaCost)))
	{
		return;
	}

	FVector BurstDirection = PlanarInputDirection.GetSafeNormal2D();

	if (BurstDirection.IsNearlyZero())
	{
		BurstDirection = Movement->Velocity.GetSafeNormal2D();
	}

	if (BurstDirection.IsNearlyZero())
	{
		if (GetController())
		{
			const FRotator ControlRotation = GetController()->GetControlRotation();
			BurstDirection = FRotationMatrix(FRotator(0.0f, ControlRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::X);
		}
		else
		{
			BurstDirection = GetActorForwardVector().GetSafeNormal2D();
		}
	}

	Movement->Velocity += BurstDirection * BurstImpulse;

	const float PlanarSpeed = Movement->Velocity.Size2D();
	if (PlanarSpeed > BurstMaxSpeed && PlanarSpeed > KINDA_SMALL_NUMBER)
	{
		const float Scale = BurstMaxSpeed / PlanarSpeed;
		Movement->Velocity.X *= Scale;
		Movement->Velocity.Y *= Scale;
	}

	LastBurstTime = CurrentTime;
	AirSpeedLimit = FMath::Max(CruiseSpeed, Movement->Velocity.Size2D());
	Movement->MaxWalkSpeed = AirSpeedLimit;
}

void AWEVORACharacter::DoBrakeStart()
{
	bBraking = true;
}

void AWEVORACharacter::DoBrakeEnd()
{
	bBraking = false;
}

void AWEVORACharacter::DoJumpStart()
{
	DoAscendStart();
}

void AWEVORACharacter::DoJumpEnd()
{
	DoAscendEnd();
}

void AWEVORACharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	bJumpLaunchPhase = true;
	FlightState = EWEVORAFlightState::Jumping;
}

void AWEVORACharacter::ResetFlightInput()
{
	bAscending = bDescending = bBraking = bBurstHeld = false;
	bJumpLaunchPhase = false;
	PlanarInputDirection = FVector::ZeroVector;
	PlanarInputMagnitude = AscendHeldTime = 0.0f;
	SpellMovementMultiplier = SpellMovementTarget = SpellMovementBlendStart = 1.0f;
	CastRecoveryRemaining = SpellMovementBlendElapsed = 0.0f;
	StopJumping();
	ConsumeMovementInputVector();
	GetCharacterMovement()->GravityScale = FallGravityScale;
}

void AWEVORACharacter::CancelSpellForEvasion()
{
	SpellWeavingComponent->CancelWeave();
	CastRecoveryRemaining = 0.0f;
}

void AWEVORACharacter::ApplySpellLaunchFeedback(const FVector& Direction, float RecoilSpeed)
{
	CastRecoveryRemaining = FMath::Max(0.0f, CastRecoveryDuration);
	// Recovery is a short impact; subsequent state changes blend back normally.
	if (CastRecoveryRemaining > 0.0f)
	{
		SpellMovementBlendStart = SpellMovementMultiplier;
		SpellMovementTarget = FMath::Clamp(CastRecoveryMultiplier, 0.0f, 1.0f);
		SpellMovementBlendElapsed = 0.0f;
	}
	// Preserve gravity and ascent/dive velocity while adding a small planar kick.
	GetCharacterMovement()->Velocity -= FVector(Direction.X, Direction.Y, 0.0f) * FMath::Max(0.0f, RecoilSpeed);
}

void AWEVORACharacter::UpdateSpellMovement(float DeltaSeconds)
{
	if (bDescending && SpellWeavingComponent->State != EWEVORAWeavingState::Idle)
	{
		CancelSpellForEvasion();
	}
	float Target = 1.0f;
	switch (SpellWeavingComponent->State)
	{
	case EWEVORAWeavingState::Weaving: Target = WeavingMovementMultiplier; break;
	case EWEVORAWeavingState::Shaping: Target = ShapingMovementMultiplier; break;
	case EWEVORAWeavingState::ReadyToCast: Target = ReadyMovementMultiplier; break;
	default: break;
	}
	// Account for a frame that crosses the end of recovery without extending the hold.
	const float RecoveryStep = FMath::Min(DeltaSeconds, CastRecoveryRemaining);
	CastRecoveryRemaining -= RecoveryStep;
	if (RecoveryStep > 0.0f)
	{
		SpellMovementBlendElapsed += RecoveryStep;
		// Reach the recovery penalty quickly, smoothly, then hold until recovery ends.
		const float RecoveryAlpha = FMath::Clamp(SpellMovementBlendElapsed /
			FMath::Max(0.001f, FMath::Min(0.04f, CastRecoveryDuration)), 0.0f, 1.0f);
		SpellMovementMultiplier = FMath::Lerp(SpellMovementBlendStart, SpellMovementTarget,
			FMath::SmoothStep(0.0f, 1.0f, RecoveryAlpha));
	}
	const float BlendStep = DeltaSeconds - RecoveryStep;
	if (BlendStep <= 0.0f) { return; }
	Target = FMath::Clamp(Target, 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(Target, SpellMovementTarget))
	{
		SpellMovementTarget = Target;
		SpellMovementBlendStart = SpellMovementMultiplier;
		SpellMovementBlendElapsed = 0.0f;
	}
	SpellMovementBlendElapsed += BlendStep;
	const float Alpha = FMath::Clamp(SpellMovementBlendElapsed / FMath::Max(0.01f, SpellMovementBlendDuration), 0.0f, 1.0f);
	SpellMovementMultiplier = FMath::Lerp(SpellMovementBlendStart, Target, FMath::SmoothStep(0.0f, 1.0f, Alpha));
}

void AWEVORACharacter::UpdateFlightBeforeMovement(float DeltaSeconds)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Falling mode retains Unreal's swept collision, ceiling response, floor detection and landing.
	// Only vertical support changes; planar speed is independent of climb/dive speed.
	UpdateSpellMovement(DeltaSeconds);
	Movement->JumpZVelocity = FMath::Max(0.0f, JumpLaunchSpeed);
	Movement->BrakingDecelerationFalling = FMath::Max(0.0f, GlideBrakingDeceleration);
	const bool bGrounded = Movement->IsMovingOnGround();
	Movement->MaxAcceleration = FMath::Max(0.0f, bGrounded ? GlideAcceleration : AirSteeringAcceleration) * SpellMovementMultiplier;
	ManaComponent->UpdateRecovery(DeltaSeconds, bGrounded);
	AscendHeldTime = bAscending ? AscendHeldTime + DeltaSeconds : 0.0f;
	AirSpeedLimit = FMath::FInterpConstantTo(AirSpeedLimit, FMath::Max(0.0f, CruiseSpeed),
		DeltaSeconds, FMath::Max(0.0f, BurstSpeedDecay));
	// Scale cruise steering but keep the existing burst reserve for evasive movement.
	Movement->MaxWalkSpeed = bGrounded ? FMath::Max(0.0f, GroundSpeed) * SpellMovementMultiplier :
		FMath::Max(0.0f, CruiseSpeed) * SpellMovementMultiplier + FMath::Max(0.0f, AirSpeedLimit - CruiseSpeed);
	Movement->MaxFlySpeed = FMath::Max(0.0f, CruiseSpeed) * SpellMovementMultiplier;

	const bool bSteering = PlanarInputMagnitude > KINDA_SMALL_NUMBER;
	if (bSteering && !bBraking)
	{
		AddMovementInput(PlanarInputDirection, PlanarInputMagnitude);
	}
	UpdateBrake(DeltaSeconds);

	if (bGrounded)
	{
		FlightState = EWEVORAFlightState::Grounded;
		Movement->GravityScale = FallGravityScale;
		bJumpLaunchPhase = false;
		AirSpeedLimit = FMath::Max(0.0f, CruiseSpeed);
		return;
	}

	if (!Movement->IsFalling()) { return; }
	if (bDescending)
	{
		FlightState = EWEVORAFlightState::Diving;
		Movement->GravityScale = FMath::Max(FallGravityScale, DiveGravityScale);
		bJumpLaunchPhase = false;
		return;
	}

	const bool bWantsAscent = bAscending && AscendHeldTime >= AscendHoldDelay;
	// Do not erase a tap jump's upward impulse when WASD is already held.
	if (bJumpLaunchPhase && Movement->Velocity.Z > 120.0f && !bWantsAscent && !bBraking && !bBurstHeld)
	{
		FlightState = EWEVORAFlightState::Jumping;
		Movement->GravityScale = FMath::Max(0.0f, FallGravityScale);
		return;
	}
	bJumpLaunchPhase = false;

	const bool bWantsSupport = bHoverEnabled && (bWantsAscent || bBraking || bBurstHeld || bSteering);
	const float ManaRate = FMath::Max(0.0f, bWantsAscent ? AscendManaPerSecond : HoverManaPerSecond);
	if (bWantsSupport && ManaComponent->Mana > 0.0f && ManaComponent->ConsumeMana(ManaRate * DeltaSeconds))
	{
		FlightState = bWantsAscent ? EWEVORAFlightState::Ascending : EWEVORAFlightState::Hovering;
		Movement->GravityScale = 0.0f;
		const float TargetZ = bWantsAscent ? FMath::Max(0.0f, AscendSpeed) * SpellMovementMultiplier : 0.0f;
		const float Response = 1.0f - FMath::Exp(-FMath::Max(0.0f, VerticalVelocityDamping) * DeltaSeconds);
		const float MaxDelta = FMath::Max(0.0f, MaxHoverAcceleration) * DeltaSeconds *
			(bWantsAscent ? SpellMovementMultiplier : 1.0f);
		Movement->Velocity.Z += FMath::Clamp((TargetZ - Movement->Velocity.Z) * Response, -MaxDelta, MaxDelta);
		return;
	}

	// Releasing all support input (or exhausting mana) restores normal gravity immediately.
	// Residual horizontal velocity alone never grants passive altitude support.
	Movement->GravityScale = FMath::Max(0.0f, FallGravityScale);
	FlightState = EWEVORAFlightState::Falling;
}

void AWEVORACharacter::UpdateBrake(float DeltaSeconds)
{
	if (!bBraking)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		const float RetainedSpeed = FMath::Exp(-FMath::Max(0.0f, BrakeInterpSpeed) * DeltaSeconds);
		Movement->Velocity.X *= RetainedSpeed;
		Movement->Velocity.Y *= RetainedSpeed;
	}
}

void AWEVORACharacter::UpdateCameraFeel(float DeltaSeconds)
{
	if (!FollowCamera)
	{
		return;
	}

	const float ReferenceSpeed = FMath::Max(BurstMaxSpeed, 1.0f);
	const float SpeedRatio = FMath::Clamp(GetVelocity().Size() / ReferenceSpeed, 0.0f, 1.0f);
	const float TargetFOV = BaseCameraFOV + SpeedFOVBoost * SpeedRatio;
	const float NewFOV = FMath::FInterpTo(
		FollowCamera->FieldOfView,
		TargetFOV,
		DeltaSeconds,
		CameraFOVInterpSpeed);

	FollowCamera->SetFieldOfView(NewFOV);
}
