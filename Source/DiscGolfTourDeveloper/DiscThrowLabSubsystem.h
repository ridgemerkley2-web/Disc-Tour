#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DiscThrowLabTypes.h"
#include "DiscThrowLabSubsystem.generated.h"

/** Project-owned Session 11 Throw Lab. This is a capture consumer, never a flight authority. */
UCLASS()
class DISCGOLFTOURDEVELOPER_API UDiscThrowLabSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    static constexpr int32 RecordSchemaVersion = 1;
    static constexpr int32 MaxRecords = 64;
    static constexpr int32 MaxReplaySamples = 2400;
    static constexpr int32 MaxGroundTransitions = 512;
    static constexpr int32 MaxSourceSamples = 28800;
    static constexpr float MaxReplaySampleRateHz = 60.0f;
    static constexpr float MaxCaptureDurationSeconds = 120.0f;

    bool RecordCompletedThrow(
        const FResolvedDiscDefinition& Disc,
        const FThrowRelease& Release,
        const TArray<FDiscTrajectorySample>& Samples,
        const TArray<FDiscGroundTransition>& Transitions,
        const FDiscFlightTelemetry& FinalTelemetry,
        const FDiscTrajectorySummary& Summary,
        FString& OutError);

    static bool BuildRecord(
        const FResolvedDiscDefinition& Disc,
        const FThrowRelease& Release,
        const TArray<FDiscTrajectorySample>& Samples,
        const TArray<FDiscGroundTransition>& Transitions,
        const FDiscFlightTelemetry& FinalTelemetry,
        const FDiscTrajectorySummary& Summary,
        FDiscThrowLabRecord& OutRecord,
        FString& OutError);

    static bool BuildBoundedReplaySamples(
        const TArray<FDiscTrajectorySample>& Source,
        const TArray<FDiscGroundTransition>& Transitions,
        TArray<FDiscTrajectorySample>& OutSamples,
        FString& OutError);

    static bool ValidateRecord(const FDiscThrowLabRecord& Record, FString& OutError);

    UFUNCTION(BlueprintPure) int32 GetRecordCount() const { return Records.Num(); }
    UFUNCTION(BlueprintPure) int32 GetSelectedRecordIndex() const;
    UFUNCTION(BlueprintPure) FString GetStatusText() const;
    UFUNCTION(BlueprintPure) FString GetComparisonText() const;
    UFUNCTION(BlueprintPure) FDiscThrowLabComparison GetComparison() const { return Comparison; }
    UFUNCTION(BlueprintPure) bool GetSelectedRecordCopy(FDiscThrowLabRecord& OutRecord) const;

    const TArray<FDiscThrowLabRecord>& GetRecords() const { return Records; }
    const FDiscThrowLabRecord* GetRecordByIndex(int32 Index) const;
    const FDiscThrowLabRecord* GetSelectedRecord() const;

    UFUNCTION(BlueprintCallable) bool SelectRecordByIndex(int32 Index, FString& OutError);
    UFUNCTION(BlueprintCallable) bool SelectRecordByOffset(int32 Offset, FString& OutError);
    UFUNCTION(BlueprintCallable) bool ToggleSelectedPinned(bool& bOutPinned, FString& OutError);
    UFUNCTION(BlueprintCallable) bool DeleteSelected(FString& OutError);
    UFUNCTION(BlueprintCallable) bool SetComparisonByIndices(
        int32 FirstIndex, int32 SecondIndex, FString& OutError);
    UFUNCTION(BlueprintCallable) void ClearComparison();
    UFUNCTION(BlueprintCallable) void ClearLab();

    UFUNCTION(BlueprintCallable) bool SaveLab(FString& OutError) const;
    UFUNCTION(BlueprintCallable) bool LoadLab(FString& OutError);
    bool SaveLabToSlot(const FString& SlotName, int32 UserIndex, FString& OutError) const;
    bool LoadLabFromSlot(const FString& SlotName, int32 UserIndex, FString& OutError);

private:
    static FString DefaultSaveSlot();
    static FDiscThrowLabComparison CompareRecords(
        const FDiscThrowLabRecord* First,
        const FDiscThrowLabRecord* Second);
    int32 FindRecordIndex(const FString& RecordId) const;
    void ClearComparisonIfRecordReferenced(const FString& RecordId);

    UPROPERTY() TArray<FDiscThrowLabRecord> Records;
    UPROPERTY() FString SelectedRecordId;
    UPROPERTY() FDiscThrowLabComparison Comparison;
};
