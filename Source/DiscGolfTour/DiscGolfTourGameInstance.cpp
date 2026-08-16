#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTour.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfPlayerExperience.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"

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

    if (DiscGolfSaveSchema::IsCurrent(InOutProfile.SaveSchemaVersion))
    {
        InOutProfile.PlayerSettings.Normalize();
        InOutProfile.CharacterProfile.Sanitize();
    }
    return bMigrated ? EMigrationResult::Migrated : EMigrationResult::AlreadyCurrent;
}

void UDiscGolfTourGameInstance::Init()
{
    Super::Init();

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
        !CharacterProfilesExactlyMatch(CharacterProfileBeforeMigration, Profile->CharacterProfile))
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

    const FDiscGolfCharacterProfileSaveData Previous = Profile->CharacterProfile;
    Profile->CharacterProfile = CharacterProfile;
    Profile->CharacterProfile.Sanitize();
    if (!SaveProfileInternal())
    {
        Profile->CharacterProfile = Previous;
        return false;
    }
    return true;
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
