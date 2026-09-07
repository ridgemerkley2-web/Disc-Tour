#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCourseDefinition.h"

namespace
{
    /** A two-hole course with alternate tees and pins on hole 1 only. */
    FDiscGolfHoleBlockoutDefinition MakeLayoutHole(int32 HoleNumber, bool bWithAlternates)
    {
        FDiscGolfHoleBlockoutDefinition Hole;
        Hole.SchemaVersion = bWithAlternates ? 2 : 1;
        Hole.HoleNumber = HoleNumber;
        Hole.HoleName = FText::FromString(FString::Printf(TEXT("Hole %d"), HoleNumber));
        Hole.Par = 3;
        Hole.TeeLocationCm = FVector(0.0f, 0.0f, 0.0f);
        Hole.BasketLocationCm = FVector(10000.0f, 0.0f, 0.0f);

        if (bWithAlternates)
        {
            FDiscGolfTeePositionDefinition Short;
            Short.TeeId = TEXT("Short");
            Short.LocationCm = FVector(2000.0f, 0.0f, 0.0f);
            FDiscGolfTeePositionDefinition Long;
            Long.TeeId = TEXT("Long");
            Long.LocationCm = FVector(-1500.0f, 0.0f, 0.0f);
            Hole.TeePositions = { Short, Long };

            FDiscGolfPinPositionDefinition Near;
            Near.PinId = TEXT("Near");
            Near.LocationCm = FVector(9000.0f, 0.0f, 0.0f);
            FDiscGolfPinPositionDefinition Far;
            Far.PinId = TEXT("Far");
            Far.LocationCm = FVector(17000.0f, 1200.0f, 0.0f);
            Far.Par = 4;
            Hole.PinPositions = { Near, Far };
        }
        return Hole;
    }

    FDiscGolfCourseManifestDefinition MakeLayoutManifest()
    {
        FDiscGolfCourseManifestDefinition Manifest;
        Manifest.SchemaVersion = 2;
        Manifest.CourseId = TEXT("LayoutTestCourse");
        Manifest.LayoutId = TEXT("Championship");
        Manifest.DisplayName = FText::FromString(TEXT("Layout Test Course"));
        for (int32 Number = 1; Number <= 2; ++Number)
        {
            FDiscGolfCourseManifestHoleEntry Entry;
            Entry.HoleNumber = Number;
            Entry.DefinitionFile = FString::Printf(TEXT("Data/LayoutTestHole%d.json"), Number);
            Manifest.Holes.Add(Entry);
        }
        return Manifest;
    }

    FDiscGolfCourseLayoutDefinition MakeLayout(FName LayoutId, FName TeeId, FName PinId)
    {
        FDiscGolfCourseLayoutDefinition Layout;
        Layout.LayoutId = LayoutId;
        FDiscGolfLayoutHoleSelection Selection;
        Selection.HoleNumber = 1;
        Selection.TeeId = TeeId;
        Selection.PinId = PinId;
        Layout.Holes.Add(Selection);
        return Layout;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseLayoutResolutionTest,
    "DiscGolfTour.CourseLayout.Resolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseLayoutResolutionTest::RunTest(const FString& Parameters)
{
    using namespace DiscGolfCourseDefinition;

    const FDiscGolfHoleBlockoutDefinition Hole = MakeLayoutHole(1, true);
    FDiscGolfResolvedHole Resolved;
    FString Error;

    // A layout selects the tee and pin, and par follows the pin.
    TestTrue(TEXT("Long/Far resolves"),
        ResolveHoleForLayout(Hole, MakeLayout(TEXT("Championship"), TEXT("Long"), TEXT("Far")),
            Resolved, Error));
    TestEqual(TEXT("Long tee is in play"), Resolved.TeeLocationCm, FVector(-1500.0f, 0.0f, 0.0f));
    TestEqual(TEXT("Far pin is in play"), Resolved.BasketLocationCm, FVector(17000.0f, 1200.0f, 0.0f));
    TestEqual(TEXT("Far pin carries par 4"), Resolved.Par, 4);

    // A pin that states no par inherits the hole's, rather than scoring par zero.
    TestTrue(TEXT("Short/Near resolves"),
        ResolveHoleForLayout(Hole, MakeLayout(TEXT("Recreational"), TEXT("Short"), TEXT("Near")),
            Resolved, Error));
    TestEqual(TEXT("Short tee is in play"), Resolved.TeeLocationCm, FVector(2000.0f, 0.0f, 0.0f));
    TestEqual(TEXT("Near pin inherits the hole par"), Resolved.Par, 3);

    // The same hole must measure differently from different tees, or layouts are
    // cosmetic. Long/Far is a materially longer hole than Short/Near.
    FDiscGolfResolvedHole LongFar;
    FDiscGolfResolvedHole ShortNear;
    ResolveHoleForLayout(Hole, MakeLayout(TEXT("A"), TEXT("Long"), TEXT("Far")), LongFar, Error);
    ResolveHoleForLayout(Hole, MakeLayout(TEXT("B"), TEXT("Short"), TEXT("Near")), ShortNear, Error);
    TestTrue(TEXT("Long/Far plays longer than Short/Near"),
        FVector::Dist(LongFar.TeeLocationCm, LongFar.BasketLocationCm)
        > FVector::Dist(ShortNear.TeeLocationCm, ShortNear.BasketLocationCm));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseLayoutFailsClosedTest,
    "DiscGolfTour.CourseLayout.FailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseLayoutFailsClosedTest::RunTest(const FString& Parameters)
{
    using namespace DiscGolfCourseDefinition;

    const FDiscGolfHoleBlockoutDefinition Hole = MakeLayoutHole(1, true);
    FDiscGolfResolvedHole Resolved;
    FString Error;

    // Substituting a position the layout did not ask for would move the basket
    // without telling anyone, and the round would score as if that were intended.
    TestFalse(TEXT("An unauthored tee is rejected"),
        ResolveHoleForLayout(Hole, MakeLayout(TEXT("Bad"), TEXT("Missing"), TEXT("Near")),
            Resolved, Error));
    TestTrue(TEXT("The tee error names the selection"), Error.Contains(TEXT("Missing")));

    TestFalse(TEXT("An unauthored pin is rejected"),
        ResolveHoleForLayout(Hole, MakeLayout(TEXT("Bad"), TEXT("Short"), TEXT("Missing")),
            Resolved, Error));
    TestTrue(TEXT("The pin error names the selection"), Error.Contains(TEXT("Missing")));

    const FDiscGolfCourseManifestDefinition Manifest = MakeLayoutManifest();
    const TArray<FDiscGolfHoleBlockoutDefinition> Holes = { Hole, MakeLayoutHole(2, false) };

    FDiscGolfCourseLayoutDefinition Duplicated = MakeLayout(TEXT("Dup"), TEXT("Short"), TEXT("Near"));
    Duplicated.Holes.Add(Duplicated.Holes[0]);
    TestFalse(TEXT("A layout selecting one hole twice is rejected"),
        ValidateLayout(Manifest, Duplicated, Holes, Error));

    FDiscGolfCourseLayoutDefinition OffCourse = MakeLayout(TEXT("Off"), TEXT("Short"), TEXT("Near"));
    OffCourse.Holes[0].HoleNumber = 99;
    TestFalse(TEXT("A layout selecting a hole the manifest lacks is rejected"),
        ValidateLayout(Manifest, OffCourse, Holes, Error));

    FDiscGolfCourseLayoutDefinition Empty;
    Empty.LayoutId = TEXT("Empty");
    TestFalse(TEXT("A layout selecting no holes is rejected"),
        ValidateLayout(Manifest, Empty, Holes, Error));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseLayoutLegacyCompatibilityTest,
    "DiscGolfTour.CourseLayout.LegacyCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseLayoutLegacyCompatibilityTest::RunTest(const FString& Parameters)
{
    using namespace DiscGolfCourseDefinition;

    // The authored Pine Ridge course is schema v1 with no layouts and no alternate
    // positions. It must keep resolving to exactly what it authored, because its
    // files are digest-pinned and cannot be edited to suit a new schema.
    const FDiscGolfHoleBlockoutDefinition Legacy = MakeLayoutHole(1, false);
    TestTrue(TEXT("A v1 hole authors no alternates"),
        Legacy.TeePositions.IsEmpty() && Legacy.PinPositions.IsEmpty());

    FDiscGolfResolvedHole Resolved;
    FString Error;
    TestTrue(TEXT("A v1 hole resolves against a default selection"),
        ResolveHoleForLayout(Legacy, MakeLayout(TEXT("Championship"), TEXT("Default"), TEXT("Default")),
            Resolved, Error));
    TestEqual(TEXT("The authored tee is unchanged"), Resolved.TeeLocationCm, Legacy.TeeLocationCm);
    TestEqual(TEXT("The authored basket is unchanged"), Resolved.BasketLocationCm, Legacy.BasketLocationCm);
    TestEqual(TEXT("The authored par is unchanged"), Resolved.Par, Legacy.Par);

    // A layout that says nothing about a hole leaves it exactly as authored.
    FDiscGolfCourseLayoutDefinition Elsewhere;
    Elsewhere.LayoutId = TEXT("Elsewhere");
    FDiscGolfLayoutHoleSelection OtherHole;
    OtherHole.HoleNumber = 2;
    Elsewhere.Holes.Add(OtherHole);
    TestTrue(TEXT("An unmentioned hole resolves"),
        ResolveHoleForLayout(Legacy, Elsewhere, Resolved, Error));
    TestEqual(TEXT("An unmentioned hole keeps its authored basket"),
        Resolved.BasketLocationCm, Legacy.BasketLocationCm);

    // A v1 manifest implies exactly one layout covering every hole it lists.
    FDiscGolfCourseManifestDefinition Manifest = MakeLayoutManifest();
    Manifest.SchemaVersion = 1;
    Manifest.Layouts.Empty();
    const FDiscGolfCourseLayoutDefinition Implicit = ImplicitLayout(Manifest);
    TestEqual(TEXT("The implicit layout takes the manifest id"), Implicit.LayoutId, Manifest.LayoutId);
    TestEqual(TEXT("The implicit layout covers every manifest hole"),
        Implicit.Holes.Num(), Manifest.Holes.Num());
    TestNull(TEXT("A v1 manifest declares no named layouts"),
        FindLayout(Manifest, TEXT("Championship")));

    const TArray<FDiscGolfHoleBlockoutDefinition> Holes = { Legacy, MakeLayoutHole(2, false) };
    TestTrue(TEXT("The implicit layout validates"),
        ValidateLayout(Manifest, Implicit, Holes, Error));

    return true;
}

#endif
