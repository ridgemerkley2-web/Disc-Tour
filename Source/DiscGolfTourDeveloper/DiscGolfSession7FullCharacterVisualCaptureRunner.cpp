#include "DiscGolfSession7FullCharacterVisualCaptureRunner.h"

#include "DiscActor.h"
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
#include "DiscGolfOutfitItem.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfThrowComponent.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameters.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace DiscGolfSession7FullCharacterVisual
{
constexpr int32 Session7VisualExpectedWidth = 1920;
constexpr int32 Session7VisualExpectedHeight = 1080;
constexpr int64 Session7VisualMinimumPngBytes = 32768;
constexpr double Session7VisualScreenshotTimeoutSeconds = 30.0;
constexpr double Session7VisualInitialUiSettleSeconds = 5.0;
constexpr float Session7VisualFullBodyFovDeg = 64.0f;
constexpr float Session7VisualHeadCloseupFovDeg = 50.0f;
constexpr const TCHAR* Session7VisualHeadMaterialObjectPath =
    TEXT("/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HeadProxy.M_DG_HeadProxy");
constexpr const TCHAR* Session7VisualHairMaterialObjectPath =
    TEXT("/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HairProxy.M_DG_HairProxy");
constexpr const TCHAR* Session7VisualOutfitMaterialObjectPath =
    TEXT("/Game/DiscGolf/Materials/Outfits/M_DG_OutfitProxy.M_DG_OutfitProxy");

const TCHAR* Session7VisualFilenames[] = {
    TEXT("01_Full_Creator_Overview.png"),
    TEXT("02_Identity_Tab.png"),
    TEXT("03_Body_Tab.png"),
    TEXT("04_Face_Tab.png"),
    TEXT("05_Hair_Tab.png"),
    TEXT("06_Appearance_Tab.png"),
    TEXT("07_Throw_Style_Tab.png"),
    TEXT("08_Outfit_Tab.png"),
    TEXT("09_Face_Default.png"),
    TEXT("10_Face_Square.png"),
    TEXT("11_Face_Narrow.png"),
    TEXT("12_Face_Round.png"),
    TEXT("13_Hair_Short.png"),
    TEXT("14_Hair_Medium.png"),
    TEXT("15_Facial_Hair.png"),
    TEXT("16_Multiple_Skin_Tones.png"),
    TEXT("17_Multiple_Eye_Colors.png"),
    TEXT("18_Hat_Hides_Hair.png"),
    TEXT("19_Hat_Removed_Hair_Restored.png"),
    TEXT("20_ShortCompact_Complete.png"),
    TEXT("21_Baseline_Complete.png"),
    TEXT("22_TallLongArms_Complete.png"),
    TEXT("23_Complete_RHBH_Release.png"),
    TEXT("24_Complete_RHBH_FollowThrough.png"),
};

const TCHAR* Session7VisualEvidenceLabels[] = {
    TEXT("One native full-character creator overview with global Reset, Randomize, Cancel and Apply controls"),
    TEXT("Identity tab: display name, handedness, voice and pronouns"),
    TEXT("Body tab: accepted body profile plus build sliders and three presets"),
    TEXT("Face tab: four presets and all twenty editable morph values"),
    TEXT("Hair tab: style, facial hair, eyebrow and color controls"),
    TEXT("Appearance tab: modular-head skin, eyes, complexion, freckles, sun, scar and tattoo"),
    TEXT("Throw Style tab: six presentation-only controls; gameplay power/spin remain normalized"),
    TEXT("Outfit tab: eleven stable-ID slots plus item and variant selection"),
    TEXT("Default face preset on the five visibly prepared proxy morph targets"),
    TEXT("Square face preset visibly changes supported proxy targets"),
    TEXT("Narrow face preset visibly changes supported proxy targets"),
    TEXT("Round face preset visibly changes supported proxy targets"),
    TEXT("Short catalog-backed proxy hair and canonical dynamic hair material"),
    TEXT("Medium catalog-backed proxy hair at the identical close camera"),
    TEXT("Catalog-backed proxy facial hair and eyebrow attached to the modular head"),
    TEXT("Second skin tone on the canonical modular-head material"),
    TEXT("Second eye color with skin/cosmetic/camera state held constant"),
    TEXT("Headwear coverage hides visible hair while hair_medium remains selected"),
    TEXT("Removing headwear restores the same hair_medium selection and color"),
    TEXT("ShortCompact complete representative character"),
    TEXT("Baseline complete representative character"),
    TEXT("TallLongArms complete representative character"),
    TEXT("Complete Baseline character at the accepted RHBH release event"),
    TEXT("Complete Baseline character at approximately 2.035 second follow-through"),
};

const TCHAR* Session7VisualTabLabels[] = {
    TEXT("Identity"), TEXT("Body"), TEXT("Face"), TEXT("Hair"),
    TEXT("Appearance"), TEXT("Throw Style"), TEXT("Outfit"),
};

const TCHAR* Session7VisualStableIds[] = {
    TEXT("hair_none"), TEXT("hair_short"), TEXT("hair_medium"),
    TEXT("hair_mohawk"), TEXT("facialhair_none"),
    TEXT("facialhair_stubble"), TEXT("facialhair_beard"),
    TEXT("brow_default"), TEXT("brow_alt"), TEXT("scar_none"),
    TEXT("scar_proxy"), TEXT("tattoo_none"), TEXT("tattoo_proxy"),
    TEXT("voice_default"), TEXT("voice_alt"), TEXT("pronouns_default"),
    TEXT("pronouns_they_them"),
};

const TCHAR* Session7VisualDeferredMorphs[] = {
    TEXT("brow_height"), TEXT("brow_depth"), TEXT("eye_size"),
    TEXT("eye_spacing"), TEXT("eye_depth"), TEXT("nose_width"),
    TEXT("nose_length"), TEXT("nose_bridge"), TEXT("cheek_width"),
    TEXT("jaw_height"), TEXT("chin_width"), TEXT("mouth_width"),
    TEXT("lip_fullness"), TEXT("ear_size"), TEXT("ear_angle"),
};

const TCHAR* Session7VisualVisibleMorphKeys[] = {
    TEXT("head_width"), TEXT("head_height"), TEXT("cheek_fullness"),
    TEXT("jaw_width"), TEXT("chin_length"),
};

const TCHAR* Session7VisualVisibleMorphTargets[] = {
    TEXT("DG_Face_HeadWidth"), TEXT("DG_Face_HeadHeight"),
    TEXT("DG_Face_CheekFullness"), TEXT("DG_Face_JawWidth"),
    TEXT("DG_Face_ChinLength"),
};

const TCHAR* Session7VisualManualCriteria[] = {
    TEXT("all_seven_creator_tabs_are_legible_and_the_preview_remains_in_the_right_pane"),
    TEXT("five_supported_proxy_face_morphs_and_four_presets_are_visibly_distinct"),
    TEXT("hair_facial_hair_brows_scar_and_tattoo_read_as_intentional_proxy_choices"),
    TEXT("skin_eye_complexion_freckles_and_sun_exposure_are_visible_on_the_modular_head"),
    TEXT("hat_hides_hair_and_removal_restores_the_same_selection_without_popping_or_detachment"),
    TEXT("short_compact_baseline_and_tall_long_arms_profiles_are_visibly_distinct"),
    TEXT("complete_character_release_and_follow_through_are_correctly_framed"),
    TEXT("no_visible_clipping_detachment_duplicate_body_or_duplicate_customization_authority"),
    TEXT("proxy_assets_are_clearly_marked_do_not_ship_and_not_mistaken_for_final_art"),
};

int32 Session7VisualBigEndianInt32(const uint8* Bytes)
{
    return (static_cast<int32>(Bytes[0]) << 24)
        | (static_cast<int32>(Bytes[1]) << 16)
        | (static_cast<int32>(Bytes[2]) << 8)
        | static_cast<int32>(Bytes[3]);
}

bool Session7VisualBodyMatches(
    const FDGBodyProfile& A,
    const FDGBodyProfile& B)
{
    return A.HeightCm == B.HeightCm
        && A.WingspanScale == B.WingspanScale
        && A.ShoulderWidthScale == B.ShoulderWidthScale
        && A.TorsoLengthScale == B.TorsoLengthScale
        && A.LegLengthScale == B.LegLengthScale
        && A.HandScale == B.HandScale
        && A.MassKg == B.MassKg;
}

TSharedPtr<FJsonValue> Session7VisualColorJson(const FLinearColor& Color)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Color.R));
    Values.Add(MakeShared<FJsonValueNumber>(Color.G));
    Values.Add(MakeShared<FJsonValueNumber>(Color.B));
    Values.Add(MakeShared<FJsonValueNumber>(Color.A));
    return MakeShared<FJsonValueArray>(Values);
}

bool BindSession7SelectedThrowProvenance(
    ADiscGolferPawn* Pawn,
    FThrowCommand& InOutCommand)
{
    UDiscBagComponent* Bag = Pawn ? Pawn->GetDiscBag() : nullptr;
    const UDiscGolfCharacterProfile* Profile = Pawn
        ? Pawn->GetRuntimeCharacterProfile() : nullptr;
    FDGDiscInstance SelectedInstance;
    if (!Bag || !Profile
        || !Bag->GetSelectedDiscInstance(SelectedInstance)
        || !SelectedInstance.InstanceId.IsValid()
        || SelectedInstance.DiscDefinitionId.IsNone()
        || Bag->GetSelectedMoldId() != SelectedInstance.DiscDefinitionId)
    {
        return false;
    }

    InOutCommand.DiscInstanceId = SelectedInstance.InstanceId;
    InOutCommand.MoldId = SelectedInstance.DiscDefinitionId;
    InOutCommand.Plastic = Bag->GetSelectedPlastic();
    InOutCommand.Handedness = Profile->Handedness;
    return true;
}
}

using namespace DiscGolfSession7FullCharacterVisual;

ADiscGolfSession7FullCharacterVisualCaptureRunner::
    ADiscGolfSession7FullCharacterVisualCaptureRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::Start()
{
    if (bStarted || bFinished)
    {
        return;
    }
    bStarted = true;
    GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    ThrowAdapter = Golfer ? Golfer->GetRHBHThrowAdapter() : nullptr;
    const UDiscGolfTourGameInstance* Instance =
        Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    ValidationSaveSlot = Instance
        ? Instance->GetSession7FullCharacterValidationSaveSlot() : FString();
    OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("CharacterFramework/Screenshots/Session7_FullCharacter"));
    ManifestPath = FPaths::Combine(
        OutputDirectory, TEXT("Session7_FullCharacter_CaptureManifest.json"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    if (!GameMode || !PlayerController || !Golfer || !ThrowAdapter || !Instance
        || ValidationSaveSlot.IsEmpty() || !Golfer->GetCosmeticCatalog()
        || !Golfer->GetOutfitCatalog() || !Golfer->GetSkeletalGolferMesh()
        || !Golfer->GetModularHeadMesh()
        || !Golfer->GetCharacterCustomizationComponent()
        || !Golfer->GetOutfitComponent()
        || UGameplayStatics::IsGamePaused(this))
    {
        Fail(TEXT("Session 7 visual runtime, guarded slot, catalogs, or modular character foundation was unavailable"));
        return;
    }
    for (const TCHAR* Filename : Session7VisualFilenames)
    {
        IFileManager::Get().Delete(
            *FPaths::Combine(OutputDirectory, Filename), false, true, true);
    }
    IFileManager::Get().Delete(*ManifestPath, false, true, true);
    if (!CleanupValidationSaveSlot())
    {
        Fail(TEXT("Session 7 visual GUID slot was not clean before persistence proof"));
        return;
    }
    SnapshotFiles(FPaths::ProjectContentDir(), true, InitialPackages);
    SnapshotFiles(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")),
        false, InitialSaveGames);
    bPersistentSnapshotReady = true;

    GameMode->SkipCurrentPresentation();
    BaselineDiscs = CountWorldDiscs();
    BaselineStrokes = GameMode->GetStrokes();
    OpeningCustomization = Golfer->GetCurrentFullCharacterCustomization();
    FString Error;
    if (!ProveCreatorAndPersistenceContracts(Error))
    {
        Fail(Error);
        return;
    }

    EvidenceDraft = DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    EvidenceDraft.Body = OpeningCustomization.Body;
    EvidenceDraft.BodyBuild = OpeningCustomization.BodyBuild;
    EvidenceDraft.ThrowStyle = OpeningCustomization.ThrowStyle;
    EvidenceDraft.Identity.Handedness = OpeningCustomization.Identity.Handedness;
    EvidenceDraft.Hair.HairStyleId = TEXT("hair_short");
    EvidenceDraft.Hair.EyebrowId = TEXT("brow_default");
    EvidenceDraft.Outfit.Equipped.Reset();
    if (!SetFirstOutfitItem(
            EDGOutfitSlot::Top, EvidenceDraft.Body, 0,
            EvidenceDraft.Outfit, Error)
        || !SetFirstOutfitItem(
            EDGOutfitSlot::Bottom, EvidenceDraft.Body, 0,
            EvidenceDraft.Outfit, Error)
        || !SetFirstOutfitItem(
            EDGOutfitSlot::Footwear, EvidenceDraft.Body, 0,
            EvidenceDraft.Outfit, Error))
    {
        Fail(Error);
        return;
    }
    FString Status;
    if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
            EvidenceDraft, false, Status)
        || !PlayerController->OpenCharacterCreator()
        || !PrepareCreatorDraft(0, EvidenceDraft, Error))
    {
        Fail(Error.IsEmpty()
            ? FString::Printf(TEXT("creator overview setup failed: %s"), *Status)
            : Error);
        return;
    }
    APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
    if (!CameraManager)
    {
        Fail(TEXT("visual capture could not acquire the real player camera manager"));
        return;
    }
    OpeningCameraFovDeg = CameraManager->GetFOVAngle();
    CameraManager->SetFOV(Session7VisualFullBodyFovDeg);
    bValidationCaptureFovLocked = true;
    RequestCapture(0);
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStarted || bFinished)
    {
        return;
    }
    if (bCapturePending)
    {
        PollCapture();
        return;
    }
    const int32 NextIndex = Captures.Num();
    if (NextIndex >= UE_ARRAY_COUNT(Session7VisualFilenames))
    {
        Pass();
        return;
    }
    if (!PrepareCapture(NextIndex) && !bFinished)
    {
        Fail(FString::Printf(
            TEXT("could not prepare Session 7 evidence frame %d"),
            NextIndex + 1));
    }
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    ProveCreatorAndPersistenceContracts(FString& OutError)
{
    if (!PlayerController->OpenCharacterCreator())
    {
        OutError = TEXT("real character creator could not open for behavioral proof");
        return false;
    }
    FDGFullCharacterCustomization Distinct =
        PlayerController->GetCharacterCreatorDraftCustomization();
    Distinct.Identity.DisplayName = TEXT("Session7 Draft Preservation");
    Distinct.Body.HeightCm = FMath::Clamp(
        Distinct.Body.HeightCm - 5.0f, 150.0f, 210.0f);
    if (!PrepareCreatorDraft(0, Distinct, OutError)
        || !PrepareCreatorDraft(1, Distinct, OutError))
    {
        return false;
    }
    bDraftPreservedAcrossTabs =
        PlayerController->GetCharacterCreatorActiveTabIndex() == 1
        && DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            PlayerController->GetCharacterCreatorDraftCustomization(), Distinct);

    FDGFullCharacterCustomization ResetTab;
    if (!PlayerController->ResetCharacterCreatorCurrentTab(1, ResetTab))
    {
        OutError = TEXT("Reset Current Tab did not execute through the controller");
        return false;
    }
    const FDGFullCharacterCustomization Defaults =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    bCurrentTabResetScoped = ResetTab.Identity.DisplayName
            == Distinct.Identity.DisplayName
        && ResetTab.Body.HeightCm == Defaults.Body.HeightCm
        && ResetTab.BodyBuild.Muscularity == Defaults.BodyBuild.Muscularity;
    FDGFullCharacterCustomization ResetAll;
    bResetAllRestoredDefaults = PlayerController->ResetCharacterCreatorAll(ResetAll)
        && DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            ResetAll, Defaults);

    FDiscGolfCharacterRandomizeLocks Locks;
    Locks.bIdentity = true;
    Locks.bOutfit = true;
    const FDGFullCharacterCustomization BeforeRandomize = ResetAll;
    FDGFullCharacterCustomization Randomized;
    if (!PlayerController->RandomizeCharacterCreatorDraft(Locks, Randomized))
    {
        OutError = TEXT("global Randomize did not execute through the real creator");
        return false;
    }
    bRandomizeLocksRespected = Randomized.Identity.DisplayName
            == BeforeRandomize.Identity.DisplayName
        && Randomized.Identity.Handedness == BeforeRandomize.Identity.Handedness
        && Randomized.Identity.VoiceId == BeforeRandomize.Identity.VoiceId
        && Randomized.Identity.PronounSetId
            == BeforeRandomize.Identity.PronounSetId
        && DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Randomized.Outfit, BeforeRandomize.Outfit);
    const FDiscGolfFullCustomizationResolution RandomizedResolution =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            Randomized, Golfer->GetCosmeticCatalog(), Golfer->GetOutfitCatalog());
    bRandomizeCatalogValid = RandomizedResolution.bAllCosmeticsResolved;

    if (!PlayerController->ApplyFullCharacterCreatorDraft(Randomized)
        || PlayerController->IsCharacterCreatorOpen()
        || !UGameplayStatics::DoesSaveGameExist(ValidationSaveSlot, 0))
    {
        OutError = TEXT("Apply did not close and save to the isolated GUID slot");
        return false;
    }
    bCreatorApplySaved = true;
    const UDiscGolfSaveGame* Reloaded = Cast<UDiscGolfSaveGame>(
        UGameplayStatics::LoadGameFromSlot(ValidationSaveSlot, 0));
    bCreatorApplyReloaded = Reloaded
        && Reloaded->SaveSchemaVersion == DiscGolfSaveSchema::CurrentVersion
        && DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Reloaded->CharacterCustomization,
            Golfer->GetCurrentFullCharacterCustomization());
    if (!bCreatorApplyReloaded || !PlayerController->OpenCharacterCreator())
    {
        OutError = TEXT("applied schema-10 character did not reload or reopen");
        return false;
    }
    TArray<uint8> BeforeCancelBytes;
    if (!FFileHelper::LoadFileToArray(
            BeforeCancelBytes, *GetValidationSavePath()))
    {
        OutError = TEXT("isolated Apply save could not be read for Cancel proof");
        return false;
    }
    const FDGFullCharacterCustomization Applied =
        PlayerController->GetCharacterCreatorDraftCustomization();
    FDGFullCharacterCustomization CancelDraft = Applied;
    CancelDraft.Appearance.SkinTone = FLinearColor(0.16f, 0.08f, 0.05f, 1.0f);
    CancelDraft.Hair.HairStyleId = TEXT("hair_mohawk");
    if (!PlayerController->PreviewFullCharacterCreatorDraft(CancelDraft))
    {
        OutError = TEXT("creator could not establish a distinct Cancel preview");
        return false;
    }
    PlayerController->CancelCharacterCreator();
    TArray<uint8> AfterCancelBytes;
    bCreatorCancelRestoredApplied = !PlayerController->IsCharacterCreatorOpen()
        && DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Golfer->GetCurrentFullCharacterCustomization(), Applied)
        && FFileHelper::LoadFileToArray(
            AfterCancelBytes, *GetValidationSavePath())
        && BeforeCancelBytes == AfterCancelBytes;

    if (!ProveSchema8Migration(OutError)
        || !ProveMissingFallbacks(OutError))
    {
        return false;
    }
    bValidationTempSlotDeleted = CleanupValidationSaveSlot();
    if (!bDraftPreservedAcrossTabs || !bCurrentTabResetScoped
        || !bResetAllRestoredDefaults || !bRandomizeLocksRespected
        || !bRandomizeCatalogValid || !bCreatorApplySaved
        || !bCreatorApplyReloaded || !bCreatorCancelRestoredApplied
        || !bSchema8MigrationReloaded
        || !bMissingCosmeticFallbacksResolved
        || !bValidationTempSlotDeleted)
    {
        OutError = TEXT("one or more Reset/Randomize/Apply/reload/Cancel/migration/fallback behavioral gates failed");
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    ProveSchema8Migration(FString& OutError)
{
    UDiscGolfSaveGame* Legacy = NewObject<UDiscGolfSaveGame>(this);
    if (!Legacy)
    {
        OutError = TEXT("schema-8 visual migration fixture allocation failed");
        return false;
    }
    const FDGFullCharacterCustomization Current =
        Golfer->GetCurrentFullCharacterCustomization();
    Legacy->SaveSchemaVersion = 8;
    Legacy->CharacterProfile = FDiscGolfCharacterProfileSaveData::FromFramework(
        Current.Body, Current.ThrowStyle, Current.Identity.Handedness,
        Current.BodyBuild);
    Legacy->OutfitLoadout = Current.Outfit;
    Legacy->CharacterCustomization =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    Legacy->CharacterCustomization.Hair.HairStyleId = TEXT("hair_mohawk");
    if (!UGameplayStatics::SaveGameToSlot(Legacy, ValidationSaveSlot, 0))
    {
        OutError = TEXT("schema-8 fixture could not use the isolated GUID slot");
        return false;
    }
    UDiscGolfSaveGame* Migrated = Cast<UDiscGolfSaveGame>(
        UGameplayStatics::LoadGameFromSlot(ValidationSaveSlot, 0));
    if (!Migrated || Migrated->SaveSchemaVersion != 8
        || DiscGolfProfilePersistence::MigrateToCurrent(*Migrated)
            != DiscGolfProfilePersistence::EMigrationResult::Migrated
        || Migrated->SaveSchemaVersion != DiscGolfSaveSchema::CurrentVersion
        || Migrated->CharacterCustomization.Hair.HairStyleId
            != FName(TEXT("hair_none"))
        || !Session7VisualBodyMatches(
            Migrated->CharacterCustomization.Body,
            Legacy->CharacterProfile.ToBodyProfile())
        || !UGameplayStatics::SaveGameToSlot(
            Migrated, ValidationSaveSlot, 0))
    {
        OutError = TEXT("schema 8 did not migrate legacy body/outfit into deterministic schema-10 defaults");
        return false;
    }
    const UDiscGolfSaveGame* Reloaded = Cast<UDiscGolfSaveGame>(
        UGameplayStatics::LoadGameFromSlot(ValidationSaveSlot, 0));
    bSchema8MigrationReloaded = Reloaded
        && Reloaded->SaveSchemaVersion == DiscGolfSaveSchema::CurrentVersion
        && DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Reloaded->CharacterCustomization,
            Migrated->CharacterCustomization);
    return bSchema8MigrationReloaded;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    ProveMissingFallbacks(FString& OutError)
{
    FDGFullCharacterCustomization Missing =
        Golfer->GetCurrentFullCharacterCustomization();
    Missing.Hair.HairStyleId = TEXT("session7_missing_hair_visual_fixture");
    Missing.Hair.FacialHairId =
        TEXT("session7_missing_facialhair_visual_fixture");
    Missing.Hair.EyebrowId = TEXT("session7_missing_eyebrow_visual_fixture");
    Missing.Appearance.ScarId = TEXT("session7_missing_scar_visual_fixture");
    Missing.Appearance.TattooIds = {
        TEXT("session7_missing_tattoo_visual_fixture") };
    FDGEquippedOutfitEntry MissingOutfit;
    MissingOutfit.Slot = EDGOutfitSlot::Accessory;
    MissingOutfit.ItemId = TEXT("session7_missing_outfit_visual_fixture");
    MissingOutfit.VariantId = TEXT("Default");
    Missing.Outfit.Equipped.Add(MissingOutfit);
    const FDiscGolfFullCustomizationResolution Resolution =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            Missing, Golfer->GetCosmeticCatalog(), Golfer->GetOutfitCatalog());
    const bool bResolved = !Resolution.bAllCosmeticsResolved
        && Resolution.Character.Hair.HairStyleId == FName(TEXT("hair_none"))
        && Resolution.Character.Hair.FacialHairId
            == FName(TEXT("facialhair_none"))
        && Resolution.Character.Hair.EyebrowId == FName(TEXT("brow_default"))
        && Resolution.Character.Appearance.ScarId == FName(TEXT("scar_none"))
        && Resolution.Character.Appearance.TattooIds.IsEmpty()
        && !DiscGolfOutfitRuntime::FindEntryForSlot(
            Resolution.Character.Outfit, EDGOutfitSlot::Accessory);
    if (!bResolved)
    {
        OutError = TEXT("missing hair/facial hair/brow/scar/tattoo/outfit did not fall back safely");
    }
    bMissingCosmeticFallbacksResolved = bResolved;
    return bResolved;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::PrepareCapture(
    int32 Index)
{
    FString Error;
    if (Index >= 1 && Index <= 7)
    {
        if (!PrepareCreatorDraft(Index - 1, EvidenceDraft, Error))
        {
            Fail(Error);
            return false;
        }
    }
    else
    {
        switch (Index)
        {
            case 8:
                DiscGolfFullCharacterRuntime::ApplyFacePreset(
                    TEXT("face_default"), EvidenceDraft.Face);
                if (PlayerController->PlayerCameraManager)
                {
                    PlayerController->PlayerCameraManager->SetFOV(
                        Session7VisualHeadCloseupFovDeg);
                }
                if (!PrepareCreatorDraft(2, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 9:
                DiscGolfFullCharacterRuntime::ApplyFacePreset(
                    TEXT("face_square"), EvidenceDraft.Face);
                if (!PrepareCreatorDraft(2, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 10:
                DiscGolfFullCharacterRuntime::ApplyFacePreset(
                    TEXT("face_narrow"), EvidenceDraft.Face);
                if (!PrepareCreatorDraft(2, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 11:
                DiscGolfFullCharacterRuntime::ApplyFacePreset(
                    TEXT("face_round"), EvidenceDraft.Face);
                if (!PrepareCreatorDraft(2, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 12:
                DiscGolfFullCharacterRuntime::ApplyFacePreset(
                    TEXT("face_default"), EvidenceDraft.Face);
                EvidenceDraft.Hair.HairStyleId = TEXT("hair_short");
                EvidenceDraft.Hair.FacialHairId = TEXT("facialhair_none");
                if (!PrepareCreatorDraft(3, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 13:
                EvidenceDraft.Hair.HairStyleId = TEXT("hair_medium");
                if (!PrepareCreatorDraft(3, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 14:
                EvidenceDraft.Hair.FacialHairId = TEXT("facialhair_beard");
                EvidenceDraft.Hair.EyebrowId = TEXT("brow_alt");
                if (!PrepareCreatorDraft(3, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 15:
                EvidenceDraft.Appearance.SkinTone =
                    FLinearColor(0.16f, 0.08f, 0.05f, 1.0f);
                EvidenceDraft.Appearance.Complexion = 0.72f;
                EvidenceDraft.Appearance.Freckles = 0.68f;
                EvidenceDraft.Appearance.SunExposure = 0.58f;
                EvidenceDraft.Appearance.ScarId = TEXT("scar_proxy");
                EvidenceDraft.Appearance.TattooIds = {TEXT("tattoo_proxy")};
                if (!PrepareCreatorDraft(4, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 16:
                EvidenceDraft.Appearance.EyeColor =
                    FLinearColor(0.08f, 0.18f, 0.26f, 1.0f);
                if (!PrepareCreatorDraft(4, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 17:
                EvidenceDraft.Hair.HairStyleId = TEXT("hair_medium");
                if (!SetCap(EvidenceDraft, Error)
                    || !PrepareCreatorDraft(6, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 18:
                EvidenceDraft.Outfit.Equipped.RemoveAll(
                    [](const FDGEquippedOutfitEntry& Entry)
                    {
                        return Entry.Slot == EDGOutfitSlot::Headwear;
                    });
                if (!PrepareCreatorDraft(6, EvidenceDraft, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 19:
                PlayerController->CancelCharacterCreator();
                if (PlayerController->IsCharacterCreatorOpen())
                {
                    Fail(TEXT("creator did not close before complete-character frames"));
                    return false;
                }
                bPreviewCameraActive = true;
                Golfer->BeginCharacterCreatorPreview();
                if (PlayerController->PlayerCameraManager)
                {
                    PlayerController->PlayerCameraManager->SetFOV(
                        Session7VisualFullBodyFovDeg);
                }
                if (!ApplyCompleteProfile(
                        TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact"),
                        TEXT("ShortCompact"), 0, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 20:
                if (!ApplyCompleteProfile(
                        TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"),
                        TEXT("Baseline"), 1, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 21:
                if (!ApplyCompleteProfile(
                        TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms"),
                        TEXT("TallLongArms"), 2, Error))
                {
                    Fail(Error);
                    return false;
                }
                break;
            case 22:
                if (bVisualThrowStarted)
                {
                    return true;
                }
                if (!ApplyCompleteProfile(
                        TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"),
                        TEXT("Baseline"), 1, Error)
                    || !BeginVisualThrow(Error))
                {
                    Fail(Error);
                    return false;
                }
                bVisualThrowStarted = true;
                return true;
            case 23:
            {
                UDiscGolfThrowComponent* Throw =
                    Golfer->FindComponentByClass<UDiscGolfThrowComponent>();
                const float Position = GetActiveMontagePosition();
                if (!Throw || Throw->CurrentPhase != EDGThrowPhase::FollowThrough
                    || Position < 1.95f)
                {
                    return true;
                }
                if (Position > 2.12f || !PauseWorldForThrowCapture(Error))
                {
                    Fail(Position > 2.12f
                        ? TEXT("follow-through capture missed the strict 1.95-2.12 second window")
                        : Error);
                    return false;
                }
                break;
            }
            default:
                break;
        }
    }

    if (Index == 22)
    {
        return true;
    }
    RequestCapture(Index);
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::PrepareCreatorDraft(
    int32 TabIndex,
    const FDGFullCharacterCustomization& Draft,
    FString& OutError)
{
    if (!PlayerController || !PlayerController->IsCharacterCreatorOpen()
        || !PlayerController->PrepareCharacterCreatorForSession7VisualEvidence(
            TabIndex, Draft)
        || PlayerController->GetCharacterCreatorActiveTabIndex()
            != FMath::Clamp(TabIndex, 0, 6))
    {
        OutError = FString::Printf(
            TEXT("creator could not prepare tab %d with the complete draft"),
            TabIndex);
        return false;
    }
    const FDGFullCharacterCustomization& Actual =
        PlayerController->GetCharacterCreatorDraftCustomization();
    if (!DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Actual, Draft))
    {
        OutError = TEXT("tab preparation changed the full-character draft");
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::ApplyCompleteProfile(
    const TCHAR* ProfileAssetPath,
    FName ProfileId,
    int32 OutfitVariantIndex,
    FString& OutError)
{
    const UDiscGolfCharacterProfile* Profile =
        LoadObject<UDiscGolfCharacterProfile>(nullptr, ProfileAssetPath);
    if (!Profile)
    {
        OutError = FString::Printf(
            TEXT("complete profile asset is unavailable: %s"),
            ProfileAssetPath);
        return false;
    }
    FDGFullCharacterCustomization Complete =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    Complete.Identity.DisplayName = FString::Printf(
        TEXT("Session 7 %s"), *ProfileId.ToString());
    Complete.Identity.Handedness = Profile->Handedness;
    Complete.Identity.VoiceId = TEXT("voice_alt");
    Complete.Identity.PronounSetId = TEXT("pronouns_they_them");
    Complete.Body = Profile->Body;
    Complete.ThrowStyle = Profile->ThrowStyle;
    Complete.BodyBuild.Muscularity = 0.62f;
    Complete.BodyBuild.BodyFat = 0.28f;
    Complete.BodyBuild.Chest = 0.25f;
    Complete.BodyBuild.Waist = -0.15f;
    Complete.BodyBuild.Hips = 0.10f;
    Complete.BodyBuild.Arms = 0.35f;
    Complete.BodyBuild.Legs = 0.20f;
    DiscGolfFullCharacterRuntime::ApplyFacePreset(
        TEXT("face_square"), Complete.Face);
    Complete.Hair.HairStyleId = TEXT("hair_medium");
    Complete.Hair.FacialHairId = TEXT("facialhair_beard");
    Complete.Hair.EyebrowId = TEXT("brow_alt");
    Complete.Hair.HairColor = FLinearColor(0.13f, 0.07f, 0.035f, 1.0f);
    Complete.Hair.FacialHairColor = Complete.Hair.HairColor;
    Complete.Hair.EyebrowColor = Complete.Hair.HairColor;
    Complete.Appearance.SkinTone = FLinearColor(0.55f, 0.35f, 0.24f, 1.0f);
    Complete.Appearance.EyeColor = FLinearColor(0.15f, 0.24f, 0.20f, 1.0f);
    Complete.Appearance.Complexion = 0.55f;
    Complete.Appearance.Freckles = 0.42f;
    Complete.Appearance.SunExposure = 0.48f;
    Complete.Appearance.ScarId = TEXT("scar_proxy");
    Complete.Appearance.TattooIds = {TEXT("tattoo_proxy")};
    if (!BuildFullOutfit(
            Complete.Body, false, OutfitVariantIndex,
            Complete.Outfit, OutError))
    {
        return false;
    }
    FString Status;
    if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
            Complete, false, Status))
    {
        OutError = FString::Printf(
            TEXT("complete profile application failed: %s"), *Status);
        return false;
    }
    ActiveProfileId = ProfileId.ToString();
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::BuildFullOutfit(
    const FDGBodyProfile& Body,
    bool bIncludeHeadwear,
    int32 VariantIndex,
    FDGOutfitLoadout& OutLoadout,
    FString& OutError) const
{
    OutLoadout.Equipped.Reset();
    for (EDGOutfitSlot Slot : DiscGolfOutfitRuntime::GetOrderedSlots())
    {
        if (!bIncludeHeadwear && Slot == EDGOutfitSlot::Headwear)
        {
            continue;
        }
        if (!SetFirstOutfitItem(
                Slot, Body, VariantIndex, OutLoadout, OutError))
        {
            return false;
        }
    }
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::SetFirstOutfitItem(
    EDGOutfitSlot Slot,
    const FDGBodyProfile& Body,
    int32 VariantIndex,
    FDGOutfitLoadout& InOutLoadout,
    FString& OutError) const
{
    const TArray<FDiscGolfOutfitOption> Options =
        DiscGolfOutfitRuntime::GetOptionsForSlot(
            Golfer->GetOutfitCatalog(), Slot, Body);
    const FDiscGolfOutfitOption* Selected = Options.FindByPredicate(
        [](const FDiscGolfOutfitOption& Option)
        {
            return Option.bCompatible && !Option.VariantIds.IsEmpty();
        });
    if (!Selected)
    {
        OutError = FString::Printf(
            TEXT("complete visual outfit slot %s has no compatible proxy"),
            *DiscGolfOutfitRuntime::GetSlotDisplayName(Slot).ToString());
        return false;
    }
    return DiscGolfOutfitRuntime::SetSlotSelection(
        InOutLoadout, Slot, Selected->ItemId,
        Selected->VariantIds[
            FMath::Clamp(VariantIndex, 0, Selected->VariantIds.Num() - 1)],
        Golfer->GetOutfitCatalog(), Body, OutError);
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::SetCap(
    FDGFullCharacterCustomization& InOutDraft,
    FString& OutError) const
{
    InOutDraft.Outfit.Equipped.RemoveAll(
        [](const FDGEquippedOutfitEntry& Entry)
        {
            return Entry.Slot == EDGOutfitSlot::Headwear;
        });
    return DiscGolfOutfitRuntime::SetSlotSelection(
        InOutDraft.Outfit, EDGOutfitSlot::Headwear,
        TEXT("proxy_s6_headwear_cap_01"), TEXT("Default"),
        Golfer->GetOutfitCatalog(), InOutDraft.Body, OutError);
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::BeginVisualThrow(
    FString& OutError)
{
    if (!ThrowAdapter || ThrowAdapter->IsThrowActive())
    {
        OutError = TEXT("accepted RHBH adapter was unavailable or already active");
        return false;
    }
    ThrowAdapter->GetAuthoritativeLaunchDelegate().Unbind();
    ThrowAdapter->GetAuthoritativeLaunchDelegate().BindUObject(
        this,
        &ADiscGolfSession7FullCharacterVisualCaptureRunner::
            HandleValidationLaunch);
    FThrowCommand Command;
    Command.ThrowStyle = EThrowStyle::Backhand;
    Command.ShotContext = EDiscShotContext::Drive;
    Command.Direction = Golfer->GetActorForwardVector();
    Command.Power01 = 0.82f;
    Command.HyzerDeg = 3.0f;
    Command.NoseAngleDeg = 1.0f;
    Command.LaunchAngleDeg = 7.0f;
    if (!BindSession7SelectedThrowProvenance(Golfer, Command)
        || !Golfer->TryStartAnimatedRHBHThrow(Command))
    {
        OutError = TEXT("accepted RHBH montage rejected the complete character");
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    HandleValidationLaunch(
        const FThrowCommand& Command,
        const FTransform& GripWorldTransform)
{
    (void)Command;
    if (ValidationReleaseCallbacks != 0
        || GripWorldTransform.ContainsNaN())
    {
        return false;
    }
    ++ValidationReleaseCallbacks;
    RequestCapture(22);
    if (bFinished)
    {
        return false;
    }
    FString Error;
    if (!PauseWorldForThrowCapture(Error))
    {
        Fail(Error);
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    PauseWorldForThrowCapture(FString& OutError)
{
    if (bCapturePausedWorld || UGameplayStatics::IsGamePaused(this)
        || !Golfer || !Golfer->GetSkeletalGolferMesh())
    {
        OutError = TEXT("throw frame could not acquire an unpaused validation world");
        return false;
    }
    UAnimInstance* Anim =
        Golfer->GetSkeletalGolferMesh()->GetAnimInstance();
    UAnimMontage* Montage = Anim
        ? Anim->GetCurrentActiveMontage() : nullptr;
    if (!Anim || !Montage)
    {
        OutError = TEXT("throw frame could not find the active montage");
        return false;
    }
    Anim->Montage_Pause(Montage);
    if (!UGameplayStatics::SetGamePaused(this, true))
    {
        Anim->Montage_Resume(Montage);
        OutError = TEXT("throw frame could not pause its watchdog clock");
        return false;
    }
    bCapturePausedWorld = true;
    ++PausedThrowCaptureCount;
    return true;
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::
    ResumeWorldAfterThrowCapture(bool bResumeMontage)
{
    if (bCapturePausedWorld)
    {
        UGameplayStatics::SetGamePaused(this, false);
        bCapturePausedWorld = false;
    }
    if (bResumeMontage && Golfer && Golfer->GetSkeletalGolferMesh())
    {
        if (UAnimInstance* Anim =
                Golfer->GetSkeletalGolferMesh()->GetAnimInstance())
        {
            Anim->Montage_Resume(Anim->GetCurrentActiveMontage());
        }
    }
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::RequestCapture(
    int32 Index)
{
    if (bCapturePending || Index != Captures.Num()
        || Index < 0 || Index >= UE_ARRAY_COUNT(Session7VisualFilenames))
    {
        Fail(TEXT("visual screenshot request was overlapping or out of order"));
        return;
    }
    FCapture Record;
    Record.Filename = Session7VisualFilenames[Index];
    Record.Evidence = Session7VisualEvidenceLabels[Index];
    Record.bHeadCloseup = Index >= 8 && Index <= 18;
    Captures.Add(Record);
    PendingCaptureIndex = Index;
    PendingCapturePath = FPaths::Combine(
        OutputDirectory, Record.Filename);
    IFileManager::Get().Delete(
        *PendingCapturePath, false, true, true);
    bCapturePending = true;
    bCaptureIssued = false;
    PendingStartedSeconds = FPlatformTime::Seconds();
    if (Index >= 8 && GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
        GEngine->AddOnScreenDebugMessage(
            910700, 120.0f, FColor::Cyan,
            FString::Printf(
                TEXT("SESSION 7 FULL CHARACTER EVIDENCE %d/24"),
                Index + 1),
            true, FVector2D(1.20f, 1.20f));
        GEngine->AddOnScreenDebugMessage(
            910701, 120.0f, FColor::White,
            Session7VisualEvidenceLabels[Index], true,
            FVector2D(0.95f, 0.95f));
    }
    else if (GEngine)
    {
        // The native creator supplies the UI identity for frames 1-8.
        GEngine->ClearOnScreenDebugMessages();
    }
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::PollCapture()
{
    if (!bCaptureIssued)
    {
        if (PendingCaptureIndex >= 0 && PendingCaptureIndex <= 7 && GEngine)
        {
            // Startup shader/static-mesh notices are not creator evidence.
            // Clear them throughout a short first-frame settle window so the
            // native Slate header and tab controls are unobstructed.
            GEngine->ClearOnScreenDebugMessages();
        }
        if (PendingCaptureIndex == 0
            && FPlatformTime::Seconds() - PendingStartedSeconds
                < Session7VisualInitialUiSettleSeconds)
        {
            return false;
        }
        FString Error;
        if (!Captures.IsValidIndex(PendingCaptureIndex)
            || !ValidateCaptureScene(
                PendingCaptureIndex,
                Captures[PendingCaptureIndex], Error))
        {
            Fail(Error.IsEmpty()
                ? TEXT("visual capture scene validation failed") : Error);
            return false;
        }
        bCaptureIssued = true;
        PendingStartedSeconds = FPlatformTime::Seconds();
        FScreenshotRequest::RequestScreenshot(
            PendingCapturePath, true, false, false,
            FIntRect(), true);
        return false;
    }
    const int64 Size = IFileManager::Get().FileSize(*PendingCapturePath);
    if (Size >= Session7VisualMinimumPngBytes)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *PendingCapturePath)
            || Bytes.Num() < 24 || Bytes[0] != 0x89 || Bytes[1] != 0x50)
        {
            Fail(TEXT("visual evidence was not a readable PNG"));
            return false;
        }
        FCapture& Capture = Captures[PendingCaptureIndex];
        Capture.Bytes = Size;
        Capture.Width = Session7VisualBigEndianInt32(&Bytes[16]);
        Capture.Height = Session7VisualBigEndianInt32(&Bytes[20]);
        FSHA1 Sha;
        Sha.Update(Bytes.GetData(), Bytes.Num());
        Sha.Final();
        uint8 Digest[FSHA1::DigestSize];
        Sha.GetHash(Digest);
        Capture.Sha1 = BytesToHex(Digest, FSHA1::DigestSize);
        if (Capture.Width != Session7VisualExpectedWidth
            || Capture.Height != Session7VisualExpectedHeight)
        {
            Fail(FString::Printf(
                TEXT("capture was %dx%d instead of 1920x1080"),
                Capture.Width, Capture.Height));
            return false;
        }
        const int32 CompletedIndex = PendingCaptureIndex;
        bCapturePending = false;
        bCaptureIssued = false;
        PendingCaptureIndex = INDEX_NONE;
        PendingCapturePath.Reset();
        ResumeWorldAfterThrowCapture(CompletedIndex == 22);
        return true;
    }
    if (FPlatformTime::Seconds() - PendingStartedSeconds
        > Session7VisualScreenshotTimeoutSeconds)
    {
        Fail(TEXT("visual screenshot timed out"));
    }
    return false;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::ValidateCaptureScene(
    int32 Index,
    FCapture& InOutCapture,
    FString& OutError) const
{
    if (!Golfer || !PlayerController || !GameMode
        || !Golfer->GetSkeletalGolferMesh()
        || !Golfer->GetModularHeadMesh()
        || !PlayerController->PlayerCameraManager)
    {
        OutError = TEXT("capture scene lost the player, controller, game mode, body or modular head");
        return false;
    }
    USkeletalMeshComponent* Body = Golfer->GetSkeletalGolferMesh();
    USkeletalMeshComponent* Head = Golfer->GetModularHeadMesh();
    const FIntPoint Viewport = GEngine && GEngine->GameViewport
            && GEngine->GameViewport->Viewport
        ? GEngine->GameViewport->Viewport->GetSizeXY()
        : FIntPoint::ZeroValue;
    if (Viewport.X != Session7VisualExpectedWidth
        || Viewport.Y != Session7VisualExpectedHeight)
    {
        OutError = TEXT("runtime viewport is not the exact 1920x1080 evidence contract");
        return false;
    }

    const FDGFullCharacterCustomization Current =
        Golfer->GetCurrentFullCharacterCustomization();
    InOutCapture.ProfileId = ActiveProfileId;
    InOutCapture.BodyHeightCm = Current.Body.HeightCm;
    InOutCapture.HairStyleId = Current.Hair.HairStyleId.ToString();
    InOutCapture.HeadwearId = GetHeadwearId();
    InOutCapture.HairColor = Current.Hair.HairColor;
    InOutCapture.SkinTone = Current.Appearance.SkinTone;
    InOutCapture.EyeColor = Current.Appearance.EyeColor;
    InOutCapture.bHairHiddenByOutfitCoverage =
        Golfer->IsHairHiddenByOutfitCoverage();
    InOutCapture.bHairComponentVisible =
        HasVisibleCustomizationComponent(Current.Hair.HairStyleId);
    InOutCapture.EquippedOutfitCount = Current.Outfit.Equipped.Num();
    InOutCapture.bVisibleProxyMorphsApplied =
        UE_ARRAY_COUNT(Session7VisualVisibleMorphKeys)
            == UE_ARRAY_COUNT(Session7VisualVisibleMorphTargets)
        && UE_ARRAY_COUNT(Session7VisualVisibleMorphKeys) == 5;
    for (int32 MorphIndex = 0;
        MorphIndex < UE_ARRAY_COUNT(Session7VisualVisibleMorphKeys);
        ++MorphIndex)
    {
        const FName Key(Session7VisualVisibleMorphKeys[MorphIndex]);
        const FName Target(Session7VisualVisibleMorphTargets[MorphIndex]);
        const float ExpectedValue = Current.Face.MorphValues.FindRef(Key);
        InOutCapture.VisibleProxyMorphValues.Add(
            Key, ExpectedValue);
        InOutCapture.bVisibleProxyMorphsApplied &=
            Head->GetSkeletalMeshAsset()
            && Head->GetSkeletalMeshAsset()->FindMorphTarget(Target)
            && FMath::IsNearlyEqual(
                Head->GetMorphTarget(Target), ExpectedValue, 0.001f);
    }

    InOutCapture.bCreatorOpen = PlayerController->IsCharacterCreatorOpen();
    InOutCapture.ActiveTabIndex = InOutCapture.bCreatorOpen
        ? PlayerController->GetCharacterCreatorActiveTabIndex() : INDEX_NONE;
    InOutCapture.ActiveTab = InOutCapture.ActiveTabIndex >= 0
            && InOutCapture.ActiveTabIndex
                < UE_ARRAY_COUNT(Session7VisualTabLabels)
        ? Session7VisualTabLabels[InOutCapture.ActiveTabIndex]
        : TEXT("Closed");
    PlayerController->GetSession7CharacterCreatorVisibleControlIds(
        InOutCapture.VisibleControlIds);
    TArray<FString> ExpectedControls;
    if (InOutCapture.bCreatorOpen)
    {
        GetExpectedControlIds(InOutCapture.ActiveTabIndex, ExpectedControls);
    }
    InOutCapture.MinimumVisibleControlCount = ExpectedControls.Num();
    InOutCapture.bExpectedControlsVisible =
        InOutCapture.VisibleControlIds == ExpectedControls;
    InOutCapture.bViewTargetIsGolfer =
        PlayerController->GetViewTarget() == Golfer;

    const FName RequiredBones[] = {
        TEXT("head"), TEXT("pelvis"), TEXT("hand_l"), TEXT("hand_r"),
        TEXT("foot_l"), TEXT("foot_r"),
    };
    int32 Projected = 0;
    int32 Inside = 0;
    FVector2D Min(FLT_MAX, FLT_MAX);
    FVector2D Max(-FLT_MAX, -FLT_MAX);
    FVector2D HeadScreen = FVector2D::ZeroVector;
    for (int32 BoneIndex = 0;
        BoneIndex < UE_ARRAY_COUNT(RequiredBones); ++BoneIndex)
    {
        const int32 MeshBoneIndex = Body->GetBoneIndex(RequiredBones[BoneIndex]);
        if (MeshBoneIndex == INDEX_NONE)
        {
            continue;
        }
        const FVector Location = Body->GetBoneLocation(RequiredBones[BoneIndex]);
        FVector2D Screen = FVector2D::ZeroVector;
        const bool bProjected = !Location.ContainsNaN()
            && PlayerController->ProjectWorldLocationToScreen(
                Location, Screen, true);
        Projected += bProjected ? 1 : 0;
        if (!bProjected)
        {
            continue;
        }
        if (BoneIndex == 0)
        {
            HeadScreen = Screen;
        }
        const bool bInside = Screen.X >= 0.0f && Screen.X < Viewport.X
            && Screen.Y >= 0.0f && Screen.Y < Viewport.Y;
        Inside += bInside ? 1 : 0;
        Min.X = FMath::Min(Min.X, Screen.X);
        Min.Y = FMath::Min(Min.Y, Screen.Y);
        Max.X = FMath::Max(Max.X, Screen.X);
        Max.Y = FMath::Max(Max.Y, Screen.Y);
    }
    InOutCapture.bFinitePose = Projected == UE_ARRAY_COUNT(RequiredBones)
        && !HeadScreen.ContainsNaN();
    InOutCapture.bBodyFramed = Projected == 6 && Inside == 6
        && Min.X >= 10.0f && Max.X <= Viewport.X - 10.0f
        && Min.Y >= 10.0f && Max.Y <= Viewport.Y - 10.0f;
    InOutCapture.bHeadFramed = Projected > 0
        && HeadScreen.X >= 0.50f * Viewport.X
        && HeadScreen.X <= 0.96f * Viewport.X
        && HeadScreen.Y >= 0.08f * Viewport.Y
        && HeadScreen.Y <= 0.72f * Viewport.Y;
    const int32 RoiHalfWidth = 180;
    const int32 RoiAbove = 190;
    const int32 RoiBelow = 210;
    InOutCapture.HeadRoi = FIntRect(
        FMath::Clamp(FMath::RoundToInt(HeadScreen.X) - RoiHalfWidth, 0, Viewport.X),
        FMath::Clamp(FMath::RoundToInt(HeadScreen.Y) - RoiAbove, 0, Viewport.Y),
        FMath::Clamp(FMath::RoundToInt(HeadScreen.X) + RoiHalfWidth, 0, Viewport.X),
        FMath::Clamp(FMath::RoundToInt(HeadScreen.Y) + RoiBelow, 0, Viewport.Y));
    InOutCapture.bPreviewInRightPane = InOutCapture.bCreatorOpen
        && InOutCapture.bViewTargetIsGolfer
        && HeadScreen.X >= 0.50f * Viewport.X
        && HeadScreen.X <= 0.96f * Viewport.X;

    TArray<UDiscGolfCharacterCustomizationComponent*> CustomizationComponents;
    Golfer->GetComponents(CustomizationComponents);
    InOutCapture.bOneCustomizationComponent =
        CustomizationComponents.Num() == 1;
    TArray<UDiscGolfOutfitComponent*> OutfitComponents;
    Golfer->GetComponents(OutfitComponents);
    InOutCapture.bOneOutfitComponent = OutfitComponents.Num() == 1;
    InOutCapture.bHeadLeaderPose = Head->LeaderPoseComponent.Get() == Body
        && Body->GetSkeletalMeshAsset() && Head->GetSkeletalMeshAsset()
        && Body->GetSkeletalMeshAsset()->GetSkeleton()
            == Head->GetSkeletalMeshAsset()->GetSkeleton();
    FString CosmeticError;
    const bool bCosmeticsValid = ValidateCosmeticComponents(
        InOutCapture.bCosmeticsAttached,
        InOutCapture.bCosmeticsCollisionFree,
        InOutCapture.bPresentationMaterialsValid,
        CosmeticError);
    InOutCapture.bOutfitAttachmentsAndMaterialsValid = bCosmeticsValid
        && InOutCapture.bPresentationMaterialsValid;
    InOutCapture.ValidatedPresentationComponentCount =
        InOutCapture.EquippedOutfitCount
        + (InOutCapture.bHairComponentVisible ? 1 : 0)
        + (Current.Hair.FacialHairId == FName(TEXT("facialhair_none"))
            ? 0 : 1)
        + 1;

    InOutCapture.bHeadMaterialIsCanonicalMid = Head->GetNumMaterials() > 0;
    InOutCapture.bHeadMaterialHasMorphTargetUsage =
        Head->GetNumMaterials() > 0;
    for (int32 MaterialIndex = 0;
        MaterialIndex < Head->GetNumMaterials(); ++MaterialIndex)
    {
        UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(
            Head->GetMaterial(MaterialIndex));
        const UMaterial* Base = Mid ? Mid->GetBaseMaterial() : nullptr;
        InOutCapture.bHeadMaterialHasMorphTargetUsage &= Base
            && Base->GetUsageByFlag(MATUSAGE_MorphTargets);
        bool bParametersMatch = Base
            && Base->GetPathName()
                == Session7VisualHeadMaterialObjectPath;
        FLinearColor Skin = FLinearColor::Transparent;
        FLinearColor Eyes = FLinearColor::Transparent;
        float Complexion = -1.0f;
        float Freckles = -1.0f;
        float Sun = -1.0f;
        float Scar = -1.0f;
        float Tattoo = -1.0f;
        bParametersMatch &= Mid
            && Mid->GetVectorParameterValue(
                FHashedMaterialParameterInfo(TEXT("DG_SkinTone")),
                Skin, true)
            && Mid->GetVectorParameterValue(
                FHashedMaterialParameterInfo(TEXT("DG_EyeColor")),
                Eyes, true)
            && Mid->GetScalarParameterValue(
                FHashedMaterialParameterInfo(TEXT("DG_Complexion")),
                Complexion, true)
            && Mid->GetScalarParameterValue(
                FHashedMaterialParameterInfo(TEXT("DG_Freckles")),
                Freckles, true)
            && Mid->GetScalarParameterValue(
                FHashedMaterialParameterInfo(TEXT("DG_SunExposure")),
                Sun, true)
            && Mid->GetScalarParameterValue(
                FHashedMaterialParameterInfo(TEXT("DG_ScarProxy")),
                Scar, true)
            && Mid->GetScalarParameterValue(
                FHashedMaterialParameterInfo(TEXT("DG_TattooProxy")),
                Tattoo, true)
            && Skin.Equals(Current.Appearance.SkinTone, 0.0001f)
            && Eyes.Equals(Current.Appearance.EyeColor, 0.0001f)
            && FMath::IsNearlyEqual(
                Complexion, Current.Appearance.Complexion, 0.0001f)
            && FMath::IsNearlyEqual(
                Freckles, Current.Appearance.Freckles, 0.0001f)
            && FMath::IsNearlyEqual(
                Sun, Current.Appearance.SunExposure, 0.0001f);
        bParametersMatch &= FMath::IsNearlyEqual(
                Scar,
                Current.Appearance.ScarId == FName(TEXT("scar_none"))
                    ? 0.0f : 1.0f,
                0.0001f)
            && FMath::IsNearlyEqual(
                Tattoo,
                Current.Appearance.TattooIds.IsEmpty() ? 0.0f : 1.0f,
                0.0001f);
        InOutCapture.bHeadMaterialIsCanonicalMid &= bParametersMatch;
    }
    InOutCapture.CameraFovDeg =
        PlayerController->PlayerCameraManager->GetFOVAngle();
    InOutCapture.bCameraFovValid = FMath::IsNearlyEqual(
        InOutCapture.CameraFovDeg,
        InOutCapture.bHeadCloseup
            ? Session7VisualHeadCloseupFovDeg
            : Session7VisualFullBodyFovDeg,
        0.1f);
    InOutCapture.ThrowPhase = GetThrowPhaseLabel();
    InOutCapture.MontagePositionSeconds = GetActiveMontagePosition();
    InOutCapture.WorldDiscDelta = CountWorldDiscs() - BaselineDiscs;
    InOutCapture.StrokeDelta = GameMode->GetStrokes() - BaselineStrokes;

    const bool bExpectedPhase = Index == 22
        ? InOutCapture.ThrowPhase == TEXT("Release")
            && InOutCapture.MontagePositionSeconds >= 1.52f
            && InOutCapture.MontagePositionSeconds <= 1.68f
        : Index == 23
            ? InOutCapture.ThrowPhase == TEXT("FollowThrough")
                && InOutCapture.MontagePositionSeconds >= 1.95f
                && InOutCapture.MontagePositionSeconds <= 2.12f
            : InOutCapture.ThrowPhase == TEXT("Idle");
    const bool bCreatorFrame = Index <= 18;
    const bool bFramingValid = InOutCapture.bHeadCloseup
        ? InOutCapture.bHeadFramed
            && InOutCapture.HeadRoi.Width() >= 32
            && InOutCapture.HeadRoi.Height() >= 32
        : InOutCapture.bBodyFramed;
    const bool bHatFrameValid = Index != 17
        || (InOutCapture.HairStyleId == TEXT("hair_medium")
            && InOutCapture.HeadwearId
                == TEXT("proxy_s6_headwear_cap_01")
            && InOutCapture.bHairHiddenByOutfitCoverage
            && !InOutCapture.bHairComponentVisible);
    const bool bRestoreFrameValid = Index != 18
        || (InOutCapture.HairStyleId == TEXT("hair_medium")
            && InOutCapture.HeadwearId.IsEmpty()
            && !InOutCapture.bHairHiddenByOutfitCoverage
            && InOutCapture.bHairComponentVisible);
    if (!InOutCapture.bFinitePose || !bFramingValid
        || !InOutCapture.bCameraFovValid || !bCosmeticsValid
        || !InOutCapture.bOneCustomizationComponent
        || !InOutCapture.bOneOutfitComponent
        || !InOutCapture.bHeadLeaderPose
        || !InOutCapture.bVisibleProxyMorphsApplied
        || !InOutCapture.bHeadMaterialIsCanonicalMid
        || !InOutCapture.bHeadMaterialHasMorphTargetUsage
        || !bExpectedPhase || InOutCapture.WorldDiscDelta != 0
        || InOutCapture.StrokeDelta != 0 || !bHatFrameValid
        || !bRestoreFrameValid
        || (bCreatorFrame && (!InOutCapture.bCreatorOpen
            || !InOutCapture.bViewTargetIsGolfer
            || !InOutCapture.bPreviewInRightPane
            || !InOutCapture.bExpectedControlsVisible)))
    {
        OutError = FString::Printf(
            TEXT("capture %d scene gate failed (body=%d head=%d finite=%d fov=%.1f/%d creator=%d tab=%d controls=%d right=%d custom=%d outfit=%d leader=%d visible_morphs=%d cosmetic=%d/%d materials=%d head_mid=%d morph_usage=%d hair=%s headwear=%s hidden=%d hair_visible=%d phase=%s montage=%.3f discs=%d strokes=%d detail=%s)"),
            Index + 1, InOutCapture.bBodyFramed ? 1 : 0,
            InOutCapture.bHeadFramed ? 1 : 0,
            InOutCapture.bFinitePose ? 1 : 0,
            InOutCapture.CameraFovDeg,
            InOutCapture.bCameraFovValid ? 1 : 0,
            InOutCapture.bCreatorOpen ? 1 : 0,
            InOutCapture.ActiveTabIndex,
            InOutCapture.bExpectedControlsVisible ? 1 : 0,
            InOutCapture.bPreviewInRightPane ? 1 : 0,
            InOutCapture.bOneCustomizationComponent ? 1 : 0,
            InOutCapture.bOneOutfitComponent ? 1 : 0,
            InOutCapture.bHeadLeaderPose ? 1 : 0,
            InOutCapture.bVisibleProxyMorphsApplied ? 1 : 0,
            InOutCapture.bCosmeticsAttached ? 1 : 0,
            InOutCapture.bCosmeticsCollisionFree ? 1 : 0,
            InOutCapture.bPresentationMaterialsValid ? 1 : 0,
            InOutCapture.bHeadMaterialIsCanonicalMid ? 1 : 0,
            InOutCapture.bHeadMaterialHasMorphTargetUsage ? 1 : 0,
            *InOutCapture.HairStyleId, *InOutCapture.HeadwearId,
            InOutCapture.bHairHiddenByOutfitCoverage ? 1 : 0,
            InOutCapture.bHairComponentVisible ? 1 : 0,
            *InOutCapture.ThrowPhase,
            InOutCapture.MontagePositionSeconds,
            InOutCapture.WorldDiscDelta, InOutCapture.StrokeDelta,
            *CosmeticError);
        return false;
    }
    return true;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    ValidateCosmeticComponents(
        bool& bOutAttached,
        bool& bOutCollisionFree,
        bool& bOutMaterialsValid,
        FString& OutError) const
{
    bOutAttached = true;
    bOutCollisionFree = true;
    bOutMaterialsValid = true;
    if (!Golfer || !Golfer->GetCosmeticCatalog()
        || !Golfer->GetOutfitCatalog()
        || !Golfer->GetModularHeadMesh()
        || !Golfer->GetSkeletalGolferMesh())
    {
        OutError = TEXT("presentation catalogs or body/head component disappeared");
        return false;
    }
    const FDGFullCharacterCustomization Current =
        Golfer->GetCurrentFullCharacterCustomization();
    TArray<UPrimitiveComponent*> Primitives;
    Golfer->GetComponents(Primitives);
    TSet<const UPrimitiveComponent*> Matched;

    const FName CosmeticIds[] = {
        Current.Hair.HairStyleId,
        Current.Hair.FacialHairId,
        Current.Hair.EyebrowId,
    };
    const FLinearColor CosmeticColors[] = {
        Current.Hair.HairColor,
        Current.Hair.FacialHairColor,
        Current.Hair.EyebrowColor,
    };
    static_assert(
        UE_ARRAY_COUNT(CosmeticIds) == UE_ARRAY_COUNT(CosmeticColors),
        "Every Session 7 hair-kind component must verify its own DG_HairColor");
    for (int32 CosmeticIndex = 0;
        CosmeticIndex < UE_ARRAY_COUNT(CosmeticIds); ++CosmeticIndex)
    {
        const FName ItemId = CosmeticIds[CosmeticIndex];
        const UDiscGolfCosmeticItem* Item =
            Golfer->GetCosmeticCatalog()->FindById(ItemId);
        const bool bNone = ItemId == FName(TEXT("hair_none"))
            || ItemId == FName(TEXT("facialhair_none"));
        const bool bCoverageHidden = CosmeticIndex == 0
            && Golfer->IsHairHiddenByOutfitCoverage();
        if (!Item)
        {
            OutError = FString::Printf(
                TEXT("selected catalog cosmetic %s is missing"),
                *ItemId.ToString());
            return false;
        }
        if (bNone || bCoverageHidden)
        {
            continue;
        }
        UPrimitiveComponent* Found = nullptr;
        for (UPrimitiveComponent* Component : Primitives)
        {
            if (Component && Component->IsRegistered()
                && Component->IsVisible()
                && Component->ComponentHasTag(
                    TEXT("DG_CustomizationCosmetic"))
                && Component->ComponentHasTag(ItemId)
                && !Matched.Contains(Component))
            {
                Found = Component;
                Matched.Add(Component);
                break;
            }
        }
        bOutAttached &= Found
            && Found->GetAttachParent() == Golfer->GetModularHeadMesh();
        bOutCollisionFree &= Found
            && Found->GetCollisionEnabled() == ECollisionEnabled::NoCollision
            && !Found->GetGenerateOverlapEvents()
            && !Found->CanEverAffectNavigation()
            && !Found->IsSimulatingPhysics();
        if (const USkeletalMeshComponent* Skeletal =
                Cast<USkeletalMeshComponent>(Found))
        {
            bOutAttached &= Skeletal->LeaderPoseComponent.Get()
                == Golfer->GetModularHeadMesh();
        }
        else if (const UStaticMeshComponent* Static =
                Cast<UStaticMeshComponent>(Found))
        {
            bOutAttached &= Static->IsUsingAbsoluteScale();
        }
        if (Found)
        {
            const UMeshComponent* MeshComponent =
                Cast<UMeshComponent>(Found);
            bOutAttached &= MeshComponent != nullptr
                && MeshComponent->GetNumMaterials() > 0;
            for (int32 MaterialIndex = 0;
                MeshComponent && MaterialIndex < MeshComponent->GetNumMaterials();
                ++MaterialIndex)
            {
                UMaterialInstanceDynamic* Mid =
                    Cast<UMaterialInstanceDynamic>(
                        MeshComponent->GetMaterial(MaterialIndex));
                const UMaterial* Base = Mid ? Mid->GetBaseMaterial() : nullptr;
                FLinearColor AppliedHairColor = FLinearColor::Transparent;
                bOutMaterialsValid &= Base
                    && Base->GetPathName()
                        == Session7VisualHairMaterialObjectPath
                    && Mid->GetVectorParameterValue(
                        FHashedMaterialParameterInfo(TEXT("DG_HairColor")),
                        AppliedHairColor, true)
                    && AppliedHairColor.Equals(
                        CosmeticColors[CosmeticIndex], 0.0001f);
            }
        }
    }

    bool bOutfitValid = true;
    for (const FDGEquippedOutfitEntry& Entry : Current.Outfit.Equipped)
    {
        const UDiscGolfOutfitItem* Item =
            Golfer->GetOutfitCatalog()->FindItemById(Entry.ItemId);
        UPrimitiveComponent* Found = nullptr;
        if (Item)
        {
            for (UPrimitiveComponent* Component : Primitives)
            {
                if (!Component || Matched.Contains(Component)
                    || !Component->IsRegistered() || !Component->IsVisible())
                {
                    continue;
                }
                const USkeletalMeshComponent* Skeletal =
                    Cast<USkeletalMeshComponent>(Component);
                const UStaticMeshComponent* Static =
                    Cast<UStaticMeshComponent>(Component);
                const bool bMeshMatches = Skeletal
                    ? !Item->SkeletalMesh.IsNull()
                        && Skeletal->GetSkeletalMeshAsset()
                            == Item->SkeletalMesh.LoadSynchronous()
                    : Static && !Item->StaticMesh.IsNull()
                        && Static->GetStaticMesh()
                            == Item->StaticMesh.LoadSynchronous();
                if (bMeshMatches)
                {
                    Found = Component;
                    Matched.Add(Component);
                    break;
                }
            }
        }
        bOutfitValid &= Item && Found;
        bOutCollisionFree &= Found
            && Found->GetCollisionEnabled() == ECollisionEnabled::NoCollision
            && !Found->GetGenerateOverlapEvents()
            && !Found->CanEverAffectNavigation()
            && !Found->IsSimulatingPhysics();
        if (const USkeletalMeshComponent* Skeletal =
                Cast<USkeletalMeshComponent>(Found))
        {
            bOutfitValid &= Skeletal->GetAttachParent()
                    == Golfer->GetSkeletalGolferMesh()
                && Skeletal->LeaderPoseComponent.Get()
                    == Golfer->GetSkeletalGolferMesh();
        }
        else if (const UStaticMeshComponent* Static =
                Cast<UStaticMeshComponent>(Found))
        {
            bOutfitValid &= Static->GetAttachParent()
                    == Golfer->GetSkeletalGolferMesh()
                && Static->GetAttachSocketName() == Item->AttachSocket
                && Static->IsUsingAbsoluteScale();
        }
        if (Found)
        {
            const UMeshComponent* MeshComponent =
                Cast<UMeshComponent>(Found);
            bOutfitValid &= MeshComponent != nullptr
                && MeshComponent->GetNumMaterials() > 0;
            for (int32 MaterialIndex = 0;
                MeshComponent && MaterialIndex < MeshComponent->GetNumMaterials();
                ++MaterialIndex)
            {
                UMaterialInstanceDynamic* Mid =
                    Cast<UMaterialInstanceDynamic>(
                        MeshComponent->GetMaterial(MaterialIndex));
                const UMaterial* Base = Mid ? Mid->GetBaseMaterial() : nullptr;
                bOutMaterialsValid &= Base
                    && Base->GetPathName()
                        == Session7VisualOutfitMaterialObjectPath;
            }
        }
    }
    bOutAttached &= bOutfitValid;
    if (!bOutAttached || !bOutCollisionFree || !bOutMaterialsValid)
    {
        OutError = TEXT("one or more hair/outfit proxy components failed catalog mesh, material, leader/socket, absolute-scale, or collision safety");
        return false;
    }
    return true;
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::GetExpectedControlIds(
    int32 TabIndex,
    TArray<FString>& OutIds) const
{
    OutIds.Reset();
    switch (FMath::Clamp(TabIndex, 0, 6))
    {
        case 0:
            OutIds = {TEXT("DisplayName"), TEXT("Handedness"),
                TEXT("Voice"), TEXT("Pronouns")};
            break;
        case 1:
            OutIds = {TEXT("Height"), TEXT("Wingspan"),
                TEXT("ShoulderWidth"), TEXT("TorsoLength"),
                TEXT("LegLength"), TEXT("HandScale"), TEXT("Mass"),
                TEXT("Muscularity"), TEXT("BodyFat"), TEXT("Chest"),
                TEXT("Waist"), TEXT("Hips"), TEXT("Arms"), TEXT("Legs"),
                TEXT("BodyPresets")};
            break;
        case 2:
            OutIds = {TEXT("FacePresets"), TEXT("HeadWidth"),
                TEXT("HeadHeight"), TEXT("BrowHeight"), TEXT("BrowDepth"),
                TEXT("EyeSize"), TEXT("EyeSpacing"), TEXT("EyeDepth"),
                TEXT("NoseWidth"), TEXT("NoseLength"), TEXT("NoseBridge"),
                TEXT("CheekWidth"), TEXT("CheekFullness"),
                TEXT("JawWidth"), TEXT("JawHeight"), TEXT("ChinWidth"),
                TEXT("ChinLength"), TEXT("MouthWidth"),
                TEXT("LipFullness"), TEXT("EarSize"), TEXT("EarAngle")};
            break;
        case 3:
            OutIds = {TEXT("HairStyle"), TEXT("FacialHair"),
                TEXT("Eyebrow"), TEXT("HairColor"),
                TEXT("FacialHairColor"), TEXT("EyebrowColor")};
            break;
        case 4:
            OutIds = {TEXT("SkinTone"), TEXT("EyeColor"),
                TEXT("Complexion"), TEXT("Freckles"),
                TEXT("SunExposure"), TEXT("Scar"), TEXT("Tattoo")};
            break;
        case 5:
            OutIds = {TEXT("RunUp"), TEXT("ReachBack"),
                TEXT("TorsoRotation"), TEXT("Brace"),
                TEXT("Explosiveness"), TEXT("FollowThrough")};
            break;
        case 6:
            OutIds = {TEXT("Headwear"), TEXT("Eyewear"), TEXT("Top"),
                TEXT("Outerwear"), TEXT("Bottom"), TEXT("Socks"),
                TEXT("Footwear"), TEXT("Glove"), TEXT("Wrist"),
                TEXT("Bag"), TEXT("Accessory"), TEXT("Item"),
                TEXT("Variant")};
            break;
        default:
            break;
    }
}

FString ADiscGolfSession7FullCharacterVisualCaptureRunner::
    GetThrowPhaseLabel() const
{
    const UDiscGolfThrowComponent* Throw = Golfer
        ? Golfer->FindComponentByClass<UDiscGolfThrowComponent>() : nullptr;
    if (!Throw)
    {
        return TEXT("Unavailable");
    }
    switch (Throw->CurrentPhase)
    {
        case EDGThrowPhase::Idle: return TEXT("Idle");
        case EDGThrowPhase::Aim: return TEXT("Aim");
        case EDGThrowPhase::RunUp: return TEXT("RunUp");
        case EDGThrowPhase::ReachBack: return TEXT("ReachBack");
        case EDGThrowPhase::Plant: return TEXT("Plant");
        case EDGThrowPhase::Acceleration: return TEXT("Acceleration");
        case EDGThrowPhase::Release: return TEXT("Release");
        case EDGThrowPhase::FollowThrough: return TEXT("FollowThrough");
        case EDGThrowPhase::Recovery: return TEXT("Recovery");
        default: return TEXT("Unknown");
    }
}

float ADiscGolfSession7FullCharacterVisualCaptureRunner::
    GetActiveMontagePosition() const
{
    if (!Golfer || !Golfer->GetSkeletalGolferMesh())
    {
        return -1.0f;
    }
    if (UAnimInstance* Anim =
            Golfer->GetSkeletalGolferMesh()->GetAnimInstance())
    {
        if (const UAnimMontage* Montage = Anim->GetCurrentActiveMontage())
        {
            return Anim->Montage_GetPosition(Montage);
        }
    }
    return -1.0f;
}

int32 ADiscGolfSession7FullCharacterVisualCaptureRunner::CountWorldDiscs() const
{
    int32 Count = 0;
    if (GetWorld())
    {
        for (TActorIterator<ADiscActor> It(GetWorld()); It; ++It)
        {
            Count += IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
        }
    }
    return Count;
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    HasVisibleCustomizationComponent(FName ItemId) const
{
    if (ItemId.IsNone() || !Golfer)
    {
        return false;
    }
    TArray<UPrimitiveComponent*> Components;
    Golfer->GetComponents(Components);
    return Components.ContainsByPredicate(
        [ItemId](const UPrimitiveComponent* Component)
        {
            return Component && Component->IsRegistered()
                && Component->IsVisible()
                && Component->ComponentHasTag(
                    TEXT("DG_CustomizationCosmetic"))
                && Component->ComponentHasTag(ItemId);
        });
}

FString ADiscGolfSession7FullCharacterVisualCaptureRunner::GetHeadwearId() const
{
    const FDGEquippedOutfitEntry* Entry = Golfer
        ? DiscGolfOutfitRuntime::FindEntryForSlot(
            Golfer->GetCurrentOutfitLoadout(), EDGOutfitSlot::Headwear)
        : nullptr;
    return Entry ? Entry->ItemId.ToString() : FString();
}

FString ADiscGolfSession7FullCharacterVisualCaptureRunner::
    GetValidationSavePath() const
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("SaveGames"),
        ValidationSaveSlot + TEXT(".sav"));
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    CleanupValidationSaveSlot()
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

void ADiscGolfSession7FullCharacterVisualCaptureRunner::SnapshotFiles(
    const FString& Root,
    bool bPackagesOnly,
    TMap<FString, FFileStamp>& Out) const
{
    Out.Reset();
    if (!IFileManager::Get().DirectoryExists(*Root))
    {
        return;
    }
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(
        Files, *Root, TEXT("*"), true, false, false);
    for (const FString& File : Files)
    {
        const FString Extension =
            FPaths::GetExtension(File, true).ToLower();
        if (bPackagesOnly
            && Extension != TEXT(".uasset")
            && Extension != TEXT(".umap")
            && Extension != TEXT(".uexp")
            && Extension != TEXT(".ubulk")
            && Extension != TEXT(".uptnl"))
        {
            continue;
        }
        const FFileStatData Stat = IFileManager::Get().GetStatData(*File);
        FFileStamp Stamp;
        Stamp.Size = Stat.FileSize;
        Stamp.TimestampTicks = Stat.ModificationTime.GetTicks();
        Out.Add(File, Stamp);
    }
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::DiffFiles(
    const TMap<FString, FFileStamp>& Before,
    const TMap<FString, FFileStamp>& After,
    TArray<FString>& Out) const
{
    Out.Reset();
    for (const TPair<FString, FFileStamp>& Pair : Before)
    {
        const FFileStamp* Current = After.Find(Pair.Key);
        if (!Current || !(Pair.Value == *Current))
        {
            Out.Add(Pair.Key);
        }
    }
    for (const TPair<FString, FFileStamp>& Pair : After)
    {
        if (!Before.Contains(Pair.Key))
        {
            Out.Add(Pair.Key);
        }
    }
    Out.Sort();
}

bool ADiscGolfSession7FullCharacterVisualCaptureRunner::
    VerifyNoPersistentWrites()
{
    if (!bPersistentSnapshotReady)
    {
        return false;
    }
    TMap<FString, FFileStamp> Packages;
    TMap<FString, FFileStamp> SaveGames;
    SnapshotFiles(FPaths::ProjectContentDir(), true, Packages);
    SnapshotFiles(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")),
        false, SaveGames);
    DiffFiles(InitialPackages, Packages, ChangedPackages);
    DiffFiles(InitialSaveGames, SaveGames, ChangedSaveGames);
    return ChangedPackages.IsEmpty() && ChangedSaveGames.IsEmpty();
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::RestoreRuntimeState()
{
    ResumeWorldAfterThrowCapture(false);
    if (ThrowAdapter)
    {
        ThrowAdapter->GetAuthoritativeLaunchDelegate().Unbind();
        if (ThrowAdapter->IsThrowActive())
        {
            ThrowAdapter->RecoverInterruptedThrow();
        }
    }
    if (PlayerController && PlayerController->IsCharacterCreatorOpen())
    {
        PlayerController->CancelCharacterCreator();
    }
    if (Golfer)
    {
        if (bPreviewCameraActive)
        {
            Golfer->EndCharacterCreatorPreview(true);
            bPreviewCameraActive = false;
        }
        FString Status;
        Golfer->ApplyFullCharacterCustomizationTransactionally(
            OpeningCustomization, true, Status);
    }
    if (bValidationCaptureFovLocked && PlayerController
        && PlayerController->PlayerCameraManager)
    {
        PlayerController->PlayerCameraManager->SetFOV(OpeningCameraFovDeg);
        PlayerController->PlayerCameraManager->UnlockFOV();
        bValidationCaptureFovLocked = false;
    }
    CleanupValidationSaveSlot();
    if (GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
    }
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::WriteManifest(
    bool bPassed,
    const FString& Error)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(
        TEXT("schema"),
        TEXT("DiscGolfTour.Session7FullCharacterVisualEvidence.v1"));
    Root->SetStringField(TEXT("status"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Root->SetStringField(TEXT("error"), Error);
    Root->SetBoolField(TEXT("manual_visual_review_required"), true);
    Root->SetStringField(TEXT("manual_review_status"), TEXT("PENDING"));
    TArray<TSharedPtr<FJsonValue>> ManualJson;
    for (const TCHAR* Criterion : Session7VisualManualCriteria)
    {
        ManualJson.Add(MakeShared<FJsonValueString>(Criterion));
    }
    Root->SetArrayField(TEXT("manual_review_criteria"), ManualJson);
    Root->SetStringField(TEXT("proxy_art_status"), TEXT("DO_NOT_SHIP"));
    Root->SetStringField(
        TEXT("cosmetic_catalog"),
        DiscGolfFullCharacterRuntime::CosmeticCatalogObjectPath);
    Root->SetStringField(
        TEXT("modular_head_mesh"),
        DiscGolfFullCharacterRuntime::HeadMeshObjectPath);
    Root->SetStringField(
        TEXT("head_material"), Session7VisualHeadMaterialObjectPath);
    Root->SetStringField(
        TEXT("hair_material"), Session7VisualHairMaterialObjectPath);
    Root->SetStringField(
        TEXT("proxy_source_root"),
        TEXT("SourceArt/DiscGolf/Characters/Customization/Proxy"));
    Root->SetStringField(
        TEXT("appearance_visual_authority"),
        TEXT("MODULAR_HEAD_ONLY"));
    Root->SetBoolField(TEXT("body_surface_appearance_deferred"), true);
    Root->SetNumberField(TEXT("expected_capture_count"), 24);
    Root->SetNumberField(TEXT("capture_count"), Captures.Num());

    TArray<TSharedPtr<FJsonValue>> VisibleMorphJson;
    for (FName Key : DiscGolfFullCharacterRuntime::GetVisibleProxyFaceMorphKeys())
    {
        VisibleMorphJson.Add(
            MakeShared<FJsonValueString>(Key.ToString()));
    }
    Root->SetArrayField(
        TEXT("visible_proxy_face_morphs"), VisibleMorphJson);
    TArray<TSharedPtr<FJsonValue>> DeferredMorphJson;
    for (const TCHAR* Key : Session7VisualDeferredMorphs)
    {
        DeferredMorphJson.Add(MakeShared<FJsonValueString>(Key));
    }
    Root->SetArrayField(
        TEXT("visual_deferred_face_morphs"), DeferredMorphJson);
    TArray<TSharedPtr<FJsonValue>> StableIdJson;
    for (const TCHAR* ItemId : Session7VisualStableIds)
    {
        StableIdJson.Add(MakeShared<FJsonValueString>(ItemId));
    }
    Root->SetArrayField(TEXT("stable_cosmetic_ids"), StableIdJson);

    Root->SetBoolField(
        TEXT("draft_preserved_across_tabs"), bDraftPreservedAcrossTabs);
    Root->SetBoolField(
        TEXT("current_tab_reset_scoped"), bCurrentTabResetScoped);
    Root->SetBoolField(
        TEXT("reset_all_restored_defaults"), bResetAllRestoredDefaults);
    Root->SetBoolField(
        TEXT("randomize_locks_respected"), bRandomizeLocksRespected);
    Root->SetBoolField(
        TEXT("randomize_catalog_valid"), bRandomizeCatalogValid);
    Root->SetBoolField(TEXT("creator_apply_saved"), bCreatorApplySaved);
    Root->SetBoolField(TEXT("creator_apply_reloaded"), bCreatorApplyReloaded);
    Root->SetBoolField(
        TEXT("creator_cancel_restored_applied"),
        bCreatorCancelRestoredApplied);
    Root->SetBoolField(
        TEXT("schema8_migration_reloaded"), bSchema8MigrationReloaded);
    Root->SetBoolField(
        TEXT("missing_cosmetic_fallbacks_resolved"),
        bMissingCosmeticFallbacksResolved);
    Root->SetBoolField(
        TEXT("validation_temp_slot_deleted"),
        bValidationTempSlotDeleted);
    Root->SetNumberField(
        TEXT("validation_release_callbacks"), ValidationReleaseCallbacks);
    Root->SetNumberField(
        TEXT("paused_throw_capture_count"), PausedThrowCaptureCount);
    Root->SetNumberField(
        TEXT("world_disc_delta"), CountWorldDiscs() - BaselineDiscs);
    Root->SetNumberField(
        TEXT("stroke_delta"),
        GameMode ? GameMode->GetStrokes() - BaselineStrokes : -1);

    bool bAllHeadMaterialsHaveMorphTargetUsage = Captures.Num() == 24;
    TArray<TSharedPtr<FJsonValue>> CaptureJson;
    for (const FCapture& Capture : Captures)
    {
        bAllHeadMaterialsHaveMorphTargetUsage &=
            Capture.bHeadMaterialHasMorphTargetUsage;
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("filename"), Capture.Filename);
        Object->SetStringField(TEXT("evidence"), Capture.Evidence);
        Object->SetStringField(TEXT("sha1"), Capture.Sha1);
        Object->SetNumberField(
            TEXT("bytes"), static_cast<double>(Capture.Bytes));
        Object->SetNumberField(TEXT("width"), Capture.Width);
        Object->SetNumberField(TEXT("height"), Capture.Height);
        Object->SetNumberField(
            TEXT("active_tab_index"), Capture.ActiveTabIndex);
        Object->SetStringField(TEXT("active_tab"), Capture.ActiveTab);
        Object->SetStringField(TEXT("profile_id"), Capture.ProfileId);
        Object->SetNumberField(
            TEXT("body_height_cm"), Capture.BodyHeightCm);
        Object->SetStringField(
            TEXT("throw_phase"), Capture.ThrowPhase);
        Object->SetNumberField(
            TEXT("montage_position_seconds"),
            Capture.MontagePositionSeconds);
        Object->SetNumberField(
            TEXT("camera_fov_deg"), Capture.CameraFovDeg);
        Object->SetStringField(
            TEXT("hair_style_id"), Capture.HairStyleId);
        Object->SetStringField(
            TEXT("headwear_id"), Capture.HeadwearId);
        Object->SetField(
            TEXT("hair_color"), Session7VisualColorJson(Capture.HairColor));
        Object->SetField(
            TEXT("skin_tone"), Session7VisualColorJson(Capture.SkinTone));
        Object->SetField(
            TEXT("eye_color"), Session7VisualColorJson(Capture.EyeColor));
        Object->SetBoolField(TEXT("creator_open"), Capture.bCreatorOpen);
        Object->SetBoolField(
            TEXT("preview_in_right_pane"), Capture.bPreviewInRightPane);
        Object->SetBoolField(
            TEXT("view_target_is_golfer"), Capture.bViewTargetIsGolfer);
        Object->SetBoolField(
            TEXT("expected_controls_visible"),
            Capture.bExpectedControlsVisible);
        Object->SetNumberField(
            TEXT("minimum_visible_control_count"),
            Capture.MinimumVisibleControlCount);
        TArray<TSharedPtr<FJsonValue>> ControlJson;
        for (const FString& Control : Capture.VisibleControlIds)
        {
            ControlJson.Add(MakeShared<FJsonValueString>(Control));
        }
        Object->SetArrayField(TEXT("visible_control_ids"), ControlJson);
        TSharedRef<FJsonObject> MorphJson = MakeShared<FJsonObject>();
        for (const TPair<FName, float>& Pair
            : Capture.VisibleProxyMorphValues)
        {
            MorphJson->SetNumberField(Pair.Key.ToString(), Pair.Value);
        }
        Object->SetObjectField(
            TEXT("visible_proxy_morph_values"), MorphJson);
        TArray<TSharedPtr<FJsonValue>> RoiJson;
        RoiJson.Add(MakeShared<FJsonValueNumber>(Capture.HeadRoi.Min.X));
        RoiJson.Add(MakeShared<FJsonValueNumber>(Capture.HeadRoi.Min.Y));
        RoiJson.Add(MakeShared<FJsonValueNumber>(Capture.HeadRoi.Max.X));
        RoiJson.Add(MakeShared<FJsonValueNumber>(Capture.HeadRoi.Max.Y));
        Object->SetArrayField(TEXT("head_roi_pixels"), RoiJson);
        Object->SetBoolField(
            TEXT("head_closeup"), Capture.bHeadCloseup);
        Object->SetBoolField(
            TEXT("head_framed"), Capture.bHeadFramed);
        Object->SetBoolField(
            TEXT("body_framed"), Capture.bBodyFramed);
        Object->SetBoolField(
            TEXT("camera_fov_valid"), Capture.bCameraFovValid);
        Object->SetBoolField(
            TEXT("finite_pose"), Capture.bFinitePose);
        Object->SetBoolField(
            TEXT("one_customization_component"),
            Capture.bOneCustomizationComponent);
        Object->SetBoolField(
            TEXT("one_outfit_component"), Capture.bOneOutfitComponent);
        Object->SetBoolField(
            TEXT("head_leader_pose"), Capture.bHeadLeaderPose);
        Object->SetBoolField(
            TEXT("visible_proxy_morphs_applied"),
            Capture.bVisibleProxyMorphsApplied);
        Object->SetBoolField(
            TEXT("cosmetics_attached"), Capture.bCosmeticsAttached);
        Object->SetBoolField(
            TEXT("cosmetics_collision_free"),
            Capture.bCosmeticsCollisionFree);
        Object->SetBoolField(
            TEXT("presentation_materials_valid"),
            Capture.bPresentationMaterialsValid);
        Object->SetBoolField(
            TEXT("head_material_is_canonical_mid"),
            Capture.bHeadMaterialIsCanonicalMid);
        Object->SetBoolField(
            TEXT("head_material_has_morph_target_usage"),
            Capture.bHeadMaterialHasMorphTargetUsage);
        Object->SetBoolField(
            TEXT("outfit_attachments_and_materials_valid"),
            Capture.bOutfitAttachmentsAndMaterialsValid);
        Object->SetNumberField(
            TEXT("equipped_outfit_count"),
            Capture.EquippedOutfitCount);
        Object->SetNumberField(
            TEXT("validated_presentation_component_count"),
            Capture.ValidatedPresentationComponentCount);
        Object->SetBoolField(
            TEXT("hair_hidden_by_outfit_coverage"),
            Capture.bHairHiddenByOutfitCoverage);
        Object->SetBoolField(
            TEXT("hair_component_visible"),
            Capture.bHairComponentVisible);
        Object->SetNumberField(
            TEXT("world_disc_delta"), Capture.WorldDiscDelta);
        Object->SetNumberField(
            TEXT("stroke_delta"), Capture.StrokeDelta);
        Object->SetStringField(
            TEXT("proxy_art_status"), TEXT("DO_NOT_SHIP"));
        CaptureJson.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("captures"), CaptureJson);
    Root->SetBoolField(
        TEXT("head_material_has_morph_target_usage_all_24"),
        bAllHeadMaterialsHaveMorphTargetUsage);

    TArray<TSharedPtr<FJsonValue>> PackageJson;
    for (const FString& Path : ChangedPackages)
    {
        PackageJson.Add(MakeShared<FJsonValueString>(Path));
    }
    TArray<TSharedPtr<FJsonValue>> SaveJson;
    for (const FString& Path : ChangedSaveGames)
    {
        SaveJson.Add(MakeShared<FJsonValueString>(Path));
    }
    Root->SetArrayField(TEXT("changed_packages"), PackageJson);
    Root->SetArrayField(TEXT("changed_save_games"), SaveJson);
    Root->SetBoolField(
        TEXT("no_persistent_writes"),
        bPersistentSnapshotReady && ChangedPackages.IsEmpty()
            && ChangedSaveGames.IsEmpty());
    Root->SetStringField(
        TEXT("allowed_outputs"),
        TEXT("Exactly 24 PNGs, this manifest and launcher validation under Saved/CharacterFramework/Screenshots/Session7_FullCharacter plus logs only."));
    FString Json;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    FFileHelper::SaveStringToFile(Json, *ManifestPath);
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::Fail(
    const FString& Reason)
{
    if (bFinished)
    {
        return;
    }
    bFinished = true;
    RestoreRuntimeState();
    bValidationTempSlotDeleted = CleanupValidationSaveSlot();
    if (bPersistentSnapshotReady)
    {
        VerifyNoPersistentWrites();
    }
    WriteManifest(false, Reason);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("DG_SESSION7_FULL_CHARACTER_VISUAL_CAPTURE: FAIL %s manifest=%s"),
        *Reason, *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::Pass()
{
    if (bFinished)
    {
        return;
    }
    bool bCaptureContract = Captures.Num() == 24;
    for (int32 Index = 0; Index < Captures.Num(); ++Index)
    {
        const FCapture& Capture = Captures[Index];
        const bool bPhase = Index == 22
            ? Capture.ThrowPhase == TEXT("Release")
                && Capture.MontagePositionSeconds >= 1.52f
                && Capture.MontagePositionSeconds <= 1.68f
            : Index == 23
                ? Capture.ThrowPhase == TEXT("FollowThrough")
                    && Capture.MontagePositionSeconds >= 1.95f
                    && Capture.MontagePositionSeconds <= 2.12f
                : Capture.ThrowPhase == TEXT("Idle");
        bCaptureContract &= Capture.Filename
                == Session7VisualFilenames[Index]
            && Capture.Width == Session7VisualExpectedWidth
            && Capture.Height == Session7VisualExpectedHeight
            && Capture.Bytes >= Session7VisualMinimumPngBytes
            && Capture.Sha1.Len() == FSHAHash::GetStringLen()
            && Capture.bFinitePose
            && Capture.bCameraFovValid
            && (Capture.bHeadCloseup
                ? Capture.bHeadFramed : Capture.bBodyFramed)
            && Capture.bOneCustomizationComponent
            && Capture.bOneOutfitComponent
            && Capture.bHeadLeaderPose
            && Capture.bVisibleProxyMorphsApplied
            && Capture.bCosmeticsAttached
            && Capture.bCosmeticsCollisionFree
            && Capture.bPresentationMaterialsValid
            && Capture.bHeadMaterialIsCanonicalMid
            && Capture.bHeadMaterialHasMorphTargetUsage
            && Capture.bOutfitAttachmentsAndMaterialsValid
            && Capture.WorldDiscDelta == 0
            && Capture.StrokeDelta == 0
            && bPhase;
    }
    const bool bFacePresets = Captures.Num() == 24
        && Captures[8].VisibleProxyMorphValues.Num() == 5
        && Captures[9].VisibleProxyMorphValues.FindRef(TEXT("jaw_width"))
            > Captures[8].VisibleProxyMorphValues.FindRef(TEXT("jaw_width"))
        && Captures[10].VisibleProxyMorphValues.FindRef(TEXT("head_width"))
            < Captures[8].VisibleProxyMorphValues.FindRef(TEXT("head_width"))
        && Captures[11].VisibleProxyMorphValues.FindRef(
            TEXT("cheek_fullness"))
            > Captures[8].VisibleProxyMorphValues.FindRef(
                TEXT("cheek_fullness"));
    const bool bHatRestore = Captures.Num() == 24
        && Captures[17].HairStyleId == TEXT("hair_medium")
        && Captures[18].HairStyleId == TEXT("hair_medium")
        && Captures[17].HeadwearId
            == TEXT("proxy_s6_headwear_cap_01")
        && Captures[18].HeadwearId.IsEmpty()
        && Captures[17].bHairHiddenByOutfitCoverage
        && !Captures[17].bHairComponentVisible
        && !Captures[18].bHairHiddenByOutfitCoverage
        && Captures[18].bHairComponentVisible
        && Captures[17].HairColor == Captures[18].HairColor;
    const bool bProfiles = Captures.Num() == 24
        && Captures[19].ProfileId == TEXT("ShortCompact")
        && Captures[20].ProfileId == TEXT("Baseline")
        && Captures[21].ProfileId == TEXT("TallLongArms")
        && Captures[19].BodyHeightCm < Captures[20].BodyHeightCm
        && Captures[20].BodyHeightCm < Captures[21].BodyHeightCm;
    const bool bBehavior = bDraftPreservedAcrossTabs
        && bCurrentTabResetScoped && bResetAllRestoredDefaults
        && bRandomizeLocksRespected && bRandomizeCatalogValid
        && bCreatorApplySaved && bCreatorApplyReloaded
        && bCreatorCancelRestoredApplied && bSchema8MigrationReloaded
        && bMissingCosmeticFallbacksResolved;
    const bool bIsolated = ValidationReleaseCallbacks == 1
        && PausedThrowCaptureCount == 2
        && CountWorldDiscs() == BaselineDiscs
        && GameMode && GameMode->GetStrokes() == BaselineStrokes;
    RestoreRuntimeState();
    bValidationTempSlotDeleted = CleanupValidationSaveSlot();
    const bool bNoWrites = VerifyNoPersistentWrites();
    if (!bCaptureContract || !bFacePresets || !bHatRestore
        || !bProfiles || !bBehavior || !bIsolated
        || !bValidationTempSlotDeleted || !bNoWrites)
    {
        Fail(TEXT("final 24-frame, creator, face, hat, profiles, one-release, gameplay-isolation, temp-slot or no-write invariant failed"));
        return;
    }
    bFinished = true;
    WriteManifest(true, TEXT(""));
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION7_FULL_CHARACTER_VISUAL_CAPTURE: PASS captures=24 tabs=7 controls=1 face_presets=4 visible_morphs=5 deferred_morphs=15 appearance_head_only=1 hat_hide_restore=1 profiles=3 release_callback=1 paused_throw_frames=2 gameplay_discs=0 strokes=0 package_writes=0 save_writes=0 validation_temp_slot_deleted=1 manual_review_required=1 manifest=%s"),
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

void ADiscGolfSession7FullCharacterVisualCaptureRunner::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (!bFinished)
    {
        RestoreRuntimeState();
    }
    Super::EndPlay(EndPlayReason);
}
