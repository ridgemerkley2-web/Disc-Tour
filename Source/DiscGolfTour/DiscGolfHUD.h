#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DiscGolfTypes.h"
#include "DiscGolfHUD.generated.h"

UCLASS()
class DISCGOLFTOUR_API ADiscGolfHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DrawMainMenu(const class ADiscGolfTourGameMode* GameMode, UFont* Medium, UFont* Small);
    void DrawControlsMenu(const class ADiscGolfTourPlayerController* PlayerController, UFont* Medium, UFont* Small);
    void DrawSettingsMenu(const class ADiscGolfTourPlayerController* PlayerController, UFont* Medium, UFont* Small);
    void DrawScorecard(const class ADiscGolfTourGameMode* GameMode, UFont* Medium, UFont* Small);
    void DrawProductionHUD(
        const class ADiscGolfTourGameMode* GameMode,
        class ADiscGolferPawn* Golfer,
        UFont* Medium,
        UFont* Small);
    FString PlasticToString(uint8 PlasticValue) const;
    FString ThrowStyleToString(uint8 ThrowStyleValue) const;
    FString LieToString(uint8 LieValue) const;
    FString ReleaseGradeToString(EReleaseGrade Grade) const;
    FString ReleaseTimingToString(EReleaseTiming Timing) const;
    FLinearColor ReleaseGradeColor(EReleaseGrade Grade) const;
    FString GroundStateToString(EDiscGroundState State) const;
    FString GroundSurfaceToString(EGroundSurfaceType Surface) const;
    FString ShotContextToString(EDiscShotContext Context) const;
    FString BasketContactToString(EBasketContactResult Result) const;
};
