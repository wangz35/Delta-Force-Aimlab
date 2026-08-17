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

AAimTrainerGameMode::AAimTrainerGameMode()
{
    DefaultPawnClass = AAimTrainerPawn::StaticClass();
    HUDClass = AAimTrainerHUD::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
}

void AAimTrainerGameMode::BeginPlay()
{
    Super::BeginPlay();
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
        bSessionActive = false;
        for (AAimTrainingTarget* Target : Targets)
        {
            if (Target)
            {
                Target->SetActorHiddenInGame(true);
                Target->SetActorEnableCollision(false);
            }
        }
    }
}

void AAimTrainerGameMode::RegisterHit(AAimTrainingTarget* Target)
{
    if (!bSessionActive || !IsValid(Target) || Target->IsHidden()) return;
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

void AAimTrainerGameMode::UpdateDefaultTargetHover(bool bHovered)
{
    if (Targets.IsValidIndex(0) && IsValid(Targets[0]))
    {
        Targets[0]->SetPersistentHover(bHovered);
    }
}

void AAimTrainerGameMode::RestartSession()
{
    Hits = 0;
    Shots = 0;
    LastReactionMs = 0.0f;
    TotalReactionMs = 0.0f;
    bSessionActive = true;
    SessionStartTime = GetWorld()->GetTimeSeconds();
    if (Targets.Num() == 0) SpawnTargets();

    for (int32 Index = 0; Index < Targets.Num(); ++Index)
    {
        if (AAimTrainingTarget* Target = Targets[Index])
        {
            Target->SetActorHiddenInGame(false);
            Target->SetActorEnableCollision(true);
            PlaceTarget(Target, Index);
        }
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

float AAimTrainerGameMode::GetAverageReactionMs() const
{
    return Hits > 0 ? TotalReactionMs / static_cast<float>(Hits) : 0.0f;
}

void AAimTrainerGameMode::BuildArena()
{
    UStaticMesh* ArenaCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!ArenaCube) return;

    auto SpawnBlock = [this, ArenaCube](const FVector& Location, const FVector& Scale)
    {
        if (AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator))
        {
            Block->GetStaticMeshComponent()->SetStaticMesh(ArenaCube);
            Block->SetActorScale3D(Scale);
        }
    };

    // A sealed 100 m long range: target lanes are 10, 20, 50 and 100 metres from spawn.
    SpawnBlock(FVector(3900.0f, 0.0f, -220.0f), FVector(128.0f, 106.0f, 0.35f));
    SpawnBlock(FVector(10300.0f, 0.0f, 1190.0f), FVector(0.3f, 106.0f, 27.8f));
    SpawnBlock(FVector(-2500.0f, 0.0f, 1190.0f), FVector(0.3f, 106.0f, 27.8f));
    SpawnBlock(FVector(3900.0f, 0.0f, 2600.0f), FVector(128.0f, 106.0f, 0.35f));
    SpawnBlock(FVector(3900.0f, 5300.0f, 1190.0f), FVector(128.0f, 0.3f, 27.8f));
    SpawnBlock(FVector(3900.0f, -5300.0f, 1190.0f), FVector(128.0f, 0.3f, 27.8f));
    SpawnBlock(FVector(-1000.0f, 0.0f, 220.0f), FVector(0.35f, 12.0f, 4.4f));

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
            PlaceTarget(Target, Index);
        }
    }
}

void AAimTrainerGameMode::PlaceTarget(AAimTrainingTarget* Target, int32 TargetIndex)
{
    if (!IsValid(Target)) return;
    if (TargetIndex == 0)
    {
        Target->Activate(FVector(2400.0f, 0.0f, 520.0f), 70.0f, 3.12f, FVector::RightVector, EAimTargetMovement::Strafe, true, false);
        return;
    }
    Target->Activate(GetTrackingTargetLocation(TargetIndex - 1), 34.0f, 0.0f, FVector::RightVector, EAimTargetMovement::Strafe, false, true);
}

void AAimTrainerGameMode::RespawnTarget(AAimTrainingTarget* Target, int32 TargetIndex)
{
    if (!bSessionActive || !IsValid(Target)) return;
    Target->SetActorHiddenInGame(false);
    Target->SetActorEnableCollision(true);
    PlaceTarget(Target, TargetIndex);
}

FVector AAimTrainerGameMode::GetTrackingTargetLocation(int32 TargetIndex) const
{
    static constexpr float TargetDistances[] = { 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const int32 LaneIndex = FMath::Clamp(TargetIndex, 0, UE_ARRAY_COUNT(TargetDistances) - 1);
    return FVector(TargetDistances[LaneIndex], FMath::FRandRange(-3600.0f, 3600.0f), FMath::FRandRange(240.0f, 900.0f));
}