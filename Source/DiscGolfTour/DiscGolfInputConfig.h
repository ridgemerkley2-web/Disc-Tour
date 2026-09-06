#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "DiscGolfInputConfig.generated.h"

class UInputMappingContext;

/**
 * Input assets consumed by the developer golfer pawn. Production content can
 * replace the source-built fallback by assigning one of these to the player
 * controller class defaults.
 */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API UDiscGolfInputConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Context")
    TObjectPtr<UInputMappingContext> GameplayMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Shot Setup")
    TObjectPtr<UInputAction> AimAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Shot Setup")
    TObjectPtr<UInputAction> PowerAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Shot Setup")
    TObjectPtr<UInputAction> HyzerAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Shot Setup")
    TObjectPtr<UInputAction> NoseAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Throw")
    TObjectPtr<UInputAction> ThrowAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Throw")
    TObjectPtr<UInputAction> ToggleThrowStyleAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Utility")
    TObjectPtr<UInputAction> ResetHoleAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Developer")
    TObjectPtr<UInputAction> CycleRegressionPresetAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Developer")
    TObjectPtr<UInputAction> RunRegressionPresetAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Developer")
    TObjectPtr<UInputAction> RunRegressionSuiteAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Presentation")
    TObjectPtr<UInputAction> ToggleShotTracerAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Presentation")
    TObjectPtr<UInputAction> InstantReplayAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Course")
    TObjectPtr<UInputAction> ToggleCourseAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Course")
    TObjectPtr<UInputAction> CourseFlyoverAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Round")
    TObjectPtr<UInputAction> NextHoleAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Round")
    TObjectPtr<UInputAction> ScorecardAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Bag")
    TObjectPtr<UInputAction> CyclePlasticAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Bag")
    TObjectPtr<UInputAction> Disc1Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Bag")
    TObjectPtr<UInputAction> Disc2Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Bag")
    TObjectPtr<UInputAction> Disc3Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Bag")
    TObjectPtr<UInputAction> Disc4Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Bag")
    TObjectPtr<UInputAction> Disc5Action;

    bool IsComplete(FString& OutMissingField) const;

    /** Stable action order shared by the source HUD and remapping screen. */
    void GetOrderedActions(TArray<const UInputAction*>& OutActions) const;

    /** Prevent device/axis-type mistakes when replacing an existing binding. */
    static bool IsRemapCandidateCompatible(const FKey& ReferenceKey, const FKey& CandidateKey, FString& OutReason);

    /** Build the asset-free keyboard, mouse, and controller regression layout. */
    static UDiscGolfInputConfig* BuildRuntimeFallback(
        UObject* Outer,
        bool bSouthpawController = false,
        float ControllerDeadZone = 0.25f);
};

/** Input Action subclass used only to attach remapping metadata at runtime. */
UCLASS(Transient)
class DISCGOLFTOUR_API UDiscGolfRuntimeInputAction final : public UInputAction
{
    GENERATED_BODY()

public:
    void ConfigurePlayerMapping(FName MappingName, const FText& DisplayName);
};
