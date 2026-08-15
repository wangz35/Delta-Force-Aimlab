#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AimTrainerPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UFloatingPawnMovement;

UCLASS()
class AIMTRACKER_API AAimTrainerPawn : public APawn
{
    GENERATED_BODY()

public:
    AAimTrainerPawn();

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    float GetMouseSensitivity() const { return MouseSensitivity; }
    float GetJumpCrosshairOffset() const;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void Fire();
    void BeginSlide();
    void EndSlide();
    void BeginJump();
    void DecreaseSensitivity();
    void IncreaseSensitivity();
    void RestartTraining();

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UCapsuleComponent> Capsule;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UFloatingPawnMovement> FloatingMovement;

    UPROPERTY(EditDefaultsOnly, Category = "Training")
    float TraceRange = 18000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Training")
    float MoveSpeed = 1200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float MouseSensitivity = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float SensitivityStep = 0.10f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float MinMouseSensitivity = 0.20f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float MaxMouseSensitivity = 3.00f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideDuration = 1.44f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideInitialSpeed = 2820.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float StandingCameraHeight = 58.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlidingCameraHeight = -72.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Jump")
    float JumpInitialVelocity = 760.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideCameraDropDuration = 0.10f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float JumpCrosshairKickPixels = 26.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Jump")
    float JumpGravity = 2500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Jump")
    float AirJumpForwardDeceleration = 600.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide Jump")
    float SlideJumpInitialVelocity = 800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide Jump")
    float SlideJumpMinimumForwardSpeed = 1450.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide Jump")
    float SlideJumpForwardDeceleration = 600.0f;

    FVector SlideDirection = FVector::ForwardVector;
    FVector SlideJumpDirection = FVector::ForwardVector;
    FVector AirJumpDirection = FVector::ForwardVector;
    float SlideElapsed = 0.0f;
    float JumpBaseHeight = 0.0f;
    float JumpVerticalVelocity = 0.0f;
    float SlideJumpForwardSpeed = 0.0f;
    float AirJumpForwardSpeed = 0.0f;
    bool bIsSliding = false;
    bool bIsJumping = false;
    bool bIsSlideJumping = false;
};
