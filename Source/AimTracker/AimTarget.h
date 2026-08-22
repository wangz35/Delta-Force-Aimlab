#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AimTarget.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class EAimTargetMovement : uint8
{
    Strafe,
    Slide,
    JumpPull
};

UCLASS()
class AIMTRACKER_API AAimTrainingTarget : public AActor
{
    GENERATED_BODY()

public:
    AAimTrainingTarget();
    virtual void Tick(float DeltaSeconds) override;

    void Activate(const FVector& NewLocation, float NewRadius, float NewSpeed, const FVector& NewTravelDirection, EAimTargetMovement NewMovementMode, bool bInPersistentOnHit, bool bInHumanoid, bool bInHorizontalOnly = false);
    void ActivateJumpArc(const FVector& StartLocation, const FVector& LandingLocation, float ArcHeight, float Duration);
    void ActivateHorizontalGaze(const FVector& NewLocation, float NewRadius, float NewSpeed, float NewTravelDistance);
    void MarkHit();
    void SetPersistentHover(bool bHovered);
    bool AddGazeFocus(float DeltaSeconds);
    bool PersistsOnHit() const { return bPersistentOnHit; }
    bool IsGazeTarget() const { return bGazeTarget; }
    float GetActivationTime() const { return ActivationTime; }

private:
    void ApplyTargetColor(const FLinearColor& Color, float GlowIntensity);

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> OuterRing;
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UPointLightComponent> Glow;

    TObjectPtr<UMaterialInstanceDynamic> OuterMaterial;
    TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
    FVector HomeLocation = FVector::ZeroVector;
    FVector TravelDirection = FVector::RightVector;
    float TravelDistance = 0.0f;
    float TravelSpeed = 0.0f;
    float ActivationTime = 0.0f;
    float OscillationOffset = 0.0f;
    float CurrentHorizontalOffset = 0.0f;
    float HorizontalDirectionSign = 1.0f;
    float DirectionChangeRemaining = 0.0f;
    FVector JumpStartLocation = FVector::ZeroVector;
    FVector JumpLandingLocation = FVector::ZeroVector;
    float JumpArcHeight = 0.0f;
    float JumpArcDuration = 1.0f;
    float JumpArcElapsed = 0.0f;
    float GazeFocusSeconds = 0.0f;
    bool bPersistentOnHit = false;
    bool bHasBeenHit = false;
    bool bHumanoid = false;
    bool bHorizontalOnly = false;
    bool bJumpArcActive = false;
    bool bGazeTarget = false;
    EAimTargetMovement MovementMode = EAimTargetMovement::Strafe;
};