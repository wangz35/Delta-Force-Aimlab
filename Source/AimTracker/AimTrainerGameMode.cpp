#include "AimTrainerGameMode.h"

#include "AimTarget.h"
#include "AimTrainerHUD.h"
#include "AimTrainerPawn.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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
        AAimTrainerPawn* TrainingPawn = GetWorld()->SpawnActor<AAimTrainerPawn>(FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
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
    if (!bSessionActive || !IsValid(Target))
    {
        return;
    }

    ++Hits;
    ++Shots;
    LastReactionMs = (GetWorld()->GetTimeSeconds() - Target->GetActivationTime()) * 1000.0f;
    TotalReactionMs += LastReactionMs;
    PlaceTarget(Target, Target->GetMovementMode());
}

void AAimTrainerGameMode::RegisterMiss()
{
    if (bSessionActive)
    {
        ++Shots;
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

    if (Targets.Num() == 0)
    {
        SpawnTargets();
    }

    for (AAimTrainingTarget* Target : Targets)
    {
        if (IsValid(Target))
        {
            Target->SetActorHiddenInGame(false);
            Target->SetActorEnableCollision(true);
            PlaceTarget(Target, Target->GetMovementMode());
        }
    }
}

float AAimTrainerGameMode::GetTimeRemaining() const
{
    if (!GetWorld())
    {
        return SessionDuration;
    }
    return FMath::Max(0.0f, SessionDuration - (GetWorld()->GetTimeSeconds() - SessionStartTime));
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
    if (!ArenaCube)
    {
        return;
    }

    auto SpawnBlock = [this, ArenaCube](const FVector& Location, const FVector& Scale)
    {
        AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
        if (Block)
        {
            Block->GetStaticMeshComponent()->SetStaticMesh(ArenaCube);
            Block->SetActorScale3D(Scale);
        }
    };

    SpawnBlock(FVector(1800.0f, 0.0f, -220.0f), FVector(42.0f, 30.0f, 0.35f));
    SpawnBlock(FVector(3700.0f, 0.0f, 850.0f), FVector(0.3f, 30.0f, 11.0f));
    SpawnBlock(FVector(-300.0f, 0.0f, 850.0f), FVector(0.3f, 30.0f, 11.0f));
    SpawnBlock(FVector(1800.0f, 0.0f, 2600.0f), FVector(42.0f, 30.0f, 0.35f));
    SpawnBlock(FVector(1850.0f, 3100.0f, 850.0f), FVector(19.0f, 0.3f, 11.0f));
    SpawnBlock(FVector(1850.0f, -3100.0f, 850.0f), FVector(19.0f, 0.3f, 11.0f));

    auto SpawnArenaLight = [this](const FVector& Location, const FLinearColor& Color, float Intensity)
    {
        APointLight* Light = GetWorld()->SpawnActor<APointLight>(Location, FRotator::ZeroRotator);
        if (Light)
        {
            if (UPointLightComponent* Component = Cast<UPointLightComponent>(Light->GetLightComponent()))
            {
                Component->SetMobility(EComponentMobility::Movable);
                Component->SetIntensity(Intensity);
            Component->SetAttenuationRadius(6500.0f);
                Component->SetLightColor(Color);
                Component->SetCastShadows(false);
            }
        }
    };

    SpawnArenaLight(FVector(800.0f, -1800.0f, 2100.0f), FLinearColor(0.52f, 0.72f, 1.0f), 950000.0f);
    SpawnArenaLight(FVector(2200.0f, 0.0f, 2400.0f), FLinearColor(1.0f, 0.78f, 0.55f), 1100000.0f);
    SpawnArenaLight(FVector(3400.0f, 1800.0f, 2100.0f), FLinearColor(0.55f, 0.82f, 1.0f), 950000.0f);
    SpawnArenaLight(FVector(1800.0f, 0.0f, 650.0f), FLinearColor(1.0f, 1.0f, 1.0f), 420000.0f);
}

void AAimTrainerGameMode::SpawnTargets()
{
    for (int32 Index = 0; Index < TargetCount; ++Index)
    {
        AAimTrainingTarget* Target = GetWorld()->SpawnActor<AAimTrainingTarget>();
        if (Target)
        {
            Targets.Add(Target);
            PlaceTarget(Target, EAimTargetMovement::Strafe);
        }
    }
}

void AAimTrainerGameMode::PlaceTarget(AAimTrainingTarget* Target, EAimTargetMovement MovementMode)
{
    Target->Activate(
        GetTrackingTargetLocation(),
        70.0f,
        0.52f,
        FVector::RightVector,
        EAimTargetMovement::Strafe
    );
}

FVector AAimTrainerGameMode::GetTrackingTargetLocation() const
{
    return FVector(2400.0f, 0.0f, 520.0f);
}
