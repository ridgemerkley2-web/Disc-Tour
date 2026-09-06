#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAIPlannerAdapter.h"
#include "DiscGolfAITypes.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession14AISmokeRunner.generated.h"

class ADiscActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class UDiscGolfAIGolferProfile;

/** One actual solver result retained until the planner selects an authoritative command. */
struct FDiscGolfSession14MeasuredCandidate
{
    FName CandidateId = NAME_None;
    FThrowCommand Command;
    FDGAIShotCandidate PlannerCandidate;
    FDiscFlightTelemetry Telemetry;
    FVector StartLocationCm = FVector::ZeroVector;
    FVector FinalLocationCm = FVector::ZeroVector;
    int32 SampleCount = 0;
    bool bHoledOut = false;
};

/**
 * Timer/delegate-driven acceptance runner for the bounded Session 14 AI proof.
 *
 * Preview discs use the production release and flight classes in the loaded Pine Ridge world,
 * but deliberately have no GameMode delegates. Only the selected measured command enters the
 * existing authoritative GameMode throw path.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession14AISmokeRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession14AISmokeRunner();
    void Start();

private:
    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY() TObjectPtr<ADiscActor> PreviewDisc;
    UPROPERTY() TObjectPtr<ADiscActor> FinalDisc;
    UPROPERTY() TObjectPtr<UDiscGolfAIGolferProfile> AIProfile;

    FDGDiscInstance SelectedDiscInstance;
    FResolvedDiscDefinition ResolvedDisc;
    FDGAIShotContext ShotContext;
    TArray<FThrowCommand> CandidateCommands;
    TArray<FDiscGolfSession14MeasuredCandidate> MeasuredCandidates;
    FDGAIShotCandidate SelectedPlannerCandidate;
    FThrowCommand SelectedCommand;
    FDiscFlightTelemetry FinalTelemetry;
    FVector PreviewStartLocationCm = FVector::ZeroVector;
    FVector BasketLocationCm = FVector::ZeroVector;
    FVector FinalAuthoritativeLocationCm = FVector::ZeroVector;
    FTimerHandle PreviewTimeoutTimer;
    FTimerHandle FinalTimeoutTimer;
    int32 PreviewIndex = 0;
    int32 SelectedMeasuredIndex = INDEX_NONE;
    int32 BaselineStrokes = 0;
    int32 BaselinePenaltyStrokes = 0;
    int32 BaselinePresentationEventCount = 0;
    int32 BaselineCompletedHoles = 0;
    int32 BaselineRoundScoreToPar = 0;
    int32 AuthoritativeRequestCount = 0;
    int32 AuthoritativeStrokeDeltaAtLaunch = 0;
    int32 FinalSampleCount = 0;
    int32 CareerRoundHistoryCount = 0;
    FName CareerEventId = NAME_None;
    FName CareerPresentingBrandId = NAME_None;
    bool bFinalHoledOut = false;
    bool bCareerCommitted = false;
    bool bCareerSaved = false;
    bool bCareerLoaded = false;
    bool bCareerRoundTripEqual = false;
    bool bCareerSlotDeleted = false;
    bool bFinished = false;
    FString AcceptedExternalUserDir;
    FString FailureReason;

    bool ResolveFixture(FString& OutError);
    void BuildCandidateCommands();
    void LaunchNextPreview();
    void CompletePreview(ADiscActor* Disc, const FVector& FinalLocationCm, bool bHoledOut);
    bool ValidatePreviewIsolation(FString& OutError) const;
    void SelectAndLaunchAuthoritative();
    bool WriteReport(bool bPassed, FString& OutPath) const;
    bool RunDisposableCareerProof(FString& OutError);
    void Pass();
    void Fail(const FString& Reason);
    void CleanupPreview();

    UFUNCTION()
    void HandlePreviewSettled(ADiscActor* Disc, FVector FinalLocation);

    UFUNCTION()
    void HandlePreviewHoledOut(ADiscActor* Disc);

    UFUNCTION()
    void HandleFinalSettled(ADiscActor* Disc, FVector FinalLocation);

    UFUNCTION()
    void HandleFinalHoledOut(ADiscActor* Disc);

    void HandlePreviewTimeout();
    void HandleFinalTimeout();
};
