#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseDefinition.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfHoleAuthoringUtility.generated.h"

USTRUCT(BlueprintType)
struct FDiscGolfHoleAuthoringDraft
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDiscGolfHoleBlockoutDefinition Definition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTeeAssigned = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBasketAssigned = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TeeClearRadiusCm = 1067.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GreenRadiusCm = 1067.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FairwayWidthCm = 1680.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName IntroCameraId = NAME_None;
};

USTRUCT(BlueprintType)
struct FDiscGolfHoleValidationResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bValid = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ExactDistanceFeet = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DisplayDistanceFeet = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FString> Errors;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FString> Warnings;
};

/** Approval-oriented source utility for hand authoring future holes without generating them. */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfHoleAuthoringUtility : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static FDiscGolfHoleAuthoringDraft CreateHoleDefinition(
        FName CourseId, FName LayoutId, int32 HoleNumber, const FText& HoleName);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void AssignTee(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, FVector TeeLocationCm);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void AssignBasket(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, FVector BasketLocationCm);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void DrawFairwaySpline(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, const TArray<FVector>& PointsCm);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void SetPar(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, int32 Par);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static float CalculateDistance(const FDiscGolfHoleAuthoringDraft& Draft);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void SetGreenRadius(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, float RadiusCm);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void SetFairwayWidth(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, float WidthCm);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void AssignIntroCamera(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, FName CameraAnchorId);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static void AssignPreviewSpline(UPARAM(ref) FDiscGolfHoleAuthoringDraft& Draft, const TArray<FVector>& PointsCm);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static FDiscGolfHoleValidationResult ValidateHole(
        const FDiscGolfHoleAuthoringDraft& Draft,
        const TArray<FDiscGolfHoleBlockoutDefinition>& ExistingHoles);
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    static bool GeneratePineRidgeHole1ValidationReport(FString& OutReportPath, FString& OutError);
};
