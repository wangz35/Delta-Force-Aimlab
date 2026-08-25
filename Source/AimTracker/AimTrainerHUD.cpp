#include "AimTrainerHUD.h"

#include "AimTrainerGameMode.h"
#include "AimTrainerPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void AAimTrainerHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
    {
        return;
    }

    AAimTrainerGameMode* GameMode = Cast<AAimTrainerGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GameMode)
    {
        return;
    }

    const AAimTrainerPawn* TrainingPawn = Cast<AAimTrainerPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    const float Sensitivity = TrainingPawn ? TrainingPawn->GetMouseSensitivity() : 1.0f;
    const float ZoomMultiplier = TrainingPawn ? TrainingPawn->GetZoomMultiplier() : 1.0f;
    const float JumpCrosshairOffset = TrainingPawn ? TrainingPawn->GetJumpCrosshairOffset() : 0.0f;
    const bool bBotScoreMode = GameMode->IsBotScoreMode();
    const bool bHasBestBotRecord = GameMode->HasBestBotRecord();
    FString ModeName;
    switch (GameMode->GetTrainingMode())
    {
    case 2: ModeName = TEXT("DUAL TRACKING"); break;
    case 3: ModeName = TEXT("JUMP BOTS"); break;
    case 4: ModeName = TEXT("MIXED PEEK BOTS + BALL"); break;
    case 5: ModeName = TEXT("RANDOM COVER PEEKS"); break;
    default: ModeName = TEXT("STANDARD RANGE"); break;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const FLinearColor Orange(1.0f, 0.24f, 0.03f, 1.0f);
    const FLinearColor PaleOrange(1.0f, 0.68f, 0.38f, 1.0f);
    const FLinearColor CrosshairGreen(0.18f, 1.0f, 0.34f, 1.0f);
    const FLinearColor Dim(0.73f, 0.77f, 0.78f, 0.92f);

    Canvas->K2_DrawBox(FVector2D(28.0f, 24.0f), FVector2D(470.0f, 94.0f), 1.0f, FLinearColor(0.008f, 0.011f, 0.014f, 0.70f));
    Canvas->K2_DrawBox(FVector2D(30.0f, 26.0f), FVector2D(4.0f, 90.0f), 1.0f, CrosshairGreen);
    DrawLabel(TEXT("AIM // RANGE"), FVector2D(52.0f, 37.0f), Orange, 1.25f);
    DrawLabel(FString::Printf(TEXT("MODE %d  //  %s  //  KEYS 1-5"), GameMode->GetTrainingMode(), *ModeName), FVector2D(53.0f, 66.0f), Dim, 0.82f);
    FString PrimaryStats;
    if (bBotScoreMode)
    {
        PrimaryStats = bHasBestBotRecord
            ? FString::Printf(TEXT("BOTS %03d     BEST %03d"), GameMode->GetBotEliminations(), GameMode->GetBestBotEliminations())
            : FString::Printf(TEXT("BOTS %03d"), GameMode->GetBotEliminations());
    }
    else
    {
        PrimaryStats = FString::Printf(TEXT("HITS %03d     SHOTS %03d     ACC %05.1f%%"), GameMode->GetHits(), GameMode->GetShots(), GameMode->GetAccuracy());
    }
    DrawLabel(PrimaryStats, FVector2D(53.0f, 91.0f), PaleOrange, 0.88f);

    const FString TimerText = FString::Printf(TEXT("%05.1f"), GameMode->GetTimeRemaining());
    DrawLabel(TimerText, FVector2D(Width - 76.0f, 42.0f), Orange, 1.45f, true);
    DrawLabel(TEXT("TIME REMAINING"), FVector2D(Width - 76.0f, 75.0f), Dim, 0.72f, true);

    const float MidX = Width * 0.5f;
    const float MidY = Height * 0.5f;
    const float CrosshairY = MidY + JumpCrosshairOffset;
    Canvas->K2_DrawLine(FVector2D(MidX - 14.0f, CrosshairY), FVector2D(MidX - 4.0f, CrosshairY), 1.5f, CrosshairGreen);
    Canvas->K2_DrawLine(FVector2D(MidX + 4.0f, CrosshairY), FVector2D(MidX + 14.0f, CrosshairY), 1.5f, CrosshairGreen);
    Canvas->K2_DrawLine(FVector2D(MidX, CrosshairY - 14.0f), FVector2D(MidX, CrosshairY - 4.0f), 1.5f, CrosshairGreen);
    Canvas->K2_DrawLine(FVector2D(MidX, CrosshairY + 4.0f), FVector2D(MidX, CrosshairY + 14.0f), 1.5f, CrosshairGreen);
    Canvas->K2_DrawBox(FVector2D(MidX - 1.5f, CrosshairY - 1.5f), FVector2D(3.0f, 3.0f), 1.0f, CrosshairGreen);

    const FString ReactionText = bBotScoreMode
        ? TEXT("TRACK A BOT FOR 0.4s TO ELIMINATE")
        : (GameMode->GetHits() > 0
            ? FString::Printf(TEXT("AVG REACTION  %04.0f ms    LAST  %04.0f ms"), GameMode->GetAverageReactionMs(), GameMode->GetLastReactionMs())
            : TEXT("AVG REACTION  ---- ms    LAST  ---- ms"));
    DrawLabel(ReactionText, FVector2D(30.0f, Height - 52.0f), Dim, 0.78f);
    DrawLabel(FString::Printf(TEXT("WASD MOVE  /  SPACE JUMP  /  SLIDE + SPACE SLIDE-JUMP  /  [ ] SENS %.2fx  /  RMB ZOOM  /  R RESTART"), Sensitivity), FVector2D(30.0f, Height - 28.0f), PaleOrange, 0.68f);
    DrawLabel(FString::Printf(TEXT("ZOOM  %.1fx    SENS  %.2f"), ZoomMultiplier, Sensitivity), FVector2D(Width - 230.0f, Height - 52.0f), PaleOrange, 0.82f);

    if (!GameMode->IsSessionActive())
    {
        Canvas->K2_DrawBox(FVector2D(0.0f, 0.0f), FVector2D(Width, Height), 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
        DrawLabel(TEXT("DRILL COMPLETE"), FVector2D(MidX, MidY - 72.0f), Orange, 1.65f, true);
        if (bBotScoreMode)
        {
            if (bHasBestBotRecord)
            {
                DrawLabel(FString::Printf(TEXT("THIS RUN  %03d BOTS"), GameMode->GetBotEliminations()), FVector2D(MidX, MidY - 20.0f), PaleOrange, 1.02f, true);
                DrawLabel(FString::Printf(TEXT("BEST  %03d BOTS"), GameMode->GetBestBotEliminations()), FVector2D(MidX, MidY + 17.0f), PaleOrange, 1.02f, true);
                DrawLabel(TEXT("PRESS R TO RUN IT BACK"), FVector2D(MidX, MidY + 67.0f), Dim, 0.88f, true);
            }
            else
            {
                DrawLabel(FString::Printf(TEXT("TOTAL ELIMINATIONS  %03d BOTS"), GameMode->GetBotEliminations()), FVector2D(MidX, MidY - 9.0f), PaleOrange, 1.02f, true);
                DrawLabel(TEXT("PRESS R TO RUN IT BACK"), FVector2D(MidX, MidY + 36.0f), Dim, 0.88f, true);
            }
        }
        else
        {
            DrawLabel(FString::Printf(TEXT("%d HITS  //  %05.1f%% ACCURACY  //  %04.0f ms RESPONSE"), GameMode->GetHits(), GameMode->GetAccuracy(), GameMode->GetAverageReactionMs()), FVector2D(MidX, MidY - 9.0f), PaleOrange, 0.92f, true);
            DrawLabel(TEXT("PRESS R TO RUN IT BACK"), FVector2D(MidX, MidY + 36.0f), Dim, 0.88f, true);
        }
    }
}
void AAimTrainerHUD::DrawLabel(const FString& Text, const FVector2D& Position, const FLinearColor& Color, float Scale, bool bCenter)
{
    if (!GEngine)
    {
        return;
    }

    UFont* Font = GEngine->GetMediumFont();
    float TextWidth = 0.0f;
    float TextHeight = 0.0f;
    GetTextSize(Text, TextWidth, TextHeight, Font, Scale);
    const FVector2D DrawPosition = bCenter ? FVector2D(Position.X - TextWidth * 0.5f, Position.Y) : Position;
    FCanvasTextItem TextItem(DrawPosition, FText::FromString(Text), Font, Color);
    TextItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(TextItem);
}
