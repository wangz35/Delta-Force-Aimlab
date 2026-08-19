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
    Camera->SetFieldOfView(BaseFieldOfView);
    Camera->bUsePawnControlRotation = true;

    FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
    FloatingMovement->MaxSpeed = MoveSpeed;
    FloatingMovement->Acceleration = 8000.0f;
    FloatingMovement->Deceleration = 10000.0f;

    RecoilYawPattern = { 0.00f, 0.024f, 0.042f, 0.060f, 0.072f, 0.072f, 0.060f, 0.048f, 0.042f, 0.036f, 0.030f, 0.024f, 0.018f, 0.012f, 0.006f, -0.006f, -0.018f, -0.036f, -0.054f, -0.072f, -0.072f, -0.054f, -0.036f, -0.018f, -0.006f, 0.006f, 0.018f, 0.036f, 0.054f, 0.072f, 0.072f, 0.054f, 0.036f, 0.018f, 0.006f };

    AutoPossessPlayer = EAutoReceiveInput::Player0;
    bUseControllerRotationPitch = true;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;
}

float AAimTrainerPawn::GetMouseSensitivity() const
{
    switch (ZoomLevel)
    {
    case 1: return MouseSensitivity3_5x;
    case 2: return MouseSensitivity7_25x;
    default: return MouseSensitivity1x;
    }
}

float& AAimTrainerPawn::GetActiveMouseSensitivity()
{
    switch (ZoomLevel)
    {
    case 1: return MouseSensitivity3_5x;
    case 2: return MouseSensitivity7_25x;
    default: return MouseSensitivity1x;
    }
}

float AAimTrainerPawn::GetScopeFieldOfView() const
{
    const float Magnification = GetZoomMultiplier();
    const float HipHalfFovRadians = FMath::DegreesToRadians(BaseFieldOfView * 0.5f);
    return FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(HipHalfFovRadians) / Magnification));
}

float AAimTrainerPawn::GetMonitorDistanceSensitivityScale() const
{
    if (ZoomLevel == 0) return 1.0f;
    const float HipHalfFovRadians = FMath::DegreesToRadians(BaseFieldOfView * 0.5f);
    const float ScopeHalfFovRadians = FMath::DegreesToRadians(GetScopeFieldOfView() * 0.5f);
    const float HipTangent = FMath::Tan(HipHalfFovRadians);
    const float ScopeTangent = FMath::Tan(ScopeHalfFovRadians);
    const float ReferenceDistance = FMath::Max(0.0f, MonitorDistanceCoefficient * 0.5f);
    // Monitor-distance matching at a configurable vertical-screen reference distance.
    return (ScopeTangent / HipTangent) * (1.0f + FMath::Square(ReferenceDistance * HipTangent)) / (1.0f + FMath::Square(ReferenceDistance * ScopeTangent));
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

float AAimTrainerPawn::GetZoomMultiplier() const
{
    static constexpr float ZoomMultipliers[] = { 1.0f, 3.5f, 7.25f };
    return ZoomMultipliers[FMath::Clamp(ZoomLevel, 0, UE_ARRAY_COUNT(ZoomMultipliers) - 1)];
}

void AAimTrainerPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const float LeanDirection = bLeanLeftHeld == bLeanRightHeld ? 0.0f : (bLeanRightHeld ? 1.0f : -1.0f);
    CurrentLeanAngle = FMath::FInterpTo(CurrentLeanAngle, LeanDirection * LeanAngle, DeltaSeconds, LeanSpeed);
    CurrentLeanOffset = FMath::FInterpTo(CurrentLeanOffset, LeanDirection * LeanSideOffset, DeltaSeconds, LeanSpeed);
    CurrentLeanYaw = FMath::FInterpTo(CurrentLeanYaw, LeanDirection * LeanAimYawDegrees, DeltaSeconds, LeanSpeed);
    if (Controller)
    {
        FRotator LeanControlRotation = Controller->GetControlRotation();
        LeanControlRotation.Roll = CurrentLeanAngle;
        LeanControlRotation.Yaw += CurrentLeanYaw - LastAppliedLeanYaw;
        Controller->SetControlRotation(LeanControlRotation);
    }
    LastAppliedLeanYaw = CurrentLeanYaw;
    FVector LeanCameraLocation = Camera->GetRelativeLocation();
    LeanCameraLocation.Y = CurrentLeanOffset;
    Camera->SetRelativeLocation(LeanCameraLocation);

    if (bIsFiring)
    {
        FireCooldown -= DeltaSeconds;
        while (FireCooldown <= 0.0f)
        {
            Fire();
            FireCooldown += FMath::Max(0.01f, FireInterval);
        }
    }

    if (AAimTrainerGameMode* HoverGameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        FHitResult HoverHit;
        FCollisionQueryParams HoverParams(SCENE_QUERY_STAT(AimTrainerHoverTrace), true, this);
        const FVector HoverStart = Camera->GetComponentLocation();
        const FVector HoverEnd = HoverStart + Camera->GetForwardVector() * TraceRange;
        const bool bHoveringDefaultTarget = GetWorld()->LineTraceSingleByChannel(HoverHit, HoverStart, HoverEnd, ECC_Visibility, HoverParams) && HoverHit.GetActor() == HoverGameMode->GetDefaultTarget();
        HoverGameMode->UpdateDefaultTargetHover(bHoveringDefaultTarget);
    }

    const float RecoilAlpha = FMath::Clamp(DeltaSeconds * RecoilSmoothingSpeed, 0.0f, 1.0f);
    const float AppliedRecoilYaw = PendingRecoilYaw * RecoilAlpha;
    const float AppliedRecoilPitch = PendingRecoilPitch * RecoilAlpha;
    AddControllerYawInput(AppliedRecoilYaw);
    AddControllerPitchInput(AppliedRecoilPitch);
    PendingRecoilYaw -= AppliedRecoilYaw;
    PendingRecoilPitch -= AppliedRecoilPitch;

    if (bScopedRecoilRecentering)
    {
        ScopedRecoilReturnYaw += AppliedRecoilYaw;
        ScopedRecoilReturnPitch += AppliedRecoilPitch;
        ScopedRecoilReturnDelay = FMath::Max(0.0f, ScopedRecoilReturnDelay - DeltaSeconds);
        if (ScopedRecoilReturnDelay <= 0.0f && FMath::Abs(PendingRecoilYaw) < 0.002f && FMath::Abs(PendingRecoilPitch) < 0.002f)
        {
            const float ReturnAlpha = FMath::Clamp(DeltaSeconds * 13.0f, 0.0f, 1.0f);
            const float ReturnYaw = ScopedRecoilReturnYaw * ReturnAlpha;
            const float ReturnPitch = ScopedRecoilReturnPitch * ReturnAlpha;
            AddControllerYawInput(-ReturnYaw);
            AddControllerPitchInput(-ReturnPitch);
            ScopedRecoilReturnYaw -= ReturnYaw;
            ScopedRecoilReturnPitch -= ReturnPitch;
            if (FMath::Abs(ScopedRecoilReturnYaw) < 0.001f && FMath::Abs(ScopedRecoilReturnPitch) < 0.001f)
            {
                bScopedRecoilRecentering = false;
            }
        }
    }

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
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AAimTrainerPawn::ToggleZoom);
    PlayerInputComponent->BindAction(TEXT("LeanLeft"), IE_Pressed, this, &AAimTrainerPawn::BeginLeanLeft);
    PlayerInputComponent->BindAction(TEXT("LeanLeft"), IE_Released, this, &AAimTrainerPawn::EndLeanLeft);
    PlayerInputComponent->BindAction(TEXT("LeanRight"), IE_Pressed, this, &AAimTrainerPawn::BeginLeanRight);
    PlayerInputComponent->BindAction(TEXT("LeanRight"), IE_Released, this, &AAimTrainerPawn::EndLeanRight);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &AAimTrainerPawn::BeginSprint);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &AAimTrainerPawn::EndSprint);
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
    AddControllerYawInput(Value * GetMouseSensitivity() * GetMonitorDistanceSensitivityScale());
}

void AAimTrainerPawn::LookUp(float Value)
{
    AddControllerPitchInput(Value * GetMouseSensitivity() * GetMonitorDistanceSensitivityScale() * VerticalSensitivityMultiplier);
}

void AAimTrainerPawn::BeginFire()
{
    if (GetZoomMultiplier() > 1.0f)
    {
        Fire();
        // Scoped fire is click-to-fire: holding the mouse cannot start an automatic burst.
        bIsFiring = false;
        FireCooldown = 0.0f;
        RecoilShotIndex = 0;
        ScopedRecoilReturnYaw = 0.0f;
        ScopedRecoilReturnPitch = 0.0f;
        ScopedRecoilReturnDelay = 0.08f;
        bScopedRecoilRecentering = true;
        return;
    }

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

    if (!RecoilYawPattern.IsEmpty())
    {
        PendingRecoilYaw -= RecoilYawPattern[RecoilShotIndex % RecoilYawPattern.Num()];
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
    float& ActiveSensitivity = GetActiveMouseSensitivity();
    ActiveSensitivity = FMath::Clamp(ActiveSensitivity - SensitivityStep, MinMouseSensitivity, MaxMouseSensitivity);
}

void AAimTrainerPawn::IncreaseSensitivity()
{
    float& ActiveSensitivity = GetActiveMouseSensitivity();
    ActiveSensitivity = FMath::Clamp(ActiveSensitivity + SensitivityStep, MinMouseSensitivity, MaxMouseSensitivity);
}

void AAimTrainerPawn::ToggleZoom()
{
    ZoomLevel = (ZoomLevel + 1) % 3;
    Camera->SetFieldOfView(GetScopeFieldOfView());
}

void AAimTrainerPawn::BeginLeanLeft(){ bLeanLeftHeld = true; }
void AAimTrainerPawn::EndLeanLeft(){ bLeanLeftHeld = false; }
void AAimTrainerPawn::BeginLeanRight(){ bLeanRightHeld = true; }
void AAimTrainerPawn::EndLeanRight(){ bLeanRightHeld = false; }

void AAimTrainerPawn::BeginSprint()
{
    bIsSprinting = true;
    FloatingMovement->MaxSpeed = MoveSpeed * SprintSpeedMultiplier;
}

void AAimTrainerPawn::EndSprint()
{
    bIsSprinting = false;
    FloatingMovement->MaxSpeed = MoveSpeed;
}

void AAimTrainerPawn::RestartTraining()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GameMode->RestartSession();
    }
}
