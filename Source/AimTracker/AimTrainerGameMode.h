#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AimTarget.h"
#include "AimTrainerGameMode.generated.h"

UCLASS()
class AIMTRACKER_API AAimTrainerGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAimTrainerGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void RegisterHit(AAimTrainingTarget* Target);
    void RegisterMiss();
    void RestartSession();
    void SetTrainingMode(int32 NewMode);
    void UpdateAimTargetFocus(AAimTrainingTarget* HoveredTarget, float DeltaSeconds);

    bool IsSessionActive() const { return bSessionActive; }
    int32 GetTrainingMode() const { return CurrentTrainingMode; }
    int32 GetHits() const { return Hits; }
    int32 GetShots() const { return Shots; }
    float GetTimeRemaining() const;
    float GetAccuracy() const;
    float GetAverageReactionMs() const;
    float GetLastReactionMs() const { return LastReactionMs; }

private:
    void BuildArena();
    void SpawnTargets();
    void PlaceTarget(AAimTrainingTarget* Target, int32 TargetIndex);
    void RespawnTarget(AAimTrainingTarget* Target, int32 TargetIndex);
    void ApplyTrainingModeVisibility();
    bool ShouldShowBaseTarget(int32 TargetIndex) const;
    void SpawnJumpTarget();
    void RemoveJumpTarget(AAimTrainingTarget* Target);
    void ClearJumpTargets();
    FVector GetTrackingTargetLocation(int32 TargetIndex) const;

    UPROPERTY(EditDefaultsOnly, Category = "Training")
    float SessionDuration = 60.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Mode 3")
    float JumpTargetSpawnInterval = 5.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Training")
    int32 TargetCount = 6;
    UPROPERTY()
    TArray<TObjectPtr<AAimTrainingTarget>> Targets;
    UPROPERTY()
    TArray<TObjectPtr<AAimTrainingTarget>> JumpTargets;

    int32 CurrentTrainingMode = 1;
    int32 Hits = 0;
    int32 Shots = 0;
    float SessionStartTime = 0.0f;
    float LastReactionMs = 0.0f;
    float TotalReactionMs = 0.0f;
    float JumpSpawnAccumulator = 0.0f;
    bool bSessionActive = false;
};