#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "AnimNotify_DiscRelease.h"
#include "AnimNotify_ThrowFinished.h"
#include "DiscGolfAnimInstance.h"
#include "../DiscGolfProductionMotion.h"
#include "DiscGolfThrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfAnimInstanceThrowBridgeContractTest,
    "DiscGolfTour.Character.Animation.ThrowBridgeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfAnimInstanceThrowBridgeContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const UClass* AnimInstanceClass = UDiscGolfAnimInstance::StaticClass();
    TestNotNull(TEXT("Throw intent is exposed to animation"),
        FindFProperty<FStructProperty>(AnimInstanceClass, TEXT("ThrowIntent")));
    TestNotNull(TEXT("Throw phase is exposed to Control Rig direct mapping"),
        FindFProperty<FEnumProperty>(AnimInstanceClass, TEXT("ThrowPhase")));
    TestNotNull(TEXT("Throw-active state is exposed to Control Rig direct mapping"),
        FindFProperty<FBoolProperty>(AnimInstanceClass, TEXT("bThrowActive")));

    const UDiscGolfAnimInstance* Defaults = GetDefault<UDiscGolfAnimInstance>();
    TestTrue(TEXT("Animation bridge defaults to Idle"),
        Defaults && Defaults->ThrowPhase == EDGThrowPhase::Idle);
    TestTrue(TEXT("Animation bridge defaults inactive"),
        Defaults && !Defaults->bThrowActive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfThrowPresentationLifecycleTest,
    "DiscGolfTour.Character.Animation.ThrowPresentationLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfThrowPresentationLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfThrowComponent* Throw = NewObject<UDiscGolfThrowComponent>();
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>();
    UAnimMontage* AuthoredMontage = LoadObject<UAnimMontage>(
        nullptr, DiscGolfProductionMotion::DriveMontage);
    TestNotNull(TEXT("Throw component fixture exists"), Throw);
    TestNotNull(TEXT("Skeletal mesh fixture exists"), Mesh);
    TestNotNull(TEXT("Active production montage fixture exists"), AuthoredMontage);
    TestTrue(TEXT("Active production montage has the complete ordered lifecycle"),
        UDiscGolfThrowComponent::IsAuthoredMontageLifecycleSafe(AuthoredMontage));
    if (!Throw || !Mesh || !AuthoredMontage)
    {
        return false;
    }

    Throw->NotifyThrowPhase(EDGThrowPhase::RunUp);
    TestEqual(TEXT("Inactive stale phase is ignored"),
        Throw->CurrentPhase, EDGThrowPhase::Idle);

    Throw->BeginAimPreview();
    TestTrue(TEXT("First press activates aim presentation"), Throw->bThrowActive);
    TestFalse(TEXT("Aim presentation is not a committed release attempt"),
        Throw->IsThrowCommitted());
    TestEqual(TEXT("First press reaches Aim"), Throw->CurrentPhase, EDGThrowPhase::Aim);
    Throw->NotifyThrowPhase(EDGThrowPhase::RunUp);
    TestEqual(TEXT("Queued montage phase cannot advance pre-commit aim"),
        Throw->CurrentPhase, EDGThrowPhase::Aim);
    FDGThrowIntent PreviewIntent;
    PreviewIntent.Power01 = 0.42f;
    PreviewIntent.AimYawDegrees = 137.0f;
    Throw->SetThrowIntent(PreviewIntent);
    TestEqual(TEXT("Aim controls can refresh pre-commit intent"),
        Throw->CurrentIntent.Power01, 0.42f);
    TestEqual(TEXT("World heading is not forwarded as a local rig yaw delta"),
        Throw->CurrentIntent.AimYawDegrees, 0.0f);
    Throw->CancelThrow();
    TestFalse(TEXT("Aim cancellation deactivates presentation"), Throw->bThrowActive);
    TestEqual(TEXT("Aim cancellation returns to Idle"),
        Throw->CurrentPhase, EDGThrowPhase::Idle);

    FDGThrowIntent CommittedIntent;
    CommittedIntent.Power01 = 0.84f;
    Throw->SetThrowIntent(CommittedIntent);
    Throw->BeginThrow();
    TestTrue(TEXT("Committed animation starts active"), Throw->bThrowActive);
    TestTrue(TEXT("Committed animation is distinguished from aim preview"),
        Throw->IsThrowCommitted());
    constexpr int64 AttemptSerial = 7;
    constexpr int32 MontageInstanceId = 42;
    TestTrue(TEXT("Committed attempt binds its exact montage instance"),
        Throw->BindCommittedMontageInstance(
            AttemptSerial, MontageInstanceId, AuthoredMontage));
    FDGThrowIntent LateMutation;
    LateMutation.Power01 = 0.10f;
    Throw->SetThrowIntent(LateMutation);
    TestEqual(TEXT("Committed animation intent is immutable"),
        Throw->CurrentIntent.Power01, 0.84f);
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::RunUp, MontageInstanceId + 1, AuthoredMontage);
    TestEqual(TEXT("Stale montage instance cannot advance a new attempt"),
        Throw->CurrentPhase, EDGThrowPhase::Aim);
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::RunUp, MontageInstanceId, AuthoredMontage);
    TestEqual(TEXT("Bound montage instance advances the committed attempt"),
        Throw->CurrentPhase, EDGThrowPhase::RunUp);
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::Plant, MontageInstanceId, AuthoredMontage);
    TestEqual(TEXT("Same-instance phase cannot skip ReachBack"),
        Throw->CurrentPhase, EDGThrowPhase::RunUp);
    Throw->NotifyDiscRelease(Mesh);
    TestFalse(TEXT("Unverified direct release is rejected before Acceleration"),
        Throw->HasNotifiedDiscRelease());
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::ReachBack, MontageInstanceId, AuthoredMontage);
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::Plant, MontageInstanceId, AuthoredMontage);
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::Acceleration, MontageInstanceId, AuthoredMontage);
    TestEqual(TEXT("Ordered committed phases reach Acceleration"),
        Throw->CurrentPhase, EDGThrowPhase::Acceleration);
    Throw->NotifyDiscReleaseFromMontage(
        Mesh, MontageInstanceId + 1, AuthoredMontage);
    TestFalse(TEXT("Stale release notify is ignored"),
        Throw->HasNotifiedDiscRelease());
    Throw->NotifyDiscReleaseFromMontage(
        Mesh, MontageInstanceId, AuthoredMontage);
    TestTrue(TEXT("Release branching point latches once"),
        Throw->HasNotifiedDiscRelease());
    TestEqual(TEXT("Release remains current after the release callback returns"),
        Throw->CurrentPhase, EDGThrowPhase::Release);
    Throw->NotifyThrowPhase(EDGThrowPhase::Recovery);
    TestEqual(TEXT("Recovery cannot skip explicit FollowThrough"),
        Throw->CurrentPhase, EDGThrowPhase::Release);
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::FollowThrough, MontageInstanceId, AuthoredMontage);
    TestEqual(TEXT("Explicit FollowThrough notify advances release"),
        Throw->CurrentPhase, EDGThrowPhase::FollowThrough);
    Throw->NotifyThrowFinished();
    TestFalse(TEXT("Unverified direct ThrowFinished is rejected before Recovery"),
        Throw->HasNotifiedThrowFinished());
    Throw->NotifyThrowPhaseFromMontage(
        EDGThrowPhase::Recovery, MontageInstanceId, AuthoredMontage);
    TestEqual(TEXT("Explicit Recovery follows FollowThrough"),
        Throw->CurrentPhase, EDGThrowPhase::Recovery);

    Throw->NotifyThrowFinishedFromMontage(
        MontageInstanceId + 1, AuthoredMontage);
    TestFalse(TEXT("Stale finish notify is ignored"),
        Throw->HasNotifiedThrowFinished());
    Throw->NotifyThrowFinishedFromMontage(MontageInstanceId, AuthoredMontage);
    TestTrue(TEXT("ThrowFinished is latched"), Throw->HasNotifiedThrowFinished());
    TestTrue(TEXT("Recovery tail stays active until montage end"), Throw->bThrowActive);
    TestEqual(TEXT("ThrowFinished does not truncate Recovery"),
        Throw->CurrentPhase, EDGThrowPhase::Recovery);
    Throw->NotifyRecoveryComplete();
    TestFalse(TEXT("Montage end closes presentation"), Throw->bThrowActive);
    TestEqual(TEXT("Completed recovery returns to Idle"),
        Throw->CurrentPhase, EDGThrowPhase::Idle);

    // Unreal evaluates the two authority-bearing notifies as branching points,
    // while authored phases are queued. Simulate one large animation tick that
    // reaches each branching point before the earlier queued callbacks flush.
    AActor* HitchOwner = NewObject<AActor>();
    UDiscGolfThrowComponent* HitchThrow =
        NewObject<UDiscGolfThrowComponent>(HitchOwner);
    USkeletalMeshComponent* HitchMesh =
        NewObject<USkeletalMeshComponent>(HitchOwner);
    HitchOwner->AddInstanceComponent(HitchThrow);
    HitchOwner->AddInstanceComponent(HitchMesh);
    TestNotNull(TEXT("Hitch-order throw fixture exists"), HitchThrow);
    TestNotNull(TEXT("Hitch-order mesh fixture exists"), HitchMesh);
    if (!HitchThrow || !HitchMesh)
    {
        return false;
    }
    HitchThrow->SetThrowIntent(CommittedIntent);
    HitchThrow->BeginThrow();
    constexpr int64 HitchAttemptSerial = 8;
    constexpr int32 HitchMontageInstanceId = 43;
    TestTrue(TEXT("Hitch-order attempt binds the validated montage"),
        HitchThrow->BindCommittedMontageInstance(
            HitchAttemptSerial, HitchMontageInstanceId, AuthoredMontage));

    FAnimNotifyEvent* ReleaseEvent = nullptr;
    FAnimNotifyEvent* FinishEvent = nullptr;
    for (FAnimNotifyEvent& Event : AuthoredMontage->Notifies)
    {
        if (Cast<UAnimNotify_DiscRelease>(Event.Notify))
        {
            ReleaseEvent = &Event;
        }
        else if (Cast<UAnimNotify_ThrowFinished>(Event.Notify))
        {
            FinishEvent = &Event;
        }
    }
    TestNotNull(TEXT("Authored release branching point exists"), ReleaseEvent);
    TestNotNull(TEXT("Authored finish branching point exists"), FinishEvent);
    if (!ReleaseEvent || !FinishEvent)
    {
        return false;
    }

    FBranchingPointNotifyPayload ReleasePayload(
        HitchMesh,
        AuthoredMontage,
        ReleaseEvent,
        HitchMontageInstanceId);
    ReleaseEvent->Notify->BranchingPointNotify(ReleasePayload);
    TestTrue(TEXT("Bound release survives queued Acceleration delivery lag"),
        HitchThrow->HasNotifiedDiscRelease());
    TestEqual(TEXT("Hitch reconciliation still pauses in Release"),
        HitchThrow->CurrentPhase, EDGThrowPhase::Release);
    FBranchingPointNotifyPayload FinishPayload(
        HitchMesh,
        AuthoredMontage,
        FinishEvent,
        HitchMontageInstanceId);
    FinishEvent->Notify->BranchingPointNotify(FinishPayload);
    TestTrue(TEXT("Bound finish survives queued recovery-phase delivery lag"),
        HitchThrow->HasNotifiedThrowFinished());
    TestEqual(TEXT("Hitch finish reconciles through authored Recovery"),
        HitchThrow->CurrentPhase, EDGThrowPhase::Recovery);
    TestTrue(TEXT("Hitch recovery tail remains active until montage end"),
        HitchThrow->bThrowActive);
    HitchThrow->NotifyRecoveryComplete();
    TestFalse(TEXT("Hitch recovery closes only at montage end"),
        HitchThrow->bThrowActive);
    return true;
}

#endif
