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
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellSelectionEffectComponent.h"

AWEVORACharacter::AWEVORACharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SpellWeavingComponent = CreateDefaultSubobject<UWEVORASpellWeavingComponent>(TEXT("SpellWeavingComponent"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	SpellSelectionEffectComponent = CreateDefaultSubobject<UWEVORASpellSelectionEffectComponent>(TEXT("SpellSelectionEffectComponent"));
	SpellSelectionEffectComponent->SetupAttachment(GetMesh());
	SpellSelectionEffectComponent->SetRelativeLocation(FVector(30.0f, 0.0f, 110.0f));

	// Keep the character upright. Rotation follows planar travel while the camera remains independent.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 620.0f, 0.0f);

	// WEVORA movement intentionally keeps a little inertia.
	Movement->GravityScale = 0.0f;
	Movement->AirControl = 1.0f;
	Movement->MaxAcceleration = GlideAcceleration;
	Movement->MaxFlySpeed = CruiseSpeed;
	Movement->BrakingDecelerationFlying = GlideBrakingDeceleration;
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = 0.25f;
	Movement->BrakingFrictionFactor = 1.0f;

	// Retain sensible walking values in case a future gameplay state temporarily returns to ground mode.
	Movement->JumpZVelocity = 500.f;
	Movement->MaxWalkSpeed = 500.f;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = 1500.0f;

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
	Super::UnPossessed();
}

void AWEVORACharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = 0.0f;
		Movement->MaxAcceleration = GlideAcceleration;
		Movement->MaxFlySpeed = CruiseSpeed;
		Movement->BrakingDecelerationFlying = GlideBrakingDeceleration;
		Movement->SetMovementMode(MOVE_Flying);
	}

	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(BaseCameraFOV);
	}
}

void AWEVORACharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// Keep live-tweakable Blueprint values reflected in the movement component.
	Movement->MaxAcceleration = GlideAcceleration;
	Movement->MaxFlySpeed = CruiseSpeed;
	Movement->BrakingDecelerationFlying = GlideBrakingDeceleration;

	UpdateVerticalMovement(DeltaSeconds);
	UpdateHover(DeltaSeconds);
	UpdateBrake(DeltaSeconds);
	UpdateCameraFeel(DeltaSeconds);
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
		// The original template jump input becomes WEVORA's ascend control.
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoAscendStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AWEVORACharacter::DoAscendEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWEVORACharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AWEVORACharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWEVORACharacter::Look);

		// Optional Enhanced Input actions. These can be assigned later in the Character Blueprint.
		if (DescendAction)
		{
			EnhancedInputComponent->BindAction(DescendAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoDescendStart);
			EnhancedInputComponent->BindAction(DescendAction, ETriggerEvent::Completed, this, &AWEVORACharacter::DoDescendEnd);
		}
		else
		{
			PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed, this, &AWEVORACharacter::DoDescendStart);
			PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Released, this, &AWEVORACharacter::DoDescendEnd);
		}

		if (BurstAction)
		{
			EnhancedInputComponent->BindAction(BurstAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoBurst);
		}
		else
		{
			PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AWEVORACharacter::DoBurst);
		}

		if (BrakeAction)
		{
			EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &AWEVORACharacter::DoBrakeStart);
			EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &AWEVORACharacter::DoBrakeEnd);
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

void AWEVORACharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AWEVORACharacter::DoMove(float Right, float Forward)
{
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
		AddMovementInput(DesiredDirection, InputMagnitude);
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
	bDescending = false;
}

void AWEVORACharacter::DoAscendEnd()
{
	bAscending = false;
}

void AWEVORACharacter::DoDescendStart()
{
	bDescending = true;
	bAscending = false;
}

void AWEVORACharacter::DoDescendEnd()
{
	bDescending = false;
}

void AWEVORACharacter::DoBurst()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || !GetWorld())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastBurstTime < BurstCooldown)
	{
		return;
	}

	FVector BurstDirection = LastPlanarInputDirection.GetSafeNormal2D();

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

void AWEVORACharacter::UpdateVerticalMovement(float DeltaSeconds)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	float VerticalInput = 0.0f;
	if (bAscending)
	{
		VerticalInput += 1.0f;
	}
	if (bDescending)
	{
		VerticalInput -= 1.0f;
	}

	if (!FMath::IsNearlyZero(VerticalInput))
	{
		AddMovementInput(FVector::UpVector, VerticalInput * VerticalInputScale);
	}
	else
	{
		// Preserve some rise/fall momentum, then gently settle instead of stopping instantly.
		Movement->Velocity.Z = FMath::FInterpTo(
			Movement->Velocity.Z,
			0.0f,
			DeltaSeconds,
			VerticalVelocityDamping);
	}
}

void AWEVORACharacter::UpdateHover(float DeltaSeconds)
{
	if (!bHoverEnabled || bAscending || bDescending || !GetWorld())
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector TraceStart = GetActorLocation();
	const float TraceLength = CapsuleHalfHeight + HoverHeight + HoverTraceExtraDistance;
	const FVector TraceEnd = TraceStart - FVector::UpVector * TraceLength;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WEVORAHoverTrace), false, this);
	FHitResult Hit;

	if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return;
	}

	const float GroundGap = FMath::Max(0.0f, Hit.Distance - CapsuleHalfHeight);
	const float HeightError = HoverHeight - GroundGap;

	const float RequestedAcceleration =
		(HeightError * HoverSpringStrength) -
		(Movement->Velocity.Z * HoverSpringDamping);

	const float HoverAcceleration = FMath::Clamp(
		RequestedAcceleration,
		-MaxHoverAcceleration,
		MaxHoverAcceleration);

	Movement->Velocity.Z += HoverAcceleration * DeltaSeconds;
}

void AWEVORACharacter::UpdateBrake(float DeltaSeconds)
{
	if (!bBraking)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->Velocity = FMath::VInterpTo(
			Movement->Velocity,
			FVector::ZeroVector,
			DeltaSeconds,
			BrakeInterpSpeed);
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
