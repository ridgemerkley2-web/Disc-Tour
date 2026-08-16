#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "Units/RigUnit.h"
#include "DiscGolfCharacterRigUnits.generated.h"

/**
 * Persistent, presentation-only state used by the Session 4 proportion unit.
 * It deliberately contains no throw or disc-flight authority.
 */
USTRUCT()
struct FDGCharacterProfileRigWorkData
{
    GENERATED_BODY()

    UPROPERTY()
    bool bLeftFootLocked = false;

    UPROPERTY()
    bool bRightFootLocked = false;

    UPROPERTY()
    FTransform LeftFootLock = FTransform::Identity;

    UPROPERTY()
    FTransform RightFootLock = FTransform::Identity;

    UPROPERTY()
    double LastAbsoluteTime = -1.0;
};

/**
 * Applies bounded body proportions and restrained throw-style presentation to
 * the pose already transferred into CR_DG_Master. The accepted PBIK unit runs
 * after this node and remains responsible for final limb correction.
 *
 * This unit may move the visual skeleton and its IK targets. It never changes
 * montage time, animation notifies, the throw command, or disc-flight values.
 */
USTRUCT(BlueprintType, meta=(DisplayName="DG Apply Character Profile", Category="Disc Golf|Character", Keywords="Body Profile Proportion Throw Style", NodeColor="0.10 0.38 0.52"))
struct DISCGOLFTOUR_API FRigUnit_DGApplyCharacterProfile : public FRigUnitMutable
{
    GENERATED_BODY()

    FRigUnit_DGApplyCharacterProfile();

    RIGVM_METHOD()
    virtual void Execute() override;

    UPROPERTY(meta=(Input))
    FDGBodyProfile BodyProfile;

    UPROPERTY(meta=(Input))
    FDGThrowStyle ThrowStyle;

    UPROPERTY(meta=(Input))
    EDGHandedness Handedness = EDGHandedness::Right;

    UPROPERTY(meta=(Input))
    EDGThrowPhase ThrowPhase = EDGThrowPhase::Idle;

    UPROPERTY(meta=(Input))
    bool bThrowActive = false;

    UPROPERTY(meta=(Output))
    bool bApplied = false;

    UPROPERTY(meta=(Output))
    bool bInputsClamped = false;

    UPROPERTY(meta=(Output))
    float HeightScale = 1.0f;

    UPROPERTY(meta=(Output))
    float ArmLengthScale = 1.0f;

    UPROPERTY(meta=(Output))
    float LegLengthScale = 1.0f;

    UPROPERTY(Transient)
    FDGCharacterProfileRigWorkData WorkData;
};
