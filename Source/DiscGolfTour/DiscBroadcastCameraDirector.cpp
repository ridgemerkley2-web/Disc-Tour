#include "DiscBroadcastCameraDirector.h"

#include "DiscActor.h"
#include "DiscFlightComponent.h"
#include "DiscGolfCourseFeatureActor.h"
#include "DiscGolfTour.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADiscBroadcastCameraDirector::ADiscBroadcastCameraDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BroadcastCameraRoot"));
    SetRootComponent(SceneRoot);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("BroadcastCamera"));
    Camera->SetupAttachment(SceneRoot);
    Camera->SetFieldOfView(CurrentFieldOfViewDeg);
}

bool ADiscBroadcastCameraDirector::InitializeForShot(
    ADiscActor* InDisc,
    const FVector& InReleaseLocationCm,
    const FVector& InBasketLocationCm,
    EDiscShotContext InShotContext)
{
    if (!IsValid(InDisc) || !InDisc->GetFlightComponent()) return false;

    TrackedDisc = InDisc;
    ReleaseLocationCm = InReleaseLocationCm;
    BasketLocationCm = InBasketLocationCm;
    ShotContext = InShotContext;
    CameraMode = EDiscBroadcastCameraMode::Launch;
    bLineOfSightAdjusted = false;
    CacheVisibleStaticGeometry();
    CacheAuthoredCameraAnchors();

    FDiscBroadcastCameraInput Input;
    if (!BuildInputFromRecordedSamples(Input)) return false;
    ApplyInitialPlan(Input);
    SetActorTickEnabled(true);
    UE_LOG(LogDiscGolfTour, Display, TEXT("Broadcast camera started: %s"),
        DiscGolfBroadcastCameraMath::ModeLabel(CameraMode));
    return true;
}

void ADiscBroadcastCameraDirector::StopTracking()
{
    TrackedDisc = nullptr;
    SetActorTickEnabled(false);
}

bool ADiscBroadcastCameraDirector::BuildInputFromRecordedSamples(FDiscBroadcastCameraInput& OutInput) const
{
    if (!IsValid(TrackedDisc) || !TrackedDisc->GetFlightComponent()) return false;

    const UDiscFlightComponent* Flight = TrackedDisc->GetFlightComponent();
    const TArray<FDiscTrajectorySample>& Samples = Flight->GetTrajectorySamples();
    OutInput.ReleaseLocationCm = ReleaseLocationCm;
    OutInput.BasketLocationCm = BasketLocationCm;
    OutInput.ShotContext = ShotContext;

    if (!Samples.IsEmpty())
    {
        const FDiscTrajectorySample& Sample = Samples.Last();
        OutInput.DiscLocationCm = Sample.WorldLocationCm;
        OutInput.VelocityMps = Sample.VelocityMps;
        OutInput.GroundState = Sample.GroundState;
        OutInput.ElapsedSeconds = FMath::Max(Sample.TimeSeconds - Samples[0].TimeSeconds, 0.0f);
    }
    else
    {
        const FDiscFlightTelemetry Telemetry = Flight->GetTelemetry();
        OutInput.DiscLocationCm = TrackedDisc->GetActorLocation();
        OutInput.VelocityMps = Telemetry.VelocityMps;
        OutInput.GroundState = Telemetry.GroundState;
        OutInput.ElapsedSeconds = Telemetry.FlightTimeSeconds;
    }
    return true;
}

FVector ADiscBroadcastCameraDirector::ResolveLineOfSight(
    const FVector& DesiredLocationCm,
    const FVector& LookAtWorldCm,
    const FVector& ShotSide,
    bool& bOutAdjusted) const
{
    bOutAdjusted = false;
    if (!GetWorld()) return DesiredLocationCm;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGolfBroadcastCameraLOS), false);
    Params.AddIgnoredActor(this);
    if (TrackedDisc) Params.AddIgnoredActor(TrackedDisc);

    FVector Candidate = DesiredLocationCm;
    for (int32 Attempt = 0; Attempt < 5; ++Attempt)
    {
        FHitResult Hit;
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            Hit, Candidate, LookAtWorldCm, ECC_Visibility, Params);
        const bool bNearTargetHit = bBlocked
            && FVector::DistSquared(Hit.ImpactPoint, LookAtWorldCm) <= FMath::Square(140.0f);
        if ((!bBlocked || bNearTargetHit) && !IsInsideVisibleStaticGeometry(Candidate))
        {
            return Candidate;
        }

        bOutAdjusted = true;
        Candidate += FVector::UpVector * 260.0f + ShotSide * 110.0f;
    }
    return Candidate;
}

bool ADiscBroadcastCameraDirector::IsInsideVisibleStaticGeometry(const FVector& LocationCm) const
{
    for (const FBox& Bounds : VisualExclusionBounds)
    {
        if (Bounds.IsInside(LocationCm)) return true;
    }
    return false;
}

void ADiscBroadcastCameraDirector::CacheVisibleStaticGeometry()
{
    VisualExclusionBounds.Reset();
    if (!GetWorld()) return;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        const AStaticMeshActor* MeshActor = *It;
        if (!IsValid(MeshActor)) continue;
        if (MeshActor->ActorHasTag(TEXT("CourseFeature.CameraAnchor"))) continue;

        FVector BoundsOrigin;
        FVector BoundsExtent;
        MeshActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
        // Include collision-disabled crowns: camera safety is about visible framing, not gameplay collision.
        const FBox CameraSafeBounds(
            BoundsOrigin - BoundsExtent - FVector(85.0f),
            BoundsOrigin + BoundsExtent + FVector(85.0f));
        VisualExclusionBounds.Add(CameraSafeBounds);
    }
}

void ADiscBroadcastCameraDirector::CacheAuthoredCameraAnchors()
{
    AuthoredCameraAnchors.Reset();
    if (!GetWorld()) return;
    for (TActorIterator<ADiscGolfCourseFeatureActor> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && It->FeatureType == EDiscGolfCourseFeatureType::CameraAnchor)
        {
            AuthoredCameraAnchors.Add(*It);
        }
    }
    AuthoredCameraAnchors.Sort([](const TWeakObjectPtr<ADiscGolfCourseFeatureActor>& A,
        const TWeakObjectPtr<ADiscGolfCourseFeatureActor>& B)
    {
        const FName AId = A.IsValid() ? A->FeatureId : NAME_None;
        const FName BId = B.IsValid() ? B->FeatureId : NAME_None;
        return AId.LexicalLess(BId);
    });
}

bool ADiscBroadcastCameraDirector::ApplyAuthoredAnchor(
    FDiscBroadcastCameraPlan& InOutPlan,
    EDiscBroadcastCameraMode Mode) const
{
    EDiscGolfCameraAnchorMode CourseMode = EDiscGolfCameraAnchorMode::Launch;
    if (Mode == EDiscBroadcastCameraMode::Fairway) CourseMode = EDiscGolfCameraAnchorMode::Fairway;
    else if (Mode == EDiscBroadcastCameraMode::Finish) CourseMode = EDiscGolfCameraAnchorMode::Finish;

    const FVector ReferenceLocation = CourseMode == EDiscGolfCameraAnchorMode::Launch
        ? ReleaseLocationCm
        : CourseMode == EDiscGolfCameraAnchorMode::Finish
            ? BasketLocationCm
            : FMath::Lerp(ReleaseLocationCm, BasketLocationCm, 0.5f);
    const ADiscGolfCourseFeatureActor* BestAnchor = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    for (const TWeakObjectPtr<ADiscGolfCourseFeatureActor>& AnchorPtr : AuthoredCameraAnchors)
    {
        const ADiscGolfCourseFeatureActor* Anchor = AnchorPtr.Get();
        if (IsValid(Anchor) && Anchor->CameraMode == CourseMode)
        {
            const float DistanceSquared = FVector::DistSquared2D(
                Anchor->GetActorLocation(), ReferenceLocation);
            if (!BestAnchor || DistanceSquared < BestDistanceSquared)
            {
                BestAnchor = Anchor;
                BestDistanceSquared = DistanceSquared;
            }
        }
    }
    if (!BestAnchor) return false;
    InOutPlan.WorldLocationCm = BestAnchor->GetActorLocation();
    InOutPlan.FieldOfViewDeg = BestAnchor->CameraFieldOfViewDeg;
    return true;
}

void ADiscBroadcastCameraDirector::ApplyInitialPlan(const FDiscBroadcastCameraInput& Input)
{
    FDiscBroadcastCameraPlan Plan = DiscGolfBroadcastCameraMath::BuildPlan(Input, CameraMode);
    bUsingAuthoredAnchor = ApplyAuthoredAnchor(Plan, CameraMode);
    const FVector ShotForward = DiscGolfBroadcastCameraMath::ShotForward(Input);
    const FVector ShotSide = FVector::CrossProduct(FVector::UpVector, ShotForward)
        .GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
    FVector ResolvedLocation = ResolveLineOfSight(
        Plan.WorldLocationCm, Plan.LookAtWorldCm, ShotSide, bLineOfSightAdjusted);
    SetActorLocation(ResolvedLocation);
    SetActorRotation((Plan.LookAtWorldCm - ResolvedLocation).Rotation());
    CurrentFieldOfViewDeg = Plan.FieldOfViewDeg;
    Camera->SetFieldOfView(CurrentFieldOfViewDeg);
}

void ADiscBroadcastCameraDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    FDiscBroadcastCameraInput Input;
    if (!BuildInputFromRecordedSamples(Input))
    {
        StopTracking();
        return;
    }

    const EDiscBroadcastCameraMode SelectedMode = DiscGolfBroadcastCameraMath::SelectMode(Input);
    const bool bModeChanged = DiscGolfBroadcastCameraMath::ModeRank(SelectedMode)
        > DiscGolfBroadcastCameraMath::ModeRank(CameraMode);
    if (bModeChanged)
    {
        CameraMode = SelectedMode;
        UE_LOG(LogDiscGolfTour, Display, TEXT("Broadcast camera transition: %s"),
            DiscGolfBroadcastCameraMath::ModeLabel(CameraMode));
    }

    FDiscBroadcastCameraPlan Plan = DiscGolfBroadcastCameraMath::BuildPlan(Input, CameraMode);
    bUsingAuthoredAnchor = ApplyAuthoredAnchor(Plan, CameraMode);
    const FVector ShotForward = DiscGolfBroadcastCameraMath::ShotForward(Input);
    const FVector ShotSide = FVector::CrossProduct(FVector::UpVector, ShotForward)
        .GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
    bool bAdjusted = false;
    const FVector ResolvedLocation = ResolveLineOfSight(
        Plan.WorldLocationCm, Plan.LookAtWorldCm, ShotSide, bAdjusted);
    bLineOfSightAdjusted = bAdjusted;

    // Putting coverage uses a deliberate broadcast cut to the basket camera. A
    // slow blend spends most of a short putt looking through the transition.
    const bool bPuttingFinishCut = bModeChanged
        && ShotContext != EDiscShotContext::Drive
        && CameraMode == EDiscBroadcastCameraMode::Finish;
    const float LocationSpeed = bModeChanged ? 4.5f : 2.8f;
    FVector SmoothedLocation = bPuttingFinishCut
        ? ResolvedLocation
        : FMath::VInterpTo(GetActorLocation(), ResolvedLocation, DeltaSeconds, LocationSpeed);
    bool bSmoothedPathAdjusted = false;
    SmoothedLocation = ResolveLineOfSight(
        SmoothedLocation, Plan.LookAtWorldCm, ShotSide, bSmoothedPathAdjusted);
    bLineOfSightAdjusted = bLineOfSightAdjusted || bSmoothedPathAdjusted;
    const FRotator DesiredRotation = (Plan.LookAtWorldCm - SmoothedLocation).Rotation();
    const FRotator SmoothedRotation = bPuttingFinishCut
        ? DesiredRotation
        : FMath::RInterpTo(
            GetActorRotation(), DesiredRotation, DeltaSeconds, bModeChanged ? 7.0f : 5.0f);
    SetActorLocationAndRotation(SmoothedLocation, SmoothedRotation);

    CurrentFieldOfViewDeg = bPuttingFinishCut
        ? Plan.FieldOfViewDeg
        : FMath::FInterpTo(
            CurrentFieldOfViewDeg, Plan.FieldOfViewDeg, DeltaSeconds, bModeChanged ? 5.0f : 3.0f);
    Camera->SetFieldOfView(CurrentFieldOfViewDeg);
}

FString ADiscBroadcastCameraDirector::GetStatusText() const
{
    return FString::Printf(TEXT("CAMERA %s  |  %s  |  LOS %s"),
        DiscGolfBroadcastCameraMath::ModeLabel(CameraMode),
        bUsingAuthoredAnchor ? TEXT("COURSE ANCHOR") : TEXT("AUTO"),
        bLineOfSightAdjusted ? TEXT("RAISED") : TEXT("CLEAR"));
}
