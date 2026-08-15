#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DiscGolfSettingsTypes.h"
#include "DiscGolfSettingsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnSettingsChanged,
    FDGUserSettingsProfile,
    Settings
);

UCLASS(Blueprintable, BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfSettingsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Settings")
    FDGUserSettingsProfile CurrentSettings;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Settings")
    FDGOnSettingsChanged OnSettingsChanged;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Settings")
    void ApplySettings(const FDGUserSettingsProfile& NewSettings);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Settings")
    void ResetToDefaults();

    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|Settings")
    void ApplySettingsToProject(const FDGUserSettingsProfile& Settings);
};
