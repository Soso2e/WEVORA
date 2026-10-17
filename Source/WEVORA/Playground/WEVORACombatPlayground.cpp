#include "Playground/WEVORACombatPlayground.h"
#include "Playground/WEVORAPlaygroundTarget.h"
#include "Playground/WEVORALocalizedPlaygroundTarget.h"
#include "Spell/WEVORALocalizedStatusComponent.h"
#include "WEVORACharacter.h"
#include "AI/WEVORAHealthComponent.h"
#include "AI/WEVORAEnemyProjectile.h"
#include "Spell/WEVORASpellDelivery.h"
#include "Spell/WEVORAImpactEffect.h"
#include "Spell/WEVORASpellWeavingComponent.h"
#include "Spell/WEVORASpellCastComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

AWEVORACombatPlayground::AWEVORACombatPlayground()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	LocalizedSampleClass = AWEVORALocalizedPlaygroundTarget::StaticClass();
}

AWEVORACombatPlayground* AWEVORACombatPlayground::Find(const UWorld* World)
{
	if (!World) { return nullptr; }
	for (TActorIterator<AWEVORACombatPlayground> It(World); It; ++It) { return *It; }
	return nullptr;
}

void AWEVORACombatPlayground::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bSpawnLocalizedSample && LocalizedSampleClass)
	{
		bool bExists = false;
		for (TActorIterator<AWEVORALocalizedPlaygroundTarget> It(GetWorld()); It; ++It) { bExists = true; break; }
		if (!bExists) { GetWorld()->SpawnActor<AWEVORALocalizedPlaygroundTarget>(LocalizedSampleClass, LocalizedSampleTransform); }
	}
	for (TActorIterator<AWEVORAPlaygroundTarget> It(GetWorld()); It; ++It)
	{
		Targets.Add(*It);
		BaseTargetSpeeds.Add(It->MotionSpeed);
	}
}

bool AWEVORACombatPlayground::IsOutsideBounds(FVector Position) const
{
	const FVector Local = Position - GetActorLocation();
	return FMath::Abs(Local.X) > BoundsExtent.X || FMath::Abs(Local.Y) > BoundsExtent.Y
		|| Local.Z > BoundsExtent.Z || Local.Z < MinimumHeight;
}

void AWEVORACombatPlayground::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) { return; }
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AWEVORACharacter* Current = PC ? Cast<AWEVORACharacter>(PC->GetPawn()) : nullptr;
	if (!Current) { return; }
	if (Pilot.Get() != Current)
	{
		Pilot = Current;
		Current->HealthComponent->bShowDamageFeedback = false;
		Current->SpellWeavingComponent->bShowDebug = false;
		EnableInput(PC);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::ResetTrial);
		InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ThisClass::ToggleGuide);
		InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ThisClass::ToggleDebug);
		InputComponent->BindKey(EKeys::F3, IE_Pressed, this, &ThisClass::ToggleLightningAssist);
		ResetWithReason(TEXT("Ready"));
	}
	if (IsOutsideBounds(Current->GetActorLocation())) { ResetWithReason(TEXT("Returned from field boundary")); }
	if (!Current->HealthComponent->IsAlive())
	{
		Current->SpellWeavingComponent->CancelWeave();
		DefeatedTime += DeltaSeconds;
		if (DefeatedTime >= 2) { ResetWithReason(TEXT("Defeated - new trial")); }
	}
	else { DefeatedTime = 0; }
	for (AWEVORAPlaygroundTarget* Target : Targets)
	{
		if (!IsValid(Target)) { continue; }
		if (IsOutsideBounds(Target->GetActorLocation())) { Target->ResetTarget(); }
		if (bDebugOverlay && !Target->IsHidden())
		{
			DrawDebugDirectionalArrow(GetWorld(), Target->GetActorLocation(), Target->GetActorLocation()
				+ Target->MeasuredVelocity * 0.5f, 25, FColor::Cyan, false, 0.12f, 0, 2);
			if (const UWEVORALocalizedStatusComponent* Local = Target->FindComponentByClass<UWEVORALocalizedStatusComponent>())
			{
				if (const UWEVORAZoneProfile* Profile = Local->GetProfile())
				{
					for (const FWEVORAZoneDefinition& Zone : Profile->Zones)
					{
						const FVector Position = Profile->GetZonePosition(Zone.ID, Target);
						const FColor Color = Local->HasStatus(Zone.ID, TEXT("Burning")) ? FColor::Orange : FColor::Cyan;
						DrawDebugSphere(GetWorld(), Position, Zone.ID == TEXT("Torso") ? 45 : 22, 12, Color, false, 0.12f);
						DrawDebugString(GetWorld(), Position + FVector(0,0,26), Zone.ID.ToString(), nullptr, Color, 0.12f, true, 0.7f);
					}
					for (const FWEVORAZoneLink& Link : Profile->Links)
					{
						DrawDebugLine(GetWorld(), Profile->GetZonePosition(Link.From, Target), Profile->GetZonePosition(Link.To, Target),
							FColor::Silver, false, 0.12f, 0, 1);
					}
				}
			}
		}
	}
}

void AWEVORACombatPlayground::ResetTrial() { ResetWithReason(TEXT("Manual reset")); }

void AWEVORACombatPlayground::ResetWithReason(const FString& Reason)
{
	if (!HasAuthority()) { return; }
	// Remove all in-flight gameplay and cosmetic actors before restoring state.
	for (TActorIterator<AWEVORASpellDelivery> It(GetWorld()); It; ++It) { It->Destroy(); }
	for (TActorIterator<AWEVORAEnemyProjectile> It(GetWorld()); It; ++It) { It->Destroy(); }
	for (TActorIterator<AWEVORAImpactEffect> It(GetWorld()); It; ++It) { It->Destroy(); }
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		AWEVORAPlaygroundTarget* Target = Targets[Index];
		if (!IsValid(Target)) { continue; }
		Target->HealthComponent->MaxHealth = FMath::Max(1.0f, TargetHealth);
		Target->MotionSpeed = BaseTargetSpeeds[Index] * FMath::Max(0.0f, TargetSpeedMultiplier);
		Target->ResetTarget();
	}
	if (AWEVORACharacter* Player = Pilot.Get())
	{
		Player->ConfigurePlaygroundMovement(PlayerSpeedMultiplier);
		Player->SpellWeavingComponent->MaxChargeTime = FMath::Max(0.1f, ChargeTime);
		Player->ResetForPlayground(PlayerStart);
	}
	DefeatedTime = 0;
	LastResetReason = Reason;
	++ResetCount;
}

void AWEVORACombatPlayground::ToggleDebug() { bDebugOverlay = !bDebugOverlay; }
void AWEVORACombatPlayground::ToggleGuide() { bShowGuide = !bShowGuide; }
void AWEVORACombatPlayground::ToggleLightningAssist() { bLightningAssist = !bLightningAssist; }
