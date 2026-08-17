#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTour.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfPlayerExperience.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
bool CharacterProfilesExactlyMatch(
    const FDiscGolfCharacterProfileSaveData& A,
    const FDiscGolfCharacterProfileSaveData& B)
{
    return A.bLeftHanded == B.bLeftHanded
        && A.HeightCm == B.HeightCm
        && A.WingspanScale == B.WingspanScale
        && A.ShoulderWidthScale == B.ShoulderWidthScale
        && A.TorsoLengthScale == B.TorsoLengthScale
        && A.LegLengthScale == B.LegLengthScale
        && A.HandScale == B.HandScale
        && A.MassKg == B.MassKg
        && A.Muscularity == B.Muscularity
        && A.BodyFat == B.BodyFat
        && A.Chest == B.Chest
        && A.Waist == B.Waist
        && A.Hips == B.Hips
        && A.Arms == B.Arms
        && A.Legs == B.Legs
        && A.RunUpIntensity == B.RunUpIntensity
        && A.ReachBackAmount == B.ReachBackAmount
        && A.TorsoRotation == B.TorsoRotation
        && A.BraceIntensity == B.BraceIntensity
        && A.Explosiveness == B.Explosiveness
        && A.FollowThrough == B.FollowThrough;
}

bool OutfitLoadoutsExactlyMatch(
    const FDGOutfitLoadout& A,
    const FDGOutfitLoadout& B)
{
    if (A.Equipped.Num() != B.Equipped.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < A.Equipped.Num(); ++Index)
    {
        const FDGEquippedOutfitEntry& EntryA = A.Equipped[Index];
        const FDGEquippedOutfitEntry& EntryB = B.Equipped[Index];
        if (EntryA.Slot != EntryB.Slot
            || EntryA.ItemId != EntryB.ItemId
            || EntryA.VariantId != EntryB.VariantId)
        {
            return false;
        }
    }
    return true;
}

bool IsSafeValidationSlotSuffix(const FString& Suffix)
{
    constexpr int32 MaximumSuffixLength = 48;
    if (Suffix.IsEmpty() || Suffix.Len() > MaximumSuffixLength)
    {
        return false;
    }
    for (const TCHAR Character : Suffix)
    {
        const bool bSafeAscii = (Character >= TEXT('A') && Character <= TEXT('Z'))
            || (Character >= TEXT('a') && Character <= TEXT('z'))
            || (Character >= TEXT('0') && Character <= TEXT('9'))
            || Character == TEXT('_');
        if (!bSafeAscii)
        {
            return false;
        }
    }
    return true;
}
}

DiscGolfProfilePersistence::EMigrationResult DiscGolfProfilePersistence::MigrateToCurrent(
    UDiscGolfSaveGame& InOutProfile)
{
    if (InOutProfile.SaveSchemaVersion > DiscGolfSaveSchema::CurrentVersion)
    {
        return EMigrationResult::FutureSchemaRejected;
    }

    bool bMigrated = false;
    if (InOutProfile.SaveSchemaVersion < 4)
    {
        if (InOutProfile.PracticeCourseId.IsNone())
        {
            InOutProfile.PracticeCourseId = TEXT("RegressionCourse");
        }
        if (InOutProfile.PracticeLayoutId.IsNone())
        {
            InOutProfile.PracticeLayoutId = TEXT("Practice");
        }
        if (InOutProfile.PracticeHoleNumber <= 0)
        {
            InOutProfile.PracticeHoleNumber = 1;
        }
        bMigrated = true;
    }
    if (InOutProfile.SaveSchemaVersion < 5)
    {
        InOutProfile.PracticeMoldId = TEXT("Apex");
        InOutProfile.PracticePlastic = EDiscPlastic::Tour;
        InOutProfile.SaveSchemaVersion = 5;
        bMigrated = true;
    }
    if (InOutProfile.SaveSchemaVersion < 6)
    {
        InOutProfile.PlayerSettings.GraphicsQuality = FMath::Clamp(
            InOutProfile.PreferredGraphicsPreset, 0, 3);
        InOutProfile.PlayerSettings.Normalize();
        InOutProfile.SaveSchemaVersion = 6;
        bMigrated = true;
    }
    if (InOutProfile.SaveSchemaVersion < 7)
    {
        // Save archives created before Session 4 leave this newly introduced
        // value at its constructor defaults. Sanitize also makes synthetic or
        // partially reconstructed legacy fixtures deterministic.
        InOutProfile.CharacterProfile.Sanitize();
        InOutProfile.SaveSchemaVersion = 7;
        bMigrated = true;
    }
    if (InOutProfile.SaveSchemaVersion < 8)
    {
        // Schema 7 predates modular outfits. The new SaveGame property loads
        // as an empty array, but reset it explicitly to make migration and
        // synthetic legacy fixtures deterministic.
        InOutProfile.OutfitLoadout.Equipped.Reset();
        InOutProfile.SaveSchemaVersion = 8;
        bMigrated = true;
    }
    if (InOutProfile.SaveSchemaVersion < 9)
    {
        // Schema 8 stored body/build/throw/handedness and outfit in the two
        // legacy properties below. Populate every new identity/face/hair/
        // appearance field from deterministic framework defaults, then copy
        // every accepted Session 4/6 value without consulting display labels
        // or content assets. A synthetic pre-v9 value in the new property is
        // overwritten so legacy archives cannot smuggle non-schema data.
        FDiscGolfCharacterProfileSaveData LegacyCharacter =
            InOutProfile.CharacterProfile;
        LegacyCharacter.Sanitize();
        FDGFullCharacterCustomization Migrated =
            DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
        Migrated.Identity.Handedness = LegacyCharacter.GetHandedness();
        Migrated.Body = LegacyCharacter.ToBodyProfile();
        Migrated.BodyBuild = LegacyCharacter.ToBodyBuildProfile();
        Migrated.ThrowStyle = LegacyCharacter.ToThrowStyle();
        Migrated.Outfit = DiscGolfOutfitRuntime::NormalizeForPersistence(
            InOutProfile.OutfitLoadout);
        DiscGolfFullCharacterRuntime::NormalizeForPersistence(Migrated);
        InOutProfile.CharacterCustomization = MoveTemp(Migrated);
        InOutProfile.SaveSchemaVersion = 9;
        bMigrated = true;
    }

    if (DiscGolfSaveSchema::IsCurrent(InOutProfile.SaveSchemaVersion))
    {
        InOutProfile.PlayerSettings.Normalize();
        DiscGolfFullCharacterRuntime::NormalizeForPersistence(
            InOutProfile.CharacterCustomization);

        // Compatibility mirrors keep accepted Session 4/6 readers and
        // isolated regression fixtures working. They are never a schema-9
        // read authority and are updated only from the complete payload.
        InOutProfile.CharacterProfile =
            FDiscGolfCharacterProfileSaveData::FromFramework(
                InOutProfile.CharacterCustomization.Body,
                InOutProfile.CharacterCustomization.ThrowStyle,
                InOutProfile.CharacterCustomization.Identity.Handedness,
                InOutProfile.CharacterCustomization.BodyBuild);
        InOutProfile.OutfitLoadout =
            DiscGolfOutfitRuntime::NormalizeForPersistence(
                InOutProfile.CharacterCustomization.Outfit);
    }
    return bMigrated ? EMigrationResult::Migrated : EMigrationResult::AlreadyCurrent;
}

bool DiscGolfProfilePersistence::TryResolveSession6OutfitValidationSaveSlot(
    const TCHAR* CommandLine,
    FString& OutSaveSlot)
{
    OutSaveSlot.Reset();
    if (!CommandLine
        || !FParse::Param(CommandLine, TEXT("Session6OutfitVisualCapture"))
        || !FParse::Param(CommandLine, TEXT("Session6OutfitValidationNoSave")))
    {
        return false;
    }

    FString RequestedSlot;
    if (!FParse::Value(
            CommandLine,
            TEXT("Session6OutfitValidationSaveSlot="),
            RequestedSlot))
    {
        return false;
    }

    constexpr TCHAR RequiredPrefix[] =
        TEXT("DiscGolfTour_Automation_Session6Outfit_");
    if (!RequestedSlot.StartsWith(RequiredPrefix, ESearchCase::CaseSensitive))
    {
        return false;
    }

    const FString Suffix = RequestedSlot.RightChop(UE_ARRAY_COUNT(RequiredPrefix) - 1);
    if (!IsSafeValidationSlotSuffix(Suffix)) return false;

    OutSaveSlot = MoveTemp(RequestedSlot);
    return true;
}

bool DiscGolfProfilePersistence::TryResolveSession7FullCharacterValidationSaveSlot(
    const TCHAR* CommandLine,
    FString& OutSaveSlot)
{
    OutSaveSlot.Reset();
    if (!CommandLine
        || !FParse::Param(CommandLine, TEXT("Session7FullCharacterValidationNoSave")))
    {
        return false;
    }
    const bool bVisual = FParse::Param(
        CommandLine, TEXT("Session7FullCharacterVisualCapture"));
    const bool bThrow = FParse::Param(
        CommandLine, TEXT("Session7FullCharacterThrowSmokeTest"));
    if (bVisual == bThrow)
    {
        return false;
    }

    FString RequestedSlot;
    if (!FParse::Value(
            CommandLine,
            TEXT("Session7FullCharacterValidationSaveSlot="),
            RequestedSlot))
    {
        return false;
    }
    constexpr TCHAR RequiredPrefix[] =
        TEXT("DiscGolfTour_Automation_Session7FullCharacter_");
    if (!RequestedSlot.StartsWith(RequiredPrefix, ESearchCase::CaseSensitive))
    {
        return false;
    }
    const FString Suffix = RequestedSlot.RightChop(
        UE_ARRAY_COUNT(RequiredPrefix) - 1);
    if (!IsSafeValidationSlotSuffix(Suffix))
    {
        return false;
    }
    OutSaveSlot = MoveTemp(RequestedSlot);
    return true;
}

void UDiscGolfTourGameInstance::Init()
{
    Super::Init();

    FString RequestedSession7Slot;
    const bool bSession7ValidationSlotWasRequested = FParse::Value(
        FCommandLine::Get(),
        TEXT("Session7FullCharacterValidationSaveSlot="),
        RequestedSession7Slot);
    FString ValidatedSession7Slot;
    if (DiscGolfProfilePersistence::TryResolveSession7FullCharacterValidationSaveSlot(
            FCommandLine::Get(), ValidatedSession7Slot))
    {
        SaveSlot = MoveTemp(ValidatedSession7Slot);
        bUsingSession7FullCharacterValidationSaveSlot = true;
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("SESSION 7 FULL CHARACTER VALIDATION SAVE SLOT: %s (validation-only; production slot unchanged)."),
            *SaveSlot);
    }
    else if (bSession7ValidationSlotWasRequested)
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Rejected unsafe or incompletely gated Session 7 full-character validation save slot; retaining the production profile slot."));
    }

    FString RequestedValidationSlot;
    const bool bValidationSlotWasRequested = FParse::Value(
        FCommandLine::Get(),
        TEXT("Session6OutfitValidationSaveSlot="),
        RequestedValidationSlot);
    FString ValidatedValidationSlot;
    if (!bUsingSession7FullCharacterValidationSaveSlot
        && DiscGolfProfilePersistence::TryResolveSession6OutfitValidationSaveSlot(
            FCommandLine::Get(), ValidatedValidationSlot))
    {
        SaveSlot = MoveTemp(ValidatedValidationSlot);
        bUsingSession6OutfitValidationSaveSlot = true;
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("SESSION 6 OUTFIT VALIDATION SAVE SLOT: %s (validation-only; production slot unchanged)."),
            *SaveSlot);
    }
    else if (!bUsingSession7FullCharacterValidationSaveSlot
        && bValidationSlotWasRequested)
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Rejected unsafe or incompletely gated Session 6 outfit validation save slot; retaining the production profile slot."));
    }

    if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SaveSlot, 0))
    {
        Profile = Cast<UDiscGolfSaveGame>(Loaded);
    }

    if (!Profile)
    {
        Profile = Cast<UDiscGolfSaveGame>(UGameplayStatics::CreateSaveGameObject(UDiscGolfSaveGame::StaticClass()));
        if (Profile)
        {
            Profile->UnlockedMolds = { TEXT("Apex"), TEXT("Vector"), TEXT("Line"), TEXT("Compass"), TEXT("Touch") };
        }
    }

    if (!Profile)
    {
        return;
    }

    const FDiscGolfCharacterProfileSaveData CharacterProfileBeforeMigration =
        Profile->CharacterProfile;
    const FDGOutfitLoadout OutfitBeforeMigration = Profile->OutfitLoadout;
    const FDGFullCharacterCustomization FullCharacterBeforeMigration =
        Profile->CharacterCustomization;
    const DiscGolfProfilePersistence::EMigrationResult MigrationResult =
        DiscGolfProfilePersistence::MigrateToCurrent(*Profile);
    if (MigrationResult == DiscGolfProfilePersistence::EMigrationResult::FutureSchemaRejected)
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Profile schema %d is newer than supported schema %d; leaving it untouched."),
            Profile->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
        return;
    }
    if (MigrationResult == DiscGolfProfilePersistence::EMigrationResult::Migrated ||
        !CharacterProfilesExactlyMatch(CharacterProfileBeforeMigration, Profile->CharacterProfile) ||
        !OutfitLoadoutsExactlyMatch(
            OutfitBeforeMigration, Profile->OutfitLoadout) ||
        !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            FullCharacterBeforeMigration, Profile->CharacterCustomization))
    {
        SaveProfile();
    }
}

FDiscGolfPlayerSettings UDiscGolfTourGameInstance::GetPlayerSettings() const
{
    return Profile ? Profile->PlayerSettings : FDiscGolfPlayerSettings();
}

void UDiscGolfTourGameInstance::UpdatePlayerSettings(const FDiscGolfPlayerSettings& Settings)
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion)) return;
    Profile->PlayerSettings = Settings;
    Profile->PlayerSettings.Normalize();
    Profile->PreferredGraphicsPreset = Profile->PlayerSettings.GraphicsQuality;
    if (GEngine && GEngine->GetGameUserSettings())
    {
        UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
        UserSettings->SetOverallScalabilityLevel(Profile->PlayerSettings.GraphicsQuality);
        UserSettings->SetScreenResolution(FIntPoint(
            Profile->PlayerSettings.ResolutionX, Profile->PlayerSettings.ResolutionY));
        const EWindowMode::Type Modes[] = {
            EWindowMode::Fullscreen,
            EWindowMode::WindowedFullscreen,
            EWindowMode::Windowed
        };
        UserSettings->SetFullscreenMode(Modes[FMath::Clamp(Profile->PlayerSettings.WindowMode, 0, 2)]);
        UserSettings->ApplySettings(false);
        UserSettings->SaveSettings();
    }
    SaveProfile();
}

FDiscGolfCharacterProfileSaveData UDiscGolfTourGameInstance::GetCharacterProfile() const
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return FDiscGolfCharacterProfileSaveData();
    }

    const FDGFullCharacterCustomization Full = GetFullCharacterCustomization();
    FDiscGolfCharacterProfileSaveData Result =
        FDiscGolfCharacterProfileSaveData::FromFramework(
            Full.Body,
            Full.ThrowStyle,
            Full.Identity.Handedness,
            Full.BodyBuild);
    Result.Sanitize();
    return Result;
}

FDGFullCharacterCustomization UDiscGolfTourGameInstance::GetFullCharacterCustomization() const
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    }
    FDGFullCharacterCustomization Result = Profile->CharacterCustomization;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Result);
    return Result;
}

bool UDiscGolfTourGameInstance::GetCharacterProfile(
    FDGBodyProfile& OutBody,
    FDGThrowStyle& OutThrowStyle,
    EDGHandedness& OutHandedness) const
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return false;
    }

    const FDiscGolfCharacterProfileSaveData Saved = GetCharacterProfile();
    OutBody = Saved.ToBodyProfile();
    OutThrowStyle = Saved.ToThrowStyle();
    OutHandedness = Saved.GetHandedness();
    return true;
}

bool UDiscGolfTourGameInstance::UpdateCharacterProfile(
    const FDiscGolfCharacterProfileSaveData& CharacterProfile)
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return false;
    }

    return UpdateCharacterProfileAndOutfit(
        CharacterProfile, GetFullCharacterCustomization().Outfit);
}

bool UDiscGolfTourGameInstance::UpdateCharacterProfile(
    const FDGBodyProfile& Body,
    const FDGThrowStyle& ThrowStyle,
    EDGHandedness Handedness)
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return false;
    }

    const FDGBodyBuildProfile ExistingBuild =
        GetFullCharacterCustomization().BodyBuild;
    return UpdateCharacterProfile(FDiscGolfCharacterProfileSaveData::FromFramework(
        Body, ThrowStyle, Handedness, ExistingBuild));
}

FDGOutfitLoadout UDiscGolfTourGameInstance::GetOutfitLoadout() const
{
    return Profile && DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion)
        ? DiscGolfOutfitRuntime::NormalizeForPersistence(
            GetFullCharacterCustomization().Outfit)
        : FDGOutfitLoadout();
}

bool UDiscGolfTourGameInstance::UpdateCharacterProfileAndOutfit(
    const FDiscGolfCharacterProfileSaveData& CharacterProfile,
    const FDGOutfitLoadout& OutfitLoadout)
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return false;
    }

    FDiscGolfCharacterProfileSaveData Safe = CharacterProfile;
    Safe.Sanitize();
    FDGFullCharacterCustomization Full = GetFullCharacterCustomization();
    Full.Identity.Handedness = Safe.GetHandedness();
    Full.Body = Safe.ToBodyProfile();
    Full.BodyBuild = Safe.ToBodyBuildProfile();
    Full.ThrowStyle = Safe.ToThrowStyle();
    Full.Outfit = DiscGolfOutfitRuntime::NormalizeForPersistence(OutfitLoadout);
    return UpdateFullCharacterCustomization(Full);
}

bool UDiscGolfTourGameInstance::UpdateFullCharacterCustomization(
    const FDGFullCharacterCustomization& CharacterCustomization)
{
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return false;
    }

    const FDGFullCharacterCustomization PreviousFull =
        Profile->CharacterCustomization;
    const FDiscGolfCharacterProfileSaveData PreviousCharacter =
        Profile->CharacterProfile;
    const FDGOutfitLoadout PreviousOutfit = Profile->OutfitLoadout;

    Profile->CharacterCustomization = CharacterCustomization;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(
        Profile->CharacterCustomization);
    Profile->CharacterProfile = FDiscGolfCharacterProfileSaveData::FromFramework(
        Profile->CharacterCustomization.Body,
        Profile->CharacterCustomization.ThrowStyle,
        Profile->CharacterCustomization.Identity.Handedness,
        Profile->CharacterCustomization.BodyBuild);
    Profile->OutfitLoadout = DiscGolfOutfitRuntime::NormalizeForPersistence(
        Profile->CharacterCustomization.Outfit);
    if (!SaveProfileInternal())
    {
        Profile->CharacterCustomization = PreviousFull;
        Profile->CharacterProfile = PreviousCharacter;
        Profile->OutfitLoadout = PreviousOutfit;
        return false;
    }
    return true;
}

void UDiscGolfTourGameInstance::SaveProfile()
{
    SaveProfileInternal();
}

bool UDiscGolfTourGameInstance::SaveProfileInternal()
{
    return Profile
        && DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion)
        && UGameplayStatics::SaveGameToSlot(Profile, SaveSlot, 0);
}
