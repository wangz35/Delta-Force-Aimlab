#include "AimTarget.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const FLinearColor TargetYellow(1.0f, 0.72f, 0.03f);
    const FLinearColor TargetBlue(0.03f, 0.38f, 1.0f);
}

AAimTrainingTarget::AAimTrainingTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    OuterRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadHitbox"));
    SetRootComponent(OuterRing);
    OuterRing->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    OuterRing->SetCollisionResponseToAllChannels(ECR_Ignore);
    OuterRing->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    OuterRing->SetGenerateOverlapEvents(false);

    Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetBody"));
    Core->SetupAttachment(OuterRing);
    Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("TargetGlow"));
    Glow->SetupAttachment(OuterRing);
    Glow->SetIntensity(3500.0f);
    Glow->SetAttenuationRadius(500.0f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        OuterRing->SetStaticMesh(SphereMesh.Object);
        Core->SetStaticMesh(SphereMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (ShapeMaterial.Succeeded())
    {
        OuterMaterial = UMaterialInstanceDynamic::Create(ShapeMaterial.Object, this);
        CoreMaterial = UMaterialInstanceDynamic::Create(ShapeMaterial.Object, this);
        OuterRing->SetMaterial(0, OuterMaterial);
        Core->SetMaterial(0, CoreMaterial);
    }
}

void AAimTrainingTarget::ApplyTargetColor(const FLinearColor& Color, float GlowIntensity)
{
    if (OuterMaterial) OuterMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    if (CoreMaterial) CoreMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    Glow->SetLightColor(Color);
    Glow->SetIntensity(GlowIntensity);
}

void AAimTrainingTarget::Activate(const FVector& NewLocation, float NewRadius, float NewSpeed, const FVector& NewTravelDirection, EAimTargetMovement NewMovementMode, bool bInPersistentOnHit, bool bInHumanoid, bool bInHorizontalOnly)
{
    HomeLocation = NewLocation;
    TravelDirection = NewTravelDirection.GetSafeNormal();
    TravelDistance = bInPersistentOnHit ? (bInHorizontalOnly ? 2000.0f : FMath::FRandRange(750.0f, 1250.0f)) : 0.0f;
    TravelSpeed = NewSpeed;
    OscillationOffset = FMath::FRandRange(0.0f, 2.0f * PI);
    CurrentHorizontalOffset = bInHorizontalOnly ? FMath::FRandRange(-TravelDistance * 0.35f, TravelDistance * 0.35f) : 0.0f;
    HorizontalDirectionSign = FMath::RandBool() ? 1.0f : -1.0f;
    DirectionChangeRemaining = FMath::FRandRange(0.65f, 1.75f);
    MovementMode = NewMovementMode;
    bPersistentOnHit = bInPersistentOnHit;
    bHasBeenHit = false;
    bHumanoid = bInHumanoid;
    bHorizontalOnly = bInHorizontalOnly;
    bJumpArcActive = false;
    bGazeTarget = false;
    GazeFocusSeconds = 0.0f;
    ActivationTime = GetWorld()->GetTimeSeconds();

    Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ApplyTargetColor(TargetYellow, 3500.0f);
    SetActorLocation(HomeLocation);
    OuterRing->SetWorldScale3D(FVector(NewRadius / 50.0f));

    if (bHumanoid)
    {
        Core->SetRelativeLocation(FVector(0.0f, 0.0f, -NewRadius * 11.85f));
        Core->SetRelativeScale3D(FVector(0.68f, 0.46f, 7.05f));
    }
    else
    {
        Core->SetRelativeLocation(FVector(-NewRadius * 0.28f, 0.0f, 0.0f));
        Core->SetRelativeScale3D(FVector(0.38f));
    }
}

void AAimTrainingTarget::ActivateJumpArc(const FVector& StartLocation, const FVector& LandingLocation, float ArcHeight, float Duration)
{
    Activate(StartLocation, 34.0f, 0.0f, FVector::RightVector, EAimTargetMovement::JumpPull, false, true, false);
    JumpStartLocation = StartLocation;
    JumpLandingLocation = LandingLocation;
    JumpArcHeight = ArcHeight;
    JumpArcDuration = FMath::Max(0.1f, Duration);
    JumpArcElapsed = 0.0f;
    bJumpArcActive = true;
    bGazeTarget = true;
    Core->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Core->SetCollisionResponseToAllChannels(ECR_Ignore);
    Core->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AAimTrainingTarget::SetPersistentHover(bool bHovered)
{
    if (!bPersistentOnHit) return;
    ApplyTargetColor(bHovered ? TargetBlue : TargetYellow, bHovered ? 6000.0f : 3500.0f);
}

bool AAimTrainingTarget::AddGazeFocus(float DeltaSeconds)
{
    if (!bGazeTarget) return false;
    GazeFocusSeconds += FMath::Max(0.0f, DeltaSeconds);
    const float Progress = FMath::Clamp(GazeFocusSeconds, 0.0f, 1.0f);
    ApplyTargetColor(FMath::Lerp(TargetYellow, TargetBlue, Progress), FMath::Lerp(3500.0f, 6000.0f, Progress));
    return GazeFocusSeconds >= 1.0f;
}

void AAimTrainingTarget::MarkHit()
{
    bHasBeenHit = true;
    ApplyTargetColor(TargetBlue, 6000.0f);
}

void AAimTrainingTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()) return;

    if (bJumpArcActive)
    {
        JumpArcElapsed += DeltaSeconds;
        const float Progress = FMath::Clamp(JumpArcElapsed / JumpArcDuration, 0.0f, 1.0f);
        FVector NewLocation = FMath::Lerp(JumpStartLocation, JumpLandingLocation, Progress);
        NewLocation.Z += 4.0f * JumpArcHeight * Progress * (1.0f - Progress);
        SetActorLocation(NewLocation);
        if (Progress >= 1.0f) bJumpArcActive = false;
        return;
    }

    if (!bPersistentOnHit || TravelSpeed <= 0.0f) return;

    FVector NewLocation = HomeLocation;
    if (bHorizontalOnly)
    {
        DirectionChangeRemaining -= DeltaSeconds;
        if (DirectionChangeRemaining <= 0.0f)
        {
            HorizontalDirectionSign *= -1.0f;
            DirectionChangeRemaining = FMath::FRandRange(0.65f, 1.75f);
        }

        CurrentHorizontalOffset += HorizontalDirectionSign * TravelSpeed * DeltaSeconds;
        if (CurrentHorizontalOffset >= TravelDistance)
        {
            CurrentHorizontalOffset = TravelDistance;
            HorizontalDirectionSign = -1.0f;
            DirectionChangeRemaining = FMath::FRandRange(0.65f, 1.75f);
        }
        else if (CurrentHorizontalOffset <= -TravelDistance)
        {
            CurrentHorizontalOffset = -TravelDistance;
            HorizontalDirectionSign = 1.0f;
            DirectionChangeRemaining = FMath::FRandRange(0.65f, 1.75f);
        }
        NewLocation += TravelDirection * CurrentHorizontalOffset;
    }
    else
    {
        const float Elapsed = GetWorld()->GetTimeSeconds() - ActivationTime;
        NewLocation += TravelDirection * FMath::PerlinNoise1D((Elapsed * TravelSpeed * 0.40f) + OscillationOffset) * TravelDistance;
        NewLocation.Z += FMath::Sin(Elapsed * 1.25f + OscillationOffset) * 240.0f;
    }
    SetActorLocation(NewLocation);
}