#include "DiscGolfSession6OutfitVisualCaptureRunner.h"

#include "DiscActor.h"
#include "DiscGolferPawn.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitItem.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfThrowComponent.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolfSaveGame.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameters.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace DiscGolfSession6OutfitVisual
{
constexpr int32 Session6ExpectedWidth = 1920;
constexpr int32 Session6ExpectedHeight = 1080;
constexpr int64 Session6MinimumPngBytes = 32768;
constexpr double Session6ScreenshotTimeoutSeconds = 30.0;
constexpr float Session6ValidationCaptureFovDeg = 64.0f;
constexpr const TCHAR* Session6CanonicalOutfitMaterialObjectPath =
    TEXT("/Game/DiscGolf/Materials/Outfits/M_DG_OutfitProxy.M_DG_OutfitProxy");

const TCHAR* Session6Filenames[] = {
    TEXT("01_Outfit_Creator_Tab.png"),
    TEXT("02_Top_Choices_And_Variants.png"),
    TEXT("03_Outerwear.png"),
    TEXT("04_Bottoms_And_Shoes.png"),
    TEXT("05_Hats_And_Eyewear.png"),
    TEXT("06_Gloves_And_Wrist.png"),
    TEXT("07_Disc_Bag.png"),
    TEXT("08_ShortCompact_Full_Outfit.png"),
    TEXT("09_Baseline_Full_Outfit.png"),
    TEXT("10_TallLongArms_Full_Outfit.png"),
    TEXT("11_Outfitted_Release_Frame.png"),
    TEXT("12_Outfitted_FollowThrough.png")
};

const TCHAR* Session6EvidenceLabels[] = {
    TEXT("Live Outfit creator with all eleven modular categories"),
    TEXT("Two Top choices and stable material/color variant preview"),
    TEXT("Outerwear layered independently from Top"),
    TEXT("Bottom, socks, and footwear modular slots"),
    TEXT("Headwear and eyewear static attachment slots"),
    TEXT("Glove leader-pose clothing and cosmetic wrist attachment"),
    TEXT("Cosmetic back bag; existing gameplay DiscBag remains authoritative"),
    TEXT("ShortCompact representative full outfit"),
    TEXT("Baseline representative full outfit"),
    TEXT("TallLongArms representative full outfit"),
    TEXT("Accepted RHBH exact release event while fully outfitted"),
    TEXT("Accepted RHBH follow-through while fully outfitted")
};

int32 Session6BigEndianInt32(const uint8* Bytes)
{
    return (static_cast<int32>(Bytes[0]) << 24)
        | (static_cast<int32>(Bytes[1]) << 16)
        | (static_cast<int32>(Bytes[2]) << 8)
        | static_cast<int32>(Bytes[3]);
}
}

using namespace DiscGolfSession6OutfitVisual;

ADiscGolfSession6OutfitVisualCaptureRunner::ADiscGolfSession6OutfitVisualCaptureRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ADiscGolfSession6OutfitVisualCaptureRunner::Start()
{
    if (bStarted || bFinished)
    {
        return;
    }
    bStarted = true;
    GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    ThrowAdapter = Golfer ? Golfer->GetRHBHThrowAdapter() : nullptr;
    OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("CharacterFramework/Screenshots/Session6_OutfitCustomization"));
    ManifestPath = FPaths::Combine(
        OutputDirectory, TEXT("Session6_Outfit_CaptureManifest.json"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    if (!GameMode || !PlayerController || !Golfer || !ThrowAdapter
        || !Golfer->GetOutfitCatalog() || !Golfer->GetSkeletalGolferMesh()
        || !Golfer->GetCharacterCreatorProfile(OpeningBody, OpeningStyle, OpeningHandedness)
        || UGameplayStatics::IsGamePaused(this))
    {
        Fail(TEXT("Session 6 visual runtime, catalog, or accepted character foundation was unavailable"));
        return;
    }

    for (const TCHAR* Filename : Session6Filenames)
    {
        IFileManager::Get().Delete(*FPaths::Combine(OutputDirectory, Filename), false, true, true);
    }
    IFileManager::Get().Delete(*ManifestPath, false, true, true);
    SnapshotFiles(FPaths::ProjectContentDir(), true, InitialPackages);
    SnapshotFiles(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")), false, InitialSaveGames);
    bPersistentSnapshotReady = true;

    GameMode->SkipCurrentPresentation();
    BaselineDiscs = CountWorldDiscs();
    BaselineStrokes = GameMode->GetStrokes();
    OpeningOutfit = Golfer->GetCurrentOutfitLoadout();
    FString Error;
    if (!ApplyFullOutfit(0, Error) || !PlayerController->OpenCharacterCreator())
    {
        Fail(Error.IsEmpty() ? TEXT("live creator could not open") : Error);
        return;
    }
    if (!PlayerController->PrepareCharacterCreatorForSession6VisualEvidence(
            EDGOutfitSlot::Top))
    {
        Fail(TEXT("creator could not enter its Outfit tab for Session 6 visual evidence"));
        return;
    }
    bCreatorOutfitTabPrepared = true;
    bCreatorWasOpened = true;
    APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
    if (!CameraManager)
    {
        Fail(TEXT("validation capture could not acquire the live player camera manager"));
        return;
    }
    OpeningCameraFovDeg = CameraManager->GetFOVAngle();
    CameraManager->SetFOV(Session6ValidationCaptureFovDeg);
    bValidationCaptureFovLocked = true;
    RequestCapture(0, Session6EvidenceLabels[0]);
}

void ADiscGolfSession6OutfitVisualCaptureRunner::Tick(float DeltaSeconds)
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
    if (NextIndex >= UE_ARRAY_COUNT(Session6Filenames))
    {
        Pass();
        return;
    }
    if (!PrepareCapture(NextIndex) && !bFinished)
    {
        Fail(FString::Printf(TEXT("could not prepare visual evidence frame %d"), NextIndex + 1));
    }
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::PrepareCapture(int32 Index)
{
    FString Error;
    switch (Index)
    {
        case 1:
            if (!ApplySlots({EDGOutfitSlot::Top}, 1, 1, Error)) { Fail(Error); return false; }
            break;
        case 2:
            if (!ApplySlots({EDGOutfitSlot::Top, EDGOutfitSlot::Outerwear}, 0, 2, Error)) { Fail(Error); return false; }
            break;
        case 3:
            if (!ApplySlots({EDGOutfitSlot::Bottom, EDGOutfitSlot::Socks, EDGOutfitSlot::Footwear}, 0, 1, Error)) { Fail(Error); return false; }
            break;
        case 4:
            if (!ApplySlots({EDGOutfitSlot::Headwear, EDGOutfitSlot::Eyewear}, 0, 2, Error)) { Fail(Error); return false; }
            break;
        case 5:
            if (!ApplySlots({EDGOutfitSlot::Glove, EDGOutfitSlot::Wrist}, 0, 1, Error)) { Fail(Error); return false; }
            break;
        case 6:
            if (!ApplySlots({EDGOutfitSlot::Bag, EDGOutfitSlot::Accessory}, 0, 2, Error)) { Fail(Error); return false; }
            break;
        case 7:
            if (!ProveCreatorApplyReloadCancel(Error)) { Fail(Error); return false; }
            bPreviewCameraActive = true;
            Golfer->BeginCharacterCreatorPreview();
            if (!ApplyProfileAsset(
                    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact"), Error)
                || !ApplyFullOutfit(0, Error)) { Fail(Error); return false; }
            break;
        case 8:
            if (!ApplyProfileAsset(
                    TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"), Error)
                || !ApplyFullOutfit(1, Error)) { Fail(Error); return false; }
            break;
        case 9:
            if (!ApplyProfileAsset(
                    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms"), Error)
                || !ApplyFullOutfit(2, Error)) { Fail(Error); return false; }
            break;
        case 10:
            if (bVisualThrowStarted)
            {
                return true;
            }
            if (!ApplyProfileAsset(
                    TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"), Error)
                || !ApplyFullOutfit(1, Error)
                || !BeginVisualThrow(Error)) { Fail(Error); return false; }
            bVisualThrowStarted = true;
            return true;
        case 11:
            // Capture only once the real framework reports FollowThrough.
            if (!Golfer->FindComponentByClass<UDiscGolfThrowComponent>()
                || Golfer->FindComponentByClass<UDiscGolfThrowComponent>()->CurrentPhase
                    != EDGThrowPhase::FollowThrough)
            {
                return true;
            }
            if (!PauseWorldForThrowCapture(Error)) { Fail(Error); return false; }
            break;
        default:
            break;
    }

    if (Index == 10)
    {
        return true;
    }
    RequestCapture(Index, Session6EvidenceLabels[Index]);
    return true;
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::ApplySlots(
    const TArray<EDGOutfitSlot>& Slots,
    int32 ItemIndex,
    int32 VariantIndex,
    FString& OutError)
{
    if (!Golfer || !Golfer->GetOutfitCatalog())
    {
        OutError = TEXT("outfit catalog disappeared during capture");
        return false;
    }
    FDGOutfitLoadout Loadout;
    for (const EDGOutfitSlot Slot : Slots)
    {
        TArray<UDiscGolfOutfitItem*> Items = Golfer->GetOutfitCatalog()->GetItemsForSlot(Slot);
        Items.RemoveAll([](const UDiscGolfOutfitItem* Item) { return Item == nullptr; });
        Items.Sort([](const UDiscGolfOutfitItem& A, const UDiscGolfOutfitItem& B)
        {
            return A.ItemId.LexicalLess(B.ItemId);
        });
        if (!Items.IsValidIndex(ItemIndex))
        {
            // Slots with one proxy remain valid when a composition asks for the
            // second Top choice index.
            if (!Items.IsValidIndex(0))
            {
                OutError = FString::Printf(TEXT("visual slot %d had no proxy item"), static_cast<int32>(Slot));
                return false;
            }
            ItemIndex = 0;
        }
        UDiscGolfOutfitItem* Item = Items[ItemIndex];
        FDGEquippedOutfitEntry Entry;
        Entry.Slot = Slot;
        Entry.ItemId = Item->ItemId;
        Entry.VariantId = Item->Variants.IsValidIndex(VariantIndex)
            ? Item->Variants[VariantIndex].VariantId
            : (Item->Variants.Num() ? Item->Variants[0].VariantId : FName(TEXT("Default")));
        Loadout.Equipped.Add(Entry);
    }
    FString Status;
    if (PlayerController && PlayerController->IsCharacterCreatorOpen())
    {
        FDGOutfitLoadout Draft;
        if (!PlayerController->ResetCharacterCreatorOutfit(Draft))
        {
            OutError = TEXT("creator outfit draft could not reset before focused visual preview");
            return false;
        }
        for (const FDGEquippedOutfitEntry& Entry : Loadout.Equipped)
        {
            if (!PlayerController->PreviewCharacterCreatorOutfitSelection(
                    Entry.Slot, Entry.ItemId, Entry.VariantId, Draft))
            {
                OutError = FString::Printf(
                    TEXT("creator rejected visual preview item %s"),
                    *Entry.ItemId.ToString());
                return false;
            }
        }
        if (!Slots.IsEmpty()
            && !PlayerController->PrepareCharacterCreatorForSession6VisualEvidence(Slots[0]))
        {
            OutError = TEXT("creator could not synchronize its Outfit tab/draft for visual evidence");
            return false;
        }
        bCreatorOutfitTabPrepared = true;
        return true;
    }
    if (!Golfer->ApplyOutfitLoadoutTransactionally(Loadout, false, Status))
    {
        OutError = FString::Printf(TEXT("visual loadout failed: %s"), *Status);
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::ApplyFullOutfit(
    int32 VariantIndex,
    FString& OutError)
{
    return ApplySlots(
        {EDGOutfitSlot::Headwear, EDGOutfitSlot::Eyewear, EDGOutfitSlot::Top,
         EDGOutfitSlot::Outerwear, EDGOutfitSlot::Bottom, EDGOutfitSlot::Socks,
         EDGOutfitSlot::Footwear, EDGOutfitSlot::Glove, EDGOutfitSlot::Wrist,
         EDGOutfitSlot::Bag, EDGOutfitSlot::Accessory},
        0, VariantIndex, OutError);
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::ApplyProfileAsset(
    const TCHAR* AssetPath,
    FString& OutError)
{
    const UDiscGolfCharacterProfile* Profile = LoadObject<UDiscGolfCharacterProfile>(nullptr, AssetPath);
    if (!Profile || !Golfer->PreviewCharacterCreatorProfile(
            Profile->Body, Profile->ThrowStyle, Profile->Handedness))
    {
        OutError = FString::Printf(TEXT("body profile preview failed: %s"), AssetPath);
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::BeginVisualThrow(FString& OutError)
{
    if (!ThrowAdapter || ThrowAdapter->IsThrowActive())
    {
        OutError = TEXT("visual RHBH adapter was unavailable or already active");
        return false;
    }
    ThrowAdapter->GetAuthoritativeLaunchDelegate().Unbind();
    ThrowAdapter->GetAuthoritativeLaunchDelegate().BindUObject(
        this, &ADiscGolfSession6OutfitVisualCaptureRunner::HandleValidationLaunch);
    FThrowCommand Command;
    Command.MoldId = TEXT("Session6VisualNoGameplayDisc");
    Command.ThrowStyle = EThrowStyle::Backhand;
    Command.ShotContext = EDiscShotContext::Drive;
    Command.Direction = Golfer->GetActorForwardVector();
    Command.Power01 = 0.82f;
    Command.HyzerDeg = 3.0f;
    Command.NoseAngleDeg = 1.0f;
    Command.LaunchAngleDeg = 7.0f;
    if (!Golfer->TryStartAnimatedRHBHThrow(Command))
    {
        OutError = TEXT("accepted RHBH montage rejected the outfitted visual fixture");
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::ProveCreatorApplyReloadCancel(
    FString& OutError)
{
    UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    const FString ValidationSlot = Instance
        ? Instance->GetSession6OutfitValidationSaveSlot() : FString();
    if (!Instance || ValidationSlot.IsEmpty()
        || !PlayerController || !PlayerController->IsCharacterCreatorOpen())
    {
        OutError = TEXT("fail-closed Session 6 temporary save slot or live creator was unavailable");
        return false;
    }
    if (UGameplayStatics::DoesSaveGameExist(ValidationSlot, 0))
    {
        OutError = TEXT("Session 6 temporary validation slot was not clean before Apply");
        return false;
    }

    FDGBodyProfile AppliedBody;
    FDGThrowStyle AppliedStyle;
    EDGHandedness AppliedHandedness = EDGHandedness::Right;
    if (!Golfer->GetCharacterCreatorProfile(
            AppliedBody, AppliedStyle, AppliedHandedness))
    {
        OutError = TEXT("creator Apply proof could not read the active profile draft");
        return false;
    }
    CreatorAppliedOutfit = Golfer->GetCurrentOutfitLoadout();
    if (!PlayerController->ApplyCharacterCreatorDraft(
            AppliedBody, AppliedStyle, AppliedHandedness)
        || PlayerController->IsCharacterCreatorOpen()
        || !UGameplayStatics::DoesSaveGameExist(ValidationSlot, 0))
    {
        OutError = TEXT("real creator Apply did not close and write the isolated validation slot");
        return false;
    }
    bCreatorApplySaved = true;

    const UDiscGolfSaveGame* Reloaded = Cast<UDiscGolfSaveGame>(
        UGameplayStatics::LoadGameFromSlot(ValidationSlot, 0));
    bCreatorApplyReloaded = Reloaded
        && Reloaded->SaveSchemaVersion == DiscGolfSaveSchema::CurrentVersion
        && DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Reloaded->OutfitLoadout, CreatorAppliedOutfit);
    if (bCreatorApplyReloaded)
    {
        FString ClearStatus;
        FString ReconstructStatus;
        const FDGOutfitLoadout EmptyLoadout;
        const bool bCleared = Golfer->ApplyOutfitLoadoutTransactionally(
                EmptyLoadout, false, ClearStatus)
            && Golfer->GetCurrentOutfitLoadout().Equipped.IsEmpty()
            && CountVisibleEquippedComponents() == 0;
        const bool bReconstructed = bCleared
            && Golfer->ApplyOutfitLoadoutTransactionally(
                Reloaded->OutfitLoadout, false, ReconstructStatus);
        bCreatorReloadReconstructedOutfit = bReconstructed
            && DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
                Golfer->GetCurrentOutfitLoadout(), CreatorAppliedOutfit)
            && CountVisibleEquippedComponents()
                == CreatorAppliedOutfit.Equipped.Num();
    }
    const bool bDeleted = UGameplayStatics::DeleteGameInSlot(ValidationSlot, 0);
    bValidationTempSlotDeleted = bDeleted
        && !UGameplayStatics::DoesSaveGameExist(ValidationSlot, 0);
    if (!bCreatorApplyReloaded)
    {
        OutError = TEXT("isolated creator Apply did not reload exact schema-8 outfit IDs and variants");
        return false;
    }
    if (!bCreatorReloadReconstructedOutfit)
    {
        OutError = TEXT("schema-8 reload did not reconstruct the exact live outfit components");
        return false;
    }
    if (!bValidationTempSlotDeleted)
    {
        OutError = TEXT("isolated Session 6 validation save slot could not be deleted");
        return false;
    }

    if (!PlayerController->OpenCharacterCreator())
    {
        OutError = TEXT("creator could not reopen for post-Apply Cancel proof");
        return false;
    }
    FDGOutfitLoadout DifferentDraft;
    if (!PlayerController->ResetCharacterCreatorOutfit(DifferentDraft))
    {
        OutError = TEXT("reopened creator could not reset its outfit draft");
        return false;
    }
    const TArray<FDiscGolfOutfitOption> TopOptions =
        PlayerController->GetCharacterCreatorOutfitOptions(EDGOutfitSlot::Top);
    const FDiscGolfOutfitOption* DifferentTop = TopOptions.FindByPredicate(
        [](const FDiscGolfOutfitOption& Option)
        {
            return Option.bCompatible && !Option.ItemId.IsNone();
        });
    if (!DifferentTop)
    {
        OutError = TEXT("reopened creator had no compatible Top for Cancel proof");
        return false;
    }
    const FName VariantId = DifferentTop->VariantIds.IsEmpty()
        ? FName(TEXT("Default")) : DifferentTop->VariantIds[0];
    if (!PlayerController->PreviewCharacterCreatorOutfitSelection(
            EDGOutfitSlot::Top,
            DifferentTop->ItemId,
            VariantId,
            DifferentDraft)
        || DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            DifferentDraft, CreatorAppliedOutfit))
    {
        OutError = TEXT("reopened creator did not establish a distinct Cancel preview");
        return false;
    }
    PlayerController->CancelCharacterCreator();
    bCreatorCancelRestoredApplied = !PlayerController->IsCharacterCreatorOpen()
        && DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Golfer->GetCurrentOutfitLoadout(), CreatorAppliedOutfit);
    if (!bCreatorCancelRestoredApplied)
    {
        OutError = TEXT("creator Cancel did not restore the newly applied opening loadout");
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::PauseWorldForThrowCapture(
    FString& OutError)
{
    if (bCapturePausedWorld || UGameplayStatics::IsGamePaused(this)
        || !Golfer || !Golfer->GetSkeletalGolferMesh())
    {
        OutError = TEXT("throw capture could not acquire an unpaused validation world");
        return false;
    }
    UAnimInstance* Anim = Golfer->GetSkeletalGolferMesh()->GetAnimInstance();
    UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
    if (!Anim || !Montage)
    {
        OutError = TEXT("throw capture could not find its active montage");
        return false;
    }
    Anim->Montage_Pause(Montage);
    if (!UGameplayStatics::SetGamePaused(this, true))
    {
        Anim->Montage_Resume(Montage);
        OutError = TEXT("throw capture could not pause its watchdog clock");
        return false;
    }
    bCapturePausedWorld = true;
    ++PausedThrowCaptureCount;
    return true;
}

void ADiscGolfSession6OutfitVisualCaptureRunner::ResumeWorldAfterThrowCapture(
    bool bResumeMontage)
{
    if (bCapturePausedWorld)
    {
        UGameplayStatics::SetGamePaused(this, false);
        bCapturePausedWorld = false;
    }
    if (bResumeMontage && Golfer && Golfer->GetSkeletalGolferMesh())
    {
        if (UAnimInstance* Anim = Golfer->GetSkeletalGolferMesh()->GetAnimInstance())
        {
            Anim->Montage_Resume(Anim->GetCurrentActiveMontage());
        }
    }
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::HandleValidationLaunch(
    const FThrowCommand& Command,
    const FTransform& GripWorldTransform)
{
    (void)Command;
    if (ValidationReleaseCallbacks != 0 || GripWorldTransform.ContainsNaN())
    {
        return false;
    }
    ++ValidationReleaseCallbacks;
    RequestCapture(10, Session6EvidenceLabels[10]);
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

void ADiscGolfSession6OutfitVisualCaptureRunner::RequestCapture(
    int32 Index,
    const FString& Evidence)
{
    if (bCapturePending || Index != Captures.Num()
        || Index < 0 || Index >= UE_ARRAY_COUNT(Session6Filenames))
    {
        Fail(TEXT("visual screenshot request was overlapping or out of order"));
        return;
    }
    FCapture Record;
    Record.Filename = Session6Filenames[Index];
    Record.Evidence = Evidence;
    Record.bCreatorOutfitTabRequired = Index <= 6;
    Record.bCreatorOutfitTabPrepared = !Record.bCreatorOutfitTabRequired
        || (bCreatorOutfitTabPrepared && PlayerController
            && PlayerController->IsCharacterCreatorOpen());
    Captures.Add(Record);
    bCreatorOutfitTabPrepared = false;
    PendingCaptureIndex = Index;
    PendingCapturePath = FPaths::Combine(OutputDirectory, Record.Filename);
    IFileManager::Get().Delete(*PendingCapturePath, false, true, true);
    bCapturePending = true;
    bCaptureIssued = false;
    PendingStartedSeconds = FPlatformTime::Seconds();
    if (Index > 6)
    {
        ShowEvidenceLabel(
            FString::Printf(TEXT("SESSION 6 OUTFIT EVIDENCE %d/12"), Index + 1),
            Evidence);
    }
    else if (GEngine)
    {
        // The live creator already renders its tab/category/status context.
        // Debug text would overlap the native Slate header in UI evidence.
        GEngine->ClearOnScreenDebugMessages();
    }
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::PollCapture()
{
    if (!bCaptureIssued)
    {
        FString SceneError;
        if (!Captures.IsValidIndex(PendingCaptureIndex)
            || !ValidateCaptureScene(
                PendingCaptureIndex, Captures[PendingCaptureIndex], SceneError))
        {
            Fail(SceneError.IsEmpty()
                ? TEXT("visual capture scene validation failed") : SceneError);
            return false;
        }
        bCaptureIssued = true;
        PendingStartedSeconds = FPlatformTime::Seconds();
        FScreenshotRequest::RequestScreenshot(
            PendingCapturePath, true, false, false, FIntRect(), true);
        return false;
    }
    const int64 Size = IFileManager::Get().FileSize(*PendingCapturePath);
    if (Size >= Session6MinimumPngBytes)
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
        Capture.Width = Session6BigEndianInt32(&Bytes[16]);
        Capture.Height = Session6BigEndianInt32(&Bytes[20]);
        FSHA1 Sha;
        Sha.Update(Bytes.GetData(), Bytes.Num());
        Sha.Final();
        uint8 Digest[FSHA1::DigestSize];
        Sha.GetHash(Digest);
        Capture.Sha1 = BytesToHex(Digest, FSHA1::DigestSize);
        if (Capture.Width != Session6ExpectedWidth || Capture.Height != Session6ExpectedHeight)
        {
            Fail(FString::Printf(TEXT("capture was %dx%d instead of 1920x1080"), Capture.Width, Capture.Height));
            return false;
        }
        const int32 CompletedIndex = PendingCaptureIndex;
        bCapturePending = false;
        bCaptureIssued = false;
        PendingCaptureIndex = INDEX_NONE;
        PendingCapturePath.Reset();
        ResumeWorldAfterThrowCapture(CompletedIndex == 10);
        return true;
    }
    if (FPlatformTime::Seconds() - PendingStartedSeconds > Session6ScreenshotTimeoutSeconds)
    {
        Fail(TEXT("visual screenshot timed out"));
    }
    return false;
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::ValidateCaptureScene(
    int32 Index,
    FCapture& InOutCapture,
    FString& OutError) const
{
    if (!Golfer || !PlayerController || !Golfer->GetSkeletalGolferMesh())
    {
        OutError = TEXT("capture scene lost its player, controller, or body mesh");
        return false;
    }

    USkeletalMeshComponent* Body = Golfer->GetSkeletalGolferMesh();
    const FName RequiredBones[] = {
        TEXT("head"), TEXT("pelvis"), TEXT("hand_l"), TEXT("hand_r"),
        TEXT("foot_l"), TEXT("foot_r")
    };
    TArray<FVector> BoneLocations;
    InOutCapture.bFinitePose = Body->GetSkeletalMeshAsset() != nullptr;
    for (FName Bone : RequiredBones)
    {
        const int32 BoneIndex = Body->GetBoneIndex(Bone);
        const FVector Location = BoneIndex != INDEX_NONE
            ? Body->GetBoneLocation(Bone) : FVector::ZeroVector;
        InOutCapture.bFinitePose &= BoneIndex != INDEX_NONE
            && !Location.ContainsNaN()
            && FMath::IsFinite(Location.X)
            && FMath::IsFinite(Location.Y)
            && FMath::IsFinite(Location.Z);
        BoneLocations.Add(Location);
    }
    if (!InOutCapture.bFinitePose)
    {
        OutError = TEXT("required body bones were missing or non-finite");
        return false;
    }

    int32 ViewWidth = 0;
    int32 ViewHeight = 0;
    PlayerController->GetViewportSize(ViewWidth, ViewHeight);
    const float MarginX = static_cast<float>(ViewWidth) * 0.025f;
    const float MarginY = static_cast<float>(ViewHeight) * 0.025f;
    FVector2D BoundsMin(static_cast<float>(ViewWidth), static_cast<float>(ViewHeight));
    FVector2D BoundsMax = FVector2D::ZeroVector;
    InOutCapture.ProjectedKeyBoneCount = 0;
    InOutCapture.KeyBonesInsideViewport = 0;
    InOutCapture.KeyBonesInsideSafeMargin = 0;
    InOutCapture.KeyBoneProjections.Reset();
    for (int32 BoneIndex = 0; BoneIndex < UE_ARRAY_COUNT(RequiredBones); ++BoneIndex)
    {
        FCapture::FKeyBoneProjection Projection;
        Projection.Bone = RequiredBones[BoneIndex].ToString();
        Projection.bProjected = ViewWidth > 0 && ViewHeight > 0
            && UGameplayStatics::ProjectWorldToScreen(
                PlayerController, BoneLocations[BoneIndex], Projection.Screen, true);
        Projection.bInsideViewport = Projection.bProjected
            && Projection.Screen.X >= 0.0f
            && Projection.Screen.X <= static_cast<float>(ViewWidth)
            && Projection.Screen.Y >= 0.0f
            && Projection.Screen.Y <= static_cast<float>(ViewHeight);
        Projection.bInsideSafeMargin = Projection.bProjected
            && Projection.Screen.X >= MarginX
            && Projection.Screen.X <= static_cast<float>(ViewWidth) - MarginX
            && Projection.Screen.Y >= MarginY
            && Projection.Screen.Y <= static_cast<float>(ViewHeight) - MarginY;
        InOutCapture.ProjectedKeyBoneCount += Projection.bProjected ? 1 : 0;
        InOutCapture.KeyBonesInsideViewport += Projection.bInsideViewport ? 1 : 0;
        InOutCapture.KeyBonesInsideSafeMargin += Projection.bInsideSafeMargin ? 1 : 0;
        if (Projection.bProjected)
        {
            BoundsMin.X = FMath::Min(BoundsMin.X, Projection.Screen.X);
            BoundsMin.Y = FMath::Min(BoundsMin.Y, Projection.Screen.Y);
            BoundsMax.X = FMath::Max(BoundsMax.X, Projection.Screen.X);
            BoundsMax.Y = FMath::Max(BoundsMax.Y, Projection.Screen.Y);
        }
        InOutCapture.KeyBoneProjections.Add(Projection);
    }
    InOutCapture.ProjectedBoundsMin = BoundsMin;
    InOutCapture.ProjectedBoundsMax = BoundsMax;
    const float FeetY = (InOutCapture.KeyBoneProjections[4].Screen.Y
        + InOutCapture.KeyBoneProjections[5].Screen.Y) * 0.5f;
    InOutCapture.SubjectScreenHeightFraction = ViewHeight > 0
        ? FMath::Abs(InOutCapture.KeyBoneProjections[0].Screen.Y - FeetY)
            / static_cast<float>(ViewHeight)
        : 0.0f;
    InOutCapture.SubjectScreenWidthFraction = ViewWidth > 0
        ? FMath::Abs(BoundsMax.X - BoundsMin.X) / static_cast<float>(ViewWidth)
        : 0.0f;
    const bool bCoreBodyInsideViewport =
        InOutCapture.KeyBoneProjections[0].bInsideViewport
        && InOutCapture.KeyBoneProjections[1].bInsideViewport
        && InOutCapture.KeyBoneProjections[4].bInsideViewport
        && InOutCapture.KeyBoneProjections[5].bInsideViewport;
    const float RightPaneMinX = static_cast<float>(ViewWidth) * 0.45f;
    const float RightPaneMaxX = static_cast<float>(ViewWidth) * 0.975f;
    InOutCapture.bCreatorViewTargetIsGolfer = Index > 6
        || PlayerController->GetViewTarget() == Golfer;
    InOutCapture.bCreatorPreviewComposedInRightPane = Index > 6
        || (InOutCapture.KeyBoneProjections[0].Screen.X >= RightPaneMinX
            && InOutCapture.KeyBoneProjections[0].Screen.X <= RightPaneMaxX
            && InOutCapture.KeyBoneProjections[1].Screen.X >= RightPaneMinX
            && InOutCapture.KeyBoneProjections[1].Screen.X <= RightPaneMaxX
            && InOutCapture.KeyBoneProjections[4].Screen.X >= RightPaneMinX
            && InOutCapture.KeyBoneProjections[4].Screen.X <= RightPaneMaxX
            && InOutCapture.KeyBoneProjections[5].Screen.X >= RightPaneMinX
            && InOutCapture.KeyBoneProjections[5].Screen.X <= RightPaneMaxX);
    InOutCapture.bBodyFramed = InOutCapture.ProjectedKeyBoneCount == 6
        && InOutCapture.KeyBonesInsideViewport == 6
        && InOutCapture.KeyBonesInsideSafeMargin >= 4
        && bCoreBodyInsideViewport
        && InOutCapture.SubjectScreenHeightFraction >= 0.20f
        && InOutCapture.SubjectScreenHeightFraction <= 0.96f
        && InOutCapture.SubjectScreenWidthFraction >= 0.04f
        && InOutCapture.SubjectScreenWidthFraction <= 0.96f;

    InOutCapture.ExpectedEquippedCount = Golfer->GetCurrentOutfitLoadout().Equipped.Num();
    InOutCapture.BodyHeightCm = Golfer->GetRuntimeCharacterProfile()
        ? Golfer->GetRuntimeCharacterProfile()->Body.HeightCm : 0.0f;
    InOutCapture.CameraFovDeg = PlayerController->PlayerCameraManager
        ? PlayerController->PlayerCameraManager->GetFOVAngle() : 0.0f;
    InOutCapture.VisibleEquippedComponentCount = CountVisibleEquippedComponents();
    InOutCapture.bOutfitComponentsVisible = InOutCapture.ExpectedEquippedCount > 0
        && InOutCapture.VisibleEquippedComponentCount == InOutCapture.ExpectedEquippedCount;
    FString SkeletalMaterialError;
    const bool bSkeletalMaterialsValid = ValidateVisibleSkeletalOutfitMaterials(
        InOutCapture, SkeletalMaterialError);
    InOutCapture.ThrowPhase = GetCurrentThrowPhaseLabel();
    InOutCapture.MontagePositionSeconds = GetActiveMontagePosition();

    const bool bExpectedPhase = Index == 10
        ? InOutCapture.ThrowPhase == TEXT("Release")
            && InOutCapture.MontagePositionSeconds >= 1.52f
            && InOutCapture.MontagePositionSeconds <= 1.68f
        : Index == 11
            ? InOutCapture.ThrowPhase == TEXT("FollowThrough")
                && InOutCapture.MontagePositionSeconds >= 1.68f
                && InOutCapture.MontagePositionSeconds <= 2.20f
            : InOutCapture.ThrowPhase == TEXT("Idle");
    if (!InOutCapture.bBodyFramed
        || !InOutCapture.bOutfitComponentsVisible
        || !bSkeletalMaterialsValid
        || !bExpectedPhase
        || (InOutCapture.bCreatorOutfitTabRequired
            && !InOutCapture.bCreatorViewTargetIsGolfer)
        || (InOutCapture.bCreatorOutfitTabRequired
            && !InOutCapture.bCreatorPreviewComposedInRightPane)
        || (InOutCapture.bCreatorOutfitTabRequired
            && !InOutCapture.bCreatorOutfitTabPrepared))
    {
        TArray<FString> ProjectionTelemetry;
        for (const FCapture::FKeyBoneProjection& Projection
            : InOutCapture.KeyBoneProjections)
        {
            ProjectionTelemetry.Add(FString::Printf(
                TEXT("%s=(%.1f,%.1f,p=%d,in=%d,safe=%d)"),
                *Projection.Bone,
                Projection.Screen.X,
                Projection.Screen.Y,
                Projection.bProjected ? 1 : 0,
                Projection.bInsideViewport ? 1 : 0,
                Projection.bInsideSafeMargin ? 1 : 0));
        }
        OutError = FString::Printf(
            TEXT("capture %d scene gate failed (framed=%d key_bones=%d/6 inside=%d safe=%d screen_height=%.3f screen_width=%.3f outfit=%d/%d skeletal=%d/%d material_mids=%d/%d skeletal_usage_mids=%d variant_mids=%d canonical=%d usage=%d params=%d phase=%s montage=%.3f outfit_tab=%d view_target_golfer=%d right_pane=%d material_detail=%s projections=[%s])"),
            Index + 1,
            InOutCapture.bBodyFramed ? 1 : 0,
            InOutCapture.ProjectedKeyBoneCount,
            InOutCapture.KeyBonesInsideViewport,
            InOutCapture.KeyBonesInsideSafeMargin,
            InOutCapture.SubjectScreenHeightFraction,
            InOutCapture.SubjectScreenWidthFraction,
            InOutCapture.VisibleEquippedComponentCount,
            InOutCapture.ExpectedEquippedCount,
            InOutCapture.VisibleSkeletalOutfitComponentCount,
            InOutCapture.ExpectedSkeletalOutfitComponentCount,
            InOutCapture.CanonicalMaterialMidCount,
            InOutCapture.ExpectedSkeletalMaterialSlotCount,
            InOutCapture.SkeletalUsageReadyMidCount,
            InOutCapture.VariantParameterMatchedMidCount,
            InOutCapture.bVisibleSkeletalOutfitMaterialsAreCanonicalMids ? 1 : 0,
            InOutCapture.bCanonicalMaterialHasSkeletalMeshUsage ? 1 : 0,
            InOutCapture.bSelectedVariantMaterialParametersMatch ? 1 : 0,
            *InOutCapture.ThrowPhase,
            InOutCapture.MontagePositionSeconds,
            InOutCapture.bCreatorOutfitTabPrepared ? 1 : 0,
            InOutCapture.bCreatorViewTargetIsGolfer ? 1 : 0,
            InOutCapture.bCreatorPreviewComposedInRightPane ? 1 : 0,
            *SkeletalMaterialError,
            *FString::Join(ProjectionTelemetry, TEXT(", ")));
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::ValidateVisibleSkeletalOutfitMaterials(
    FCapture& InOutCapture,
    FString& OutError) const
{
    InOutCapture.ExpectedSkeletalOutfitComponentCount = 0;
    InOutCapture.VisibleSkeletalOutfitComponentCount = 0;
    InOutCapture.ExpectedSkeletalMaterialSlotCount = 0;
    InOutCapture.CanonicalMaterialMidCount = 0;
    InOutCapture.SkeletalUsageReadyMidCount = 0;
    InOutCapture.VariantParameterMatchedMidCount = 0;
    InOutCapture.SelectedSkeletalVariants.Reset();
    InOutCapture.SkeletalMaterialDiagnostics.Reset();
    if (!Golfer || !Golfer->GetOutfitCatalog())
    {
        OutError = TEXT("golfer or outfit catalog unavailable");
        return false;
    }

    TArray<USkeletalMeshComponent*> SkeletalComponents;
    Golfer->GetComponents(SkeletalComponents);
    TSet<const USkeletalMeshComponent*> MatchedComponents;
    for (const USkeletalMeshComponent* Component : SkeletalComponents)
    {
        if (Component && Component != Golfer->GetSkeletalGolferMesh()
            && Component->IsRegistered() && Component->IsVisible())
        {
            ++InOutCapture.VisibleSkeletalOutfitComponentCount;
        }
    }
    bool bCatalogVariantsValid = true;
    bool bEveryMatchedComponentHasMaterialSlots = true;
    const FName PrimaryColorParameter(TEXT("DG_PrimaryColor"));
    const FName SecondaryColorParameter(TEXT("DG_SecondaryColor"));
    const FName AccentColorParameter(TEXT("DG_AccentColor"));
    const FName RoughnessBiasParameter(TEXT("DG_RoughnessBias"));
    for (const FDGEquippedOutfitEntry& Entry : Golfer->GetCurrentOutfitLoadout().Equipped)
    {
        const UDiscGolfOutfitItem* Item =
            Golfer->GetOutfitCatalog()->FindItemById(Entry.ItemId);
        if (!Item || Item->SkeletalMesh.IsNull())
        {
            continue;
        }
        ++InOutCapture.ExpectedSkeletalOutfitComponentCount;
        InOutCapture.SelectedSkeletalVariants.Add(FString::Printf(
            TEXT("%s=%s"),
            *DiscGolfOutfitRuntime::GetSlotDisplayName(Entry.Slot).ToString(),
            *Entry.VariantId.ToString()));

        FDGOutfitVariant Variant;
        const bool bExactVariant = Item->FindVariant(Entry.VariantId, Variant)
            && Variant.VariantId == Entry.VariantId;
        const bool bExactParameterSchema = bExactVariant
            && Variant.VectorParameters.Num() == 3
            && Variant.VectorParameters.Contains(PrimaryColorParameter)
            && Variant.VectorParameters.Contains(SecondaryColorParameter)
            && Variant.VectorParameters.Contains(AccentColorParameter)
            && Variant.ScalarParameters.Num() == 1
            && Variant.ScalarParameters.Contains(RoughnessBiasParameter);
        bCatalogVariantsValid &= bExactParameterSchema;

        const USkeletalMesh* ExpectedMesh = Item->SkeletalMesh.LoadSynchronous();
        USkeletalMeshComponent* MatchedComponent = nullptr;
        for (USkeletalMeshComponent* Component : SkeletalComponents)
        {
            if (Component && Component != Golfer->GetSkeletalGolferMesh()
                && !MatchedComponents.Contains(Component)
                && Component->GetSkeletalMeshAsset() == ExpectedMesh
                && Component->IsRegistered() && Component->IsVisible())
            {
                MatchedComponent = Component;
                MatchedComponents.Add(Component);
                break;
            }
        }
        if (!MatchedComponent)
        {
            InOutCapture.SkeletalMaterialDiagnostics.Add(FString::Printf(
                TEXT("%s:%s missing visible skeletal component"),
                *Entry.ItemId.ToString(), *Entry.VariantId.ToString()));
            continue;
        }
        const int32 MaterialCount = MatchedComponent->GetNumMaterials();
        bEveryMatchedComponentHasMaterialSlots &= MaterialCount > 0;
        InOutCapture.ExpectedSkeletalMaterialSlotCount += MaterialCount;
        if (MaterialCount <= 0 || !bExactParameterSchema)
        {
            InOutCapture.SkeletalMaterialDiagnostics.Add(FString::Printf(
                TEXT("%s:%s material_slots=%d catalog_schema=%d"),
                *Entry.ItemId.ToString(), *Entry.VariantId.ToString(),
                MaterialCount, bExactParameterSchema ? 1 : 0));
        }
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(
                MatchedComponent->GetMaterial(MaterialIndex));
            const UMaterial* BaseMaterial = Mid ? Mid->GetBaseMaterial() : nullptr;
            const bool bCanonicalMid = BaseMaterial
                && BaseMaterial->GetPathName()
                    == Session6CanonicalOutfitMaterialObjectPath;
            const bool bSkeletalUsageReady = bCanonicalMid
                && BaseMaterial->GetUsageByFlag(MATUSAGE_SkeletalMesh);
            InOutCapture.CanonicalMaterialMidCount += bCanonicalMid ? 1 : 0;
            InOutCapture.SkeletalUsageReadyMidCount += bSkeletalUsageReady ? 1 : 0;

            bool bParametersMatch = bSkeletalUsageReady && bExactParameterSchema;
            if (bParametersMatch)
            {
                for (const TPair<FName, FLinearColor>& Pair : Variant.VectorParameters)
                {
                    FLinearColor Actual = FLinearColor::Transparent;
                    bParametersMatch &= Mid->GetVectorParameterValue(
                        FHashedMaterialParameterInfo(Pair.Key), Actual, true)
                        && Actual.Equals(Pair.Value, 0.0001f);
                }
                for (const TPair<FName, float>& Pair : Variant.ScalarParameters)
                {
                    float Actual = 0.0f;
                    bParametersMatch &= Mid->GetScalarParameterValue(
                        FHashedMaterialParameterInfo(Pair.Key), Actual, true)
                        && FMath::IsNearlyEqual(Actual, Pair.Value, 0.0001f);
                }
            }
            InOutCapture.VariantParameterMatchedMidCount += bParametersMatch ? 1 : 0;
            InOutCapture.SkeletalMaterialDiagnostics.Add(FString::Printf(
                TEXT("%s:%s[%d] mid=%d base=%s skeletal_usage=%d params=%d"),
                *Entry.ItemId.ToString(), *Entry.VariantId.ToString(), MaterialIndex,
                Mid ? 1 : 0,
                BaseMaterial ? *BaseMaterial->GetPathName() : TEXT("None"),
                bSkeletalUsageReady ? 1 : 0,
                bParametersMatch ? 1 : 0));
        }
    }

    InOutCapture.bVisibleSkeletalOutfitMaterialsAreCanonicalMids =
        InOutCapture.VisibleSkeletalOutfitComponentCount
            == InOutCapture.ExpectedSkeletalOutfitComponentCount
        && MatchedComponents.Num()
            == InOutCapture.ExpectedSkeletalOutfitComponentCount
        && bEveryMatchedComponentHasMaterialSlots
        && InOutCapture.CanonicalMaterialMidCount
            == InOutCapture.ExpectedSkeletalMaterialSlotCount;
    InOutCapture.bCanonicalMaterialHasSkeletalMeshUsage =
        bEveryMatchedComponentHasMaterialSlots
        && InOutCapture.SkeletalUsageReadyMidCount
            == InOutCapture.ExpectedSkeletalMaterialSlotCount;
    InOutCapture.bSelectedVariantMaterialParametersMatch = bCatalogVariantsValid
        && InOutCapture.VisibleSkeletalOutfitComponentCount
            == InOutCapture.ExpectedSkeletalOutfitComponentCount
        && MatchedComponents.Num()
            == InOutCapture.ExpectedSkeletalOutfitComponentCount
        && bEveryMatchedComponentHasMaterialSlots
        && InOutCapture.VariantParameterMatchedMidCount
            == InOutCapture.ExpectedSkeletalMaterialSlotCount;
    if (!InOutCapture.bVisibleSkeletalOutfitMaterialsAreCanonicalMids
        || !InOutCapture.bCanonicalMaterialHasSkeletalMeshUsage
        || !InOutCapture.bSelectedVariantMaterialParametersMatch)
    {
        OutError = FString::Join(
            InOutCapture.SkeletalMaterialDiagnostics, TEXT("; "));
        return false;
    }
    return true;
}

int32 ADiscGolfSession6OutfitVisualCaptureRunner::CountVisibleEquippedComponents() const
{
    if (!Golfer || !Golfer->GetOutfitCatalog())
    {
        return 0;
    }
    TArray<USkeletalMeshComponent*> SkeletalComponents;
    TArray<UStaticMeshComponent*> StaticComponents;
    Golfer->GetComponents(SkeletalComponents);
    Golfer->GetComponents(StaticComponents);
    TSet<const UPrimitiveComponent*> MatchedComponents;
    for (const FDGEquippedOutfitEntry& Entry : Golfer->GetCurrentOutfitLoadout().Equipped)
    {
        const UDiscGolfOutfitItem* Item = Golfer->GetOutfitCatalog()->FindItemById(Entry.ItemId);
        if (!Item)
        {
            continue;
        }
        if (!Item->SkeletalMesh.IsNull())
        {
            const USkeletalMesh* ExpectedMesh = Item->SkeletalMesh.LoadSynchronous();
            for (const USkeletalMeshComponent* Component : SkeletalComponents)
            {
                if (Component && Component != Golfer->GetSkeletalGolferMesh()
                    && Component->GetSkeletalMeshAsset() == ExpectedMesh
                    && Component->IsRegistered() && Component->IsVisible())
                {
                    MatchedComponents.Add(Component);
                    break;
                }
            }
        }
        else if (!Item->StaticMesh.IsNull())
        {
            const UStaticMesh* ExpectedMesh = Item->StaticMesh.LoadSynchronous();
            for (const UStaticMeshComponent* Component : StaticComponents)
            {
                if (Component && Component != Golfer->GetHeldDiscVisual()
                    && Component->GetStaticMesh() == ExpectedMesh
                    && Component->IsRegistered() && Component->IsVisible())
                {
                    MatchedComponents.Add(Component);
                    break;
                }
            }
        }
    }
    return MatchedComponents.Num();
}

float ADiscGolfSession6OutfitVisualCaptureRunner::GetActiveMontagePosition() const
{
    if (!Golfer || !Golfer->GetSkeletalGolferMesh())
    {
        return -1.0f;
    }
    if (UAnimInstance* Anim = Golfer->GetSkeletalGolferMesh()->GetAnimInstance())
    {
        if (const UAnimMontage* Montage = Anim->GetCurrentActiveMontage())
        {
            return Anim->Montage_GetPosition(Montage);
        }
    }
    return -1.0f;
}

FString ADiscGolfSession6OutfitVisualCaptureRunner::GetCurrentThrowPhaseLabel() const
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

void ADiscGolfSession6OutfitVisualCaptureRunner::ShowEvidenceLabel(
    const FString& Heading,
    const FString& Detail) const
{
    if (!GEngine) return;
    GEngine->ClearOnScreenDebugMessages();
    GEngine->AddOnScreenDebugMessage(910600, 120.0f, FColor::Cyan, Heading, true, FVector2D(1.35f, 1.35f));
    GEngine->AddOnScreenDebugMessage(910601, 120.0f, FColor::White, Detail, true, FVector2D(1.05f, 1.05f));
}

int32 ADiscGolfSession6OutfitVisualCaptureRunner::CountWorldDiscs() const
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

void ADiscGolfSession6OutfitVisualCaptureRunner::SnapshotFiles(
    const FString& Root,
    bool bPackagesOnly,
    TMap<FString, FFileStamp>& Out) const
{
    Out.Reset();
    if (!IFileManager::Get().DirectoryExists(*Root)) return;
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*"), true, false, false);
    for (const FString& File : Files)
    {
        const FString Ext = FPaths::GetExtension(File, true).ToLower();
        if (bPackagesOnly && Ext != TEXT(".uasset") && Ext != TEXT(".umap")) continue;
        FFileStatData Stat = IFileManager::Get().GetStatData(*File);
        FFileStamp Stamp;
        Stamp.Size = Stat.FileSize;
        Stamp.TimestampTicks = Stat.ModificationTime.GetTicks();
        Out.Add(File, Stamp);
    }
}

void ADiscGolfSession6OutfitVisualCaptureRunner::DiffFiles(
    const TMap<FString, FFileStamp>& Before,
    const TMap<FString, FFileStamp>& After,
    TArray<FString>& Out) const
{
    Out.Reset();
    for (const TPair<FString, FFileStamp>& Pair : Before)
    {
        const FFileStamp* Current = After.Find(Pair.Key);
        if (!Current || !(Pair.Value == *Current)) Out.Add(Pair.Key);
    }
    for (const TPair<FString, FFileStamp>& Pair : After)
    {
        if (!Before.Contains(Pair.Key)) Out.Add(Pair.Key);
    }
    Out.Sort();
}

bool ADiscGolfSession6OutfitVisualCaptureRunner::VerifyNoPersistentWrites()
{
    if (!bPersistentSnapshotReady)
    {
        return false;
    }
    TMap<FString, FFileStamp> Packages;
    TMap<FString, FFileStamp> SaveGames;
    SnapshotFiles(FPaths::ProjectContentDir(), true, Packages);
    SnapshotFiles(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")), false, SaveGames);
    DiffFiles(InitialPackages, Packages, ChangedPackages);
    DiffFiles(InitialSaveGames, SaveGames, ChangedSaveGames);
    return ChangedPackages.IsEmpty() && ChangedSaveGames.IsEmpty();
}

void ADiscGolfSession6OutfitVisualCaptureRunner::RestoreRuntimeState()
{
    ResumeWorldAfterThrowCapture(false);
    if (PlayerController && PlayerController->IsCharacterCreatorOpen())
    {
        PlayerController->CancelCharacterCreator();
    }
    if (ThrowAdapter && ThrowAdapter->IsThrowActive())
    {
        ThrowAdapter->RecoverInterruptedThrow();
    }
    if (Golfer)
    {
        if (bPreviewCameraActive) Golfer->EndCharacterCreatorPreview(true);
        Golfer->PreviewCharacterCreatorProfile(OpeningBody, OpeningStyle, OpeningHandedness);
        FString Status;
        Golfer->ApplyOutfitLoadoutTransactionally(OpeningOutfit, true, Status);
    }
    if (bValidationCaptureFovLocked && PlayerController
        && PlayerController->PlayerCameraManager)
    {
        PlayerController->PlayerCameraManager->SetFOV(OpeningCameraFovDeg);
        PlayerController->PlayerCameraManager->UnlockFOV();
        bValidationCaptureFovLocked = false;
    }
    if (GEngine) GEngine->ClearOnScreenDebugMessages();
}

void ADiscGolfSession6OutfitVisualCaptureRunner::WriteManifest(bool bPassed, const FString& Error)
{
    bool bCreatorViewTargetIsGolfer = Captures.Num() >= 7;
    bool bAllVisibleSkeletalOutfitMaterialsAreCanonicalMids = Captures.Num() == 12;
    bool bCanonicalOutfitMaterialHasSkeletalMeshUsage = Captures.Num() == 12;
    bool bAllSelectedVariantMaterialParametersMatch = Captures.Num() == 12;
    for (int32 CaptureIndex = 0; CaptureIndex < FMath::Min(Captures.Num(), 7); ++CaptureIndex)
    {
        bCreatorViewTargetIsGolfer &= Captures[CaptureIndex].bCreatorViewTargetIsGolfer;
    }
    for (const FCapture& Capture : Captures)
    {
        bAllVisibleSkeletalOutfitMaterialsAreCanonicalMids &=
            Capture.bVisibleSkeletalOutfitMaterialsAreCanonicalMids;
        bCanonicalOutfitMaterialHasSkeletalMeshUsage &=
            Capture.bCanonicalMaterialHasSkeletalMeshUsage;
        bAllSelectedVariantMaterialParametersMatch &=
            Capture.bSelectedVariantMaterialParametersMatch;
    }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("DiscGolfTour.Session6OutfitVisualEvidence.v2"));
    Root->SetStringField(TEXT("status"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Root->SetStringField(TEXT("error"), Error);
    Root->SetBoolField(TEXT("manual_visual_review_required"), true);
    Root->SetNumberField(TEXT("expected_capture_count"), 12);
    Root->SetNumberField(TEXT("capture_count"), Captures.Num());
    Root->SetNumberField(TEXT("validation_release_callbacks"), ValidationReleaseCallbacks);
    Root->SetNumberField(TEXT("paused_throw_capture_count"), PausedThrowCaptureCount);
    Root->SetBoolField(TEXT("creator_apply_saved"), bCreatorApplySaved);
    Root->SetBoolField(TEXT("creator_apply_reloaded"), bCreatorApplyReloaded);
    Root->SetBoolField(
        TEXT("creator_reload_reconstructed_outfit"),
        bCreatorReloadReconstructedOutfit);
    Root->SetBoolField(TEXT("validation_temp_slot_deleted"), bValidationTempSlotDeleted);
    Root->SetBoolField(
        TEXT("creator_cancel_restored_applied"), bCreatorCancelRestoredApplied);
    Root->SetBoolField(
        TEXT("creator_view_target_is_golfer"), bCreatorViewTargetIsGolfer);
    Root->SetBoolField(
        TEXT("all_visible_skeletal_outfit_materials_are_canonical_mids"),
        bAllVisibleSkeletalOutfitMaterialsAreCanonicalMids);
    Root->SetBoolField(
        TEXT("canonical_outfit_material_has_skeletal_mesh_usage"),
        bCanonicalOutfitMaterialHasSkeletalMeshUsage);
    Root->SetBoolField(
        TEXT("all_selected_variant_material_parameters_match"),
        bAllSelectedVariantMaterialParametersMatch);
    Root->SetStringField(
        TEXT("canonical_outfit_material"),
        Session6CanonicalOutfitMaterialObjectPath);
    Root->SetNumberField(TEXT("world_disc_delta"), CountWorldDiscs() - BaselineDiscs);
    Root->SetNumberField(TEXT("stroke_delta"), GameMode ? GameMode->GetStrokes() - BaselineStrokes : -1);
    TArray<TSharedPtr<FJsonValue>> CaptureJson;
    for (const FCapture& Capture : Captures)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("filename"), Capture.Filename);
        Object->SetStringField(TEXT("evidence"), Capture.Evidence);
        Object->SetStringField(TEXT("sha1"), Capture.Sha1);
        Object->SetNumberField(TEXT("bytes"), static_cast<double>(Capture.Bytes));
        Object->SetNumberField(TEXT("width"), Capture.Width);
        Object->SetNumberField(TEXT("height"), Capture.Height);
        Object->SetNumberField(TEXT("expected_equipped_count"), Capture.ExpectedEquippedCount);
        Object->SetNumberField(TEXT("visible_equipped_component_count"), Capture.VisibleEquippedComponentCount);
        Object->SetNumberField(
            TEXT("expected_skeletal_outfit_component_count"),
            Capture.ExpectedSkeletalOutfitComponentCount);
        Object->SetNumberField(
            TEXT("visible_skeletal_outfit_component_count"),
            Capture.VisibleSkeletalOutfitComponentCount);
        Object->SetNumberField(
            TEXT("expected_skeletal_material_slot_count"),
            Capture.ExpectedSkeletalMaterialSlotCount);
        Object->SetNumberField(
            TEXT("canonical_material_mid_count"),
            Capture.CanonicalMaterialMidCount);
        Object->SetNumberField(
            TEXT("skeletal_usage_ready_mid_count"),
            Capture.SkeletalUsageReadyMidCount);
        Object->SetNumberField(
            TEXT("variant_parameter_matched_mid_count"),
            Capture.VariantParameterMatchedMidCount);
        Object->SetNumberField(TEXT("projected_key_bone_count"), Capture.ProjectedKeyBoneCount);
        Object->SetNumberField(TEXT("key_bones_inside_viewport"), Capture.KeyBonesInsideViewport);
        Object->SetNumberField(TEXT("key_bones_inside_safe_margin"), Capture.KeyBonesInsideSafeMargin);
        Object->SetNumberField(TEXT("subject_screen_height_fraction"), Capture.SubjectScreenHeightFraction);
        Object->SetNumberField(TEXT("subject_screen_width_fraction"), Capture.SubjectScreenWidthFraction);
        Object->SetNumberField(TEXT("projected_bounds_min_x"), Capture.ProjectedBoundsMin.X);
        Object->SetNumberField(TEXT("projected_bounds_min_y"), Capture.ProjectedBoundsMin.Y);
        Object->SetNumberField(TEXT("projected_bounds_max_x"), Capture.ProjectedBoundsMax.X);
        Object->SetNumberField(TEXT("projected_bounds_max_y"), Capture.ProjectedBoundsMax.Y);
        Object->SetNumberField(TEXT("montage_position_seconds"), Capture.MontagePositionSeconds);
        Object->SetNumberField(TEXT("body_height_cm"), Capture.BodyHeightCm);
        Object->SetNumberField(TEXT("camera_fov_deg"), Capture.CameraFovDeg);
        Object->SetStringField(TEXT("throw_phase"), Capture.ThrowPhase);
        Object->SetBoolField(TEXT("body_framed"), Capture.bBodyFramed);
        Object->SetBoolField(TEXT("finite_pose"), Capture.bFinitePose);
        Object->SetBoolField(TEXT("outfit_components_visible"), Capture.bOutfitComponentsVisible);
        Object->SetBoolField(
            TEXT("visible_skeletal_outfit_materials_are_canonical_mids"),
            Capture.bVisibleSkeletalOutfitMaterialsAreCanonicalMids);
        Object->SetBoolField(
            TEXT("canonical_material_has_skeletal_mesh_usage"),
            Capture.bCanonicalMaterialHasSkeletalMeshUsage);
        Object->SetBoolField(
            TEXT("selected_variant_material_parameters_match"),
            Capture.bSelectedVariantMaterialParametersMatch);
        Object->SetBoolField(TEXT("creator_outfit_tab_required"), Capture.bCreatorOutfitTabRequired);
        Object->SetBoolField(TEXT("creator_outfit_tab_prepared"), Capture.bCreatorOutfitTabPrepared);
        Object->SetBoolField(
            TEXT("creator_view_target_is_golfer"),
            Capture.bCreatorViewTargetIsGolfer);
        Object->SetBoolField(
            TEXT("creator_preview_composed_in_right_pane"),
            Capture.bCreatorPreviewComposedInRightPane);
        TArray<TSharedPtr<FJsonValue>> VariantJson;
        for (const FString& Variant : Capture.SelectedSkeletalVariants)
        {
            VariantJson.Add(MakeShared<FJsonValueString>(Variant));
        }
        Object->SetArrayField(TEXT("selected_skeletal_variants"), VariantJson);
        TArray<TSharedPtr<FJsonValue>> MaterialDiagnosticJson;
        for (const FString& Diagnostic : Capture.SkeletalMaterialDiagnostics)
        {
            MaterialDiagnosticJson.Add(MakeShared<FJsonValueString>(Diagnostic));
        }
        Object->SetArrayField(
            TEXT("skeletal_material_diagnostics"), MaterialDiagnosticJson);
        TArray<TSharedPtr<FJsonValue>> BoneJson;
        for (const FCapture::FKeyBoneProjection& Projection : Capture.KeyBoneProjections)
        {
            TSharedRef<FJsonObject> BoneObject = MakeShared<FJsonObject>();
            BoneObject->SetStringField(TEXT("bone"), Projection.Bone);
            BoneObject->SetNumberField(TEXT("screen_x"), Projection.Screen.X);
            BoneObject->SetNumberField(TEXT("screen_y"), Projection.Screen.Y);
            BoneObject->SetBoolField(TEXT("projected"), Projection.bProjected);
            BoneObject->SetBoolField(TEXT("inside_viewport"), Projection.bInsideViewport);
            BoneObject->SetBoolField(TEXT("inside_safe_margin"), Projection.bInsideSafeMargin);
            BoneJson.Add(MakeShared<FJsonValueObject>(BoneObject));
        }
        Object->SetArrayField(TEXT("key_bone_projections"), BoneJson);
        CaptureJson.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("captures"), CaptureJson);
    TArray<TSharedPtr<FJsonValue>> PackageJson;
    for (const FString& Path : ChangedPackages) PackageJson.Add(MakeShared<FJsonValueString>(Path));
    TArray<TSharedPtr<FJsonValue>> SaveJson;
    for (const FString& Path : ChangedSaveGames) SaveJson.Add(MakeShared<FJsonValueString>(Path));
    Root->SetArrayField(TEXT("changed_packages"), PackageJson);
    Root->SetArrayField(TEXT("changed_save_games"), SaveJson);
    Root->SetBoolField(TEXT("no_persistent_writes"), bPersistentSnapshotReady
        && ChangedPackages.IsEmpty() && ChangedSaveGames.IsEmpty());
    Root->SetStringField(TEXT("allowed_outputs"), TEXT("Twelve PNGs and this manifest under Saved/CharacterFramework/Screenshots/Session6_OutfitCustomization only."));
    FString Json;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    FFileHelper::SaveStringToFile(Json, *ManifestPath);
}

void ADiscGolfSession6OutfitVisualCaptureRunner::Fail(const FString& Reason)
{
    if (bFinished) return;
    bFinished = true;
    if (bPersistentSnapshotReady) VerifyNoPersistentWrites();
    RestoreRuntimeState();
    WriteManifest(false, Reason);
    UE_LOG(LogDiscGolfTour, Error, TEXT("DG_SESSION6_OUTFIT_VISUAL_CAPTURE: FAIL %s manifest=%s"), *Reason, *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession6OutfitVisualCaptureRunner::Pass()
{
    if (bFinished) return;
    bool bCapturesValid = Captures.Num() == 12;
    for (int32 CaptureIndex = 0; CaptureIndex < Captures.Num(); ++CaptureIndex)
    {
        const FCapture& Capture = Captures[CaptureIndex];
        const bool bPhaseExact = CaptureIndex == 10
            ? Capture.ThrowPhase == TEXT("Release")
                && Capture.MontagePositionSeconds >= 1.52f
                && Capture.MontagePositionSeconds <= 1.68f
            : CaptureIndex == 11
                ? Capture.ThrowPhase == TEXT("FollowThrough")
                    && Capture.MontagePositionSeconds >= 1.68f
                    && Capture.MontagePositionSeconds <= 2.20f
                : Capture.ThrowPhase == TEXT("Idle");
        bCapturesValid &= Capture.Width == Session6ExpectedWidth
            && Capture.Height == Session6ExpectedHeight
            && Capture.Bytes >= Session6MinimumPngBytes
            && Capture.Sha1.Len() == FSHAHash::GetStringLen()
            && Capture.bBodyFramed
            && Capture.bFinitePose
            && Capture.ProjectedKeyBoneCount == 6
            && Capture.KeyBonesInsideViewport == 6
            && Capture.KeyBonesInsideSafeMargin >= 4
            && FMath::IsNearlyEqual(
                Capture.CameraFovDeg, Session6ValidationCaptureFovDeg, 0.1f)
            && Capture.bOutfitComponentsVisible
            && Capture.ExpectedEquippedCount == Capture.VisibleEquippedComponentCount
            && Capture.bVisibleSkeletalOutfitMaterialsAreCanonicalMids
            && Capture.bCanonicalMaterialHasSkeletalMeshUsage
            && Capture.bSelectedVariantMaterialParametersMatch
            && Capture.VisibleSkeletalOutfitComponentCount
                == Capture.ExpectedSkeletalOutfitComponentCount
            && Capture.CanonicalMaterialMidCount
                == Capture.ExpectedSkeletalMaterialSlotCount
            && Capture.SkeletalUsageReadyMidCount
                == Capture.ExpectedSkeletalMaterialSlotCount
            && Capture.VariantParameterMatchedMidCount
                == Capture.ExpectedSkeletalMaterialSlotCount
            && (!Capture.bCreatorOutfitTabRequired || Capture.bCreatorOutfitTabPrepared)
            && (!Capture.bCreatorOutfitTabRequired
                || Capture.bCreatorViewTargetIsGolfer)
            && (!Capture.bCreatorOutfitTabRequired
                || Capture.bCreatorPreviewComposedInRightPane)
            && bPhaseExact;
    }
    const bool bIsolated = ValidationReleaseCallbacks == 1
        && PausedThrowCaptureCount == 2
        && CountWorldDiscs() == BaselineDiscs
        && GameMode && GameMode->GetStrokes() == BaselineStrokes
        && !UGameplayStatics::IsGamePaused(this);
    const bool bProfileOrdering = Captures.Num() == 12
        && Captures[7].BodyHeightCm < Captures[8].BodyHeightCm
        && Captures[8].BodyHeightCm < Captures[9].BodyHeightCm;
    const bool bNoWrites = VerifyNoPersistentWrites();
    const bool bCreatorPersistence = bCreatorApplySaved
        && bCreatorApplyReloaded
        && bCreatorReloadReconstructedOutfit
        && bValidationTempSlotDeleted
        && bCreatorCancelRestoredApplied;
    if (!bCapturesValid || !bIsolated || !bProfileOrdering
        || !bNoWrites || !bCreatorWasOpened || !bCreatorPersistence)
    {
        Fail(TEXT("final 12-frame, one-release, gameplay-isolation, creator, or no-write invariant failed"));
        return;
    }
    bFinished = true;
    RestoreRuntimeState();
    WriteManifest(true, TEXT(""));
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION6_OUTFIT_VISUAL_CAPTURE: PASS captures=12 canonical_skeletal_material_mids=1 selected_variant_parameters_match=1 creator=1 creator_view_target_is_golfer=1 creator_apply_saved=1 creator_apply_reloaded=1 creator_reload_reconstructed_outfit=1 validation_temp_slot_deleted=1 creator_cancel_restored_applied=1 profiles=3 release_callback=1 gameplay_discs=0 strokes=0 package_writes=0 save_writes=0 manifest=%s"),
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

void ADiscGolfSession6OutfitVisualCaptureRunner::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (!bFinished) RestoreRuntimeState();
    Super::EndPlay(EndPlayReason);
}
