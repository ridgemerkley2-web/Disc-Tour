#include "DiscGolfSession7FullCharacterSmokeRunner.h"

#include "DiscBagComponent.h"
#include "DiscGolfCharacterCustomizationComponent.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfCosmeticCatalog.h"
#include "DiscGolfCosmeticItem.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolferPawn.h"
#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitComponent.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "ThrowControllerComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace DiscGolfSession7FullCharacterSmoke
{
const FName Session7SmokeGripSocket(TEXT("disc_grip_r"));
const FName Session7SmokeDefaultVariant(TEXT("Default"));
const FName Session7SmokeCapId(TEXT("proxy_s6_headwear_cap_01"));

bool Session7SmokeBodyMatches(
    const FDGBodyProfile& A,
    const FDGBodyProfile& B)
{
    return FMath::IsNearlyEqual(A.HeightCm, B.HeightCm)
        && FMath::IsNearlyEqual(A.WingspanScale, B.WingspanScale)
        && FMath::IsNearlyEqual(A.ShoulderWidthScale, B.ShoulderWidthScale)
        && FMath::IsNearlyEqual(A.TorsoLengthScale, B.TorsoLengthScale)
        && FMath::IsNearlyEqual(A.LegLengthScale, B.LegLengthScale)
        && FMath::IsNearlyEqual(A.HandScale, B.HandScale)
        && FMath::IsNearlyEqual(A.MassKg, B.MassKg);
}

bool Session7SmokeThrowStyleMatches(
    const FDGThrowStyle& A,
    const FDGThrowStyle& B)
{
    return FMath::IsNearlyEqual(A.RunUpIntensity, B.RunUpIntensity)
        && FMath::IsNearlyEqual(A.ReachBackAmount, B.ReachBackAmount)
        && FMath::IsNearlyEqual(A.TorsoRotation, B.TorsoRotation)
        && FMath::IsNearlyEqual(A.BraceIntensity, B.BraceIntensity)
        && FMath::IsNearlyEqual(A.Explosiveness, B.Explosiveness)
        && FMath::IsNearlyEqual(A.FollowThrough, B.FollowThrough)
        && FMath::IsNearlyEqual(A.PowerMultiplier, B.PowerMultiplier)
        && FMath::IsNearlyEqual(A.SpinMultiplier, B.SpinMultiplier);
}

bool Session7SmokeLoadBytesIfPresent(
    const FString& Path,
    TArray<uint8>& OutBytes)
{
    OutBytes.Reset();
    return !IFileManager::Get().FileExists(*Path)
        || FFileHelper::LoadFileToArray(OutBytes, *Path);
}
}

using namespace DiscGolfSession7FullCharacterSmoke;

void ADiscGolfSession7FullCharacterSmokeRunner::Start()
{
    FParse::Value(
        FCommandLine::Get(), TEXT("Session7FullCharacterFixture="),
        RequestedFixture);
    FParse::Value(
        FCommandLine::Get(), TEXT("Session4Profile="), RequestedProfile);
    if (RequestedFixture.IsEmpty())
    {
        RequestedFixture = TEXT("BaselineDefaultShortSimple");
    }
    if (RequestedProfile.IsEmpty())
    {
        RequestedProfile = TEXT("Baseline");
    }

    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    UDiscGolfTourGameInstance* Instance =
        Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    ValidationSaveSlot = Instance
        ? Instance->GetSession7FullCharacterValidationSaveSlot() : FString();
    if (!Golfer || !PlayerController || !GameMode || !Instance
        || ValidationSaveSlot.IsEmpty() || !Golfer->GetCosmeticCatalog()
        || !Golfer->GetOutfitCatalog() || !Golfer->GetSkeletalGolferMesh()
        || !Golfer->GetModularHeadMesh()
        || !Golfer->GetCharacterCustomizationComponent()
        || !Golfer->GetOutfitComponent())
    {
        Fail(TEXT("isolated slot, player, catalogs, modular head, or one framework component was unavailable"));
        return;
    }
    if (!Instance->GetProfile()
        || Instance->GetProfile()->SaveSchemaVersion != DiscGolfSaveSchema::CurrentVersion)
    {
        Fail(TEXT("Session 7 runner did not start from the current schema-9 profile"));
        return;
    }

    OpeningCustomization = Golfer->GetCurrentFullCharacterCustomization();
    OpeningCustomizationAuthority = Golfer->GetCharacterCustomizationComponent();
    OpeningOutfitAuthority = Golfer->GetOutfitComponent();
    FString Error;
    if (!ValidateRequestedProfile(Error)
        || !SnapshotThrowAuthority(AuthorityBeforeCustomization))
    {
        Fail(Error.IsEmpty()
            ? TEXT("could not snapshot accepted throw authority") : Error);
        return;
    }
    if (!RunHairCoveragePreservationTrial(Error))
    {
        Fail(Error);
        return;
    }
    if (!BuildAndApplyFixture(Error))
    {
        Fail(Error);
        return;
    }
    ExpectedCustomization = Golfer->GetCurrentFullCharacterCustomization();
    if (!ValidateThrowAuthorityUnchanged(Error)
        || !ValidateRuntimePresentation(Error))
    {
        Fail(Error);
        return;
    }
    bValidationTempSlotDeleted = CleanupValidationSaveSlot();
    if (!bValidationTempSlotDeleted)
    {
        Fail(TEXT("the isolated Session 7 GUID save slot could not be deleted before the throw"));
        return;
    }

    bPreflightPassed = true;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 7 FULL CHARACTER THROW SMOKE START: fixture=%s profile=%s validation_slot_clean=1; delegating release, authoritative disc, flight and recovery to accepted Session 3."),
        *RequestedFixture, *RequestedProfile);
    Super::Start();
}

bool ADiscGolfSession7FullCharacterSmokeRunner::ValidateRequestedProfile(
    FString& OutError) const
{
    if (!Golfer)
    {
        OutError = TEXT("requested profile assertion lost the golfer");
        return false;
    }
    FDGBodyProfile ActualBody;
    FDGThrowStyle ActualStyle;
    EDGHandedness ActualHandedness = EDGHandedness::Left;
    if (!Golfer->GetCharacterCreatorProfile(
            ActualBody, ActualStyle, ActualHandedness))
    {
        OutError = TEXT("requested profile could not be read from the runtime pawn");
        return false;
    }

    FDGBodyProfile ExpectedBody;
    FDGThrowStyle ExpectedStyle;
    EDGHandedness ExpectedHandedness = EDGHandedness::Right;
    const FString Normalized = RequestedProfile.ToLower();
    const TCHAR* AssetPath = nullptr;
    if (Normalized == TEXT("shortcompact"))
    {
        AssetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact");
    }
    else if (Normalized == TEXT("baseline"))
    {
        AssetPath = TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter");
    }
    else if (Normalized == TEXT("talllongarms"))
    {
        AssetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms");
    }
    if (AssetPath)
    {
        const UDiscGolfCharacterProfile* Expected =
            LoadObject<UDiscGolfCharacterProfile>(nullptr, AssetPath);
        if (!Expected)
        {
            OutError = FString::Printf(
                TEXT("requested Session 4 profile asset is unavailable: %s"),
                AssetPath);
            return false;
        }
        ExpectedBody = Expected->Body;
        ExpectedStyle = Expected->ThrowStyle;
        ExpectedHandedness = Expected->Handedness;
    }
    else if (Normalized == TEXT("slidermax"))
    {
        using namespace DiscGolfCharacterCreatorSchema;
        ExpectedBody.HeightCm = MaxHeightCm;
        ExpectedBody.WingspanScale = MaxWingspanScale;
        ExpectedBody.ShoulderWidthScale = MaxShoulderWidthScale;
        ExpectedBody.TorsoLengthScale = MaxTorsoLengthScale;
        ExpectedBody.LegLengthScale = MaxLegLengthScale;
        ExpectedBody.HandScale = MaxHandScale;
        ExpectedBody.MassKg = MaxMassKg;
        ExpectedStyle.RunUpIntensity = 1.0f;
        ExpectedStyle.ReachBackAmount = 1.0f;
        ExpectedStyle.TorsoRotation = 1.0f;
        ExpectedStyle.BraceIntensity = 1.0f;
        ExpectedStyle.Explosiveness = 1.0f;
        ExpectedStyle.FollowThrough = 1.0f;
    }
    else
    {
        OutError = FString::Printf(
            TEXT("unsupported Session 7 profile assertion: %s"),
            *RequestedProfile);
        return false;
    }

    const FDiscGolfCharacterProfileSaveData SafeExpected =
        FDiscGolfCharacterProfileSaveData::FromFramework(
            ExpectedBody, ExpectedStyle, ExpectedHandedness);
    ExpectedBody = SafeExpected.ToBodyProfile();
    ExpectedStyle = SafeExpected.ToThrowStyle();
    ExpectedHandedness = SafeExpected.GetHandedness();
    if (ActualHandedness != ExpectedHandedness
        || !Session7SmokeBodyMatches(ActualBody, ExpectedBody)
        || !Session7SmokeThrowStyleMatches(ActualStyle, ExpectedStyle))
    {
        OutError = FString::Printf(
            TEXT("-Session4Profile=%s was not asserted on the actual pawn (actual_height=%.2f expected_height=%.2f)"),
            *RequestedProfile, ActualBody.HeightCm, ExpectedBody.HeightCm);
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterSmokeRunner::SnapshotThrowAuthority(
    FThrowAuthoritySnapshot& OutSnapshot) const
{
    if (!Golfer || !Golfer->GetThrowController()
        || !Golfer->GetSkeletalGolferMesh())
    {
        return false;
    }
    const UThrowControllerComponent* Controller = Golfer->GetThrowController();
    USkeletalMeshComponent* SkeletalMesh = Golfer->GetSkeletalGolferMesh();
    OutSnapshot.Power01 = Controller->GetPower01();
    OutSnapshot.HyzerDeg = Controller->GetHyzerDeg();
    OutSnapshot.NoseDeg = Controller->GetNoseDeg();
    OutSnapshot.LaunchAngleDeg = Controller->GetLaunchAngleDeg();
    OutSnapshot.ThrowStyle = Controller->GetThrowStyle();
    OutSnapshot.ShotContext = Controller->GetShotContext();
    OutSnapshot.AimDirection = Golfer->GetActorForwardVector();
    OutSnapshot.DiscBag = Golfer->GetDiscBag();
    OutSnapshot.GripSkeletalMesh = SkeletalMesh->GetSkeletalMeshAsset();
    OutSnapshot.GripBoneIndex = SkeletalMesh->GetBoneIndex(Session7SmokeGripSocket);
    OutSnapshot.GripComponentTransform =
        SkeletalMesh->GetSocketTransform(Session7SmokeGripSocket, RTS_Component);
    return OutSnapshot.DiscBag != nullptr
        && OutSnapshot.GripSkeletalMesh != nullptr
        && OutSnapshot.GripBoneIndex != INDEX_NONE
        && !OutSnapshot.AimDirection.ContainsNaN()
        && !OutSnapshot.AimDirection.IsNearlyZero()
        && !OutSnapshot.GripComponentTransform.ContainsNaN()
        && !OutSnapshot.GripComponentTransform.GetLocation().IsNearlyZero()
        && OutSnapshot.GripComponentTransform.GetLocation().SizeSquared()
            < FMath::Square(10000.0f);
}

bool ADiscGolfSession7FullCharacterSmokeRunner::ValidateThrowAuthorityUnchanged(
    FString& OutError) const
{
    FThrowAuthoritySnapshot Current;
    if (!SnapshotThrowAuthority(Current))
    {
        OutError = TEXT("throw authority disappeared after full-character application");
        return false;
    }
    if (!FMath::IsNearlyEqual(Current.Power01, AuthorityBeforeCustomization.Power01)
        || !FMath::IsNearlyEqual(Current.HyzerDeg, AuthorityBeforeCustomization.HyzerDeg)
        || !FMath::IsNearlyEqual(Current.NoseDeg, AuthorityBeforeCustomization.NoseDeg)
        || !FMath::IsNearlyEqual(
            Current.LaunchAngleDeg, AuthorityBeforeCustomization.LaunchAngleDeg)
        || Current.ThrowStyle != AuthorityBeforeCustomization.ThrowStyle
        || Current.ShotContext != AuthorityBeforeCustomization.ShotContext
        || !Current.AimDirection.Equals(
            AuthorityBeforeCustomization.AimDirection, 0.0001f)
        || Current.DiscBag != AuthorityBeforeCustomization.DiscBag)
    {
        OutError = TEXT("character presentation changed aim, power/timing inputs, shot context, or inventory authority");
        return false;
    }
    if (Current.GripSkeletalMesh != AuthorityBeforeCustomization.GripSkeletalMesh
        || Current.GripBoneIndex != AuthorityBeforeCustomization.GripBoneIndex)
    {
        OutError = TEXT("character presentation replaced the accepted skeletal mesh or disc_grip_r bone contract");
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterSmokeRunner::RunHairCoveragePreservationTrial(
    FString& OutError)
{
    FDGFullCharacterCustomization Hat = OpeningCustomization;
    Hat.Hair.HairStyleId = TEXT("hair_medium");
    Hat.Outfit.Equipped.Reset();
    FString Status;
    if (!DiscGolfOutfitRuntime::SetSlotSelection(
            Hat.Outfit, EDGOutfitSlot::Headwear,
            Session7SmokeCapId, Session7SmokeDefaultVariant,
            Golfer->GetOutfitCatalog(), Hat.Body, Status)
        || !Golfer->ApplyFullCharacterCustomizationTransactionally(
            Hat, false, Status))
    {
        OutError = FString::Printf(
            TEXT("hat/hair coverage setup failed: %s"), *Status);
        return false;
    }
    const FDGFullCharacterCustomization Covered =
        Golfer->GetCurrentFullCharacterCustomization();
    bool bCoveredHairComponentVisible = false;
    TArray<UPrimitiveComponent*> Primitives;
    Golfer->GetComponents(Primitives);
    for (const UPrimitiveComponent* Component : Primitives)
    {
        bCoveredHairComponentVisible |= Component && Component->IsRegistered()
            && Component->ComponentHasTag(TEXT("hair_medium"));
    }
    if (Covered.Hair.HairStyleId != FName(TEXT("hair_medium"))
        || !Golfer->IsHairHiddenByOutfitCoverage()
        || bCoveredHairComponentVisible)
    {
        OutError = TEXT("headwear did not hide only the visible hairstyle while preserving hair_medium");
        return false;
    }

    FDGFullCharacterCustomization Restored = Covered;
    Restored.Outfit.Equipped.Reset();
    if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
            Restored, false, Status))
    {
        OutError = FString::Printf(
            TEXT("hair restoration application failed: %s"), *Status);
        return false;
    }
    bool bRestoredHairComponentVisible = false;
    Primitives.Reset();
    Golfer->GetComponents(Primitives);
    for (const UPrimitiveComponent* Component : Primitives)
    {
        bRestoredHairComponentVisible |= Component && Component->IsRegistered()
            && Component->ComponentHasTag(TEXT("hair_medium"));
    }
    const FDGFullCharacterCustomization Uncovered =
        Golfer->GetCurrentFullCharacterCustomization();
    if (Uncovered.Hair.HairStyleId != FName(TEXT("hair_medium"))
        || Golfer->IsHairHiddenByOutfitCoverage()
        || !bRestoredHairComponentVisible
        || Uncovered.Hair.HairColor != Covered.Hair.HairColor)
    {
        OutError = TEXT("removing headwear did not restore the same hairstyle selection and color");
        return false;
    }
    if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
            OpeningCustomization, true, Status))
    {
        OutError = TEXT("hair coverage trial could not restore its opening character");
        return false;
    }
    bHairHatSelectionPreserved = true;
    return true;
}

bool ADiscGolfSession7FullCharacterSmokeRunner::BuildAndApplyFixture(
    FString& OutError)
{
    if (RequestedFixture.Equals(TEXT("Schema8Migration"), ESearchCase::IgnoreCase)
        && !RunSchema8MigrationTrial(OutError))
    {
        return false;
    }
    if (RequestedFixture.Equals(TEXT("RandomizeApplyReload"), ESearchCase::IgnoreCase))
    {
        return RunRandomizeApplyReloadTrial(OutError);
    }
    if (RequestedFixture.Equals(TEXT("RandomizeCancel"), ESearchCase::IgnoreCase))
    {
        return RunRandomizeCancelTrial(OutError);
    }

    FDGFullCharacterCustomization Requested = OpeningCustomization;
    bool bFullOutfit = false;
    bool bIncludeHeadwear = false;
    bool bMissingFallback = false;
    if (RequestedFixture.Equals(
            TEXT("BaselineDefaultShortSimple"), ESearchCase::IgnoreCase))
    {
        DiscGolfFullCharacterRuntime::ApplyFacePreset(
            TEXT("face_default"), Requested.Face);
        Requested.Hair.HairStyleId = TEXT("hair_short");
        FixturePassTokens = TEXT("face_default=1 hair_short=1 simple_outfit=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("BaselineSquareBeardHatFull"), ESearchCase::IgnoreCase))
    {
        DiscGolfFullCharacterRuntime::ApplyFacePreset(
            TEXT("face_square"), Requested.Face);
        Requested.Hair.HairStyleId = TEXT("hair_medium");
        Requested.Hair.FacialHairId = TEXT("facialhair_beard");
        bFullOutfit = true;
        bIncludeHeadwear = true;
        FixturePassTokens = TEXT("face_square=1 facialhair_beard=1 hat_hides_hair=1 full_outfit=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("ShortNarrowMedium"), ESearchCase::IgnoreCase))
    {
        DiscGolfFullCharacterRuntime::ApplyFacePreset(
            TEXT("face_narrow"), Requested.Face);
        Requested.Hair.HairStyleId = TEXT("hair_medium");
        FixturePassTokens = TEXT("face_narrow=1 hair_medium=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("TallRoundBeardFull"), ESearchCase::IgnoreCase))
    {
        DiscGolfFullCharacterRuntime::ApplyFacePreset(
            TEXT("face_round"), Requested.Face);
        Requested.Hair.HairStyleId = TEXT("hair_short");
        Requested.Hair.FacialHairId = TEXT("facialhair_beard");
        bFullOutfit = true;
        FixturePassTokens = TEXT("face_round=1 facialhair_beard=1 full_outfit=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("BodyFaceExtremes"), ESearchCase::IgnoreCase))
    {
        Requested.BodyBuild.Muscularity = 1.0f;
        Requested.BodyBuild.BodyFat = 0.0f;
        Requested.BodyBuild.Chest = 1.0f;
        Requested.BodyBuild.Waist = -1.0f;
        Requested.BodyBuild.Hips = 1.0f;
        Requested.BodyBuild.Arms = -1.0f;
        Requested.BodyBuild.Legs = 1.0f;
        int32 MorphIndex = 0;
        for (FName Key : DiscGolfFullCharacterRuntime::GetFaceMorphKeys())
        {
            Requested.Face.MorphValues.Add(
                Key, (MorphIndex++ % 2 == 0) ? -1.0f : 1.0f);
        }
        // The proxy contract has four stable preset IDs. Keep the fixture on a
        // valid ID while the explicit 20-value map carries the custom extremes.
        Requested.Face.PresetId = TEXT("face_default");
        bFullOutfit = true;
        FixturePassTokens = TEXT("body_face_min_max=1 full_outfit=1");
    }
    else if (RequestedFixture.Equals(TEXT("MissingHair"), ESearchCase::IgnoreCase))
    {
        Requested.Hair.HairStyleId = TEXT("session7_missing_hair_fixture");
        bMissingFallback = true;
        FixturePassTokens = TEXT("missing_hair_fallback=1 missing_fallback=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("MissingFacialHairEyebrow"), ESearchCase::IgnoreCase))
    {
        Requested.Hair.FacialHairId = TEXT("session7_missing_facialhair_fixture");
        Requested.Hair.EyebrowId = TEXT("session7_missing_eyebrow_fixture");
        bMissingFallback = true;
        FixturePassTokens = TEXT("missing_facialhair_fallback=1 missing_eyebrow_fallback=1 missing_fallback=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("MissingScarTattooOutfit"), ESearchCase::IgnoreCase))
    {
        Requested.Appearance.ScarId = TEXT("session7_missing_scar_fixture");
        Requested.Appearance.TattooIds = {TEXT("session7_missing_tattoo_fixture")};
        bMissingFallback = true;
        FixturePassTokens = TEXT("missing_scar_tattoo_fallback=1 missing_outfit_fallback=1 missing_fallback=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("Schema8Migration"), ESearchCase::IgnoreCase))
    {
        DiscGolfFullCharacterRuntime::ApplyFacePreset(
            TEXT("face_default"), Requested.Face);
        Requested.Hair.HairStyleId = TEXT("hair_short");
        FixturePassTokens = TEXT("schema8_migration=1 migration_reload=1 migration_preserved_schema8=1");
    }
    else if (RequestedFixture.Equals(
            TEXT("CompleteCharacterRHBH"), ESearchCase::IgnoreCase))
    {
        Requested.Identity.DisplayName = TEXT("Session Seven Complete");
        Requested.Identity.VoiceId = TEXT("voice_alt");
        Requested.Identity.PronounSetId = TEXT("pronouns_they_them");
        DiscGolfFullCharacterRuntime::ApplyFacePreset(
            TEXT("face_square"), Requested.Face);
        Requested.Hair.HairStyleId = TEXT("hair_mohawk");
        Requested.Hair.FacialHairId = TEXT("facialhair_beard");
        Requested.Hair.EyebrowId = TEXT("brow_alt");
        Requested.Appearance.SkinTone = FLinearColor(0.40f, 0.23f, 0.15f, 1.0f);
        Requested.Appearance.EyeColor = FLinearColor(0.08f, 0.18f, 0.26f, 1.0f);
        Requested.Appearance.Complexion = 0.70f;
        Requested.Appearance.Freckles = 0.65f;
        Requested.Appearance.SunExposure = 0.55f;
        Requested.Appearance.ScarId = TEXT("scar_proxy");
        Requested.Appearance.TattooIds = {TEXT("tattoo_proxy")};
        bFullOutfit = true;
        FixturePassTokens = TEXT("complete_character=1 release_components_stable=1 followthrough_components_stable=1");
    }
    else
    {
        OutError = FString::Printf(
            TEXT("unknown Session 7 full-character fixture: %s"),
            *RequestedFixture);
        return false;
    }

    if (!BuildOutfit(
            bFullOutfit, bIncludeHeadwear, Requested.Outfit, OutError))
    {
        return false;
    }
    if (RequestedFixture.Equals(
            TEXT("MissingScarTattooOutfit"), ESearchCase::IgnoreCase))
    {
        FDGEquippedOutfitEntry Missing;
        Missing.Slot = EDGOutfitSlot::Accessory;
        Missing.ItemId = TEXT("session7_missing_outfit_fixture");
        Missing.VariantId = Session7SmokeDefaultVariant;
        Requested.Outfit.Equipped.Add(Missing);
    }

    const FDiscGolfFullCustomizationResolution Resolution =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            Requested, Golfer->GetCosmeticCatalog(), Golfer->GetOutfitCatalog());
    if (bMissingFallback == Resolution.bAllCosmeticsResolved)
    {
        OutError = bMissingFallback
            ? TEXT("missing cosmetic fixture did not report a fallback")
            : TEXT("catalog-valid fixture unexpectedly required a fallback");
        return false;
    }
    if (RequestedFixture.Equals(TEXT("MissingHair"), ESearchCase::IgnoreCase)
        && Resolution.Character.Hair.HairStyleId != FName(TEXT("hair_none")))
    {
        OutError = TEXT("missing hair did not resolve to hair_none");
        return false;
    }
    if (RequestedFixture.Equals(
            TEXT("MissingFacialHairEyebrow"), ESearchCase::IgnoreCase)
        && (Resolution.Character.Hair.FacialHairId
                != FName(TEXT("facialhair_none"))
            || Resolution.Character.Hair.EyebrowId
                != FName(TEXT("brow_default"))))
    {
        OutError = TEXT("missing facial hair/eyebrow did not resolve to schema fallbacks");
        return false;
    }
    if (RequestedFixture.Equals(
            TEXT("MissingScarTattooOutfit"), ESearchCase::IgnoreCase)
        && (Resolution.Character.Appearance.ScarId
                != FName(TEXT("scar_none"))
            || !Resolution.Character.Appearance.TattooIds.IsEmpty()
            || DiscGolfOutfitRuntime::FindEntryForSlot(
                Resolution.Character.Outfit, EDGOutfitSlot::Accessory)))
    {
        OutError = TEXT("missing scar/tattoo/outfit did not resolve to safe none");
        return false;
    }

    FString Status;
    if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
            Requested, bMissingFallback, Status)
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Golfer->GetCurrentFullCharacterCustomization(), Resolution.Character))
    {
        OutError = FString::Printf(
            TEXT("full-character fixture did not apply canonically: %s"), *Status);
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterSmokeRunner::RunSchema8MigrationTrial(
    FString& OutError)
{
    UDiscGolfSaveGame* Legacy = NewObject<UDiscGolfSaveGame>(this);
    if (!Legacy)
    {
        OutError = TEXT("could not allocate schema-8 migration fixture");
        return false;
    }
    Legacy->SaveSchemaVersion = 8;
    Legacy->CharacterProfile = FDiscGolfCharacterProfileSaveData::FromFramework(
        OpeningCustomization.Body,
        OpeningCustomization.ThrowStyle,
        OpeningCustomization.Identity.Handedness,
        OpeningCustomization.BodyBuild);
    FString OutfitError;
    if (!BuildOutfit(false, false, Legacy->OutfitLoadout, OutfitError))
    {
        OutError = OutfitError;
        return false;
    }
    Legacy->CharacterCustomization =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    DiscGolfFullCharacterRuntime::ApplyFacePreset(
        TEXT("face_round"), Legacy->CharacterCustomization.Face);
    Legacy->CharacterCustomization.Hair.HairStyleId = TEXT("hair_mohawk");
    if (!UGameplayStatics::SaveGameToSlot(Legacy, ValidationSaveSlot, 0))
    {
        OutError = TEXT("schema-8 fixture could not write the isolated GUID slot");
        return false;
    }
    UDiscGolfSaveGame* Loaded = Cast<UDiscGolfSaveGame>(
        UGameplayStatics::LoadGameFromSlot(ValidationSaveSlot, 0));
    if (!Loaded || Loaded->SaveSchemaVersion != 8
        || DiscGolfProfilePersistence::MigrateToCurrent(*Loaded)
            != DiscGolfProfilePersistence::EMigrationResult::Migrated
        || Loaded->SaveSchemaVersion != DiscGolfSaveSchema::CurrentVersion)
    {
        OutError = TEXT("schema-8 fixture did not migrate exactly once to schema 9");
        return false;
    }
    FDGFullCharacterCustomization Expected =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    Expected.Identity.Handedness = Legacy->CharacterProfile.GetHandedness();
    Expected.Body = Legacy->CharacterProfile.ToBodyProfile();
    Expected.BodyBuild = Legacy->CharacterProfile.ToBodyBuildProfile();
    Expected.ThrowStyle = Legacy->CharacterProfile.ToThrowStyle();
    Expected.Outfit = DiscGolfOutfitRuntime::NormalizeForPersistence(
        Legacy->OutfitLoadout);
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Expected);
    if (!DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Loaded->CharacterCustomization, Expected)
        || Loaded->CharacterCustomization.Face.PresetId
            != FName(TEXT("face_default"))
        || Loaded->CharacterCustomization.Hair.HairStyleId
            != FName(TEXT("hair_none"))
        || !FMath::IsNearlyEqual(
            Loaded->CharacterCustomization.ThrowStyle.PowerMultiplier, 1.0f)
        || !FMath::IsNearlyEqual(
            Loaded->CharacterCustomization.ThrowStyle.SpinMultiplier, 1.0f)
        || !UGameplayStatics::SaveGameToSlot(Loaded, ValidationSaveSlot, 0))
    {
        OutError = TEXT("schema-8 values/defaults or normalized throw multipliers were not preserved");
        return false;
    }
    const UDiscGolfSaveGame* Reloaded = Cast<UDiscGolfSaveGame>(
        UGameplayStatics::LoadGameFromSlot(ValidationSaveSlot, 0));
    if (!Reloaded || Reloaded->SaveSchemaVersion != DiscGolfSaveSchema::CurrentVersion
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Reloaded->CharacterCustomization, Expected))
    {
        OutError = TEXT("migrated schema-9 full character did not survive disk reload");
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterSmokeRunner::RunRandomizeApplyReloadTrial(
    FString& OutError)
{
    if (!PlayerController->OpenCharacterCreator())
    {
        OutError = TEXT("randomize/apply fixture could not open the real creator");
        return false;
    }
    const FDGFullCharacterCustomization Before =
        PlayerController->GetCharacterCreatorDraftCustomization();
    FDiscGolfCharacterRandomizeLocks Locks;
    Locks.bIdentity = true;
    Locks.bOutfit = true;
    FDGFullCharacterCustomization Randomized;
    if (!PlayerController->RandomizeCharacterCreatorDraft(Locks, Randomized)
        || Randomized.Identity.DisplayName != Before.Identity.DisplayName
        || Randomized.Identity.Handedness != Before.Identity.Handedness
        || Randomized.Identity.VoiceId != Before.Identity.VoiceId
        || Randomized.Identity.PronounSetId != Before.Identity.PronounSetId
        || !DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Randomized.Outfit, Before.Outfit))
    {
        PlayerController->CancelCharacterCreator();
        OutError = TEXT("randomize did not preserve locked identity and outfit categories");
        return false;
    }
    const FDiscGolfFullCustomizationResolution Resolution =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            Randomized, Golfer->GetCosmeticCatalog(), Golfer->GetOutfitCatalog());
    if (!Resolution.bAllCosmeticsResolved
        || !PlayerController->ApplyFullCharacterCreatorDraft(Randomized))
    {
        if (PlayerController->IsCharacterCreatorOpen())
        {
            PlayerController->CancelCharacterCreator();
        }
        OutError = TEXT("catalog-valid randomized character could not Apply");
        return false;
    }
    const UDiscGolfSaveGame* Reloaded = Cast<UDiscGolfSaveGame>(
        UGameplayStatics::LoadGameFromSlot(ValidationSaveSlot, 0));
    if (!Reloaded || Reloaded->SaveSchemaVersion != DiscGolfSaveSchema::CurrentVersion
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Reloaded->CharacterCustomization,
            Golfer->GetCurrentFullCharacterCustomization()))
    {
        OutError = TEXT("Randomize -> Apply did not reload the complete schema-9 payload from disk");
        return false;
    }
    FixturePassTokens = TEXT("randomize_catalog_valid=1 randomize_locks_respected=1 apply_reloaded=1 validation_temp_slot_deleted=1");
    return true;
}

bool ADiscGolfSession7FullCharacterSmokeRunner::RunRandomizeCancelTrial(
    FString& OutError)
{
    TArray<uint8> SaveBefore;
    if (!Session7SmokeLoadBytesIfPresent(
            GetValidationSavePath(), SaveBefore)
        || !PlayerController->OpenCharacterCreator())
    {
        OutError = TEXT("randomize/cancel fixture could not snapshot the slot or open the creator");
        return false;
    }
    const FDGFullCharacterCustomization Before =
        PlayerController->GetCharacterCreatorDraftCustomization();
    FDiscGolfCharacterRandomizeLocks Locks;
    Locks.bIdentity = true;
    Locks.bOutfit = true;
    FDGFullCharacterCustomization Randomized;
    const bool bRandomized = PlayerController->RandomizeCharacterCreatorDraft(
        Locks, Randomized);
    const FDiscGolfFullCustomizationResolution Resolution =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            Randomized, Golfer->GetCosmeticCatalog(), Golfer->GetOutfitCatalog());
    PlayerController->CancelCharacterCreator();
    TArray<uint8> SaveAfter;
    if (!bRandomized || !Resolution.bAllCosmeticsResolved
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Golfer->GetCurrentFullCharacterCustomization(), Before)
        || !Session7SmokeLoadBytesIfPresent(
            GetValidationSavePath(), SaveAfter)
        || SaveBefore != SaveAfter)
    {
        OutError = TEXT("Randomize -> Cancel did not restore exactly or changed the isolated save");
        return false;
    }
    FixturePassTokens = TEXT("randomize_catalog_valid=1 cancel_restored_exact=1 cancel_wrote_no_save=1");
    return true;
}

bool ADiscGolfSession7FullCharacterSmokeRunner::BuildOutfit(
    bool bFull,
    bool bIncludeHeadwear,
    FDGOutfitLoadout& OutLoadout,
    FString& OutError) const
{
    OutLoadout.Equipped.Reset();
    if (bFull)
    {
        for (EDGOutfitSlot Slot : DiscGolfOutfitRuntime::GetOrderedSlots())
        {
            if (!bIncludeHeadwear && Slot == EDGOutfitSlot::Headwear)
            {
                continue;
            }
            if (!AddFirstCompatibleOutfitItem(
                    Slot, 0, OutLoadout, OutError))
            {
                return false;
            }
        }
        return true;
    }
    return AddFirstCompatibleOutfitItem(
            EDGOutfitSlot::Top, 0, OutLoadout, OutError)
        && AddFirstCompatibleOutfitItem(
            EDGOutfitSlot::Bottom, 0, OutLoadout, OutError)
        && AddFirstCompatibleOutfitItem(
            EDGOutfitSlot::Footwear, 0, OutLoadout, OutError);
}

bool ADiscGolfSession7FullCharacterSmokeRunner::AddFirstCompatibleOutfitItem(
    EDGOutfitSlot Slot,
    int32 VariantIndex,
    FDGOutfitLoadout& InOutLoadout,
    FString& OutError) const
{
    const TArray<FDiscGolfOutfitOption> Options =
        DiscGolfOutfitRuntime::GetOptionsForSlot(
            Golfer->GetOutfitCatalog(), Slot, OpeningCustomization.Body);
    const FDiscGolfOutfitOption* Selected = nullptr;
    for (const FDiscGolfOutfitOption& Option : Options)
    {
        if (Option.bCompatible && !Option.VariantIds.IsEmpty())
        {
            Selected = &Option;
            break;
        }
    }
    if (!Selected)
    {
        OutError = FString::Printf(
            TEXT("outfit slot %s had no compatible proxy option"),
            *DiscGolfOutfitRuntime::GetSlotDisplayName(Slot).ToString());
        return false;
    }
    const FName Variant = Selected->VariantIds[
        FMath::Clamp(VariantIndex, 0, Selected->VariantIds.Num() - 1)];
    return DiscGolfOutfitRuntime::SetSlotSelection(
        InOutLoadout, Slot, Selected->ItemId, Variant,
        Golfer->GetOutfitCatalog(), OpeningCustomization.Body, OutError);
}

bool ADiscGolfSession7FullCharacterSmokeRunner::ValidateRuntimePresentation(
    FString& OutError) const
{
    if (!Golfer || !Golfer->GetSkeletalGolferMesh()
        || !Golfer->GetModularHeadMesh())
    {
        OutError = TEXT("runtime presentation lost the body or modular head");
        return false;
    }
    int32 PawnCount = 0;
    for (TActorIterator<ADiscGolferPawn> It(GetWorld()); It; ++It)
    {
        PawnCount += IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
    }
    TArray<UDiscGolfCharacterCustomizationComponent*> CustomizationComponents;
    Golfer->GetComponents(CustomizationComponents);
    const int32 CustomizationCount = CustomizationComponents.Num();
    TArray<UDiscGolfOutfitComponent*> OutfitComponents;
    Golfer->GetComponents(OutfitComponents);
    const int32 OutfitCount = OutfitComponents.Num();
    USkeletalMeshComponent* Body = Golfer->GetSkeletalGolferMesh();
    USkeletalMeshComponent* Head = Golfer->GetModularHeadMesh();
    USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
    USkeletalMesh* HeadAsset = Head->GetSkeletalMeshAsset();
    if (PawnCount != 1 || CustomizationCount != 1 || OutfitCount != 1
        || Golfer->GetCharacterCustomizationComponent()
            != OpeningCustomizationAuthority.Get()
        || Golfer->GetOutfitComponent() != OpeningOutfitAuthority.Get()
        || !BodyAsset || !HeadAsset
        || HeadAsset->GetPathName()
            != DiscGolfFullCharacterRuntime::HeadMeshObjectPath
        || BodyAsset->GetSkeleton() != HeadAsset->GetSkeleton()
        || Head->LeaderPoseComponent.Get() != Body
        || Head->GetCollisionEnabled() != ECollisionEnabled::NoCollision
        || Head->GetGenerateOverlapEvents()
        || Head->CanEverAffectNavigation()
        || Head->IsSimulatingPhysics())
    {
        OutError = TEXT("one-pawn/component, master-skeleton, head leader-pose, or collision invariant failed");
        return false;
    }
    const FDGFullCharacterCustomization Current =
        Golfer->GetCurrentFullCharacterCustomization();
    if (!DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Current, ExpectedCustomization)
        || !FMath::IsNearlyEqual(Current.ThrowStyle.PowerMultiplier, 1.0f)
        || !FMath::IsNearlyEqual(Current.ThrowStyle.SpinMultiplier, 1.0f))
    {
        OutError = TEXT("complete customization changed or creator-only throw multipliers escaped normalization");
        return false;
    }
    return ValidateSelectedCosmeticComponents(OutError);
}

bool ADiscGolfSession7FullCharacterSmokeRunner::ValidateSelectedCosmeticComponents(
    FString& OutError) const
{
    const FDGFullCharacterCustomization Current =
        Golfer->GetCurrentFullCharacterCustomization();
    const FName Ids[] = {
        Current.Hair.HairStyleId,
        Current.Hair.FacialHairId,
        Current.Hair.EyebrowId,
    };
    TArray<UPrimitiveComponent*> Primitives;
    Golfer->GetComponents(Primitives);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ids); ++Index)
    {
        const FName ItemId = Ids[Index];
        const UDiscGolfCosmeticItem* Item =
            Golfer->GetCosmeticCatalog()->FindById(ItemId);
        const bool bNone = ItemId == FName(TEXT("hair_none"))
            || ItemId == FName(TEXT("facialhair_none"));
        if (!Item || bNone || (Index == 0 && Golfer->IsHairHiddenByOutfitCoverage()))
        {
            continue;
        }
        UPrimitiveComponent* Match = nullptr;
        for (UPrimitiveComponent* Component : Primitives)
        {
            if (Component && Component->IsRegistered()
                && Component->ComponentHasTag(TEXT("DG_CustomizationCosmetic"))
                && Component->ComponentHasTag(ItemId))
            {
                Match = Component;
                break;
            }
        }
        if (!Match || Match->GetAttachParent() != Golfer->GetModularHeadMesh()
            || Match->GetCollisionEnabled() != ECollisionEnabled::NoCollision
            || Match->GetGenerateOverlapEvents()
            || Match->CanEverAffectNavigation()
            || Match->IsSimulatingPhysics())
        {
            OutError = FString::Printf(
                TEXT("selected cosmetic %s was detached or retained physical authority"),
                *ItemId.ToString());
            return false;
        }
        if (const USkeletalMeshComponent* Skeletal =
                Cast<USkeletalMeshComponent>(Match))
        {
            if (Skeletal->LeaderPoseComponent.Get()
                != Golfer->GetModularHeadMesh())
            {
                OutError = FString::Printf(
                    TEXT("skeletal cosmetic %s did not follow the modular head"),
                    *ItemId.ToString());
                return false;
            }
        }
        else if (const UStaticMeshComponent* Static =
                Cast<UStaticMeshComponent>(Match))
        {
            if (!Static->IsUsingAbsoluteScale())
            {
                OutError = FString::Printf(
                    TEXT("static cosmetic %s did not isolate legacy socket scale"),
                    *ItemId.ToString());
                return false;
            }
        }
    }
    return true;
}

FString ADiscGolfSession7FullCharacterSmokeRunner::GetValidationSavePath() const
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("SaveGames"),
        ValidationSaveSlot + TEXT(".sav"));
}

bool ADiscGolfSession7FullCharacterSmokeRunner::CleanupValidationSaveSlot()
{
    if (ValidationSaveSlot.IsEmpty())
    {
        return false;
    }
    if (UGameplayStatics::DoesSaveGameExist(ValidationSaveSlot, 0))
    {
        UGameplayStatics::DeleteGameInSlot(ValidationSaveSlot, 0);
    }
    return !UGameplayStatics::DoesSaveGameExist(ValidationSaveSlot, 0)
        && !IFileManager::Get().FileExists(*GetValidationSavePath());
}

void ADiscGolfSession7FullCharacterSmokeRunner::Fail(const FString& Reason)
{
    const bool bSlotClean = CleanupValidationSaveSlot();
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("SESSION 7 FULL CHARACTER THROW SMOKE FAIL: fixture=%s profile=%s temp_slot_deleted=%d reason=%s"),
        *RequestedFixture, *RequestedProfile, bSlotClean ? 1 : 0, *Reason);
    Super::Fail(Reason);
}

void ADiscGolfSession7FullCharacterSmokeRunner::Pass()
{
    FString Error;
    if (!bPreflightPassed || !bHairHatSelectionPreserved
        || !ValidateRuntimePresentation(Error)
        || !CleanupValidationSaveSlot())
    {
        Fail(Error.IsEmpty()
            ? TEXT("post-recovery full-character or isolated-slot invariant failed")
            : Error);
        return;
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 7 FULL CHARACTER THROW SMOKE PASS: fixture=%s profile=%s runtime_profile_asserted=1 full_customization=1 schema9=1 one_pawn=1 one_customization_component=1 one_outfit_component=1 head_master_skeleton=1 head_leader_pose=1 cosmetics_attached=1 cosmetics_collision_free=1 disc_grip_r_contract_preserved=1 appearance_authority_unchanged=1 hair_hat_selection_preserved=1 one_animation=1 one_release=1 one_authoritative_disc=1 one_completed_flight=1 one_recovery=1 validation_temp_slot_deleted=1 %s"),
        *RequestedFixture, *RequestedProfile, *FixturePassTokens);
    Super::Pass();
}
