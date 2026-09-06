#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCourseRules.h"
#include "../DiscGolfMath.h"
#include "../DiscGolfSaveGame.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseSurfaceMappingTest,
    "DiscGolfTour.Rules.SurfaceMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseSurfaceMappingTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Light rough uses rough ground response"),
        DiscGolfCourseRules::GroundResponseSurface(ECourseSurfaceType::LightRough), EGroundSurfaceType::Rough);
    TestEqual(TEXT("Deep rough uses rough ground response"),
        DiscGolfCourseRules::GroundResponseSurface(ECourseSurfaceType::DeepRough), EGroundSurfaceType::Rough);
    TestEqual(TEXT("Hazard uses rough ground response"),
        DiscGolfCourseRules::GroundResponseSurface(ECourseSurfaceType::Hazard), EGroundSurfaceType::Rough);
    TestEqual(TEXT("OB grass preserves fairway ground response"),
        DiscGolfCourseRules::GroundResponseSurface(ECourseSurfaceType::OutOfBounds), EGroundSurfaceType::Fairway);

    UPhysicalMaterial* DeepRoughMaterial = NewObject<UPhysicalMaterial>(
        GetTransientPackage(), FName(TEXT("PM_Surface_DeepRough")));
    UPhysicalMaterial* ObMaterial = NewObject<UPhysicalMaterial>(
        GetTransientPackage(), FName(TEXT("PM_Surface_OutOfBounds")));
    TestTrue(TEXT("Named physical material exposes a course identity"),
        DiscGolfCourseRules::HasSurfaceIdentity(nullptr, DeepRoughMaterial));
    TestEqual(TEXT("Physical material name resolves deep rough"),
        DiscGolfCourseRules::ResolveSurface(nullptr, DeepRoughMaterial), ECourseSurfaceType::DeepRough);
    TestEqual(TEXT("Physical material name resolves OB"),
        DiscGolfCourseRules::ResolveSurface(nullptr, ObMaterial), ECourseSurfaceType::OutOfBounds);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfLieClassificationTest,
    "DiscGolfTour.Rules.LieClassification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfLieClassificationTest::RunTest(const FString& Parameters)
{
    const FVector Basket(10000.0f, 0.0f, 0.0f);
    const FDiscGolfLieState Rough = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::LightRough, FVector::ZeroVector, FVector::ZeroVector, Basket);
    TestEqual(TEXT("Long light rough is a light-rough lie"), Rough.LieType, ELieType::LightRough);
    TestEqual(TEXT("Long rough remains a drive"), Rough.ShotContext, EDiscShotContext::Drive);
    TestTrue(TEXT("Light rough has a power penalty"), Rough.Effects.PowerMultiplier < 1.0f);

    const FDiscGolfLieState NearRough = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::LightRough, FVector(9500.0f, 0.0f, 0.0f), FVector::ZeroVector, Basket);
    TestEqual(TEXT("Distance retains Circle 1 context over rough display"), NearRough.LieType, ELieType::Circle1);
    TestEqual(TEXT("Near rough uses putting context"), NearRough.ShotContext, EDiscShotContext::Circle1Putt);
    TestTrue(TEXT("Near rough still retains rough effects"), NearRough.Effects.PowerMultiplier < 1.0f);

    const FDiscGolfLieState Tee = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::TeePad, FVector::ZeroVector, FVector::ZeroVector, Basket, true);
    TestEqual(TEXT("Reset tee is explicit"), Tee.LieType, ELieType::Tee);
    TestEqual(TEXT("Tee reset has no penalty"), Tee.PenaltyStrokes, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPenaltyResolutionTest,
    "DiscGolfTour.Rules.PenaltyResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPenaltyResolutionTest::RunTest(const FString& Parameters)
{
    const FVector Basket(11000.0f, 800.0f, 0.0f);
    const FVector RawOb(6000.0f, -4100.0f, 3.0f);
    const FVector Relief(5900.0f, -3500.0f, 0.0f);
    const FDiscGolfLieState Ob = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::OutOfBounds, RawOb, Relief, Basket);
    TestEqual(TEXT("OB adds one stroke"), Ob.PenaltyStrokes, 1);
    TestEqual(TEXT("OB records its penalty identity"), Ob.PenaltyType, EDiscGolfPenaltyType::OutOfBounds);
    TestEqual(TEXT("OB uses last-in-bounds relief"), Ob.ReliefRule, EDiscGolfReliefRule::LastInBounds);
    TestTrue(TEXT("OB lie moves to relief"), Ob.LieLocationCm.Equals(Relief));
    TestEqual(TEXT("OB relief resumes on fairway"), Ob.PlayingSurface, ECourseSurfaceType::Fairway);

    const FVector RawHazard(8300.0f, 3900.0f, 3.0f);
    const FDiscGolfLieState Hazard = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::Hazard, RawHazard, Relief, Basket);
    TestEqual(TEXT("Hazard adds one stroke"), Hazard.PenaltyStrokes, 1);
    TestEqual(TEXT("Hazard plays from the result"), Hazard.ReliefRule, EDiscGolfReliefRule::PlayFromResult);
    TestTrue(TEXT("Hazard lie stays at the disc"), Hazard.LieLocationCm.Equals(RawHazard));
    TestTrue(TEXT("Hazard carries a stance effect"), Hazard.Effects.PowerMultiplier < 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfLieEffectApplicationTest,
    "DiscGolfTour.Rules.LieEffectApplication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfLieEffectApplicationTest::RunTest(const FString& Parameters)
{
    FThrowCommand Command;
    Command.Power01 = 0.80f;
    Command.TimingError = 0.50f;
    const FLieEffectProfile DeepRough = DiscGolfCourseRules::LieEffects(ECourseSurfaceType::DeepRough);
    const FThrowCommand Modified = DiscGolfCourseRules::ApplyLieEffects(Command, DeepRough);
    TestTrue(TEXT("Deep rough reduces authored power deterministically"),
        FMath::IsNearlyEqual(Modified.Power01, 0.704f));
    TestTrue(TEXT("Deep rough amplifies an existing timing miss"),
        FMath::IsNearlyEqual(Modified.TimingError, 0.625f));

    Command.TimingError = 0.0f;
    const FThrowCommand Perfect = DiscGolfCourseRules::ApplyLieEffects(Command, DeepRough);
    TestTrue(TEXT("A perfect release remains centered in deep rough"), FMath::IsNearlyZero(Perfect.TimingError));
    TestTrue(TEXT("Input command is not mutated"), FMath::IsNearlyEqual(Command.Power01, 0.80f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfObReliefPlacementTest,
    "DiscGolfTour.Rules.ObReliefPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfObReliefPlacementTest::RunTest(const FString& Parameters)
{
    const FVector Inside(100.0f, 0.0f, 12.0f);
    const FVector Outside(0.0f, 0.0f, 5.0f);
    const FVector Relief = DiscGolfCourseRules::ReliefPointInsideBoundary(Inside, Outside);
    TestTrue(TEXT("Relief moves one meter farther in bounds"), Relief.Equals(FVector(200.0f, 0.0f, 12.0f)));
    TestTrue(TEXT("Negative relief distance is safely clamped"),
        DiscGolfCourseRules::ReliefPointInsideBoundary(Inside, Outside, -20.0f).Equals(Inside));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSurfaceProbeFailClosedTest,
    "DiscGolfTour.Rules.SurfaceProbeFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSurfaceProbeFailClosedTest::RunTest(const FString& Parameters)
{
    const FVector RawNearGround(1000.0f, 2000.0f, 64.0f);
    const FVector Ground(1000.0f, 2000.0f, 0.0f);
    const FVector RawUnderTerrain(1000.0f, 2000.0f, -8000.0f);
    TestFalse(TEXT("A missing trace is never accepted through the default fairway value"),
        DiscGolfCourseRules::IsInBoundsSurfaceProbe(
            false, ECourseSurfaceType::Fairway, RawNearGround, Ground));
    TestTrue(TEXT("A traced fairway is in bounds"),
        DiscGolfCourseRules::IsInBoundsSurfaceProbe(
            true, ECourseSurfaceType::Fairway, RawNearGround, Ground));
    TestFalse(TEXT("A traced OB surface is outside the playable boundary"),
        DiscGolfCourseRules::IsInBoundsSurfaceProbe(
            true, ECourseSurfaceType::OutOfBounds, RawNearGround, Ground));
    TestFalse(TEXT("A traced hazard is not eligible as last-in-bounds relief"),
        DiscGolfCourseRules::IsInBoundsSurfaceProbe(
            true, ECourseSurfaceType::Hazard, RawNearGround, Ground));
    TestFalse(TEXT("Another hole's typed terrain cannot support a far-below-world lie"),
        DiscGolfCourseRules::IsSupportedSettledSurfaceProbe(
            true, RawUnderTerrain, Ground));
    TestTrue(TEXT("An airborne in-bounds trajectory sample can project to its typed surface"),
        DiscGolfCourseRules::IsInBoundsSurfaceProbe(
            true, ECourseSurfaceType::Fairway,
            FVector(1000.0f, 2000.0f, 500.0f), Ground));
    TestTrue(TEXT("A supported hazard remains a valid play-from-result surface"),
        DiscGolfCourseRules::IsSupportedSettledSurfaceProbe(
            true, RawNearGround, Ground));

    const FVector UnsupportedRaw(42000.0f, -17000.0f, -8000.0f);
    const FVector ProvenRelief(9800.0f, 600.0f, 0.0f);
    const FVector Basket(11000.0f, 800.0f, 80.0f);
    const FDiscGolfLieState Recovered = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::OutOfBounds, UnsupportedRaw, ProvenRelief, Basket);
    TestEqual(TEXT("Unsupported settled locations receive an OB penalty"),
        Recovered.PenaltyType, EDiscGolfPenaltyType::OutOfBounds);
    TestTrue(TEXT("Unsupported settled locations recover to the proven relief"),
        Recovered.LieLocationCm.Equals(ProvenRelief));
    TestTrue(TEXT("Raw unsupported telemetry remains available for diagnosis"),
        Recovered.RawDiscLocationCm.Equals(UnsupportedRaw));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfLieSaveSerializationTest,
    "DiscGolfTour.Rules.LieSaveSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfLieSaveSerializationTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Source = NewObject<UDiscGolfSaveGame>();
    Source->bHasPracticeRoundSnapshot = true;
    Source->PracticeStrokes = 4;
    Source->PracticePenaltyStrokes = 1;
    Source->PracticeCourseId = TEXT("PineRidgeChampionship");
    Source->PracticeMoldId = TEXT("Line");
    Source->PracticePlastic = EDiscPlastic::Crystal;
    Source->PracticeLieState = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::Hazard,
        FVector(8300.0f, 3900.0f, 3.0f),
        FVector::ZeroVector,
        FVector(11000.0f, 800.0f, 0.0f));

    TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        Archive.ArIsSaveGame = true;
        Source->Serialize(Archive);
    }

    UDiscGolfSaveGame* Restored = NewObject<UDiscGolfSaveGame>();
    {
        FMemoryReader Reader(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Reader, false);
        Archive.ArIsSaveGame = true;
        Restored->Serialize(Archive);
    }

    TestTrue(TEXT("Round snapshot survives SaveGame serialization"), Restored->bHasPracticeRoundSnapshot);
    TestEqual(TEXT("Total strokes survive SaveGame serialization"), Restored->PracticeStrokes, 4);
    TestEqual(TEXT("Penalty strokes survive SaveGame serialization"), Restored->PracticePenaltyStrokes, 1);
    TestEqual(TEXT("Course identity survives SaveGame serialization"), Restored->PracticeCourseId,
        FName(TEXT("PineRidgeChampionship")));
    TestEqual(TEXT("Selected mold survives SaveGame serialization"), Restored->PracticeMoldId,
        FName(TEXT("Line")));
    TestEqual(TEXT("Selected plastic survives SaveGame serialization"), Restored->PracticePlastic,
        EDiscPlastic::Crystal);
    TestEqual(TEXT("Hazard identity survives SaveGame serialization"),
        Restored->PracticeLieState.SurfaceAtRest, ECourseSurfaceType::Hazard);
    TestEqual(TEXT("Lie effect profile survives SaveGame serialization"),
        Restored->PracticeLieState.Effects.ProfileId, FName(TEXT("Hazard")));
    TestTrue(TEXT("Lie location survives SaveGame serialization"),
        Restored->PracticeLieState.LieLocationCm.Equals(Source->PracticeLieState.LieLocationCm));
    return true;
}

#endif
