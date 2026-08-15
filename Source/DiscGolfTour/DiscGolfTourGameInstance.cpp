#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTour.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfPlayerExperience.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"

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

    if (Profile && Profile->SaveSchemaVersion > DiscGolfSaveSchema::CurrentVersion)
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Profile schema %d is newer than supported schema %d; leaving it untouched."),
            Profile->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
        return;
    }

    bool bProfileMigrated = false;
    if (Profile && Profile->SaveSchemaVersion < 4)
    {
        if (Profile->PracticeCourseId.IsNone()) Profile->PracticeCourseId = TEXT("RegressionCourse");
        if (Profile->PracticeLayoutId.IsNone()) Profile->PracticeLayoutId = TEXT("Practice");
        if (Profile->PracticeHoleNumber <= 0) Profile->PracticeHoleNumber = 1;
        bProfileMigrated = true;
    }
    if (Profile && Profile->SaveSchemaVersion < 5)
    {
        Profile->PracticeMoldId = TEXT("Apex");
        Profile->PracticePlastic = EDiscPlastic::Tour;
        Profile->SaveSchemaVersion = 5;
        bProfileMigrated = true;
    }
    if (Profile && Profile->SaveSchemaVersion < DiscGolfSaveSchema::CurrentVersion)
    {
        Profile->PlayerSettings.GraphicsQuality = FMath::Clamp(Profile->PreferredGraphicsPreset, 0, 3);
        Profile->PlayerSettings.Normalize();
        Profile->SaveSchemaVersion = DiscGolfSaveSchema::CurrentVersion;
        bProfileMigrated = true;
    }
    if (Profile && DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        Profile->PlayerSettings.Normalize();
    }
    if (bProfileMigrated)
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

void UDiscGolfTourGameInstance::SaveProfile()
{
    if (Profile && DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        UGameplayStatics::SaveGameToSlot(Profile, SaveSlot, 0);
    }
}
