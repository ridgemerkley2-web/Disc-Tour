#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTour.h"
#include "DiscGolfSaveGame.h"
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

    if (DiscGolfSaveSchema::IsCurrent(InOutProfile.SaveSchemaVersion))
    {
        InOutProfile.PlayerSettings.Normalize();
        InOutProfile.CharacterProfile.Sanitize();
        InOutProfile.OutfitLoadout = DiscGolfOutfitRuntime::NormalizeForPersistence(
            InOutProfile.OutfitLoadout);
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
    constexpr int32 MaximumSuffixLength = 48;
    if (!RequestedSlot.StartsWith(RequiredPrefix, ESearchCase::CaseSensitive))
    {
        return false;
    }

    const FString Suffix = RequestedSlot.RightChop(UE_ARRAY_COUNT(RequiredPrefix) - 1);
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

    OutSaveSlot = MoveTemp(RequestedSlot);
    return true;
}

void UDiscGolfTourGameInstance::Init()
{
    Super::Init();

    FString RequestedValidationSlot;
    const bool bValidationSlotWasRequested = FParse::Value(
        FCommandLine::Get(),
        TEXT("Session6OutfitValidationSaveSlot="),
        RequestedValidationSlot);
    FString ValidatedValidationSlot;
    if (DiscGolfProfilePersistence::TryResolveSession6OutfitValidationSaveSlot(
            FCommandLine::Get(), ValidatedValidationSlot))
    {
        SaveSlot = MoveTemp(ValidatedValidationSlot);
        bUsingSession6OutfitValidationSaveSlot = true;
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("SESSION 6 OUTFIT VALIDATION SAVE SLOT: %s (validation-only; production slot unchanged)."),
            *SaveSlot);
    }
    else if (bValidationSlotWasRequested)
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
            OutfitBeforeMigration, Profile->OutfitLoadout))
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

    FDiscGolfCharacterProfileSaveData Result = Profile->CharacterProfile;
    Result.Sanitize();
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

    return UpdateCharacterProfileAndOutfit(CharacterProfile, Profile->OutfitLoadout);
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

    const FDGBodyBuildProfile ExistingBuild = Profile->CharacterProfile.ToBodyBuildProfile();
    return UpdateCharacterProfile(FDiscGolfCharacterProfileSaveData::FromFramework(
        Body, ThrowStyle, Handedness, ExistingBuild));
}

FDGOutfitLoadout UDiscGolfTourGameInstance::GetOutfitLoadout() const
{
    return Profile && DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion)
        ? DiscGolfOutfitRuntime::NormalizeForPersistence(Profile->OutfitLoadout)
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

    const FDiscGolfCharacterProfileSaveData PreviousCharacter = Profile->CharacterProfile;
    const FDGOutfitLoadout PreviousOutfit = Profile->OutfitLoadout;
    Profile->CharacterProfile = CharacterProfile;
    Profile->CharacterProfile.Sanitize();
    Profile->OutfitLoadout = DiscGolfOutfitRuntime::NormalizeForPersistence(OutfitLoadout);
    if (!SaveProfileInternal())
    {
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
