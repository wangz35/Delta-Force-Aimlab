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
    float GetZoomMultiplier() const;
    float GetJumpCrosshairOffset() const;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void BeginFire();
    void EndFire();
    void Fire();
    void BeginSlide();
    void EndSlide();
    void BeginJump();
    void LaunchJump(bool bFromSlide);
    void DecreaseSensitivity();
    void IncreaseSensitivity();
    void RestartTraining();
    void ToggleZoom();
    void BeginLeanLeft();
    void EndLeanLeft();
    void BeginLeanRight();
    void EndLeanRight();
    void BeginSprint();
    void EndSprint();

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

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Sprint")
    float SprintSpeedMultiplier = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float MouseSensitivity = 0.50f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float SensitivityStep = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float MinMouseSensitivity = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float MaxMouseSensitivity = 0.50f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float VerticalSensitivityMultiplier = 1.30f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming|Zoom")
    float BaseFieldOfView = 90.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Lean")
    float LeanAngle = 12.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Lean")
    float LeanSpeed = 7.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Lean")
    float LeanSideOffset = 18.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float FireInterval = 0.064516f;

    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float RecoilVerticalKick = 0.29f;

    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float RecoilSmoothingSpeed = 18.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideDuration = 0.70f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideInitialSpeed = 4230.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideBurstDuration = 0.08f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideBaseSpeed = 2700.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float StandingCameraHeight = 28.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlidingCameraHeight = -200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Jump")
    float JumpInitialVelocity = 2051.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
    float SlideCameraDropDuration = 0.10f;

    UPROPERTY(EditDefaultsOnly, Category = "Aiming")
    float JumpCrosshairKickPixels = 26.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Jump")
    float JumpGravity = 5600.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Jump")
    float AirJumpForwardDeceleration = 600.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide Jump")
    float SlideJumpInitialVelocity = 2157.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide Jump")
    float SlideJumpMinimumForwardSpeed = 1450.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide Jump")
    float SlideJumpForwardDeceleration = 600.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide Jump")
    float SlideJumpMomentumMultiplier = 1.35f;

    FVector SlideDirection = FVector::ForwardVector;
    FVector SlideJumpDirection = FVector::ForwardVector;
    FVector AirJumpDirection = FVector::ForwardVector;
    float SlideElapsed = 0.0f;
    float JumpBaseHeight = 0.0f;
    float JumpVerticalVelocity = 0.0f;
    float SlideJumpForwardSpeed = 0.0f;
    float AirJumpForwardSpeed = 0.0f;
    float FireCooldown = 0.0f;
    int32 RecoilShotIndex = 0;
    float PendingRecoilYaw = 0.0f;
    float PendingRecoilPitch = 0.0f;
    int32 ZoomLevel = 0;
    float CurrentLeanAngle = 0.0f;
    float CurrentLeanOffset = 0.0f;
    bool bLeanLeftHeld = false;
    bool bLeanRightHeld = false;
    TArray<float> RecoilYawPattern;
    bool bIsSliding = false;
    bool bIsJumping = false;
    bool bIsSlideJumping = false;
    bool bQueuedSlideJump = false;
    bool bIsFiring = false;
    bool bIsSprinting = false;
};
