#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SaveGame.h"
#include "AimTarget.h"
#include "AimTrainerGameMode.generated.h"

class AStaticMeshActor;
UCLASS()
class AIMTRACKER_API UAimTrainerSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 Mode3BestBots = 0;

    UPROPERTY(SaveGame)
    int32 Mode4BestBots = 0;
};

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
    bool IsBotScoreMode() const { return CurrentTrainingMode == 3 || CurrentTrainingMode == 4 || CurrentTrainingMode == 5; }
    bool HasBestBotRecord() const { return CurrentTrainingMode == 3 || CurrentTrainingMode == 4; }
    int32 GetTrainingMode() const { return CurrentTrainingMode; }
    int32 GetHits() const { return Hits; }
    int32 GetShots() const { return Shots; }
    int32 GetBotEliminations() const { return BotEliminations; }
    int32 GetBestBotEliminations() const;
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
    void SpawnHorizontalBot();
    void RemoveHorizontalBotTarget(AAimTrainingTarget* Target);
    void SpawnMode5PeekTarget();
    void ScheduleMode5NextTarget();
    void SetMode5CoverVisible(bool bVisible);
    void ClearDynamicBotTargets();
    void RegisterBotElimination(AAimTrainingTarget* Target);
    void FinishSession();
    void LoadRecords();
    void SaveRecords();
    FVector GetTrackingTargetLocation(int32 TargetIndex) const;

    UPROPERTY(EditDefaultsOnly, Category = "Training")
    float SessionDuration = 60.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Mode 3")
    float JumpTargetSpawnInterval = 2.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Mode 4")
    int32 HorizontalBotCount = 3;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Mode 5")
    float Mode5RespawnDelay = 1.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Training")
    int32 TargetCount = 6;
    UPROPERTY()
    TArray<TObjectPtr<AAimTrainingTarget>> Targets;
    UPROPERTY()
    TArray<TObjectPtr<AAimTrainingTarget>> JumpTargets;
    UPROPERTY()
    TArray<TObjectPtr<AAimTrainingTarget>> HorizontalBotTargets;
    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> Mode5CoverWalls;
    UPROPERTY()
    TObjectPtr<UAimTrainerSaveGame> Records;

    int32 CurrentTrainingMode = 1;
    int32 Hits = 0;
    int32 Shots = 0;
    int32 BotEliminations = 0;
    float SessionStartTime = 0.0f;
    float LastReactionMs = 0.0f;
    float TotalReactionMs = 0.0f;
    float JumpSpawnAccumulator = 0.0f;
    FTimerHandle Mode5SpawnTimer;
    bool bSessionActive = false;
};
