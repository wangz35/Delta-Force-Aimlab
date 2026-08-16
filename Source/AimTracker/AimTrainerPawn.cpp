#include "AimTrainerPawn.h"

#include "AimTrainerGameMode.h"
#include "AimTarget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"

AAimTrainerPawn::AAimTrainerPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("PlayerCapsule"));
    Capsule->InitCapsuleSize(34.0f, 86.0f);
    Capsule->SetCollisionProfileName(TEXT("Pawn"));
    SetRootComponent(Capsule);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(Capsule);
    Camera->SetRelativeLocation(FVector(0.0f, 0.0f, StandingCameraHeight));
    Camera->bUsePawnControlRotation = true;

    FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
    FloatingMovement->MaxSpeed = MoveSpeed;
    FloatingMovement->Acceleration = 8000.0f;
    FloatingMovement->Deceleration = 10000.0f;

    RecoilYawPattern = { 0.00f, 0.10f, 0.18f, 0.22f, 0.22f, 0.24f, 0.22f, 0.14f, -0.12f, -0.12f, 0.14f, 0.14f, 0.10f, -0.08f, -0.14f, -0.16f, -0.20f, -0.20f, -0.16f, -0.14f, -0.10f, 0.08f, 0.12f, 0.16f, 0.18f, 0.18f, 0.14f, 0.10f, 0.08f };

    AutoPossessPlayer = EAutoReceiveInput::Player0;
    bUseControllerRotationPitch = true;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;
}

float AAimTrainerPawn::GetJumpCrosshairOffset() const
{
    if (!bIsJumping || JumpVerticalVelocity <= 0.0f)
    {
        return 0.0f;
    }

    const float LaunchVelocity = bIsSlideJumping ? SlideJumpInitialVelocity : JumpInitialVelocity;
    const float AscentProgress = 1.0f - FMath::Clamp(JumpVerticalVelocity / LaunchVelocity, 0.0f, 1.0f);
    if (AscentProgress <= 0.45f)
    {
        return FMath::Lerp(JumpCrosshairKickPixels, -JumpCrosshairKickPixels * 0.65f, AscentProgress / 0.45f);
    }
    return FMath::Lerp(-JumpCrosshairKickPixels * 0.65f, 0.0f, (AscentProgress - 0.45f) / 0.55f);
}

void AAimTrainerPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bIsFiring)
    {
        FireCooldown -= DeltaSeconds;
        while (FireCooldown <= 0.0f)
        {
            Fire();
            FireCooldown += FMath::Max(0.01f, FireInterval);
        }
    }

    const float RecoilAlpha = FMath::Clamp(DeltaSeconds * RecoilSmoothingSpeed, 0.0f, 1.0f);
    const float AppliedRecoilYaw = PendingRecoilYaw * RecoilAlpha;
    const float AppliedRecoilPitch = PendingRecoilPitch * RecoilAlpha;
    AddControllerYawInput(AppliedRecoilYaw);
    AddControllerPitchInput(AppliedRecoilPitch);
    PendingRecoilYaw -= AppliedRecoilYaw;
    PendingRecoilPitch -= AppliedRecoilPitch;

    if (bIsSliding)
    {
        SlideElapsed += DeltaSeconds;
        const float Progress = FMath::Clamp(SlideElapsed / SlideDuration, 0.0f, 1.0f);
        const float Speed = SlideElapsed <= SlideBurstDuration ? SlideInitialSpeed : SlideBaseSpeed;
        AddActorWorldOffset(SlideDirection * Speed * DeltaSeconds, true);
        const float RecoveryProgress = FMath::Clamp((SlideElapsed - SlideCameraDropDuration) / FMath::Max(0.01f, SlideDuration - SlideCameraDropDuration), 0.0f, 1.0f);
        const float CameraHeight = SlideElapsed <= SlideCameraDropDuration
            ? FMath::Lerp(StandingCameraHeight, SlidingCameraHeight, SlideElapsed / SlideCameraDropDuration)
            : FMath::Lerp(SlidingCameraHeight, StandingCameraHeight, RecoveryProgress);
        Camera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraHeight));

        if (Progress >= 1.0f)
        {
            EndSlide();
        }
    }

    if (bIsJumping)
    {
        if (bIsSlideJumping && SlideJumpForwardSpeed > 0.0f)
        {
            AddActorWorldOffset(SlideJumpDirection * SlideJumpForwardSpeed * DeltaSeconds, true);
            SlideJumpForwardSpeed = FMath::Max(0.0f, SlideJumpForwardSpeed - SlideJumpForwardDeceleration * DeltaSeconds);
        }
        else if (AirJumpForwardSpeed > 0.0f)
        {
            AddActorWorldOffset(AirJumpDirection * AirJumpForwardSpeed * DeltaSeconds, true);
            AirJumpForwardSpeed = FMath::Max(0.0f, AirJumpForwardSpeed - AirJumpForwardDeceleration * DeltaSeconds);
        }

        JumpVerticalVelocity -= JumpGravity * DeltaSeconds;
        const FVector CurrentLocation = GetActorLocation();
        const float NextHeight = CurrentLocation.Z + JumpVerticalVelocity * DeltaSeconds;
        if (NextHeight <= JumpBaseHeight)
        {
            SetActorLocation(FVector(CurrentLocation.X, CurrentLocation.Y, JumpBaseHeight), true);
            bIsJumping = false;
            bIsSlideJumping = false;
            JumpVerticalVelocity = 0.0f;
            SlideJumpForwardSpeed = 0.0f;
            AirJumpForwardSpeed = 0.0f;
        }
        else
        {
            AddActorWorldOffset(FVector::UpVector * JumpVerticalVelocity * DeltaSeconds, true);
        }
    }
}

void AAimTrainerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    check(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AAimTrainerPawn::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AAimTrainerPawn::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AAimTrainerPawn::Turn);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AAimTrainerPawn::LookUp);
    PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AAimTrainerPawn::BeginFire);
    PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &AAimTrainerPawn::EndFire);
    PlayerInputComponent->BindAction(TEXT("Slide"), IE_Pressed, this, &AAimTrainerPawn::BeginSlide);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AAimTrainerPawn::BeginJump);
    PlayerInputComponent->BindAction(TEXT("SensitivityDown"), IE_Pressed, this, &AAimTrainerPawn::DecreaseSensitivity);
    PlayerInputComponent->BindAction(TEXT("SensitivityUp"), IE_Pressed, this, &AAimTrainerPawn::IncreaseSensitivity);
    PlayerInputComponent->BindAction(TEXT("Restart"), IE_Pressed, this, &AAimTrainerPawn::RestartTraining);
}

void AAimTrainerPawn::MoveForward(float Value)
{
    if (bIsSliding || bIsJumping || FMath::IsNearlyZero(Value))
    {
        return;
    }

    const FRotator FlatRotation(0.0f, Controller ? Controller->GetControlRotation().Yaw : 0.0f, 0.0f);
    AddMovementInput(FlatRotation.Vector(), Value);
}

void AAimTrainerPawn::MoveRight(float Value)
{
    if (bIsSliding || bIsJumping || FMath::IsNearlyZero(Value))
    {
        return;
    }

    const FRotator FlatRotation(0.0f, Controller ? Controller->GetControlRotation().Yaw : 0.0f, 0.0f);
    AddMovementInput(FRotationMatrix(FlatRotation).GetUnitAxis(EAxis::Y), Value);
}

void AAimTrainerPawn::Turn(float Value)
{
    AddControllerYawInput(Value * MouseSensitivity);
}

void AAimTrainerPawn::LookUp(float Value)
{
    AddControllerPitchInput(Value * MouseSensitivity * VerticalSensitivityMultiplier);
}

void AAimTrainerPawn::BeginFire()
{
    if (!bIsFiring)
    {
        bIsFiring = true;
        FireCooldown = 0.0f;
    }
}

void AAimTrainerPawn::EndFire()
{
    bIsFiring = false;
    FireCooldown = 0.0f;
    RecoilShotIndex = 0;
}

void AAimTrainerPawn::Fire()
{
    AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GameMode || !GameMode->IsSessionActive())
    {
        return;
    }

    if (RecoilYawPattern.IsValidIndex(RecoilShotIndex))
    {
        PendingRecoilYaw -= RecoilYawPattern[RecoilShotIndex];
    }
    PendingRecoilPitch -= RecoilVerticalKick;
    ++RecoilShotIndex;

    const FVector TraceStart = Camera->GetComponentLocation();
    const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * TraceRange;
    FHitResult Hit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AimTrainerTrace), true, this);
    TraceParams.bReturnPhysicalMaterial = false;

    const bool bHitSomething = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
    if (bHitSomething)
    {
        if (AAimTrainingTarget* Target = Cast<AAimTrainingTarget>(Hit.GetActor()))
        {
            GameMode->RegisterHit(Target);
            return;
        }
    }

    GameMode->RegisterMiss();
}

void AAimTrainerPawn::BeginSlide()
{
    if (bIsSliding || bIsJumping || !Controller)
    {
        return;
    }

    const FRotator FlatRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    SlideDirection = FlatRotation.Vector().GetSafeNormal();
    SlideElapsed = 0.0f;
    bIsSliding = true;
    FloatingMovement->StopMovementImmediately();
}

void AAimTrainerPawn::EndSlide()
{
    const bool bLaunchQueuedJump = bQueuedSlideJump;
    bQueuedSlideJump = false;
    bIsSliding = false;
    Camera->SetRelativeLocation(FVector(0.0f, 0.0f, StandingCameraHeight));
    if (bLaunchQueuedJump)
    {
        LaunchJump(true);
    }
}

void AAimTrainerPawn::BeginJump()
{
    if (bIsJumping)
    {
        return;
    }

    if (bIsSliding)
    {
        bQueuedSlideJump = true;
        return;
    }

    LaunchJump(false);
}

void AAimTrainerPawn::LaunchJump(bool bFromSlide)
{
    const FVector HorizontalVelocity(FloatingMovement->Velocity.X, FloatingMovement->Velocity.Y, 0.0f);
    FloatingMovement->StopMovementImmediately();

    bIsSlideJumping = bFromSlide;
    if (bIsSlideJumping)
    {
        const float SlideProgress = FMath::Clamp(SlideElapsed / SlideDuration, 0.0f, 1.0f);
        const float CurrentSlideSpeed = SlideElapsed <= SlideBurstDuration ? SlideInitialSpeed : SlideBaseSpeed;
        SlideJumpDirection = SlideDirection;
        SlideJumpForwardSpeed = FMath::Max(SlideJumpMinimumForwardSpeed, CurrentSlideSpeed * 0.84f) * SlideJumpMomentumMultiplier;
    }
    else
    {
        AirJumpDirection = HorizontalVelocity.GetSafeNormal();
        AirJumpForwardSpeed = HorizontalVelocity.Size();
    }

    JumpBaseHeight = GetActorLocation().Z;
    JumpVerticalVelocity = bIsSlideJumping ? SlideJumpInitialVelocity : JumpInitialVelocity;
    bIsJumping = true;
}void AAimTrainerPawn::DecreaseSensitivity()
{
    MouseSensitivity = FMath::Clamp(MouseSensitivity - SensitivityStep, MinMouseSensitivity, MaxMouseSensitivity);
}

void AAimTrainerPawn::IncreaseSensitivity()
{
    MouseSensitivity = FMath::Clamp(MouseSensitivity + SensitivityStep, MinMouseSensitivity, MaxMouseSensitivity);
}

void AAimTrainerPawn::RestartTraining()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GameMode->RestartSession();
    }
}
