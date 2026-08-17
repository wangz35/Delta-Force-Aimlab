#include "AimTarget.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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

void AAimTrainingTarget::Activate(const FVector& NewLocation, float NewRadius, float NewSpeed, const FVector& NewTravelDirection, EAimTargetMovement NewMovementMode, bool bInPersistentOnHit, bool bInHumanoid)
{
    HomeLocation = NewLocation;
    TravelDirection = NewTravelDirection.GetSafeNormal();
    TravelDistance = bInPersistentOnHit ? FMath::FRandRange(120.0f, 420.0f) : 0.0f;
    TravelSpeed = NewSpeed;
    OscillationOffset = FMath::FRandRange(0.0f, 2.0f * PI);
    MovementMode = NewMovementMode;
    bPersistentOnHit = bInPersistentOnHit;
    bHasBeenHit = false;
    bHumanoid = bInHumanoid;
    ActivationTime = GetWorld()->GetTimeSeconds();

    const FLinearColor Yellow(1.0f, 0.72f, 0.03f);
    if (OuterMaterial) OuterMaterial->SetVectorParameterValue(TEXT("Color"), Yellow);
    if (CoreMaterial) CoreMaterial->SetVectorParameterValue(TEXT("Color"), Yellow);
    Glow->SetLightColor(Yellow);
    Glow->SetIntensity(3500.0f);
    SetActorLocation(HomeLocation);
    OuterRing->SetWorldScale3D(FVector(NewRadius / 50.0f));

    if (bHumanoid)
    {
        // Root sphere is the only collision component: the body is visual-only, so only headshots count.
        Core->SetRelativeLocation(FVector(0.0f, 0.0f, -NewRadius * 2.9f));
        Core->SetRelativeScale3D(FVector(0.68f, 0.46f, 2.35f));
    }
    else
    {
        Core->SetRelativeLocation(FVector(-NewRadius * 0.28f, 0.0f, 0.0f));
        Core->SetRelativeScale3D(FVector(0.38f));
    }
}

void AAimTrainingTarget::SetPersistentHover(bool bHovered)
{
    if (!bPersistentOnHit) return;
    const FLinearColor Yellow(1.0f, 0.72f, 0.03f);
    const FLinearColor Blue(0.03f, 0.38f, 1.0f);
    const FLinearColor Color = bHovered ? Blue : Yellow;
    if (OuterMaterial) OuterMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    if (CoreMaterial) CoreMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    Glow->SetLightColor(Color);
    Glow->SetIntensity(bHovered ? 6000.0f : 3500.0f);
}

void AAimTrainingTarget::MarkHit()
{
    const FLinearColor Blue(0.03f, 0.38f, 1.0f);
    if (OuterMaterial) OuterMaterial->SetVectorParameterValue(TEXT("Color"), Blue);
    if (CoreMaterial) CoreMaterial->SetVectorParameterValue(TEXT("Color"), Blue);
    Glow->SetLightColor(Blue);
    Glow->SetIntensity(6000.0f);
}

void AAimTrainingTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPersistentOnHit || TravelSpeed <= 0.0f || !GetWorld()) return;

    // The legacy ball keeps its earlier bounded random left/right and vertical motion.
    const float Elapsed = GetWorld()->GetTimeSeconds() - ActivationTime;
    FVector NewLocation = HomeLocation;
    NewLocation += TravelDirection * FMath::Sin(Elapsed * TravelSpeed + OscillationOffset) * TravelDistance;
    NewLocation.Z += FMath::Sin(Elapsed * 1.25f + OscillationOffset) * 240.0f;
    SetActorLocation(NewLocation);
}