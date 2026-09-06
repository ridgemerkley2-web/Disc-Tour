#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "../DiscBagComponent.h"
#include "../DiscGolfMath.h"
#include "../DiscGolfRHBHThrowAdapterComponent.h"
#include "../DiscGolferPawn.h"
#include "../ThrowControllerComponent.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfThrowComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#include <limits>

namespace
{
    FThrowCommand MakeValidAnimatedCommand()
    {
        FThrowCommand Command;
        Command.DiscInstanceId = FGuid::NewGuid();
        Command.MoldId = TEXT("Apex");
        Command.Plastic = EDiscPlastic::Tour;
        Command.ThrowStyle = EThrowStyle::Backhand;
        Command.ShotContext = EDiscShotContext::Drive;
        Command.Handedness = EDGHandedness::Right;
        Command.Direction = FVector(1.0f, 0.0f, 0.1f).GetSafeNormal();
        Command.Power01 = 0.82f;
        Command.HyzerDeg = 3.0f;
        Command.NoseAngleDeg = 1.0f;
        Command.LaunchAngleDeg = 7.0f;
        Command.TimingError = 0.0f;
        return Command;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfThrowControllerFailClosedIngressTest,
    "DiscGolfTour.Gameplay.ThrowIngress.ControllerFailClosedAndAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfThrowControllerFailClosedIngressTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    UThrowControllerComponent* Controller = NewObject<UThrowControllerComponent>();
    FThrowCommand Output;
    Output.MoldId = TEXT("UnchangedSentinel");
    Output.Power01 = 0.51f;

    TestFalse(TEXT("Malformed first press is rejected"), Controller->HandleThrowPress(
        NAME_None, EDiscPlastic::Tour, FVector::ForwardVector, Output));
    TestFalse(TEXT("Malformed first press never starts timing"), Controller->IsTimingActive());
    TestEqual(TEXT("Malformed first press does not touch output"),
        Output.MoldId, FName(TEXT("UnchangedSentinel")));

    TestFalse(TEXT("Valid first press starts timing"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, FVector::ForwardVector, Output));
    TestTrue(TEXT("Valid first press owns timing"), Controller->IsTimingActive());

    Controller->SetShotContext(static_cast<EDiscShotContext>(255), 8.0f);
    TestTrue(TEXT("Invalid context is atomic and does not cancel timing"),
        Controller->IsTimingActive());
    TestTrue(TEXT("Invalid context preserves Drive"),
        Controller->GetShotContext() == EDiscShotContext::Drive);
    Controller->SetShotContext(EDiscShotContext::Circle1Putt, NaN);
    TestTrue(TEXT("Non-finite context distance is atomic"), Controller->IsTimingActive());

    TestFalse(TEXT("Malformed second press is rejected"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, FVector::ZeroVector, Output));
    TestFalse(TEXT("Malformed second press releases timing ownership"),
        Controller->IsTimingActive());
    TestEqual(TEXT("Malformed second press leaves output mold untouched"),
        Output.MoldId, FName(TEXT("UnchangedSentinel")));
    TestEqual(TEXT("Malformed second press leaves output scalar untouched"),
        Output.Power01, 0.51f);

    const float PowerBefore = Controller->GetPower01();
    const float HyzerBefore = Controller->GetHyzerDeg();
    const float NoseBefore = Controller->GetNoseDeg();
    Controller->AdjustPower(NaN, 1.0f);
    Controller->AdjustPower(1.0f, Infinity);
    Controller->AdjustPower(1.0f, -0.01f);
    Controller->AdjustHyzer(Infinity, 1.0f);
    Controller->AdjustHyzer(1.0f, -0.01f);
    Controller->AdjustNose(1.0f, NaN);
    Controller->AdjustNose(1.0f, -0.01f);
    TestEqual(TEXT("Non-finite power inputs do not mutate power"),
        Controller->GetPower01(), PowerBefore);
    TestEqual(TEXT("Non-finite hyzer inputs do not mutate hyzer"),
        Controller->GetHyzerDeg(), HyzerBefore);
    TestEqual(TEXT("Non-finite nose inputs do not mutate nose"),
        Controller->GetNoseDeg(), NoseBefore);

    Controller->SetAccessibilityAssist(0.5f, 1.5f);
    const float AssistedTimingSpan = Controller->GetTimingMissSpan01();
    Controller->SetAccessibilityAssist(NaN, 1.0f);
    Controller->SetAccessibilityAssist(0.0f, Infinity);
    TestEqual(TEXT("Accessibility update is atomic for non-finite input"),
        Controller->GetTimingMissSpan01(), AssistedTimingSpan);
    TestTrue(TEXT("Non-finite aim input returns a deterministic invalid direction"),
        UThrowControllerComponent::ApplyAimAssistToDirection(
            FVector(NaN, 0.0f, 0.0f), FVector::ForwardVector, 0.5f).IsZero());
    TestTrue(TEXT("Non-finite timing input remains outside the accepted command contract"),
        UThrowControllerComponent::NormalizeTimingErrorForAccessibility(
            NaN, 0.82f, 0.18f, 1.0f)
            > DiscGolfMath::ThrowCommandMaximumTimingError);

    TestFalse(TEXT("Valid timing can restart"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, FVector::ForwardVector, Output));
    Controller->AdvanceTiming(0.1f);
    TestTrue(TEXT("Finite timing advance moves the active needle"),
        Controller->GetTimingNeedle01() > 0.0f);
    Controller->AdvanceTiming(NaN);
    TestFalse(TEXT("Non-finite tick cancels timing deterministically"),
        Controller->IsTimingActive());
    TestEqual(TEXT("Non-finite tick resets the needle"),
        Controller->GetTimingNeedle01(), 0.0f);
    TestFalse(TEXT("Timing can restart before negative-delta test"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, FVector::ForwardVector, Output));
    Controller->AdvanceTiming(-0.01f);
    TestFalse(TEXT("Negative tick cancels timing deterministically"),
        Controller->IsTimingActive());

    TestFalse(TEXT("Final valid first press starts timing"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, FVector::ForwardVector, Output));
    TestTrue(TEXT("Final valid second press captures command"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, FVector::ForwardVector, Output));
    TestTrue(TEXT("Captured command satisfies shared ingress contract"),
        DiscGolfMath::IsThrowCommandValid(Output));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfAnimatedIngressAndEquipmentLockTest,
    "DiscGolfTour.Gameplay.ThrowIngress.AdapterValidationAndEquipmentLock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfAnimatedIngressAndEquipmentLockTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    UDiscBagComponent* Bag = NewObject<UDiscBagComponent>();
    UThrowControllerComponent* Controller = NewObject<UThrowControllerComponent>();
    UDiscGolfThrowComponent* Framework = NewObject<UDiscGolfThrowComponent>();
    UDiscGolfCharacterProfile* Profile = NewObject<UDiscGolfCharacterProfile>();
    AActor* AdapterOwner = NewObject<AActor>();
    UDiscGolfRHBHThrowAdapterComponent* Adapter =
        NewObject<UDiscGolfRHBHThrowAdapterComponent>(AdapterOwner);
    Framework->CharacterProfile = Profile;
    Profile->Handedness = EDGHandedness::Right;
    Adapter->Configure(Framework, nullptr, nullptr);
    Adapter->ConfigureIngressContracts(Controller, Bag);
    int32 LaunchCount = 0;
    bool bEquipmentLockedDuringLaunch = false;
    Adapter->GetAuthoritativeLaunchDelegate().BindWeakLambda(
        AdapterOwner,
        [&LaunchCount, &bEquipmentLockedDuringLaunch, Bag](
            const FThrowCommand&,
            const FTransform&)
        {
            ++LaunchCount;
            bEquipmentLockedDuringLaunch = Bag->IsEquipmentMutationLocked();
            return true;
        });

    FThrowCommand Valid = MakeValidAnimatedCommand();
    Valid.DiscInstanceId = Bag->GetSelectedDiscInstanceId();
    TestTrue(TEXT("Valid fixture has the selected stable instance"),
        Valid.DiscInstanceId.IsValid());
    TestTrue(TEXT("Exact selected equipment provenance is accepted"),
        UDiscGolfRHBHThrowAdapterComponent::IsSelectedEquipmentProvenanceExact(
            Valid, Bag));
    TestFalse(TEXT("NaN watchdog timeout is rejected"),
        UDiscGolfRHBHThrowAdapterComponent::IsWatchdogTimeoutValid(NaN));
    TestFalse(TEXT("Infinite watchdog timeout is rejected"),
        UDiscGolfRHBHThrowAdapterComponent::IsWatchdogTimeoutValid(Infinity));
    TestFalse(TEXT("Zero watchdog timeout is rejected"),
        UDiscGolfRHBHThrowAdapterComponent::IsWatchdogTimeoutValid(0.0f));
    TestFalse(TEXT("Effectively permanent watchdog timeout is rejected"),
        UDiscGolfRHBHThrowAdapterComponent::IsWatchdogTimeoutValid(61.0f));
    TestTrue(TEXT("Default-range watchdog timeout is accepted"),
        UDiscGolfRHBHThrowAdapterComponent::IsWatchdogTimeoutValid(5.0f));
    FThrowCommand WorldHeadingCommand = Valid;
    WorldHeadingCommand.Direction = FVector(0.0f, 1.0f, 0.1f).GetSafeNormal();
    const FDGThrowIntent PresentationIntent =
        UDiscGolfRHBHThrowAdapterComponent::
            BuildPresentationIntentForAnimatedRHBH(WorldHeadingCommand);
    TestTrue(TEXT("Presentation intent copies immutable throw scalars"),
        PresentationIntent.ThrowType == EDGThrowType::Backhand
        && PresentationIntent.Power01 == WorldHeadingCommand.Power01
        && PresentationIntent.HyzerDegrees == WorldHeadingCommand.HyzerDeg
        && PresentationIntent.NoseDegrees == WorldHeadingCommand.NoseAngleDeg);
    TestEqual(TEXT("World heading never becomes a local rig aim offset"),
        PresentationIntent.AimYawDegrees, 0.0f);
    FThrowCommand SyntheticPresentationCommand = Valid;
    SyntheticPresentationCommand.DiscInstanceId.Invalidate();
    TestTrue(TEXT("Presentation-only developer commands retain synthetic instance compatibility"),
        UDiscGolfRHBHThrowAdapterComponent::IsCommandEligibleForAnimatedRHBH(
            SyntheticPresentationCommand,
            EDGHandedness::Right,
            EDiscShotContext::Drive));
    TestFalse(TEXT("A missing player instance is not exact selected provenance"),
        UDiscGolfRHBHThrowAdapterComponent::IsSelectedEquipmentProvenanceExact(
            SyntheticPresentationCommand, Bag));
    FThrowCommand Circle1Putt = Valid;
    Circle1Putt.ShotContext = EDiscShotContext::Circle1Putt;
    TestTrue(TEXT("Circle 1 right-backhand putt is eligible for authored motion"),
        UDiscGolfRHBHThrowAdapterComponent::IsCommandEligibleForAnimatedRHBH(
            Circle1Putt,
            EDGHandedness::Right,
            EDiscShotContext::Circle1Putt));
    FThrowCommand Circle2Putt = Valid;
    Circle2Putt.ShotContext = EDiscShotContext::Circle2Putt;
    TestTrue(TEXT("Circle 2 right-backhand putt is eligible for authored motion"),
        UDiscGolfRHBHThrowAdapterComponent::IsCommandEligibleForAnimatedRHBH(
            Circle2Putt,
            EDGHandedness::Right,
            EDiscShotContext::Circle2Putt));
    TestFalse(TEXT("A valid right-backhand command cannot cross shot contexts"),
        UDiscGolfRHBHThrowAdapterComponent::IsCommandEligibleForAnimatedRHBH(
            Circle1Putt,
            EDGHandedness::Right,
            EDiscShotContext::Circle2Putt));
    const auto RejectsWithoutSideEffects = [this, Adapter, Bag, Framework](
        const TCHAR* Label,
        const FThrowCommand& Candidate)
    {
        const int64 AttemptSerialBefore = Adapter->GetAttemptSerial();
        TestFalse(Label, Adapter->TryBeginValidatedTransactionForTesting(Candidate));
        TestFalse(TEXT("Rejected command leaves transaction idle"), Adapter->IsThrowActive());
        TestEqual(TEXT("Rejected command does not consume attempt serial"),
            Adapter->GetAttemptSerial(), AttemptSerialBefore);
        TestFalse(TEXT("Rejected command does not lock equipment"),
            Bag->IsEquipmentMutationLocked());
        TestFalse(TEXT("Rejected command does not begin framework presentation"),
            Framework->bThrowActive);
    };

    FThrowCommand Invalid = Valid;
    Invalid.DiscInstanceId.Invalidate();
    RejectsWithoutSideEffects(
        TEXT("Missing selected disc identity is rejected before authoritative presentation"),
        Invalid);
    Invalid = Valid;
    Invalid.DiscInstanceId = FGuid::NewGuid();
    RejectsWithoutSideEffects(
        TEXT("Mismatched selected disc identity is rejected before authoritative presentation"),
        Invalid);
    Invalid = Valid;
    Invalid.MoldId = TEXT("Vector");
    RejectsWithoutSideEffects(
        TEXT("Selected mold mismatch is rejected before authoritative presentation"),
        Invalid);
    Invalid = Valid;
    Invalid.Plastic = EDiscPlastic::Base;
    RejectsWithoutSideEffects(
        TEXT("Selected plastic mismatch is rejected before authoritative presentation"),
        Invalid);
    Invalid = Valid;
    Invalid.MoldId = NAME_None;
    RejectsWithoutSideEffects(TEXT("Missing mold is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.Plastic = static_cast<EDiscPlastic>(255);
    RejectsWithoutSideEffects(TEXT("Invalid plastic is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.ThrowStyle = EThrowStyle::Forehand;
    RejectsWithoutSideEffects(TEXT("Forehand is outside the animated RHBH slice"), Invalid);
    Invalid = Valid;
    Invalid.ThrowStyle = static_cast<EThrowStyle>(255);
    RejectsWithoutSideEffects(TEXT("Invalid throw style is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.ShotContext = static_cast<EDiscShotContext>(255);
    RejectsWithoutSideEffects(TEXT("Invalid shot context is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.Handedness = EDGHandedness::Left;
    RejectsWithoutSideEffects(TEXT("Left-handed command cannot start RHBH presentation"), Invalid);
    Invalid = Valid;
    Invalid.Handedness = static_cast<EDGHandedness>(255);
    RejectsWithoutSideEffects(TEXT("Invalid handedness is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.Direction.X = NaN;
    RejectsWithoutSideEffects(TEXT("Non-finite direction is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.Direction = FVector::ZeroVector;
    RejectsWithoutSideEffects(TEXT("Zero direction is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.Power01 = NaN;
    RejectsWithoutSideEffects(TEXT("Non-finite power is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.Power01 = DiscGolfMath::ThrowCommandMaximumPower01 + 0.01f;
    RejectsWithoutSideEffects(TEXT("Out-of-range power is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.HyzerDeg = Infinity;
    RejectsWithoutSideEffects(TEXT("Non-finite hyzer is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.HyzerDeg = DiscGolfMath::ThrowCommandMaximumHyzerDeg + 0.01f;
    RejectsWithoutSideEffects(TEXT("Out-of-range hyzer is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.NoseAngleDeg = DiscGolfMath::ThrowCommandMaximumNoseAngleDeg + 0.01f;
    RejectsWithoutSideEffects(TEXT("Out-of-range nose is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.LaunchAngleDeg = DiscGolfMath::ThrowCommandMaximumLaunchAngleDeg + 0.01f;
    RejectsWithoutSideEffects(TEXT("Out-of-range launch angle is rejected before presentation"), Invalid);
    Invalid = Valid;
    Invalid.TimingError = DiscGolfMath::ThrowCommandMaximumTimingError + 0.01f;
    RejectsWithoutSideEffects(TEXT("Out-of-range timing is rejected before presentation"), Invalid);

    Profile->Handedness = EDGHandedness::Left;
    RejectsWithoutSideEffects(TEXT("Left-handed runtime profile rejects RHBH presentation"), Valid);
    Profile->Handedness = EDGHandedness::Right;
    Controller->SetShotContext(EDiscShotContext::Circle1Putt, 6.0f);
    RejectsWithoutSideEffects(TEXT("Current shot-context mismatch rejects presentation"), Valid);
    Controller->SetShotContext(EDiscShotContext::Drive, 0.0f);

    TestTrue(TEXT("Valid RHBH command begins the guarded transaction"),
        Adapter->TryBeginValidatedTransactionForTesting(Valid));
    const FThrowCommand CachedValid = Adapter->GetLastAuthoritativeCommand();
    TestTrue(TEXT("Admission caches the exact supplied stable instance without repair"),
        CachedValid.DiscInstanceId == Valid.DiscInstanceId);
    TestTrue(TEXT("Admission preserves the remaining immutable command snapshot"),
        CachedValid.MoldId == Valid.MoldId
        && CachedValid.Plastic == Valid.Plastic
        && CachedValid.Handedness == Valid.Handedness
        && CachedValid.ShotContext == Valid.ShotContext
        && CachedValid.Direction == Valid.Direction
        && CachedValid.Power01 == Valid.Power01
        && CachedValid.HyzerDeg == Valid.HyzerDeg
        && CachedValid.NoseAngleDeg == Valid.NoseAngleDeg
        && CachedValid.LaunchAngleDeg == Valid.LaunchAngleDeg
        && CachedValid.TimingError == Valid.TimingError);
    TestTrue(TEXT("Equipment locks for the entire animated transaction"),
        Bag->IsEquipmentMutationLocked());
    const FDGDiscBagLoadout Before = Bag->GetEquipmentLoadout();
    const FGuid BeforeSelectedId = Bag->GetSelectedDiscInstanceId();
    const int32 BeforeIndex = Bag->GetSelectedIndex();
    const EDiscPlastic BeforePlastic = Bag->GetSelectedPlastic();

    FDGDiscBagLoadout Changed = Before;
    Changed.Discs[0].MassGrams -= 1.0f;
    FString Error;
    TestFalse(TEXT("ApplyEquipmentLoadout is locked"), Bag->ApplyEquipmentLoadout(Changed, Error));
    TestTrue(TEXT("Locked apply returns a diagnostic"), !Error.IsEmpty());
    TestFalse(TEXT("SelectEquipment is locked"),
        Bag->SelectEquipment(TEXT("Vector"), EDiscPlastic::Crystal));
    Bag->SelectDiscIndex(2);
    Bag->CyclePlastic();
    TestFalse(TEXT("SelectDiscInstance is locked"),
        Bag->SelectDiscInstance(Before.Discs[1].InstanceId));
    TestFalse(TEXT("ToggleSelectedFavorite is locked"), Bag->ToggleSelectedFavorite());
    TestFalse(TEXT("LoadEquipment is locked"), Bag->LoadEquipment(Error));
    TestFalse(TEXT("LoadEquipmentFromSlot is locked"),
        Bag->LoadEquipmentFromSlot(TEXT("DGT_LockedFixture"), 0, Error));
    TestEqual(TEXT("Locked methods preserve stable selection"),
        Bag->GetSelectedDiscInstanceId(), BeforeSelectedId);
    TestEqual(TEXT("Locked methods preserve selected index"), Bag->GetSelectedIndex(), BeforeIndex);
    TestTrue(TEXT("Locked methods preserve selected plastic"),
        Bag->GetSelectedPlastic() == BeforePlastic);
    TestEqual(TEXT("Locked apply cannot partially mutate mass"),
        Bag->GetEquipmentLoadout().Discs[0].MassGrams, Before.Discs[0].MassGrams);
    TestTrue(TEXT("Pre-release cancellation recovers transaction"), Adapter->CancelBeforeRelease());
    TestFalse(TEXT("Pre-release cancellation unlocks equipment"),
        Bag->IsEquipmentMutationLocked());
    TestTrue(TEXT("The same valid loadout mutation succeeds after recovery"),
        Bag->ApplyEquipmentLoadout(Changed, Error));
    TestTrue(TEXT("Equipment mutation resumes after recovery"),
        Bag->SelectEquipment(TEXT("Vector"), EDiscPlastic::Crystal));
    TestTrue(TEXT("Original authoritative equipment can be reselected"),
        Bag->SelectEquipment(Valid.MoldId, Valid.Plastic));

    TestTrue(TEXT("Second valid transaction begins"),
        Adapter->TryBeginValidatedTransactionForTesting(Valid));
    Adapter->TriggerReleaseForTesting(FTransform::Identity);
    TestEqual(TEXT("Release reaches gameplay authority once"), LaunchCount, 1);
    TestTrue(TEXT("Equipment is locked inside the synchronous launch callback"),
        bEquipmentLockedDuringLaunch);
    TestTrue(TEXT("Equipment remains locked through committed follow-through"),
        Bag->IsEquipmentMutationLocked());
    TestTrue(TEXT("Released transaction recovers from interruption"),
        Adapter->RecoverInterruptedThrow());
    TestFalse(TEXT("Post-release recovery unlocks equipment"),
        Bag->IsEquipmentMutationLocked());

    TestTrue(TEXT("Watchdog fixture begins"),
        Adapter->TryBeginValidatedTransactionForTesting(Valid));
    Adapter->TriggerWatchdogForTesting();
    TestFalse(TEXT("Watchdog recovery unlocks equipment"),
        Bag->IsEquipmentMutationLocked());

    Controller->SetShotContext(EDiscShotContext::Circle1Putt, 6.0f);
    TestTrue(TEXT("Circle 1 right-backhand putt begins the guarded transaction"),
        Adapter->TryBeginValidatedTransactionForTesting(Circle1Putt));
    TestTrue(TEXT("Circle 1 putt cancellation recovers the transaction"),
        Adapter->CancelBeforeRelease());
    Controller->SetShotContext(EDiscShotContext::Circle2Putt, 14.0f);
    TestTrue(TEXT("Circle 2 right-backhand putt begins the guarded transaction"),
        Adapter->TryBeginValidatedTransactionForTesting(Circle2Putt));
    TestTrue(TEXT("Circle 2 putt cancellation recovers the transaction"),
        Adapter->CancelBeforeRelease());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPawnFaceLocationFiniteTest,
    "DiscGolfTour.Gameplay.ThrowIngress.FaceLocationFinite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPawnFaceLocationFiniteTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("Transient face-location world is created"), World);
    if (!World)
    {
        return false;
    }
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    ADiscGolferPawn* Pawn = World->SpawnActor<ADiscGolferPawn>();
    TestNotNull(TEXT("Transient golfer is spawned"), Pawn);
    if (!Pawn)
    {
        return false;
    }

    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    const float Maximum = std::numeric_limits<float>::max();
    Pawn->SetActorLocation(FVector::ZeroVector);
    const FRotator Sentinel(0.0f, 23.0f, 0.0f);
    Pawn->SetActorRotation(Sentinel);
    Pawn->FaceLocation(FVector(NaN, 0.0f, 0.0f));
    TestTrue(TEXT("NaN target preserves rotation"), Pawn->GetActorRotation().Equals(Sentinel));
    Pawn->FaceLocation(FVector(0.0f, Infinity, 0.0f));
    TestTrue(TEXT("Infinite target preserves rotation"), Pawn->GetActorRotation().Equals(Sentinel));
    Pawn->FaceLocation(FVector(Maximum, Maximum, 0.0f));
    TestTrue(TEXT("Non-finite derived distance preserves rotation"),
        Pawn->GetActorRotation().Equals(Sentinel));
    Pawn->FaceLocation(FVector::ZeroVector);
    TestTrue(TEXT("Coincident target preserves rotation"), Pawn->GetActorRotation().Equals(Sentinel));
    Pawn->FaceLocation(FVector(0.0f, 0.0f, 1000.0f));
    TestTrue(TEXT("Vertical-only target preserves yaw"), Pawn->GetActorRotation().Equals(Sentinel));

    Pawn->FaceLocation(FVector(0.0f, 1000.0f, 0.0f));
    TestFalse(TEXT("Valid target produces finite rotation"),
        Pawn->GetActorRotation().ContainsNaN());
    TestTrue(TEXT("Valid target produces expected yaw"),
        FMath::IsNearlyEqual(Pawn->GetActorRotation().Yaw, 90.0f, 0.01f));
    return true;
}

#endif
