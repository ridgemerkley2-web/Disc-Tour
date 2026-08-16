#pragma once

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/**
 * Stable Session 5 validation paths shared by the guarded runtime seam and
 * evidence runners.  These are validation assets, not a replacement for the
 * accepted prototype gameplay montage.
 */
namespace DiscGolfSession5MocapValidation
{
inline constexpr const TCHAR* SourceSequence =
    TEXT("/Game/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW.A_DG_RHBH_SyntheticSource_RAW");
inline constexpr const TCHAR* SourceSkeletalMesh =
    TEXT("/Game/DiscGolf/Animation/Mocap/Source/SK_DG_RHBH_SyntheticSource.SK_DG_RHBH_SyntheticSource");
inline constexpr const TCHAR* RetargetedSequence =
    TEXT("/Game/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG.A_DG_RHBH_Synthetic_RTG");
inline constexpr const TCHAR* CleanedSequence =
    TEXT("/Game/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN.A_DG_RHBH_Synthetic_CLN");
inline constexpr const TCHAR* PipelineTestMontage =
    TEXT("/Game/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001.AM_DG_RHBH_SyntheticPipelineTest_v001");
inline constexpr const TCHAR* ProductionSequence =
    TEXT("/Game/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001.A_DG_RHBH_SyntheticPipelineTest_v001");
inline constexpr const TCHAR* PrototypeMontage =
    TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype");
inline constexpr const TCHAR* TargetSkeletalMesh =
    TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master");

inline bool IsPipelineRuntimeValidationRequested()
{
    return FApp::IsUnattended()
        && (FParse::Param(FCommandLine::Get(), TEXT("Session5MocapPipelineSmokeTest"))
            || FParse::Param(FCommandLine::Get(), TEXT("Session5MocapVisualCapture")));
}
}
