#include "AimTarget.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AAimTrainingTarget::AAimTrainingTarget()
{
    PrimaryActorTick.bCanEverTick = true;

    OuterRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OuterRing"));
    SetRootComponent(OuterRing);
    OuterRing->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    OuterRing->SetCollisionResponseToAllChannels(ECR_Ignore);
    OuterRing->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    OuterRing->SetGenerateOverlapEvents(false);

    Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
    Core->SetupAttachment(OuterRing);
    Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("TargetGlow"));
    Glow->SetupAttachment(OuterRing);
    Glow->SetIntensity(3500.0f);
    Glow->SetAttenuationRadius(500.0f);
    Glow->SetLightColor(FLinearColor(1.0f, 0.28f, 0.04f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        OuterRing->SetStaticMesh(SphereMesh.Object);
        Core->SetStaticMesh(SphereMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (ShapeMaterial.Succeeded())
    {
        if (UMaterialInstanceDynamic* OuterMaterial = UMaterialInstanceDynamic::Create(ShapeMaterial.Object, this))
        {
            OuterMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.16f, 0.025f, 0.005f));
            OuterRing->SetMaterial(0, OuterMaterial);
        }
        if (UMaterialInstanceDynamic* CoreMaterial = UMaterialInstanceDynamic::Create(ShapeMaterial.Object, this))
        {
            CoreMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.2f, 0.01f));
            Core->SetMaterial(0, CoreMaterial);
        }
    }
}

void AAimTrainingTarget::Activate(const FVector& NewLocation, float NewRadius, float NewSpeed, const FVector& NewTravelDirection, EAimTargetMovement NewMovementMode)
{
    HomeLocation = NewLocation;
    TravelDirection = NewTravelDirection.GetSafeNormal();
    TravelDistance = FMath::FRandRange(120.0f, 420.0f);
    TravelSpeed = NewSpeed;
    OscillationOffset = FMath::FRandRange(0.0f, 2.0f * PI);
    MovementMode = NewMovementMode;
    ActivationTime = GetWorld()->GetTimeSeconds();

    SetActorLocation(HomeLocation);
    OuterRing->SetWorldScale3D(FVector(NewRadius / 50.0f));
    Core->SetRelativeLocation(FVector(-NewRadius * 0.28f, 0.0f, 0.0f));
    Core->SetRelativeScale3D(FVector(0.38f));
}

void AAimTrainingTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (TravelSpeed <= 0.0f || !GetWorld())
    {
        return;
    }

    const float Elapsed = GetWorld()->GetTimeSeconds() - ActivationTime;
    FVector NewLocation = HomeLocation;

    switch (MovementMode)
    {
    case EAimTargetMovement::Slide:
    {
        const float Phase = FMath::Frac((Elapsed * (0.72f + TravelSpeed * 0.12f)) + OscillationOffset / (2.0f * PI));
        const float LegProgress = Phase < 0.5f ? Phase * 2.0f : (Phase - 0.5f) * 2.0f;
        const float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, LegProgress, 2.8f);
        const float SlideOffset = Phase < 0.5f
            ? FMath::Lerp(-TravelDistance, TravelDistance, EasedProgress)
            : FMath::Lerp(TravelDistance, -TravelDistance, EasedProgress);
        NewLocation += TravelDirection * SlideOffset;
        NewLocation.Z -= 95.0f;
        break;
    }
    case EAimTargetMovement::JumpPull:
    {
        const float Phase = FMath::Frac((Elapsed / 1.85f) + OscillationOffset / (2.0f * PI));
        const float LegProgress = Phase < 0.5f ? Phase * 2.0f : (Phase - 0.5f) * 2.0f;
        const float PullProgress = Phase < 0.5f
            ? FMath::InterpEaseOut(0.0f, 1.0f, LegProgress, 1.6f)
            : FMath::InterpEaseIn(0.0f, 1.0f, LegProgress, 3.0f);
        const float PullOffset = Phase < 0.5f
            ? FMath::Lerp(-TravelDistance, TravelDistance, PullProgress)
            : FMath::Lerp(TravelDistance, -TravelDistance, PullProgress);
        const float JumpHeight = FMath::Sin(Phase * PI) * FMath::Max(260.0f, TravelDistance * 1.8f);
        NewLocation += TravelDirection * PullOffset + FVector::UpVector * JumpHeight;
        break;
    }
    case EAimTargetMovement::Strafe:
    default:
    {
        const float Offset = FMath::Sin(Elapsed * TravelSpeed + OscillationOffset) * TravelDistance;
        NewLocation += TravelDirection * Offset;
        break;
    }
    }

    SetActorLocation(NewLocation);
}
