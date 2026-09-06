#include "DiscGolfSession6OutfitSmokeRunner.h"

#include "DiscBagComponent.h"
#include "DiscGolferPawn.h"
#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitComponent.h"
#include "DiscGolfOutfitItem.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "ThrowControllerComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace DiscGolfSession6OutfitSmoke
{
const FName Session6GripSocket(TEXT("disc_grip_r"));
const FName Session6DefaultVariant(TEXT("Default"));
constexpr float Session6TransformToleranceCm = 0.02f;

bool Session6TransformsNearlyEqual(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), Session6TransformToleranceCm)
        && A.GetRotation().Equals(B.GetRotation(), 0.0005f)
        && A.GetScale3D().Equals(B.GetScale3D(), 0.0005f);
}

bool Session6BodyProfilesMatch(const FDGBodyProfile& A, const FDGBodyProfile& B)
{
    return FMath::IsNearlyEqual(A.HeightCm, B.HeightCm)
        && FMath::IsNearlyEqual(A.WingspanScale, B.WingspanScale)
        && FMath::IsNearlyEqual(A.ShoulderWidthScale, B.ShoulderWidthScale)
        && FMath::IsNearlyEqual(A.TorsoLengthScale, B.TorsoLengthScale)
        && FMath::IsNearlyEqual(A.LegLengthScale, B.LegLengthScale)
        && FMath::IsNearlyEqual(A.HandScale, B.HandScale)
        && FMath::IsNearlyEqual(A.MassKg, B.MassKg);
}

bool Session6ThrowStylesMatch(const FDGThrowStyle& A, const FDGThrowStyle& B)
{
    return FMath::IsNearlyEqual(A.RunUpIntensity, B.RunUpIntensity)
        && FMath::IsNearlyEqual(A.ReachBackAmount, B.ReachBackAmount)
        && FMath::IsNearlyEqual(A.TorsoRotation, B.TorsoRotation)
        && FMath::IsNearlyEqual(A.BraceIntensity, B.BraceIntensity)
        && FMath::IsNearlyEqual(A.Explosiveness, B.Explosiveness)
        && FMath::IsNearlyEqual(A.FollowThrough, B.FollowThrough)
        && FMath::IsNearlyEqual(A.PowerMultiplier, B.PowerMultiplier)
        && FMath::IsNearlyEqual(A.SpinMultiplier, B.SpinMultiplier);
}
}

using namespace DiscGolfSession6OutfitSmoke;

void ADiscGolfSession6OutfitSmokeRunner::Start()
{
    FParse::Value(FCommandLine::Get(), TEXT("Session6OutfitFixture="), RequestedFixture);
    FParse::Value(FCommandLine::Get(), TEXT("Session4Profile="), RequestedProfile);
    if (RequestedFixture.IsEmpty())
    {
        RequestedFixture = TEXT("BaselineCore");
    }
    if (RequestedProfile.IsEmpty())
    {
        RequestedProfile = TEXT("Baseline");
    }
    bMissingItemFixture = RequestedFixture.Equals(TEXT("MissingItem"), ESearchCase::IgnoreCase);

    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    if (!Golfer || !GameMode || !Golfer->GetOutfitComponent()
        || !Golfer->GetOutfitCatalog() || !Golfer->GetSkeletalGolferMesh())
    {
        Fail(TEXT("player, game mode, catalog, body mesh, or installed outfit component was unavailable"));
        return;
    }

    FString Error;
    if (!ValidateRequestedBodyProfile(Error))
    {
        Fail(Error);
        return;
    }

    if (!SnapshotThrowAuthority(AuthorityBeforeOutfit))
    {
        Fail(TEXT("could not snapshot the existing throw authority before applying cosmetics"));
        return;
    }

    FDGOutfitLoadout RequestedLoadout;
    if (!BuildFixtureLoadout(RequestedLoadout, Error))
    {
        Fail(Error);
        return;
    }

    if (bMissingItemFixture)
    {
        FDGEquippedOutfitEntry Missing;
        Missing.Slot = EDGOutfitSlot::Accessory;
        Missing.ItemId = TEXT("session6_missing_deleted_cosmetic_fixture");
        Missing.VariantId = Session6DefaultVariant;
        RequestedLoadout.Equipped.Add(Missing);
    }

    FString ApplyStatus;
    if (!Golfer->ApplyOutfitLoadoutTransactionally(
            RequestedLoadout,
            bMissingItemFixture,
            ApplyStatus))
    {
        Fail(FString::Printf(TEXT("transactional outfit application failed: %s"), *ApplyStatus));
        return;
    }

    const FDiscGolfOutfitResolution Resolution = DiscGolfOutfitRuntime::ResolveCanonicalLoadout(
        RequestedLoadout,
        Golfer->GetOutfitCatalog(),
        Golfer->GetRuntimeCharacterProfile()->Body);
    ExpectedCanonicalLoadout = Resolution.Loadout;
    ExpectedEquippedCount = ExpectedCanonicalLoadout.Equipped.Num();
    if (bMissingItemFixture
        && DiscGolfOutfitRuntime::FindEntryForSlot(
            ExpectedCanonicalLoadout, EDGOutfitSlot::Accessory) != nullptr)
    {
        Fail(TEXT("missing/deleted cosmetic was retained instead of resolving to safe none"));
        return;
    }
    if (!DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Golfer->GetCurrentOutfitLoadout(), ExpectedCanonicalLoadout))
    {
        Fail(TEXT("equipped loadout did not match the canonical catalog resolution"));
        return;
    }
    if (!ValidateEquippedPresentation(Error) || !ValidateThrowAuthorityUnchanged(Error))
    {
        Fail(Error);
        return;
    }

    bOutfitPreflightPassed = true;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 6 OUTFIT THROW SMOKE START: fixture=%s profile=%s equipped=%d missing_fallback=%d; delegating exactly-once launch/flight/recovery to the accepted Session 3 contract."),
        *RequestedFixture, *RequestedProfile, ExpectedEquippedCount,
        bMissingItemFixture ? 1 : 0);
    Super::Start();
}

bool ADiscGolfSession6OutfitSmokeRunner::ValidateRequestedBodyProfile(
    FString& OutError) const
{
    if (!Golfer)
    {
        OutError = TEXT("requested profile validation had no golfer");
        return false;
    }
    FDGBodyProfile ActualBody;
    FDGThrowStyle ActualStyle;
    EDGHandedness ActualHandedness = EDGHandedness::Left;
    if (!Golfer->GetCharacterCreatorProfile(ActualBody, ActualStyle, ActualHandedness))
    {
        OutError = TEXT("requested profile validation could not read the active runtime profile");
        return false;
    }

    FDGBodyProfile ExpectedBody;
    FDGThrowStyle ExpectedStyle;
    EDGHandedness ExpectedHandedness = EDGHandedness::Right;
    const FString Normalized = RequestedProfile.ToLower();
    const TCHAR* AssetPath = nullptr;
    if (Normalized == TEXT("shortcompact"))
    {
        AssetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact");
    }
    else if (Normalized == TEXT("baseline"))
    {
        AssetPath = TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter");
    }
    else if (Normalized == TEXT("talllongarms"))
    {
        AssetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms");
    }
    if (AssetPath)
    {
        const UDiscGolfCharacterProfile* Expected =
            LoadObject<UDiscGolfCharacterProfile>(nullptr, AssetPath);
        if (!Expected)
        {
            OutError = FString::Printf(TEXT("requested profile asset was unavailable: %s"), AssetPath);
            return false;
        }
        ExpectedBody = Expected->Body;
        ExpectedStyle = Expected->ThrowStyle;
        ExpectedHandedness = Expected->Handedness;
    }
    else if (Normalized == TEXT("slidermax"))
    {
        ExpectedBody.HeightCm = DiscGolfCharacterCreatorSchema::MaxHeightCm;
        ExpectedBody.WingspanScale = DiscGolfCharacterCreatorSchema::MaxWingspanScale;
        ExpectedBody.ShoulderWidthScale = DiscGolfCharacterCreatorSchema::MaxShoulderWidthScale;
        ExpectedBody.TorsoLengthScale = DiscGolfCharacterCreatorSchema::MaxTorsoLengthScale;
        ExpectedBody.LegLengthScale = DiscGolfCharacterCreatorSchema::MaxLegLengthScale;
        ExpectedBody.HandScale = DiscGolfCharacterCreatorSchema::MaxHandScale;
        ExpectedBody.MassKg = DiscGolfCharacterCreatorSchema::MaxMassKg;
        ExpectedStyle.RunUpIntensity = 1.0f;
        ExpectedStyle.ReachBackAmount = 1.0f;
        ExpectedStyle.TorsoRotation = 1.0f;
        ExpectedStyle.BraceIntensity = 1.0f;
        ExpectedStyle.Explosiveness = 1.0f;
        ExpectedStyle.FollowThrough = 1.0f;
    }
    else
    {
        OutError = FString::Printf(TEXT("unsupported Session 6 profile assertion: %s"), *RequestedProfile);
        return false;
    }

    // Match the same Session 4 adapter boundary used by the real Pawn. This
    // also proves creator-only style cannot alter authoritative power/spin.
    const FDiscGolfCharacterProfileSaveData ExpectedRuntime =
        FDiscGolfCharacterProfileSaveData::FromFramework(
            ExpectedBody, ExpectedStyle, ExpectedHandedness);
    ExpectedBody = ExpectedRuntime.ToBodyProfile();
    ExpectedStyle = ExpectedRuntime.ToThrowStyle();
    ExpectedHandedness = ExpectedRuntime.GetHandedness();

    if (ActualHandedness != ExpectedHandedness
        || !Session6BodyProfilesMatch(ActualBody, ExpectedBody)
        || !Session6ThrowStylesMatch(ActualStyle, ExpectedStyle))
    {
        OutError = FString::Printf(
            TEXT("-Session4Profile=%s was not applied exactly to the actual runtime pawn (actual_height=%.2f expected_height=%.2f actual_hand=%d expected_hand=%d)"),
            *RequestedProfile, ActualBody.HeightCm, ExpectedBody.HeightCm,
            static_cast<int32>(ActualHandedness),
            static_cast<int32>(ExpectedHandedness));
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitSmokeRunner::BuildFixtureLoadout(
    FDGOutfitLoadout& OutLoadout,
    FString& OutError) const
{
    OutLoadout.Equipped.Reset();
    const bool bCore = RequestedFixture.Equals(TEXT("BaselineCore"), ESearchCase::IgnoreCase);
    const bool bLayered = RequestedFixture.Equals(TEXT("BaselineLayered"), ESearchCase::IgnoreCase);
    const bool bFull = RequestedFixture.Equals(TEXT("ShortFull"), ESearchCase::IgnoreCase)
        || RequestedFixture.Equals(TEXT("TallFull"), ESearchCase::IgnoreCase)
        || RequestedFixture.Equals(TEXT("SliderExtremeFull"), ESearchCase::IgnoreCase)
        || bMissingItemFixture;
    if (!bCore && !bLayered && !bFull)
    {
        OutError = FString::Printf(TEXT("unknown Session 6 outfit fixture: %s"), *RequestedFixture);
        return false;
    }

    const int32 VariantIndex = RequestedFixture.Equals(
        TEXT("TallFull"), ESearchCase::IgnoreCase) ? 1
        : (RequestedFixture.Equals(TEXT("SliderExtremeFull"), ESearchCase::IgnoreCase) ? 2 : 0);
    if (bCore)
    {
        return AddFirstCompatibleItem(EDGOutfitSlot::Top, 0, 0, OutLoadout, OutError)
            && AddFirstCompatibleItem(EDGOutfitSlot::Bottom, 0, 0, OutLoadout, OutError)
            && AddFirstCompatibleItem(EDGOutfitSlot::Footwear, 0, 0, OutLoadout, OutError);
    }
    if (bLayered)
    {
        return AddFirstCompatibleItem(EDGOutfitSlot::Outerwear, 0, 1, OutLoadout, OutError)
            && AddFirstCompatibleItem(EDGOutfitSlot::Headwear, 0, 1, OutLoadout, OutError)
            && AddFirstCompatibleItem(EDGOutfitSlot::Bag, 0, 1, OutLoadout, OutError);
    }

    for (const EDGOutfitSlot Slot : DiscGolfOutfitRuntime::GetOrderedSlots())
    {
        // The missing-reference row deliberately leaves Accessory empty so its
        // fabricated unavailable entry can prove the exact fallback slot.
        if (bMissingItemFixture && Slot == EDGOutfitSlot::Accessory)
        {
            continue;
        }
        if (!AddFirstCompatibleItem(Slot, 0, VariantIndex, OutLoadout, OutError))
        {
            return false;
        }
    }
    return true;
}

bool ADiscGolfSession6OutfitSmokeRunner::AddFirstCompatibleItem(
    EDGOutfitSlot Slot,
    int32 ItemIndex,
    int32 VariantIndex,
    FDGOutfitLoadout& InOutLoadout,
    FString& OutError) const
{
    if (!Golfer || !Golfer->GetOutfitCatalog() || !Golfer->GetRuntimeCharacterProfile())
    {
        OutError = TEXT("fixture item resolution lost the player catalog or body profile");
        return false;
    }
    const TArray<FDiscGolfOutfitOption> Options = DiscGolfOutfitRuntime::GetOptionsForSlot(
        Golfer->GetOutfitCatalog(),
        Slot,
        Golfer->GetRuntimeCharacterProfile()->Body);
    TArray<const FDiscGolfOutfitOption*> Compatible;
    for (const FDiscGolfOutfitOption& Option : Options)
    {
        if (Option.bCompatible)
        {
            Compatible.Add(&Option);
        }
    }
    if (!Compatible.IsValidIndex(ItemIndex) || !Compatible[ItemIndex])
    {
        OutError = FString::Printf(
            TEXT("slot %s did not expose required compatible proxy item index %d"),
            *DiscGolfOutfitRuntime::GetSlotDisplayName(Slot).ToString(), ItemIndex);
        return false;
    }

    const FDiscGolfOutfitOption& Option = *Compatible[ItemIndex];
    FName VariantId = Session6DefaultVariant;
    if (Option.VariantIds.Num() > 0)
    {
        VariantId = Option.VariantIds[FMath::Clamp(VariantIndex, 0, Option.VariantIds.Num() - 1)];
    }
    FString Status;
    if (!DiscGolfOutfitRuntime::SetSlotSelection(
            InOutLoadout, Slot, Option.ItemId, VariantId,
            Golfer->GetOutfitCatalog(), Golfer->GetRuntimeCharacterProfile()->Body, Status))
    {
        OutError = Status;
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitSmokeRunner::SnapshotThrowAuthority(
    FThrowAuthoritySnapshot& OutSnapshot) const
{
    if (!Golfer || !Golfer->GetThrowController() || !Golfer->GetSkeletalGolferMesh())
    {
        return false;
    }
    const UThrowControllerComponent* Controller = Golfer->GetThrowController();
    OutSnapshot.Power01 = Controller->GetPower01();
    OutSnapshot.HyzerDeg = Controller->GetHyzerDeg();
    OutSnapshot.NoseDeg = Controller->GetNoseDeg();
    OutSnapshot.LaunchAngleDeg = Controller->GetLaunchAngleDeg();
    OutSnapshot.ThrowStyle = Controller->GetThrowStyle();
    OutSnapshot.ShotContext = Controller->GetShotContext();
    OutSnapshot.AimDirection = Golfer->GetActorForwardVector();
    OutSnapshot.DiscBag = Golfer->GetDiscBag();
    OutSnapshot.GripComponentTransform = Golfer->GetSkeletalGolferMesh()->GetSocketTransform(
        Session6GripSocket, RTS_Component);
    return !OutSnapshot.AimDirection.ContainsNaN()
        && !OutSnapshot.AimDirection.IsNearlyZero()
        && OutSnapshot.DiscBag != nullptr
        && !OutSnapshot.GripComponentTransform.ContainsNaN();
}

bool ADiscGolfSession6OutfitSmokeRunner::ValidateThrowAuthorityUnchanged(
    FString& OutError) const
{
    FThrowAuthoritySnapshot Current;
    if (!SnapshotThrowAuthority(Current))
    {
        OutError = TEXT("throw authority was unavailable after outfit application");
        return false;
    }
    if (!FMath::IsNearlyEqual(Current.Power01, AuthorityBeforeOutfit.Power01)
        || !FMath::IsNearlyEqual(Current.HyzerDeg, AuthorityBeforeOutfit.HyzerDeg)
        || !FMath::IsNearlyEqual(Current.NoseDeg, AuthorityBeforeOutfit.NoseDeg)
        || !FMath::IsNearlyEqual(Current.LaunchAngleDeg, AuthorityBeforeOutfit.LaunchAngleDeg)
        || Current.ThrowStyle != AuthorityBeforeOutfit.ThrowStyle
        || Current.ShotContext != AuthorityBeforeOutfit.ShotContext
        || !Current.AimDirection.Equals(AuthorityBeforeOutfit.AimDirection, 0.0001f)
        || Current.DiscBag != AuthorityBeforeOutfit.DiscBag
        || !Session6TransformsNearlyEqual(
            Current.GripComponentTransform, AuthorityBeforeOutfit.GripComponentTransform))
    {
        OutError = TEXT("cosmetic application changed aim, power, timing inputs, throw style/context, disc inventory authority, or disc_grip_r");
        return false;
    }
    return true;
}

bool ADiscGolfSession6OutfitSmokeRunner::ValidateEquippedPresentation(
    FString& OutError) const
{
    if (!Golfer || !Golfer->GetOutfitCatalog() || !Golfer->GetSkeletalGolferMesh())
    {
        OutError = TEXT("outfit presentation validation lost the pawn foundation");
        return false;
    }

    USkeletalMeshComponent* Body = Golfer->GetSkeletalGolferMesh();
    TArray<USkeletalMeshComponent*> SkeletalComponents;
    TArray<UStaticMeshComponent*> StaticComponents;
    Golfer->GetComponents(SkeletalComponents);
    Golfer->GetComponents(StaticComponents);
    int32 Matched = 0;

    for (const FDGEquippedOutfitEntry& Entry : ExpectedCanonicalLoadout.Equipped)
    {
        const UDiscGolfOutfitItem* Item = Golfer->GetOutfitCatalog()->FindItemById(Entry.ItemId);
        if (!Item || Item->Slot != Entry.Slot || !Item->SupportsHeight(
                Golfer->GetRuntimeCharacterProfile()->Body.HeightCm))
        {
            OutError = FString::Printf(TEXT("resolved item %s was missing, cross-slotted, or height-incompatible"), *Entry.ItemId.ToString());
            return false;
        }

        UPrimitiveComponent* FoundPrimitive = nullptr;
        if (!Item->SkeletalMesh.IsNull())
        {
            USkeletalMesh* ExpectedMesh = Item->SkeletalMesh.LoadSynchronous();
            for (USkeletalMeshComponent* Component : SkeletalComponents)
            {
                if (Component && Component != Body
                    && Component->GetSkeletalMeshAsset() == ExpectedMesh)
                {
                    FoundPrimitive = Component;
                    if (Item->bUseLeaderPose
                        && Component->LeaderPoseComponent.Get() != Body)
                    {
                        OutError = FString::Printf(TEXT("skeletal outfit %s did not follow the actual player body leader pose"), *Entry.ItemId.ToString());
                        return false;
                    }
                    if (!ExpectedMesh || ExpectedMesh->GetSkeleton()
                        != Body->GetSkeletalMeshAsset()->GetSkeleton())
                    {
                        OutError = FString::Printf(TEXT("skeletal outfit %s did not use SKEL_DG_Master"), *Entry.ItemId.ToString());
                        return false;
                    }
                    break;
                }
            }
        }
        else
        {
            UStaticMesh* ExpectedMesh = Item->StaticMesh.LoadSynchronous();
            for (UStaticMeshComponent* Component : StaticComponents)
            {
                if (Component && Component != Golfer->GetHeldDiscVisual()
                    && Component->GetStaticMesh() == ExpectedMesh)
                {
                    FoundPrimitive = Component;
                    if (Component->GetAttachParent() != Body
                        || Component->GetAttachSocketName() != Item->AttachSocket
                        || !Component->IsUsingAbsoluteScale()
                        || !Session6TransformsNearlyEqual(
                            Component->GetRelativeTransform(),
                            Item->RelativeAttachmentTransform))
                    {
                        OutError = FString::Printf(
                            TEXT("static cosmetic %s did not retain its declared body socket/transform or absolute-scale isolation"),
                            *Entry.ItemId.ToString());
                        return false;
                    }
                    break;
                }
            }
        }
        if (!FoundPrimitive)
        {
            OutError = FString::Printf(TEXT("equipped proxy %s had no matching runtime mesh component"), *Entry.ItemId.ToString());
            return false;
        }
        if (FoundPrimitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision
            || FoundPrimitive->GetGenerateOverlapEvents()
            || FoundPrimitive->CanEverAffectNavigation()
            || FoundPrimitive->IsSimulatingPhysics())
        {
            OutError = FString::Printf(TEXT("cosmetic %s retained collision, overlap, navigation, or simulation authority"), *Entry.ItemId.ToString());
            return false;
        }
        ++Matched;
    }

    if (Matched != ExpectedEquippedCount)
    {
        OutError = TEXT("runtime component cardinality did not match the canonical outfit");
        return false;
    }
    return true;
}

void ADiscGolfSession6OutfitSmokeRunner::Fail(const FString& Reason)
{
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("SESSION 6 OUTFIT THROW SMOKE FAIL: fixture=%s profile=%s reason=%s"),
        *RequestedFixture, *RequestedProfile, *Reason);
    Super::Fail(Reason);
}

void ADiscGolfSession6OutfitSmokeRunner::Pass()
{
    FString Error;
    if (!bOutfitPreflightPassed
        || !DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Golfer->GetCurrentOutfitLoadout(), ExpectedCanonicalLoadout)
        || !ValidateEquippedPresentation(Error))
    {
        Fail(Error.IsEmpty()
            ? TEXT("outfit/loadout authority changed after the completed accepted throw")
            : Error);
        return;
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 6 OUTFIT THROW SMOKE PASS: fixture=%s profile=%s equipped=%d missing_fallback=%d runtime_profile_asserted=1 leader_pose=1 master_skeleton=1 collision_free=1 disc_grip_r_unchanged=1 visual_bag_not_inventory=1 authority_fields_unchanged=1 one_animation=1 one_release=1 one_authoritative_disc=1 one_completed_flight=1 one_recovery=1."),
        *RequestedFixture, *RequestedProfile, ExpectedEquippedCount,
        bMissingItemFixture ? 1 : 0);
    Super::Pass();
}
