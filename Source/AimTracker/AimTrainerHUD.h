#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AimTrainerHUD.generated.h"

UCLASS()
class AIMTRACKER_API AAimTrainerHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DrawLabel(const FString& Text, const FVector2D& Position, const FLinearColor& Color, float Scale = 1.0f, bool bCenter = false);
};
