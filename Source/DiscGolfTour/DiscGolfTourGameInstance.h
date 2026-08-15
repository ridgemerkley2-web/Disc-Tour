#pragma once

#include "CoreMinimal.h"
#include "DiscGolfPlayerExperience.h"
#include "Engine/GameInstance.h"
#include "DiscGolfTourGameInstance.generated.h"

class UDiscGolfSaveGame;

UCLASS()
class DISCGOLFTOUR_API UDiscGolfTourGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    UFUNCTION(BlueprintCallable) void SaveProfile();
    UFUNCTION(BlueprintPure) UDiscGolfSaveGame* GetProfile() const { return Profile; }
    UFUNCTION(BlueprintPure) FDiscGolfPlayerSettings GetPlayerSettings() const;
    UFUNCTION(BlueprintCallable) void UpdatePlayerSettings(const FDiscGolfPlayerSettings& Settings);

private:
    UPROPERTY() TObjectPtr<UDiscGolfSaveGame> Profile;
    FString SaveSlot = TEXT("DiscGolfTour_Profile_0");
};
