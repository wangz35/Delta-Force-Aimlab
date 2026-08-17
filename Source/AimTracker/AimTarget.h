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
    void Activate(const FVector& NewLocation, float NewRadius, float NewSpeed, const FVector& NewTravelDirection, EAimTargetMovement NewMovementMode, bool bInPersistentOnHit, bool bInHumanoid);
    void MarkHit();
    void SetPersistentHover(bool bHovered);
    bool PersistsOnHit() const { return bPersistentOnHit; }
    float GetActivationTime() const { return ActivationTime; }

private:
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
    bool bPersistentOnHit = false;
    bool bHasBeenHit = false;
    bool bHumanoid = false;
    EAimTargetMovement MovementMode = EAimTargetMovement::Strafe;
};