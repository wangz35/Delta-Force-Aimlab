#include "AimTrainerGameMode.h"

#include "AimTrainerHUD.h"
#include "AimTrainerPawn.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
namespace
{
    const FString RecordsSlotName(TEXT("AimTrainerRecords"));
    constexpr int32 RecordsUserIndex = 0;
}

AAimTrainerGameMode::AAimTrainerGameMode()
{
    DefaultPawnClass = AAimTrainerPawn::StaticClass();
    HUDClass = AAimTrainerHUD::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
}

void AAimTrainerGameMode::BeginPlay()
{
    Super::BeginPlay();
    LoadRecords();
    BuildArena();
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (PlayerController && !PlayerController->GetPawn())
    {
        AAimTrainerPawn* TrainingPawn = GetWorld()->SpawnActor<AAimTrainerPawn>(FVector::ZeroVector, FRotator::ZeroRotator);
        PlayerController->Possess(TrainingPawn);
    }
    RestartSession();
}

void AAimTrainerGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bSessionActive && GetTimeRemaining() <= 0.0f)
    {
        FinishSession();
        return;
    }

    if (bSessionActive && CurrentTrainingMode == 3)
    {
        JumpSpawnAccumulator += DeltaSeconds;
        while (JumpSpawnAccumulator >= JumpTargetSpawnInterval)
        {
            JumpSpawnAccumulator -= JumpTargetSpawnInterval;
            SpawnJumpTarget();
        }
    }

    if (bSessionActive && CurrentTrainingMode == 4)
    {
        HorizontalBotTargets.RemoveAll([](const TObjectPtr<AAimTrainingTarget>& Target)
        {
            return !IsValid(Target);
        });
        const int32 MissingBotCount = FMath::Max(0, HorizontalBotCount - HorizontalBotTargets.Num());
        for (int32 Index = 0; Index < MissingBotCount; ++Index)
        {
            SpawnHorizontalBot();
        }
    }
}

void AAimTrainerGameMode::RegisterHit(AAimTrainingTarget* Target)
{
    if (!bSessionActive || !IsValid(Target) || Target->IsHidden()) return;
    if (Target->IsGazeTarget())
    {
        ++Shots;
        return;
    }

    ++Hits;
    ++Shots;
    LastReactionMs = (GetWorld()->GetTimeSeconds() - Target->GetActivationTime()) * 1000.0f;
    TotalReactionMs += LastReactionMs;
    const int32 TargetIndex = Targets.IndexOfByKey(Target);
    if (TargetIndex == INDEX_NONE) return;

    if (Target->PersistsOnHit())
    {
        Target->MarkHit();
        return;
    }

    Target->SetActorHiddenInGame(true);
    Target->SetActorEnableCollision(false);
    FTimerHandle RespawnTimer;
    FTimerDelegate RespawnDelegate;
    RespawnDelegate.BindUObject(this, &AAimTrainerGameMode::RespawnTarget, Target, TargetIndex);
    GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, 0.12f, false);
}

void AAimTrainerGameMode::RegisterMiss()
{
    if (bSessionActive) ++Shots;
}

void AAimTrainerGameMode::UpdateAimTargetFocus(AAimTrainingTarget* HoveredTarget, float DeltaSeconds)
{
    for (AAimTrainingTarget* Target : Targets)
    {
        if (IsValid(Target) && Target->PersistsOnHit())
        {
            Target->SetPersistentHover(Target == HoveredTarget);
        }
    }

    if (bSessionActive && CurrentTrainingMode == 7)
    {
        const float SampleDelta = FMath::Max(0.0f, DeltaSeconds);
        TrackingSampleSeconds += SampleDelta;
        if (Targets.IsValidIndex(1) && IsValid(HoveredTarget) && HoveredTarget == Targets[1])
        {
            TrackingOnTargetSeconds += SampleDelta;
        }
    }

    if (!bSessionActive || !IsValid(HoveredTarget) || !HoveredTarget->IsGazeTarget()) return;

    const bool bTrackedJumpBot = (CurrentTrainingMode == 3 || CurrentTrainingMode == 5) && JumpTargets.Contains(HoveredTarget);
    const bool bTrackedHorizontalBot = CurrentTrainingMode == 4 && HorizontalBotTargets.Contains(HoveredTarget);
    if ((bTrackedJumpBot || bTrackedHorizontalBot) && HoveredTarget->AddGazeFocus(DeltaSeconds))
    {
        RegisterBotElimination(HoveredTarget);
    }
}

void AAimTrainerGameMode::SetTrainingMode(int32 NewMode)
{
    if (NewMode < 1 || NewMode > 7) return;
    CurrentTrainingMode = NewMode;
    RestartSession();
}

void AAimTrainerGameMode::RestartSession()
{
    Hits = 0;
    Shots = 0;
    BotEliminations = 0;
    LastReactionMs = 0.0f;
    TotalReactionMs = 0.0f;
    TrackingSampleSeconds = 0.0f;
    TrackingOnTargetSeconds = 0.0f;
    bSessionActive = true;
    SessionStartTime = GetWorld()->GetTimeSeconds();
    JumpSpawnAccumulator = 0.0f;
    ClearDynamicBotTargets();

    if (Targets.Num() == 0) SpawnTargets();
    for (int32 Index = 0; Index < Targets.Num(); ++Index)
    {
        if (AAimTrainingTarget* Target = Targets[Index])
        {
            PlaceTarget(Target, Index);
        }
    }
    ApplyTrainingModeVisibility();

    if (CurrentTrainingMode == 4)
    {
        for (int32 Index = 0; Index < HorizontalBotCount; ++Index)
        {
            SpawnHorizontalBot();
        }
    }
    else if (CurrentTrainingMode == 5)
    {
        if (AAimTrainerPawn* TrainingPawn = Cast<AAimTrainerPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            TrainingPawn->SetActorLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
            TrainingPawn->SetActorRotation(FRotator::ZeroRotator, ETeleportType::TeleportPhysics);
        }
        if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
        {
            PlayerController->SetControlRotation(FRotator::ZeroRotator);
        }
        SpawnMode5PeekTarget();
    }
}

float AAimTrainerGameMode::GetTimeRemaining() const
{
    return GetWorld() ? FMath::Max(0.0f, SessionDuration - (GetWorld()->GetTimeSeconds() - SessionStartTime)) : SessionDuration;
}

float AAimTrainerGameMode::GetAccuracy() const
{
    return Shots > 0 ? static_cast<float>(Hits) / static_cast<float>(Shots) * 100.0f : 0.0f;
}

float AAimTrainerGameMode::GetTrackingAccuracy() const
{
    return TrackingSampleSeconds > SMALL_NUMBER
        ? TrackingOnTargetSeconds / TrackingSampleSeconds * 100.0f
        : 0.0f;
}

float AAimTrainerGameMode::GetBestTrackingAccuracy() const
{
    const float SavedBest = Records ? Records->Mode7BestTrackingPercent : 0.0f;
    return FMath::Max(SavedBest, GetTrackingAccuracy());
}

float AAimTrainerGameMode::GetAverageReactionMs() const
{
    return Hits > 0 ? TotalReactionMs / static_cast<float>(Hits) : 0.0f;
}

int32 AAimTrainerGameMode::GetBestBotEliminations() const
{
    if (!Records) return BotEliminations;
    const int32 SavedBest = CurrentTrainingMode == 3
        ? Records->Mode3BestBots
        : (CurrentTrainingMode == 4 ? Records->Mode4BestBots : 0);
    return FMath::Max(SavedBest, BotEliminations);
}

void AAimTrainerGameMode::FinishSession()
{
    if (!bSessionActive) return;
    bSessionActive = false;

    bool bNewRecord = false;
    if (Records && CurrentTrainingMode == 3 && BotEliminations > Records->Mode3BestBots)
    {
        Records->Mode3BestBots = BotEliminations;
        bNewRecord = true;
    }
    else if (Records && CurrentTrainingMode == 4 && BotEliminations > Records->Mode4BestBots)
    {
        Records->Mode4BestBots = BotEliminations;
        bNewRecord = true;
    }
    else if (Records && CurrentTrainingMode == 7 && GetTrackingAccuracy() > Records->Mode7BestTrackingPercent)
    {
        Records->Mode7BestTrackingPercent = GetTrackingAccuracy();
        bNewRecord = true;
    }
    if (bNewRecord)
    {
        SaveRecords();
    }

    for (AAimTrainingTarget* Target : Targets)
    {
        if (IsValid(Target))
        {
            Target->SetActorHiddenInGame(true);
            Target->SetActorEnableCollision(false);
        }
    }
    ClearDynamicBotTargets();
}

void AAimTrainerGameMode::LoadRecords()
{
    if (UGameplayStatics::DoesSaveGameExist(RecordsSlotName, RecordsUserIndex))
    {
        Records = Cast<UAimTrainerSaveGame>(UGameplayStatics::LoadGameFromSlot(RecordsSlotName, RecordsUserIndex));
    }
    if (!Records)
    {
        Records = Cast<UAimTrainerSaveGame>(UGameplayStatics::CreateSaveGameObject(UAimTrainerSaveGame::StaticClass()));
    }
    if (Records)
    {
        Records->Mode3BestBots = FMath::Max(0, Records->Mode3BestBots);
        Records->Mode4BestBots = FMath::Max(0, Records->Mode4BestBots);
        Records->Mode7BestTrackingPercent = FMath::Clamp(Records->Mode7BestTrackingPercent, 0.0f, 100.0f);
    }
}

void AAimTrainerGameMode::SaveRecords()
{
    if (Records)
    {
        UGameplayStatics::SaveGameToSlot(Records, RecordsSlotName, RecordsUserIndex);
    }
}

void AAimTrainerGameMode::BuildArena()
{
    UStaticMesh* ArenaCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!ArenaCube) return;

    auto SpawnBlock = [this, ArenaCube](const FVector& Location, const FVector& Scale) -> AStaticMeshActor*
    {
        if (AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator))
        {
            Block->GetStaticMeshComponent()->SetStaticMesh(ArenaCube);
            Block->SetActorScale3D(Scale);
            return Block;
        }
        return nullptr;
    };

    SpawnBlock(FVector(3900.0f, 0.0f, -220.0f), FVector(128.0f, 106.0f, 0.35f));
    SpawnBlock(FVector(10300.0f, 0.0f, 1190.0f), FVector(0.3f, 106.0f, 27.8f));
    SpawnBlock(FVector(-2500.0f, 0.0f, 1190.0f), FVector(0.3f, 106.0f, 27.8f));
    SpawnBlock(FVector(3900.0f, 0.0f, 2600.0f), FVector(128.0f, 106.0f, 0.35f));
    SpawnBlock(FVector(3900.0f, 5300.0f, 1190.0f), FVector(128.0f, 0.3f, 27.8f));
    SpawnBlock(FVector(3900.0f, -5300.0f, 1190.0f), FVector(128.0f, 0.3f, 27.8f));
    SpawnBlock(FVector(-1000.0f, 0.0f, 220.0f), FVector(0.35f, 12.0f, 4.4f));

    if (AStaticMeshActor* LeftCover = SpawnBlock(FVector(2600.0f, -3100.0f, 25.0f), FVector(0.4f, 44.0f, 4.5f)))
    {
        Mode5CoverWalls.Add(LeftCover);
    }
    if (AStaticMeshActor* RightCover = SpawnBlock(FVector(2600.0f, 3100.0f, 25.0f), FVector(0.4f, 44.0f, 4.5f)))
    {
        Mode5CoverWalls.Add(RightCover);
    }
    SetMode5CoverVisible(false);

    auto SpawnArenaLight = [this](const FVector& Location, const FLinearColor& Color)
    {
        if (APointLight* Light = GetWorld()->SpawnActor<APointLight>(Location, FRotator::ZeroRotator))
        {
            if (UPointLightComponent* Component = Cast<UPointLightComponent>(Light->GetLightComponent()))
            {
                Component->SetMobility(EComponentMobility::Movable);
                Component->SetIntensity(1050000.0f);
                Component->SetAttenuationRadius(6500.0f);
                Component->SetLightColor(Color);
                Component->SetCastShadows(false);
            }
        }
    };

    SpawnArenaLight(FVector(800.0f, -1800.0f, 2100.0f), FLinearColor(0.52f, 0.72f, 1.0f));
    SpawnArenaLight(FVector(3000.0f, 1800.0f, 2100.0f), FLinearColor(0.48f, 0.72f, 1.0f));
    SpawnArenaLight(FVector(5500.0f, -1800.0f, 2100.0f), FLinearColor(0.55f, 0.82f, 1.0f));
    SpawnArenaLight(FVector(7800.0f, 1800.0f, 2100.0f), FLinearColor(0.48f, 0.72f, 1.0f));
    SpawnArenaLight(FVector(9800.0f, 0.0f, 2100.0f), FLinearColor(0.55f, 0.82f, 1.0f));
}

void AAimTrainerGameMode::SpawnTargets()
{
    for (int32 Index = 0; Index < TargetCount; ++Index)
    {
        if (AAimTrainingTarget* Target = GetWorld()->SpawnActor<AAimTrainingTarget>())
        {
            Targets.Add(Target);
        }
    }
}

void AAimTrainerGameMode::PlaceTarget(AAimTrainingTarget* Target, int32 TargetIndex)
{
    if (!IsValid(Target)) return;
    if (TargetIndex == 0)
    {
        Target->Activate(FVector(500.0f, 0.0f, 520.0f), 70.0f, 0.936f, FVector::RightVector, EAimTargetMovement::Strafe, true, false);
        return;
    }
    if (TargetIndex == 1)
    {
        if (CurrentTrainingMode == 7)
        {
            Target->Activate(FVector(2000.0f, 0.0f, 520.0f), 22.0f, ReactiveTrackingSpeed, FVector::RightVector, EAimTargetMovement::Strafe, true, false, true, ReactiveTrackingTravelDistance);
            return;
        }
        Target->Activate(FVector(2000.0f, 0.0f, 520.0f), 22.0f, 210.0f, FVector::RightVector, EAimTargetMovement::Strafe, true, false, true);
        return;
    }
    if (CurrentTrainingMode == 6)
    {
        const float Distance = FMath::FRandRange(1400.0f, 4500.0f);
        const float SideSign = (TargetIndex % 2 == 0) ? -1.0f : 1.0f;
        const FVector SwitchLocation(
            Distance,
            SideSign * FMath::FRandRange(Distance * 0.12f, Distance * 0.55f),
            FMath::FRandRange(300.0f, 1050.0f));
        const float TargetRadius = FMath::Clamp(Distance * 0.012f, 18.0f, 48.0f);
        Target->Activate(SwitchLocation, TargetRadius, 0.0f, FVector::RightVector, EAimTargetMovement::Strafe, false, false);
        return;
    }
    const FVector TargetLocation = GetTrackingTargetLocation(TargetIndex - 2);
    if (CurrentTrainingMode == 2)
    {
        // All distance targets use the same world-space tracking speed.
        const float HorizontalSpeed = 420.0f;
        const float HorizontalTravelDistance = TargetLocation.X * 0.40f;
        Target->Activate(TargetLocation, 34.0f, HorizontalSpeed, FVector::RightVector, EAimTargetMovement::Strafe, false, true, true, HorizontalTravelDistance, true);
        return;
    }
    Target->Activate(TargetLocation, 34.0f, 0.0f, FVector::RightVector, EAimTargetMovement::Strafe, false, true);
}

bool AAimTrainerGameMode::ShouldShowBaseTarget(int32 TargetIndex) const
{
    if (CurrentTrainingMode == 1) return TargetIndex == 0 || TargetIndex >= 2;
    if (CurrentTrainingMode == 2) return TargetIndex == 0 || TargetIndex >= 2;
    if (CurrentTrainingMode == 3) return false;
    if (CurrentTrainingMode == 4) return TargetIndex == 1;
    if (CurrentTrainingMode == 5) return false;
    if (CurrentTrainingMode == 6) return TargetIndex >= 2;
    if (CurrentTrainingMode == 7) return TargetIndex == 1;
    return false;
}

void AAimTrainerGameMode::ApplyTrainingModeVisibility()
{
    for (int32 Index = 0; Index < Targets.Num(); ++Index)
    {
        if (AAimTrainingTarget* Target = Targets[Index])
        {
            const bool bVisible = ShouldShowBaseTarget(Index);
            Target->SetActorHiddenInGame(!bVisible);
            Target->SetActorEnableCollision(bVisible);
        }
    }
    SetMode5CoverVisible(CurrentTrainingMode == 5);
}

void AAimTrainerGameMode::RespawnTarget(AAimTrainingTarget* Target, int32 TargetIndex)
{
    if (!bSessionActive || !IsValid(Target)) return;
    PlaceTarget(Target, TargetIndex);
    const bool bVisible = ShouldShowBaseTarget(TargetIndex);
    Target->SetActorHiddenInGame(!bVisible);
    Target->SetActorEnableCollision(bVisible);
}

void AAimTrainerGameMode::SpawnJumpTarget()
{
    if (CurrentTrainingMode != 3 || !bSessionActive) return;
    JumpTargets.RemoveAll([](const TObjectPtr<AAimTrainingTarget>& Target)
    {
        return !IsValid(Target);
    });
    if (JumpTargets.Num() >= 3)
    {
        RemoveJumpTarget(JumpTargets[0]);
    }

    AAimTrainingTarget* Target = GetWorld()->SpawnActor<AAimTrainingTarget>();
    if (!Target) return;

    const float SideSign = FMath::RandBool() ? 1.0f : -1.0f;
    const float Distance = FMath::FRandRange(2200.0f, 3600.0f);
    const FVector StartLocation(Distance, SideSign * FMath::FRandRange(2300.0f, 3200.0f), 210.0f);
    const FVector LandingLocation(Distance + FMath::FRandRange(-350.0f, 350.0f), SideSign * FMath::FRandRange(450.0f, 1050.0f), 210.0f);
    Target->ActivateJumpArc(StartLocation, LandingLocation, FMath::FRandRange(434.0f, 616.0f), FMath::FRandRange(1.05f, 1.30f));
    JumpTargets.Add(Target);
}

void AAimTrainerGameMode::RemoveJumpTarget(AAimTrainingTarget* Target)
{
    JumpTargets.Remove(Target);
    if (IsValid(Target)) Target->Destroy();
}

void AAimTrainerGameMode::SpawnHorizontalBot()
{
    if (CurrentTrainingMode != 4 || !bSessionActive) return;
    AAimTrainingTarget* Target = GetWorld()->SpawnActor<AAimTrainingTarget>();
    if (!Target) return;

    const float SideSign = FMath::RandBool() ? 1.0f : -1.0f;
    const float Distance = FMath::FRandRange(2600.0f, 4200.0f);
    const float HorizontalSpeed = FMath::FRandRange(210.0f, 275.0f);
    const float HorizontalTravelDistance = FMath::FRandRange(1050.0f, 1650.0f);

    if (FMath::RandBool())
    {
        const FVector StartLocation(
            Distance,
            SideSign * FMath::FRandRange(1650.0f, 2300.0f),
            210.0f);
        const FVector LandingLocation(
            Distance + FMath::FRandRange(-220.0f, 220.0f),
            SideSign * FMath::FRandRange(350.0f, 750.0f),
            210.0f);
        Target->ActivateJumpArc(
            StartLocation,
            LandingLocation,
            FMath::FRandRange(360.0f, 480.0f),
            FMath::FRandRange(0.85f, 1.05f),
            HorizontalSpeed,
            HorizontalTravelDistance);
    }
    else
    {
        const FVector LaneCenter(
            Distance,
            FMath::FRandRange(-180.0f, 180.0f),
            210.0f);
        Target->ActivateHorizontalGaze(
            LaneCenter,
            34.0f,
            HorizontalSpeed,
            HorizontalTravelDistance,
            true,
            SideSign);
    }
    HorizontalBotTargets.Add(Target);
}

void AAimTrainerGameMode::RemoveHorizontalBotTarget(AAimTrainingTarget* Target)
{
    HorizontalBotTargets.Remove(Target);
    if (IsValid(Target)) Target->Destroy();
}

void AAimTrainerGameMode::SpawnMode5PeekTarget()
{
    if (CurrentTrainingMode != 5 || !bSessionActive)
    {
        return;
    }

    JumpTargets.RemoveAll([](const TObjectPtr<AAimTrainingTarget>& Target)
    {
        return !IsValid(Target);
    });
    if (JumpTargets.Num() > 0)
    {
        return;
    }

    AAimTrainingTarget* Target = GetWorld()->SpawnActor<AAimTrainingTarget>();
    if (!Target)
    {
        return;
    }

    const float SideSign = FMath::RandBool() ? 1.0f : -1.0f;
    const float Distance = FMath::FRandRange(2920.0f, 3080.0f);
    const float HorizontalSpeed = FMath::FRandRange(420.0f, 550.0f);
    const float HorizontalTravelDistance = FMath::FRandRange(700.0f, 900.0f);
    const FVector LaneCenter(Distance, SideSign * 450.0f, 210.0f);

    if (FMath::RandBool())
    {
        const FVector StartLocation(
            Distance,
            SideSign * (450.0f + HorizontalTravelDistance),
            210.0f);
        Target->ActivateJumpArc(
            StartLocation,
            LaneCenter,
            FMath::FRandRange(360.0f, 460.0f),
            FMath::FRandRange(0.85f, 1.0f),
            HorizontalSpeed,
            HorizontalTravelDistance);
    }
    else
    {
        Target->ActivateHorizontalGaze(
            LaneCenter,
            34.0f,
            HorizontalSpeed,
            HorizontalTravelDistance,
            true,
            SideSign);
    }
    JumpTargets.Add(Target);
}

void AAimTrainerGameMode::ScheduleMode5NextTarget()
{
    if (CurrentTrainingMode != 5 || !bSessionActive)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(Mode5SpawnTimer);
    GetWorldTimerManager().SetTimer(
        Mode5SpawnTimer,
        this,
        &AAimTrainerGameMode::SpawnMode5PeekTarget,
        Mode5RespawnDelay,
        false);
}
void AAimTrainerGameMode::RegisterBotElimination(AAimTrainingTarget* Target)
{
    if (!bSessionActive || !IsValid(Target)) return;

    if ((CurrentTrainingMode == 3 || CurrentTrainingMode == 5) && JumpTargets.Contains(Target))
    {
        const bool bScheduleMode5Target = CurrentTrainingMode == 5;
        ++BotEliminations;
        RemoveJumpTarget(Target);
        if (bScheduleMode5Target)
        {
            ScheduleMode5NextTarget();
        }
    }
    else if (CurrentTrainingMode == 4 && HorizontalBotTargets.Contains(Target))
    {
        ++BotEliminations;
        RemoveHorizontalBotTarget(Target);
        SpawnHorizontalBot();
    }
}

void AAimTrainerGameMode::ClearDynamicBotTargets()
{
    GetWorldTimerManager().ClearTimer(Mode5SpawnTimer);

    for (AAimTrainingTarget* Target : JumpTargets)
    {
        if (IsValid(Target)) Target->Destroy();
    }
    JumpTargets.Reset();

    for (AAimTrainingTarget* Target : HorizontalBotTargets)
    {
        if (IsValid(Target)) Target->Destroy();
    }
    HorizontalBotTargets.Reset();
}

void AAimTrainerGameMode::SetMode5CoverVisible(bool bVisible)
{
    for (AStaticMeshActor* Wall : Mode5CoverWalls)
    {
        if (!IsValid(Wall))
        {
            continue;
        }

        Wall->SetActorHiddenInGame(!bVisible);
        Wall->SetActorEnableCollision(bVisible);
        if (UStaticMeshComponent* MeshComponent = Wall->GetStaticMeshComponent())
        {
            MeshComponent->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            MeshComponent->SetCollisionResponseToAllChannels(bVisible ? ECR_Block : ECR_Ignore);
        }
    }
}
FVector AAimTrainerGameMode::GetTrackingTargetLocation(int32 TargetIndex) const
{
    static constexpr float TargetDistances[] = { 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const int32 LaneIndex = FMath::Clamp(TargetIndex, 0, UE_ARRAY_COUNT(TargetDistances) - 1);
    return FVector(TargetDistances[LaneIndex], FMath::FRandRange(-3600.0f, 3600.0f), FMath::FRandRange(240.0f, 900.0f));
}
