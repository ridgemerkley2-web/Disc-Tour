#include "DiscGolfCharacterProfileRuntime.h"

namespace
{
    float SanitizeFinite(float Value, float DefaultValue, float MinValue, float MaxValue)
    {
        return FMath::IsFinite(Value)
            ? FMath::Clamp(Value, MinValue, MaxValue)
            : DefaultValue;
    }
}

void FDiscGolfCharacterProfileSaveData::Sanitize()
{
    using namespace DiscGolfCharacterCreatorSchema;

    HeightCm = SanitizeFinite(HeightCm, DefaultHeightCm, MinHeightCm, MaxHeightCm);
    WingspanScale = SanitizeFinite(
        WingspanScale, DefaultBodyScale, MinWingspanScale, MaxWingspanScale);
    ShoulderWidthScale = SanitizeFinite(
        ShoulderWidthScale, DefaultBodyScale, MinShoulderWidthScale, MaxShoulderWidthScale);
    TorsoLengthScale = SanitizeFinite(
        TorsoLengthScale, DefaultBodyScale, MinTorsoLengthScale, MaxTorsoLengthScale);
    LegLengthScale = SanitizeFinite(
        LegLengthScale, DefaultBodyScale, MinLegLengthScale, MaxLegLengthScale);
    HandScale = SanitizeFinite(HandScale, DefaultBodyScale, MinHandScale, MaxHandScale);
    MassKg = SanitizeFinite(MassKg, DefaultMassKg, MinMassKg, MaxMassKg);

    Muscularity = SanitizeFinite(Muscularity, DefaultMuscularity, 0.0f, 1.0f);
    BodyFat = SanitizeFinite(BodyFat, DefaultBodyFat, 0.0f, 1.0f);
    Chest = SanitizeFinite(Chest, DefaultBodyShape, -1.0f, 1.0f);
    Waist = SanitizeFinite(Waist, DefaultBodyShape, -1.0f, 1.0f);
    Hips = SanitizeFinite(Hips, DefaultBodyShape, -1.0f, 1.0f);
    Arms = SanitizeFinite(Arms, DefaultBodyShape, -1.0f, 1.0f);
    Legs = SanitizeFinite(Legs, DefaultBodyShape, -1.0f, 1.0f);

    RunUpIntensity = SanitizeFinite(
        RunUpIntensity, DefaultRunUpIntensity, MinStyleValue, MaxStyleValue);
    ReachBackAmount = SanitizeFinite(
        ReachBackAmount, DefaultReachBackAmount, MinStyleValue, MaxStyleValue);
    TorsoRotation = SanitizeFinite(
        TorsoRotation, DefaultTorsoRotation, MinStyleValue, MaxStyleValue);
    BraceIntensity = SanitizeFinite(
        BraceIntensity, DefaultBraceIntensity, MinStyleValue, MaxStyleValue);
    Explosiveness = SanitizeFinite(
        Explosiveness, DefaultExplosiveness, MinStyleValue, MaxStyleValue);
    FollowThrough = SanitizeFinite(
        FollowThrough, DefaultFollowThrough, MinStyleValue, MaxStyleValue);
}

FDGBodyProfile FDiscGolfCharacterProfileSaveData::ToBodyProfile() const
{
    FDiscGolfCharacterProfileSaveData Safe = *this;
    Safe.Sanitize();

    FDGBodyProfile Body;
    Body.HeightCm = Safe.HeightCm;
    Body.WingspanScale = Safe.WingspanScale;
    Body.ShoulderWidthScale = Safe.ShoulderWidthScale;
    Body.TorsoLengthScale = Safe.TorsoLengthScale;
    Body.LegLengthScale = Safe.LegLengthScale;
    Body.HandScale = Safe.HandScale;
    Body.MassKg = Safe.MassKg;
    return Body;
}

FDGBodyBuildProfile FDiscGolfCharacterProfileSaveData::ToBodyBuildProfile() const
{
    FDiscGolfCharacterProfileSaveData Safe = *this;
    Safe.Sanitize();

    FDGBodyBuildProfile BodyBuild;
    BodyBuild.Muscularity = Safe.Muscularity;
    BodyBuild.BodyFat = Safe.BodyFat;
    BodyBuild.Chest = Safe.Chest;
    BodyBuild.Waist = Safe.Waist;
    BodyBuild.Hips = Safe.Hips;
    BodyBuild.Arms = Safe.Arms;
    BodyBuild.Legs = Safe.Legs;
    return BodyBuild;
}

FDGThrowStyle FDiscGolfCharacterProfileSaveData::ToThrowStyle() const
{
    FDiscGolfCharacterProfileSaveData Safe = *this;
    Safe.Sanitize();

    FDGThrowStyle ThrowStyle;
    ThrowStyle.RunUpIntensity = Safe.RunUpIntensity;
    ThrowStyle.ReachBackAmount = Safe.ReachBackAmount;
    ThrowStyle.TorsoRotation = Safe.TorsoRotation;
    ThrowStyle.BraceIntensity = Safe.BraceIntensity;
    ThrowStyle.Explosiveness = Safe.Explosiveness;
    ThrowStyle.FollowThrough = Safe.FollowThrough;

    // Character-creator style is presentation-only in Session 4. Never let a
    // persisted profile become a second source of authoritative flight power.
    ThrowStyle.PowerMultiplier = 1.0f;
    ThrowStyle.SpinMultiplier = 1.0f;
    return ThrowStyle;
}

EDGHandedness FDiscGolfCharacterProfileSaveData::GetHandedness() const
{
    return bLeftHanded ? EDGHandedness::Left : EDGHandedness::Right;
}

FDiscGolfCharacterProfileSaveData FDiscGolfCharacterProfileSaveData::FromFramework(
    const FDGBodyProfile& Body,
    const FDGThrowStyle& ThrowStyle,
    EDGHandedness Handedness,
    const FDGBodyBuildProfile& BodyBuild)
{
    FDiscGolfCharacterProfileSaveData Result;
    Result.bLeftHanded = Handedness == EDGHandedness::Left;
    Result.HeightCm = Body.HeightCm;
    Result.WingspanScale = Body.WingspanScale;
    Result.ShoulderWidthScale = Body.ShoulderWidthScale;
    Result.TorsoLengthScale = Body.TorsoLengthScale;
    Result.LegLengthScale = Body.LegLengthScale;
    Result.HandScale = Body.HandScale;
    Result.MassKg = Body.MassKg;

    Result.Muscularity = BodyBuild.Muscularity;
    Result.BodyFat = BodyBuild.BodyFat;
    Result.Chest = BodyBuild.Chest;
    Result.Waist = BodyBuild.Waist;
    Result.Hips = BodyBuild.Hips;
    Result.Arms = BodyBuild.Arms;
    Result.Legs = BodyBuild.Legs;

    Result.RunUpIntensity = ThrowStyle.RunUpIntensity;
    Result.ReachBackAmount = ThrowStyle.ReachBackAmount;
    Result.TorsoRotation = ThrowStyle.TorsoRotation;
    Result.BraceIntensity = ThrowStyle.BraceIntensity;
    Result.Explosiveness = ThrowStyle.Explosiveness;
    Result.FollowThrough = ThrowStyle.FollowThrough;
    Result.Sanitize();
    return Result;
}
