#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Solo/WEVORAArenaConfig.h"
#include "WEVORARunDirector.generated.h"

class AWEVORACharacter;

/** Owns only this run's combatants. Tick-based scheduling has no orphaned timer callbacks. */
UCLASS(Blueprintable)
class WEVORA_API AWEVORARunDirector : public AActor
{
	GENERATED_BODY()
public:
	AWEVORARunDirector();
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UWEVORAArenaConfig> Config;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutoStart = true;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) EWEVORARunState State = EWEVORARunState::Idle;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) int32 WaveIndex = -1;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) int32 SpawnedCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) int32 KilledCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) float Countdown = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) FString ResultReason;
	UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<AWEVORACharacter> Pilot;
	UPROPERTY(Transient, BlueprintReadOnly) TArray<TObjectPtr<AWEVORAEnemy>> Enemies;
	UFUNCTION(BlueprintCallable) bool StartRun(AWEVORACharacter* Player);
	UFUNCTION(BlueprintCallable) void Retry();
	UFUNCTION(BlueprintCallable) void ContinueAfterReward();
	UFUNCTION(BlueprintPure) bool IsCombatActive() const;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	FTransform PlayerStart;
	float WaveElapsed = 0;
	float SpawnElapsed = 0;
	bool bCompletionPending = false;
	void BeginWave();
	void SpawnEnemy();
	void FinishRun(bool bWon, const FString& Reason);
	void CleanupCombat();
	UFUNCTION() void PlayerDied();
};
