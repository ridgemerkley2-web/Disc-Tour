#include "DiscGolfDiscLibrary.h"

EDGDiscWearStage UDiscGolfDiscLibrary::GetWearStage(const FDGDiscInstance& Disc)
{
    return Disc.GetWearStage();
}

FDGDiscFlightNumbers UDiscGolfDiscLibrary::ApplyPresentationWear(
    const FDGDiscFlightNumbers& BaseNumbers,
    float Wear01,
    float PlasticStabilityOffset)
{
    FDGDiscFlightNumbers Result = BaseNumbers;
    const float Wear = FMath::Clamp(Wear01, 0.0f, 1.0f);

    // Presentation/recommendation adjustment only. The calibrated physics model
    // must decide how wear changes its actual aerodynamic coefficients.
    Result.Turn = FMath::Clamp(
        BaseNumbers.Turn - Wear * 1.25f + PlasticStabilityOffset,
        -5.0f,
        1.0f
    );

    Result.Fade = FMath::Clamp(
        BaseNumbers.Fade - Wear * 0.75f + PlasticStabilityOffset,
        0.0f,
        5.0f
    );

    return Result;
}
