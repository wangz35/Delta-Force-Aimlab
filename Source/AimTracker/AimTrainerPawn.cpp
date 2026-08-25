#include "AimTrainerPawn.h"

#include "AimTrainerGameMode.h"
#include "AimTarget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
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

    // Fire subtracts these samples: positive values pull left, negative values pull right.
    RecoilYawPattern = {
        // Shots 1-5: pull right (total -0.1925 degrees).
        -0.0250f, -0.0325f, -0.0400f, -0.0450f, -0.0500f,
        // Shots 6-15: pull left across the center (total +0.3850 degrees).
         0.0180f,  0.0260f,  0.0340f,  0.0420f,  0.0520f,
         0.0520f,  0.0480f,  0.0440f,  0.0380f,  0.0310f,
        // Shots 16-25: pull right across the center (total -0.3850 degrees).
        -0.0180f, -0.0260f, -0.0340f, -0.0420f, -0.0520f,
        -0.0520f, -0.0480f, -0.0440f, -0.0380f, -0.0310f,
        // Shots 26-35: pull left across the center (total +0.3850 degrees).
         0.0180f,  0.0260f,  0.0340f,  0.0420f,  0.0520f,
         0.0520f,  0.0480f,  0.0440f,  0.0380f,  0.0310f,
        // Shots 36-40: pull right and finish exactly at the starting horizontal position.
        -0.0500f, -0.0450f, -0.0400f, -0.0325f, -0.0250f
    };

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

    const float SafeSlideJumpAirtimeScale = FMath::Max(0.01f, SlideJumpAirtimeScale);
    const float LaunchVelocity = bIsSlideJumping ? SlideJumpInitialVelocity / SafeSlideJumpAirtimeScale : JumpInitialVelocity;
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

    const float ExpectedFieldOfView = GetScopeFieldOfView();
    if (!FMath::IsNearlyEqual(Camera->FieldOfView, ExpectedFieldOfView, 0.01f))
    {
        Camera->SetFieldOfView(ExpectedFieldOfView);
    }

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
        AAimTrainingTarget* HoveredTarget = nullptr;
        if (GetWorld()->LineTraceSingleByChannel(HoverHit, HoverStart, HoverEnd, ECC_Visibility, HoverParams))
        {
            HoveredTarget = Cast<AAimTrainingTarget>(HoverHit.GetActor());
        }
        HoverGameMode->UpdateAimTargetFocus(HoveredTarget, DeltaSeconds);
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
        const float SafeTargetDistance = FMath::Max(1.0f, SlideTargetDistance);
        const float BoostedSpeed = SlideInitialSpeed * SlideBoostSpeedMultiplier;
        float RemainingSlideTickTime = DeltaSeconds;
        constexpr float SlideIntegrationStep = 1.0f / 240.0f;
        while (RemainingSlideTickTime > KINDA_SMALL_NUMBER && SlideDistanceTraveled < SafeTargetDistance)
        {
            const float StepSeconds = FMath::Min(RemainingSlideTickTime, SlideIntegrationStep);
            const float StepProgress = FMath::Clamp(SlideDistanceTraveled / SafeTargetDistance, 0.0f, 1.0f);
            if (StepProgress < SlideInitialPhaseEndProgress)
            {
                SlideCurrentSpeed = BoostedSpeed;
            }
            else if (StepProgress < SlideSlowdownStartProgress)
            {
                SlideCurrentSpeed = SlideCruiseSpeed;
            }
            else if (StepProgress < SlideSlowdownEndProgress)
            {
                const float SlowdownAlpha = FMath::Clamp(
                    (StepProgress - SlideSlowdownStartProgress) /
                    FMath::Max(KINDA_SMALL_NUMBER, SlideSlowdownEndProgress - SlideSlowdownStartProgress),
                    0.0f,
                    1.0f);
                SlideCurrentSpeed = FMath::Lerp(SlideCruiseSpeed, SlideBaseSpeed, SlowdownAlpha);
            }
            else
            {
                SlideCurrentSpeed = SlideBaseSpeed;
            }

            const float RemainingDistance = FMath::Max(0.0f, SafeTargetDistance - SlideDistanceTraveled);
            const float RequestedDistance = FMath::Min(SlideCurrentSpeed * StepSeconds, RemainingDistance);
            const FVector LocationBeforeMove = GetActorLocation();
            AddActorWorldOffset(SlideDirection * RequestedDistance, true);
            const float ActualDistance = FVector::Dist2D(LocationBeforeMove, GetActorLocation());
            SlideDistanceTraveled += ActualDistance;
            RemainingSlideTickTime -= StepSeconds;

            if (RequestedDistance > KINDA_SMALL_NUMBER && ActualDistance <= KINDA_SMALL_NUMBER)
            {
                break;
            }
        }
        const float PathProgress = FMath::Clamp(SlideDistanceTraveled / SafeTargetDistance, 0.0f, 1.0f);

        const float InitialPhaseDistance = SafeTargetDistance * SlideInitialPhaseEndProgress;
        const float InitialPhaseDuration = InitialPhaseDistance / FMath::Max(1.0f, BoostedSpeed);
        const float CameraDropEndDistance = SlideCameraDropDuration <= InitialPhaseDuration
            ? BoostedSpeed * SlideCameraDropDuration
            : InitialPhaseDistance + SlideCruiseSpeed * (SlideCameraDropDuration - InitialPhaseDuration);
        const float CameraDropEndProgress = FMath::Clamp(CameraDropEndDistance / SafeTargetDistance, 0.0f, 0.95f);
        const float CameraHeight = SlideElapsed <= SlideCameraDropDuration
            ? FMath::Lerp(StandingCameraHeight, SlidingCameraHeight, SlideElapsed / FMath::Max(0.01f, SlideCameraDropDuration))
            : FMath::Lerp(
                SlidingCameraHeight,
                StandingCameraHeight,
                FMath::Clamp((PathProgress - CameraDropEndProgress) / FMath::Max(0.01f, 1.0f - CameraDropEndProgress), 0.0f, 1.0f));
        Camera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraHeight));

        if (PathProgress >= 1.0f - KINDA_SMALL_NUMBER || SlideElapsed >= SlideDuration)
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

        const float SafeSlideJumpAirtimeScale = FMath::Max(0.01f, SlideJumpAirtimeScale);
        const float SlideJumpGravityScale = 1.0f / FMath::Square(SafeSlideJumpAirtimeScale);
        const float ActiveJumpGravity = JumpGravity * (bIsSlideJumping ? SlideJumpGravityScale : 1.0f);
        JumpVerticalVelocity -= ActiveJumpGravity * DeltaSeconds;
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
    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode1);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode2);
    PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode3);
    PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode4);
    PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode5);
    PlayerInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode1);
    PlayerInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode2);
    PlayerInputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode3);
    PlayerInputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode4);
    PlayerInputComponent->BindKey(EKeys::NumPadFive, IE_Pressed, this, &AAimTrainerPawn::SelectTrainingMode5);
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
        PendingRecoilYaw -= RecoilYawPattern[RecoilShotIndex % RecoilYawPattern.Num()] * RecoilHorizontalKickMultiplier;
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
    SlideDistanceTraveled = 0.0f;
    SlideCurrentSpeed = SlideInitialSpeed * SlideBoostSpeedMultiplier;
    bIsSliding = true;
    FloatingMovement->StopMovementImmediately();
}

void AAimTrainerPawn::EndSlide()
{
    const bool bLaunchQueuedJump = bQueuedSlideJump;
    bQueuedSlideJump = false;
    bIsSliding = false;
    SlideCurrentSpeed = SlideBaseSpeed;
    Camera->SetRelativeLocation(FVector(0.0f, 0.0f, StandingCameraHeight));
    if (bLaunchQueuedJump)
    {
        LaunchJump(true);
    }
    else
    {
        SlideCurrentSpeed = 0.0f;
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
        const float ExitSlideSpeed = FMath::Max(SlideBaseSpeed, SlideCurrentSpeed);
        SlideJumpDirection = SlideDirection;
        SlideJumpForwardSpeed = FMath::Max(SlideJumpMinimumForwardSpeed, ExitSlideSpeed * SlideJumpMomentumMultiplier);
        SlideCurrentSpeed = 0.0f;
    }
    else
    {
        AirJumpDirection = HorizontalVelocity.GetSafeNormal();
        AirJumpForwardSpeed = HorizontalVelocity.Size();
    }

    JumpBaseHeight = GetActorLocation().Z;
    const float SafeSlideJumpAirtimeScale = FMath::Max(0.01f, SlideJumpAirtimeScale);
    JumpVerticalVelocity = bIsSlideJumping ? SlideJumpInitialVelocity / SafeSlideJumpAirtimeScale : JumpInitialVelocity;
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
    if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        const bool bBlockedMouseChord =
            PlayerController->IsInputKeyDown(EKeys::LeftMouseButton) ||
            PlayerController->IsInputKeyDown(EKeys::LeftControl) ||
            PlayerController->IsInputKeyDown(EKeys::RightControl);
        if (bBlockedMouseChord)
        {
            return;
        }
    }

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

void AAimTrainerPawn::SelectTrainingMode1()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this))) GameMode->SetTrainingMode(1);
}

void AAimTrainerPawn::SelectTrainingMode2()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this))) GameMode->SetTrainingMode(2);
}

void AAimTrainerPawn::SelectTrainingMode3()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this))) GameMode->SetTrainingMode(3);
}

void AAimTrainerPawn::SelectTrainingMode4()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this))) GameMode->SetTrainingMode(4);
}

void AAimTrainerPawn::SelectTrainingMode5()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this))) GameMode->SetTrainingMode(5);
}
void AAimTrainerPawn::RestartTraining()
{
    if (AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GameMode->RestartSession();
    }
}
