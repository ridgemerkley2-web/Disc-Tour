#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DGFrameworkCourseDefinition.h"
#include "DiscGolfDiscDefinition.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfPlasticDefinition.h"
#include "DiscGolfTournamentDefinition.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBrandLicenseDefaultsFailClosedTest,
    "DiscGolfTour.BrandLicense.DefaultsFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBrandLicenseDefaultsFailClosedTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    UDiscGolfCourseDefinition* Course =
        NewObject<UDiscGolfCourseDefinition>(GetTransientPackage());
    UDiscGolfDiscDefinition* Disc =
        NewObject<UDiscGolfDiscDefinition>(GetTransientPackage());
    UDiscGolfPlasticDefinition* Plastic =
        NewObject<UDiscGolfPlasticDefinition>(GetTransientPackage());
    UDiscGolfTournamentDefinition* Tournament =
        NewObject<UDiscGolfTournamentDefinition>(GetTransientPackage());

    TestNotNull(TEXT("A transient course definition can be created"), Course);
    TestNotNull(TEXT("A transient disc definition can be created"), Disc);
    TestNotNull(TEXT("A transient plastic definition can be created"), Plastic);
    TestNotNull(TEXT("A transient tournament definition can be created"), Tournament);
    if (!Course || !Disc || !Plastic || !Tournament)
    {
        return false;
    }

    const FName GenericBrand(TEXT("dg_generic"));
    TestEqual(TEXT("Course definitions default to the generic brand"),
        Course->BrandId, GenericBrand);
    TestEqual(TEXT("Disc definitions default to the generic brand"),
        Disc->BrandId, GenericBrand);
    TestEqual(TEXT("Plastic definitions default to the generic brand"),
        Plastic->BrandId, GenericBrand);
    TestEqual(TEXT("Tournament definitions default to the generic brand"),
        Tournament->PresentingBrandId, GenericBrand);
    TestEqual(TEXT("Disc definitions default to the generic stamp"),
        Disc->DefaultStampId, FName(TEXT("dg_generic_default")));

    const FDGDiscInstance DefaultDiscInstance;
    const FDGDiscBagLoadout DefaultBag;
    TestEqual(TEXT("Disc instances default to the generic stamp"),
        DefaultDiscInstance.StampId, FName(TEXT("dg_generic_default")));
    TestEqual(TEXT("Bag loadouts default to generic equipment"),
        DefaultBag.BagEquipmentId, FName(TEXT("dg_generic_bag_default")));

    return !HasAnyErrors();
}

#endif
