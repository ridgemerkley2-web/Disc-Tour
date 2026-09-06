#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DiscGolfCareerProgressSaveGame.h"
#include "DiscGolfCompetitionRuntime.h"
#include "DiscGolfCareerSubsystem.generated.h"

struct FDiscGolfRoundState;

/** Bounded Session 14 career proof with an isolated save domain. */
UCLASS()
class DISCGOLFTOURDEVELOPER_API UDiscGolfCareerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    static constexpr int32 MaxCompletedEvents = 128;
    static constexpr int32 MaxRoundHistory = 128;
    static constexpr int32 MaxSponsorships = 32;

    const FDGCareerProgress& GetProgress() const { return Progress; }
    UFUNCTION(BlueprintPure) FDGCareerProgress GetProgressCopy() const { return Progress; }
    UFUNCTION(BlueprintCallable) void ResetToDefaults();

    bool CommitCompletedRound(
        const FDiscGolfCompetitionRuntimeDefinition& Event,
        const FDiscGolfRoundState& Round,
        FString& OutError);

    static bool ValidateProgress(const FDGCareerProgress& Candidate, FString& OutError);

    UFUNCTION(BlueprintCallable) bool SaveCareer(FString& OutError) const;
    UFUNCTION(BlueprintCallable) bool LoadCareer(FString& OutError);
    bool SaveCareerToSlot(const FString& SlotName, int32 UserIndex, FString& OutError) const;
    bool LoadCareerFromSlot(const FString& SlotName, int32 UserIndex, FString& OutError);

    /** Public so import failure atomicity and future-schema rejection can be tested without disk writes. */
    bool LoadCareerFromSaveGame(
        const UDiscGolfCareerProgressSaveGame* Save,
        FString& OutError);

private:
    static FString DefaultSaveSlot();

    UPROPERTY() FDGCareerProgress Progress;
};
