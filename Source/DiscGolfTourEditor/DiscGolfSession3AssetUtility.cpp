#include "DiscGolfSession3AssetUtility.h"
#include "DiscGolfSession4AssetUtility.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "JsonObjectConverter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace DiscGolfSession3Assets
{
constexpr int32 FrameRate = 60;
constexpr int32 FrameCount = 168;
constexpr float DurationSeconds = 2.8f;

const TCHAR* MeshPath = TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master");
const TCHAR* SkeletonPath = TEXT("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master");
const TCHAR* AnimBlueprintPath = TEXT("/Game/DiscGolf/Animation/ABP_DG_Player.ABP_DG_Player");
const TCHAR* SequencePath = TEXT("/Game/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.A_DG_RHBH_Prototype");
const TCHAR* MontagePath = TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype");

const TCHAR* LocalRefPoseClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_LocalRefPose");
const TCHAR* SlotClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_Slot");
const TCHAR* ControlRigNodeClassPath = TEXT("/Script/ControlRigDeveloper.AnimGraphNode_ControlRig");
const TCHAR* RootClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_Root");
const TCHAR* PhaseNotifyClassPath = TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_ThrowPhase");
const TCHAR* ReleaseNotifyClassPath = TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_DiscRelease");
const TCHAR* FinishNotifyClassPath = TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_ThrowFinished");
const TCHAR* AnimInstanceParentPath = TEXT("/Script/DiscGolfRuntimeFoundation.DiscGolfAnimInstance");

const FName DefaultSlot(TEXT("DefaultSlot"));

struct FPoseKey
{
    int32 Frame = 0;
    TMap<FName, FRotator> RotationDeltas;
    TMap<FName, FVector> TranslationDeltas;
};

struct FPhaseEventSpec
{
    const TCHAR* Phase;
    int32 Frame;
};

constexpr FPhaseEventSpec PhaseEvents[] = {
    {TEXT("Aim"), 0},
    {TEXT("RunUp"), 12},
    {TEXT("ReachBack"), 48},
    {TEXT("Plant"), 70},
    {TEXT("Acceleration"), 84},
    {TEXT("FollowThrough"), 100},
    {TEXT("Recovery"), 132},
};
constexpr int32 ReleaseFrame = 96;
constexpr int32 FinishFrame = 162;

const TArray<FName>& AnimatedBones()
{
    static const TArray<FName> Bones = {
        TEXT("root"), TEXT("pelvis"),
        TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"),
        TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
        TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
        TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
        TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"),
    };
    return Bones;
}

void AddRotation(FPoseKey& Pose, const TCHAR* Bone, float Pitch, float Yaw, float Roll)
{
    Pose.RotationDeltas.Add(FName(Bone), FRotator(Pitch, Yaw, Roll));
}

TArray<FPoseKey> BuildPoseKeys()
{
    TArray<FPoseKey> Poses;
    auto AddPose = [&Poses](int32 Frame) -> FPoseKey&
    {
        FPoseKey& Pose = Poses.AddDefaulted_GetRef();
        Pose.Frame = Frame;
        return Pose;
    };

    // Setup: athletic, slightly closed stance with the throwing shoulder loaded.
    {
        FPoseKey& P = AddPose(0);
        AddRotation(P, TEXT("pelvis"), -3, -8, 0);
        AddRotation(P, TEXT("spine_02"), 2, 5, -2);
        AddRotation(P, TEXT("spine_04"), 3, 8, -3);
        AddRotation(P, TEXT("upperarm_r"), -8, 24, -12);
        AddRotation(P, TEXT("lowerarm_r"), 0, 20, -18);
        AddRotation(P, TEXT("hand_r"), 4, 0, -8);
        AddRotation(P, TEXT("upperarm_l"), -5, -15, 8);
        AddRotation(P, TEXT("lowerarm_l"), 0, -12, 12);
        AddRotation(P, TEXT("thigh_l"), -5, 0, 2);
        AddRotation(P, TEXT("thigh_r"), 6, 0, -2);
        AddRotation(P, TEXT("calf_l"), 8, 0, 0);
        AddRotation(P, TEXT("calf_r"), 6, 0, 0);
    }
    // First compact step.
    {
        FPoseKey& P = AddPose(12);
        P.TranslationDeltas.Add(TEXT("root"), FVector(1.0, 1.5, 0.5));
        AddRotation(P, TEXT("pelvis"), -5, 2, 1);
        AddRotation(P, TEXT("spine_02"), 3, 8, -2);
        AddRotation(P, TEXT("spine_04"), 4, 12, -4);
        AddRotation(P, TEXT("upperarm_r"), -7, 34, -15);
        AddRotation(P, TEXT("lowerarm_r"), 0, 24, -22);
        AddRotation(P, TEXT("upperarm_l"), -4, -24, 9);
        AddRotation(P, TEXT("lowerarm_l"), 0, -18, 14);
        AddRotation(P, TEXT("thigh_l"), -12, 2, 4);
        AddRotation(P, TEXT("thigh_r"), 15, -2, -3);
        AddRotation(P, TEXT("calf_l"), 18, 0, 0);
        AddRotation(P, TEXT("calf_r"), 4, 0, 0);
    }
    // X-step crossover while the upper body continues to coil.
    {
        FPoseKey& P = AddPose(36);
        P.TranslationDeltas.Add(TEXT("root"), FVector(-1.5, 3.0, 0.2));
        AddRotation(P, TEXT("pelvis"), -6, 16, -2);
        AddRotation(P, TEXT("spine_02"), 4, 20, -3);
        AddRotation(P, TEXT("spine_04"), 5, 27, -5);
        AddRotation(P, TEXT("upperarm_r"), -6, 48, -18);
        AddRotation(P, TEXT("lowerarm_r"), 0, 28, -20);
        AddRotation(P, TEXT("hand_r"), 4, 4, -10);
        AddRotation(P, TEXT("upperarm_l"), -4, -35, 12);
        AddRotation(P, TEXT("lowerarm_l"), 0, -22, 16);
        AddRotation(P, TEXT("thigh_l"), 13, -8, 8);
        AddRotation(P, TEXT("thigh_r"), -14, 8, -8);
        AddRotation(P, TEXT("calf_l"), 10, 0, 0);
        AddRotation(P, TEXT("calf_r"), 22, 0, 0);
    }
    // Full but controlled reachback; the off arm counterbalances forward.
    {
        FPoseKey& P = AddPose(54);
        P.TranslationDeltas.Add(TEXT("root"), FVector(-2.0, 3.5, -0.5));
        AddRotation(P, TEXT("pelvis"), -7, 34, -3);
        AddRotation(P, TEXT("spine_01"), 2, 8, 0);
        AddRotation(P, TEXT("spine_02"), 4, 32, -4);
        AddRotation(P, TEXT("spine_03"), 4, 40, -5);
        AddRotation(P, TEXT("spine_04"), 5, 47, -6);
        AddRotation(P, TEXT("clavicle_r"), -2, 12, -8);
        AddRotation(P, TEXT("upperarm_r"), -4, 78, -14);
        AddRotation(P, TEXT("lowerarm_r"), 0, 12, -9);
        AddRotation(P, TEXT("hand_r"), 3, 6, -8);
        AddRotation(P, TEXT("clavicle_l"), 1, -8, 5);
        AddRotation(P, TEXT("upperarm_l"), -5, -48, 15);
        AddRotation(P, TEXT("lowerarm_l"), 0, -24, 20);
        AddRotation(P, TEXT("thigh_l"), -8, -4, 4);
        AddRotation(P, TEXT("thigh_r"), 12, 5, -5);
        AddRotation(P, TEXT("calf_l"), 18, 0, 0);
        AddRotation(P, TEXT("calf_r"), 16, 0, 0);
    }
    // Plant/brace: center lowers and the front side firms before rotation opens.
    {
        FPoseKey& P = AddPose(70);
        P.TranslationDeltas.Add(TEXT("root"), FVector(-3.0, 3.8, -2.0));
        AddRotation(P, TEXT("pelvis"), -9, 25, -5);
        AddRotation(P, TEXT("spine_02"), 6, 28, -5);
        AddRotation(P, TEXT("spine_04"), 7, 38, -7);
        AddRotation(P, TEXT("upperarm_r"), -3, 70, -16);
        AddRotation(P, TEXT("lowerarm_r"), 0, 24, -18);
        AddRotation(P, TEXT("upperarm_l"), -4, -42, 16);
        AddRotation(P, TEXT("lowerarm_l"), 0, -28, 22);
        AddRotation(P, TEXT("thigh_l"), -5, -3, 8);
        AddRotation(P, TEXT("thigh_r"), 9, 4, -7);
        AddRotation(P, TEXT("calf_l"), 25, 0, 0);
        AddRotation(P, TEXT("calf_r"), 18, 0, 0);
        AddRotation(P, TEXT("foot_l"), -6, 0, 0);
    }
    // Acceleration: hips lead, elbow bends, and the disc approaches the power pocket.
    {
        FPoseKey& P = AddPose(84);
        P.TranslationDeltas.Add(TEXT("root"), FVector(-2.5, 3.8, -1.5));
        AddRotation(P, TEXT("pelvis"), -7, -4, -4);
        AddRotation(P, TEXT("spine_02"), 6, 4, -4);
        AddRotation(P, TEXT("spine_04"), 7, 10, -6);
        AddRotation(P, TEXT("upperarm_r"), -4, 30, -20);
        AddRotation(P, TEXT("lowerarm_r"), 0, 48, -32);
        AddRotation(P, TEXT("hand_r"), 2, -6, -12);
        AddRotation(P, TEXT("upperarm_l"), -2, -22, 18);
        AddRotation(P, TEXT("lowerarm_l"), 0, -30, 24);
        AddRotation(P, TEXT("thigh_l"), -4, -2, 8);
        AddRotation(P, TEXT("thigh_r"), 8, 3, -6);
        AddRotation(P, TEXT("calf_l"), 24, 0, 0);
        AddRotation(P, TEXT("calf_r"), 16, 0, 0);
    }
    // Exact release pose at frame 96 (1.6 s).
    {
        FPoseKey& P = AddPose(96);
        P.TranslationDeltas.Add(TEXT("root"), FVector(-1.0, 4.0, -0.8));
        AddRotation(P, TEXT("pelvis"), -4, -34, -2);
        AddRotation(P, TEXT("spine_01"), 2, -8, 0);
        AddRotation(P, TEXT("spine_02"), 4, -26, -2);
        AddRotation(P, TEXT("spine_03"), 3, -34, -3);
        AddRotation(P, TEXT("spine_04"), 4, -42, -4);
        AddRotation(P, TEXT("clavicle_r"), -1, -8, -8);
        AddRotation(P, TEXT("upperarm_r"), -2, -38, -18);
        AddRotation(P, TEXT("lowerarm_r"), 0, 8, -8);
        AddRotation(P, TEXT("hand_r"), 1, -8, -5);
        AddRotation(P, TEXT("upperarm_l"), 0, 10, 16);
        AddRotation(P, TEXT("lowerarm_l"), 0, -12, 16);
        AddRotation(P, TEXT("thigh_l"), -3, 0, 7);
        AddRotation(P, TEXT("thigh_r"), 7, 0, -5);
        AddRotation(P, TEXT("calf_l"), 20, 0, 0);
        AddRotation(P, TEXT("calf_r"), 14, 0, 0);
    }
    // Uninterrupted follow-through around a retained brace.
    {
        FPoseKey& P = AddPose(112);
        P.TranslationDeltas.Add(TEXT("root"), FVector(0.5, 4.0, 0.0));
        AddRotation(P, TEXT("pelvis"), -1, -57, 2);
        AddRotation(P, TEXT("spine_02"), 1, -52, 4);
        AddRotation(P, TEXT("spine_04"), 1, -66, 6);
        AddRotation(P, TEXT("upperarm_r"), 10, -88, -8);
        AddRotation(P, TEXT("lowerarm_r"), 8, -18, 10);
        AddRotation(P, TEXT("hand_r"), 6, -6, 12);
        AddRotation(P, TEXT("upperarm_l"), 3, 38, 8);
        AddRotation(P, TEXT("lowerarm_l"), 0, 18, 8);
        AddRotation(P, TEXT("thigh_l"), 2, 2, 5);
        AddRotation(P, TEXT("thigh_r"), 2, -2, -4);
        AddRotation(P, TEXT("calf_l"), 14, 0, 0);
        AddRotation(P, TEXT("calf_r"), 10, 0, 0);
    }
    // Balanced recovery, still visibly finishing the throw.
    {
        FPoseKey& P = AddPose(138);
        P.TranslationDeltas.Add(TEXT("root"), FVector(1.0, 2.0, 0.5));
        AddRotation(P, TEXT("pelvis"), 0, -22, 1);
        AddRotation(P, TEXT("spine_02"), 0, -18, 2);
        AddRotation(P, TEXT("spine_04"), 0, -24, 3);
        AddRotation(P, TEXT("upperarm_r"), 4, -42, -2);
        AddRotation(P, TEXT("lowerarm_r"), 2, -20, 8);
        AddRotation(P, TEXT("upperarm_l"), 2, 18, 4);
        AddRotation(P, TEXT("lowerarm_l"), 0, 10, 6);
        AddRotation(P, TEXT("thigh_l"), 0, 0, 2);
        AddRotation(P, TEXT("thigh_r"), 0, 0, -2);
        AddRotation(P, TEXT("calf_l"), 8, 0, 0);
        AddRotation(P, TEXT("calf_r"), 8, 0, 0);
    }
    // Neutral playable stance at the end of the montage.
    AddPose(FrameCount);
    return Poses;
}

struct FCurveSpec
{
    FName Name;
    TArray<TPair<int32, float>> Keys;
};

TArray<FCurveSpec> BuildCurveSpecs()
{
    return {
        {TEXT("DG_FootPlant_L"), {{0, 0.f}, {54, 0.f}, {70, 1.f}, {112, 1.f}, {138, 0.f}, {168, 0.f}}},
        {TEXT("DG_FootPlant_R"), {{0, 0.f}, {12, 1.f}, {36, 0.f}, {54, 0.f}, {112, 0.f}, {168, 0.f}}},
        {TEXT("DG_ReachbackAlpha"), {{0, 0.f}, {12, 0.1f}, {48, 0.85f}, {54, 1.f}, {84, 0.35f}, {96, 0.f}, {168, 0.f}}},
        {TEXT("DG_BraceAlpha"), {{0, 0.f}, {54, 0.1f}, {70, 1.f}, {112, 1.f}, {138, 0.2f}, {168, 0.f}}},
        {TEXT("DG_ReleaseApproachAlpha"), {{0, 0.f}, {70, 0.f}, {84, 0.45f}, {96, 1.f}, {100, 0.f}, {168, 0.f}}},
        {TEXT("DG_FollowThroughAlpha"), {{0, 0.f}, {96, 0.f}, {100, 0.35f}, {112, 1.f}, {138, 0.4f}, {168, 0.f}}},
    };
}

float FrameToTime(int32 Frame)
{
    return static_cast<float>(Frame) / static_cast<float>(FrameRate);
}

FString JsonString(const TSharedRef<FJsonObject>& Object)
{
    FString Result;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(Object, Writer);
    return Result;
}

FString FailureJson(const FString& Error)
{
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), TEXT("FAIL"));
    Root->SetStringField(TEXT("error"), Error);
    return JsonString(Root);
}

template <typename TObjectType>
TObjectType* LoadChecked(const TCHAR* Path, FString& Error)
{
    TObjectType* Asset = LoadObject<TObjectType>(nullptr, Path);
    if (!Asset)
    {
        Error = FString::Printf(TEXT("Required asset did not load: %s"), Path);
    }
    return Asset;
}

bool SaveAsset(UObject* Asset, FString& Error)
{
    if (!Asset)
    {
        Error = TEXT("Attempted to save a null asset");
        return false;
    }
    UPackage* Package = Asset->GetOutermost();
    Package->MarkPackageDirty();
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    SaveArgs.Error = GError;
    if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
    {
        Error = FString::Printf(TEXT("Failed to save %s to %s"), *Asset->GetPathName(), *Filename);
        return false;
    }
    return true;
}

UAnimSequence* CreateSequence(USkeleton* Skeleton, USkeletalMesh* Mesh, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(FString(SequencePath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(FString(SequencePath));
    UPackage* Package = CreatePackage(*PackageName);
    UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->PreviewSkeletalMesh = Mesh;
    UAnimSequence* Sequence = Cast<UAnimSequence>(Factory->FactoryCreateNew(
        UAnimSequence::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!Sequence)
    {
        Error = TEXT("AnimSequenceFactory failed to create A_DG_RHBH_Prototype");
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Sequence);
    return Sequence;
}

FQuat PoseComponentRotationAtKey(const FPoseKey& Pose, const FReferenceSkeleton& RefSkeleton,
    int32 BoneIndex)
{
    // Pose values describe component-space orientation intent. When a key does not
    // mention a bone, inherit the nearest keyed ancestor so a torso turn moves its
    // descendants rigidly instead of being counteracted by an implicit identity.
    for (int32 CurrentIndex = BoneIndex; CurrentIndex != INDEX_NONE;
        CurrentIndex = RefSkeleton.GetParentIndex(CurrentIndex))
    {
        const FName CurrentBone = RefSkeleton.GetBoneName(CurrentIndex);
        if (const FRotator* Rotation = Pose.RotationDeltas.Find(CurrentBone))
        {
            return Rotation->Quaternion();
        }
    }
    return FQuat::Identity;
}

FQuat PoseComponentRotationAtFrame(const TArray<FPoseKey>& Poses,
    const FReferenceSkeleton& RefSkeleton, int32 BoneIndex, int32 Frame)
{
    int32 UpperIndex = 1;
    while (UpperIndex < Poses.Num() && Poses[UpperIndex].Frame < Frame)
    {
        ++UpperIndex;
    }
    UpperIndex = FMath::Clamp(UpperIndex, 1, Poses.Num() - 1);
    const FPoseKey& A = Poses[UpperIndex - 1];
    const FPoseKey& B = Poses[UpperIndex];
    const float LinearAlpha = static_cast<float>(Frame - A.Frame) /
        static_cast<float>(FMath::Max(1, B.Frame - A.Frame));
    const float Alpha = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(LinearAlpha, 0.f, 1.f));
    const FQuat QA = PoseComponentRotationAtKey(A, RefSkeleton, BoneIndex);
    const FQuat QB = PoseComponentRotationAtKey(B, RefSkeleton, BoneIndex);
    return FQuat::Slerp(QA, QB, Alpha).GetNormalized();
}

FVector PoseTranslationAtFrame(const TArray<FPoseKey>& Poses, FName Bone, int32 Frame)
{
    int32 UpperIndex = 1;
    while (UpperIndex < Poses.Num() && Poses[UpperIndex].Frame < Frame)
    {
        ++UpperIndex;
    }
    UpperIndex = FMath::Clamp(UpperIndex, 1, Poses.Num() - 1);
    const FPoseKey& A = Poses[UpperIndex - 1];
    const FPoseKey& B = Poses[UpperIndex];
    const float LinearAlpha = static_cast<float>(Frame - A.Frame) /
        static_cast<float>(FMath::Max(1, B.Frame - A.Frame));
    const float Alpha = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(LinearAlpha, 0.f, 1.f));
    return FMath::Lerp(A.TranslationDeltas.FindRef(Bone), B.TranslationDeltas.FindRef(Bone), Alpha);
}

void BuildComponentPose(const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& LocalPose, TArray<FTransform>& OutComponentPose)
{
    OutComponentPose.SetNum(LocalPose.Num());
    for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
    {
        const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        OutComponentPose[BoneIndex] = ParentIndex == INDEX_NONE
            ? LocalPose[BoneIndex]
            : LocalPose[BoneIndex] * OutComponentPose[ParentIndex];
    }
}

void BuildAuthoredLocalRotations(const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& RefPose, const TArray<FPoseKey>& Poses,
    TArray<TArray<FQuat>>& OutLocalRotations)
{
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefPose, RefComponentPose);
    OutLocalRotations.SetNum(FrameCount + 1);

    for (int32 Frame = 0; Frame <= FrameCount; ++Frame)
    {
        TArray<FTransform> TargetComponentPose = RefComponentPose;
        for (int32 BoneIndex = 0; BoneIndex < RefPose.Num(); ++BoneIndex)
        {
            const FQuat ComponentIntent = PoseComponentRotationAtFrame(
                Poses, RefSkeleton, BoneIndex, Frame);
            TargetComponentPose[BoneIndex].SetRotation(
                (ComponentIntent * RefComponentPose[BoneIndex].GetRotation()).GetNormalized());
        }

        TArray<FQuat>& LocalRotations = OutLocalRotations[Frame];
        LocalRotations.SetNum(RefPose.Num());
        for (int32 BoneIndex = 0; BoneIndex < RefPose.Num(); ++BoneIndex)
        {
            const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
            LocalRotations[BoneIndex] = ParentIndex == INDEX_NONE
                ? TargetComponentPose[BoneIndex].GetRotation()
                : TargetComponentPose[BoneIndex]
                    .GetRelativeTransform(TargetComponentPose[ParentIndex])
                    .GetRotation().GetNormalized();
        }
    }
}

bool AuthorSequence(UAnimSequence*& Sequence, USkeleton* Skeleton, USkeletalMesh* Mesh, bool& bCreated, FString& Error)
{
    Sequence = LoadObject<UAnimSequence>(nullptr, SequencePath);
    if (!Sequence)
    {
        Sequence = CreateSequence(Skeleton, Mesh, Error);
        bCreated = true;
    }
    if (!Sequence)
    {
        return false;
    }
    if (Sequence->GetSkeleton() != Skeleton)
    {
        Error = TEXT("A_DG_RHBH_Prototype exists with the wrong skeleton; refusing to replace it");
        return false;
    }

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
    const TArray<FPoseKey> Poses = BuildPoseKeys();
    TArray<TArray<FQuat>> AuthoredLocalRotations;
    BuildAuthoredLocalRotations(RefSkeleton, RefPose, Poses, AuthoredLocalRotations);
    IAnimationDataController& Controller = Sequence->GetController();
    Controller.OpenBracket(NSLOCTEXT("DiscGolfSession3", "AuthorRHBH", "Author Session 3 RHBH Prototype"), false);
    Controller.RemoveAllBoneTracks(false);
    Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, false);
    Controller.SetFrameRate(FFrameRate(FrameRate, 1), false);
    Controller.SetNumberOfFrames(FFrameNumber(FrameCount), false);

    for (const FName Bone : AnimatedBones())
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
        if (!RefPose.IsValidIndex(BoneIndex))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Master skeleton has no required animation bone %s"), *Bone.ToString());
            return false;
        }
        if (!Controller.AddBoneCurve(Bone, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not add animation track %s"), *Bone.ToString());
            return false;
        }

        TArray<FVector3f> PositionKeys;
        TArray<FQuat4f> RotationKeys;
        TArray<FVector3f> ScaleKeys;
        PositionKeys.Reserve(FrameCount + 1);
        RotationKeys.Reserve(FrameCount + 1);
        ScaleKeys.Reserve(FrameCount + 1);
        const FTransform& Reference = RefPose[BoneIndex];
        for (int32 Frame = 0; Frame <= FrameCount; ++Frame)
        {
            const FVector Position = Reference.GetTranslation() + PoseTranslationAtFrame(Poses, Bone, Frame);
            const FQuat Rotation = AuthoredLocalRotations[Frame][BoneIndex];
            const FVector Scale = Reference.GetScale3D();
            PositionKeys.Emplace(static_cast<float>(Position.X), static_cast<float>(Position.Y), static_cast<float>(Position.Z));
            RotationKeys.Emplace(static_cast<float>(Rotation.X), static_cast<float>(Rotation.Y),
                static_cast<float>(Rotation.Z), static_cast<float>(Rotation.W));
            ScaleKeys.Emplace(static_cast<float>(Scale.X), static_cast<float>(Scale.Y), static_cast<float>(Scale.Z));
        }
        if (!Controller.SetBoneTrackKeys(Bone, PositionKeys, RotationKeys, ScaleKeys, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not set animation keys for %s"), *Bone.ToString());
            return false;
        }
    }

    for (const FCurveSpec& Curve : BuildCurveSpecs())
    {
        const FAnimationCurveIdentifier CurveId(Curve.Name, ERawCurveTrackTypes::RCT_Float);
        if (!Controller.AddCurve(CurveId, AACF_DefaultCurve, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not add animation curve %s"), *Curve.Name.ToString());
            return false;
        }
        TArray<FRichCurveKey> Keys;
        for (const TPair<int32, float>& Key : Curve.Keys)
        {
            FRichCurveKey& RichKey = Keys.Emplace_GetRef(FrameToTime(Key.Key), Key.Value);
            RichKey.InterpMode = RCIM_Cubic;
            RichKey.TangentMode = RCTM_Auto;
        }
        if (!Controller.SetCurveKeys(CurveId, Keys, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not set animation curve keys for %s"), *Curve.Name.ToString());
            return false;
        }
    }
    Controller.CloseBracket(false);

    Sequence->bLoop = false;
    Sequence->RateScale = 1.f;
    Sequence->SetPreviewMesh(Mesh);
    Sequence->PostEditChange();
    return SaveAsset(Sequence, Error);
}

UAnimMontage* CreateMontage(UAnimSequence* Sequence, USkeleton* Skeleton, USkeletalMesh* Mesh, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(FString(MontagePath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(FString(MontagePath));
    UPackage* Package = CreatePackage(*PackageName);
    UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->SourceAnimation = Sequence;
    Factory->PreviewSkeletalMesh = Mesh;
    UAnimMontage* Montage = Cast<UAnimMontage>(Factory->FactoryCreateNew(
        UAnimMontage::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!Montage)
    {
        Error = TEXT("AnimMontageFactory failed to create AM_DG_RHBH_Prototype");
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Montage);
    return Montage;
}

bool SetPhaseProperty(UAnimNotify* Notify, const TCHAR* PhaseName, FString& Error)
{
    FEnumProperty* PhaseProperty = FindFProperty<FEnumProperty>(Notify->GetClass(), TEXT("Phase"));
    if (!PhaseProperty || !PhaseProperty->GetEnum())
    {
        Error = TEXT("Throw phase notify has no reflected Phase enum property");
        return false;
    }
    UEnum* Enum = PhaseProperty->GetEnum();
    int64 Value = Enum->GetValueByNameString(PhaseName);
    if (Value == INDEX_NONE)
    {
        Value = Enum->GetValueByNameString(FString::Printf(TEXT("EDGThrowPhase::%s"), PhaseName));
    }
    if (Value == INDEX_NONE)
    {
        Error = FString::Printf(TEXT("Could not resolve EDGThrowPhase %s"), PhaseName);
        return false;
    }
    void* ValueAddress = PhaseProperty->ContainerPtrToValuePtr<void>(Notify);
    PhaseProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValueAddress, Value);
    return true;
}

bool AddNotify(UAnimMontage* Montage, UClass* NotifyClass, float Time, int32 TrackIndex,
    const FGuid& Guid, const TCHAR* PhaseName, bool bBranchingPoint, FString& Error)
{
    UAnimNotify* Notify = NewObject<UAnimNotify>(Montage, NotifyClass, NAME_None, RF_Transactional);
    if (!Notify)
    {
        Error = FString::Printf(TEXT("Could not instantiate notify class %s"), *NotifyClass->GetPathName());
        return false;
    }
    if (PhaseName && !SetPhaseProperty(Notify, PhaseName, Error))
    {
        return false;
    }
    FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
    Event.Notify = Notify;
    Event.NotifyName = FName(*Notify->GetNotifyName());
    Event.TrackIndex = TrackIndex;
    Event.Guid = Guid;
    Event.MontageTickType = bBranchingPoint
        ? EMontageNotifyTickType::BranchingPoint
        : EMontageNotifyTickType::Queued;
    Event.Link(Montage, Time, 0);
    return true;
}

bool AuthorMontage(UAnimMontage*& Montage, UAnimSequence* Sequence, USkeleton* Skeleton,
    USkeletalMesh* Mesh, bool& bCreated, FString& Error)
{
    Montage = LoadObject<UAnimMontage>(nullptr, MontagePath);
    if (!Montage)
    {
        Montage = CreateMontage(Sequence, Skeleton, Mesh, Error);
        bCreated = true;
    }
    if (!Montage)
    {
        return false;
    }
    if (Montage->GetSkeleton() != Skeleton)
    {
        Error = TEXT("AM_DG_RHBH_Prototype exists with the wrong skeleton; refusing to replace it");
        return false;
    }

    Montage->Modify();
    Montage->SlotAnimTracks.Reset();
    FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
    SlotTrack.SlotName = DefaultSlot;
    FAnimSegment& Segment = SlotTrack.AnimTrack.AnimSegments.AddDefaulted_GetRef();
    Segment.SetAnimReference(Sequence, true);
    Segment.StartPos = 0.f;
    Segment.AnimStartTime = 0.f;
    Segment.AnimEndTime = DurationSeconds;
    Segment.AnimPlayRate = 1.f;
    Segment.LoopingCount = 1;
    Montage->SetCompositeLength(DurationSeconds);

    Montage->CompositeSections.Reset();
    FCompositeSection& Section = Montage->CompositeSections.AddDefaulted_GetRef();
    Section.SectionName = TEXT("Default");
    Section.Link(Montage, 0.f, 0);

    Montage->Notifies.Reset();
    Montage->AnimNotifyTracks.Reset();
    FAnimNotifyTrack& PhaseTrack = Montage->AnimNotifyTracks.AddDefaulted_GetRef();
    PhaseTrack.TrackName = TEXT("DG Phases");
    PhaseTrack.TrackColor = FLinearColor(0.16f, 0.50f, 0.92f);
    FAnimNotifyTrack& EventTrack = Montage->AnimNotifyTracks.AddDefaulted_GetRef();
    EventTrack.TrackName = TEXT("DG Events");
    EventTrack.TrackColor = FLinearColor(0.95f, 0.45f, 0.12f);

    UClass* PhaseClass = LoadObject<UClass>(nullptr, PhaseNotifyClassPath);
    UClass* ReleaseClass = LoadObject<UClass>(nullptr, ReleaseNotifyClassPath);
    UClass* FinishClass = LoadObject<UClass>(nullptr, FinishNotifyClassPath);
    if (!PhaseClass || !ReleaseClass || !FinishClass)
    {
        Error = TEXT("DiscGolfRuntimeFoundation notify classes did not load");
        return false;
    }

    int32 NotifyOrdinal = 1;
    for (const FPhaseEventSpec& Spec : PhaseEvents)
    {
        const FGuid Guid(0xD6150000u + NotifyOrdinal, 0xA11E0001u, 0x50000000u + NotifyOrdinal, 0x00000003u);
        if (!AddNotify(Montage, PhaseClass, FrameToTime(Spec.Frame), 0, Guid,
            Spec.Phase, false, Error))
        {
            return false;
        }
        ++NotifyOrdinal;
    }
    if (!AddNotify(Montage, ReleaseClass, FrameToTime(ReleaseFrame), 1,
        FGuid(0xD61500F0u, 0xA11E0002u, 0x50000001u, 0x00000003u), nullptr, true, Error))
    {
        return false;
    }
    if (!AddNotify(Montage, FinishClass, FrameToTime(FinishFrame), 1,
        FGuid(0xD61500FFu, 0xA11E0002u, 0x50000002u, 0x00000003u), nullptr, true, Error))
    {
        return false;
    }

    Montage->SortNotifies();
    Montage->RefreshCacheData();
    Montage->SetPreviewMesh(Mesh);
    Montage->bEnableAutoBlendOut = true;
    Montage->PostEditChange();
    return SaveAsset(Montage, Error);
}

UEdGraph* FindAnimGraph(UAnimBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        if (Graph && Graph->GetName().Equals(TEXT("AnimGraph"), ESearchCase::IgnoreCase))
        {
            return Graph;
        }
    }
    return nullptr;
}

UClass* LoadGraphNodeClass(const TCHAR* Path, FString& Error)
{
    UClass* Class = LoadObject<UClass>(nullptr, Path);
    if (!Class || !Class->IsChildOf(UEdGraphNode::StaticClass()))
    {
        Error = FString::Printf(TEXT("Animation graph node class did not load: %s"), Path);
        return nullptr;
    }
    return Class;
}

UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    for (UEdGraphPin* Pin : Node->GetAllPins())
    {
        if (Pin && Pin->Direction == Direction)
        {
            return Pin;
        }
    }
    return nullptr;
}

bool SetSlotName(UEdGraphNode* SlotNode, FString& Error)
{
    FStructProperty* NodeProperty = FindFProperty<FStructProperty>(SlotNode->GetClass(), TEXT("Node"));
    if (!NodeProperty)
    {
        Error = TEXT("AnimGraph slot node has no reflected Node struct");
        return false;
    }
    FNameProperty* SlotProperty = FindFProperty<FNameProperty>(NodeProperty->Struct, TEXT("SlotName"));
    if (!SlotProperty)
    {
        Error = TEXT("AnimGraph slot runtime struct has no SlotName property");
        return false;
    }
    void* NodeAddress = NodeProperty->ContainerPtrToValuePtr<void>(SlotNode);
    SlotProperty->SetPropertyValue_InContainer(NodeAddress, DefaultSlot);
    return true;
}

FName ReadSlotName(const UEdGraphNode* SlotNode)
{
    const FStructProperty* NodeProperty = FindFProperty<FStructProperty>(SlotNode->GetClass(), TEXT("Node"));
    if (!NodeProperty)
    {
        return NAME_None;
    }
    const FNameProperty* SlotProperty = FindFProperty<FNameProperty>(NodeProperty->Struct, TEXT("SlotName"));
    if (!SlotProperty)
    {
        return NAME_None;
    }
    const void* NodeAddress = NodeProperty->ContainerPtrToValuePtr<void>(SlotNode);
    return SlotProperty->GetPropertyValue_InContainer(NodeAddress);
}

UEdGraphNode* CreateGraphNode(UEdGraph* Graph, UClass* Class, const FGuid& Guid,
    int32 X, int32 Y)
{
    UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, Class, NAME_None, RF_Transactional);
    Graph->AddNode(Node, false, false);
    Node->NodeGuid = Guid;
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    return Node;
}

bool AuthorAnimGraph(UAnimBlueprint* Blueprint, FString& Error)
{
    UEdGraph* Graph = FindAnimGraph(Blueprint);
    if (!Graph)
    {
        Error = TEXT("ABP_DG_Player has no AnimGraph");
        return false;
    }
    UClass* RefClass = LoadGraphNodeClass(LocalRefPoseClassPath, Error);
    UClass* SlotClass = LoadGraphNodeClass(SlotClassPath, Error);
    UClass* RootClass = LoadGraphNodeClass(RootClassPath, Error);
    if (!RefClass || !SlotClass || !RootClass)
    {
        return false;
    }

    UEdGraphNode* Root = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && Node->GetClass() == RootClass)
        {
            if (Root)
            {
                Error = TEXT("ABP_DG_Player AnimGraph contains duplicate root nodes");
                return false;
            }
            Root = Node;
        }
    }
    if (!Root)
    {
        Error = TEXT("ABP_DG_Player AnimGraph has no root node");
        return false;
    }

    Graph->Modify();
    Blueprint->Modify();
    const TArray<TObjectPtr<UEdGraphNode>> ExistingNodes = Graph->Nodes;
    for (UEdGraphNode* Node : ExistingNodes)
    {
        if (Node && Node != Root)
        {
            Graph->RemoveNode(Node, true, false);
        }
    }
    for (UEdGraphPin* Pin : Root->GetAllPins())
    {
        if (Pin)
        {
            Pin->BreakAllPinLinks(false, false);
        }
    }
    Root->NodePosX = 520;
    Root->NodePosY = 0;

    UEdGraphNode* RefNode = CreateGraphNode(Graph, RefClass,
        FGuid(0xD6151001u, 0xA11E0003u, 0x50000001u, 0x00000003u), -360, 0);
    UEdGraphNode* SlotNode = CreateGraphNode(Graph, SlotClass,
        FGuid(0xD6151002u, 0xA11E0003u, 0x50000002u, 0x00000003u), 40, 0);
    if (!SetSlotName(SlotNode, Error))
    {
        return false;
    }

    UEdGraphPin* RefOut = FindPosePin(RefNode, EGPD_Output);
    UEdGraphPin* SlotIn = FindPosePin(SlotNode, EGPD_Input);
    UEdGraphPin* SlotOut = FindPosePin(SlotNode, EGPD_Output);
    UEdGraphPin* RootIn = FindPosePin(Root, EGPD_Input);
    if (!RefOut || !SlotIn || !SlotOut || !RootIn)
    {
        Error = TEXT("Could not resolve RefPose -> DefaultSlot -> Root pose pins");
        return false;
    }
    RefOut->MakeLinkTo(SlotIn, false);
    SlotOut->MakeLinkTo(RootIn, false);
    Graph->NotifyGraphChanged();

    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    if (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings)
    {
        Error = FString::Printf(TEXT("ABP_DG_Player compile status is %d"), static_cast<int32>(Blueprint->Status));
        return false;
    }
    return SaveAsset(Blueprint, Error);
}

FString PhaseNameFromNotify(const UAnimNotify* Notify)
{
    const FEnumProperty* PhaseProperty = FindFProperty<FEnumProperty>(Notify->GetClass(), TEXT("Phase"));
    if (!PhaseProperty || !PhaseProperty->GetEnum())
    {
        return FString();
    }
    const void* ValueAddress = PhaseProperty->ContainerPtrToValuePtr<void>(Notify);
    const int64 Value = PhaseProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValueAddress);
    return PhaseProperty->GetEnum()->GetNameStringByValue(Value);
}

bool NearlyEqual(float A, float B, float Tolerance = 0.0005f)
{
    return FMath::Abs(A - B) <= Tolerance;
}

bool ValidateSequenceAsset(UAnimSequence* Sequence, USkeleton* Skeleton,
    TSharedPtr<FJsonObject>& OutObject, FString& Error)
{
    OutObject = MakeShared<FJsonObject>();
    if (!Sequence)
    {
        Error = TEXT("A_DG_RHBH_Prototype is missing");
        return false;
    }
    if (Sequence->GetSkeleton() != Skeleton)
    {
        Error = TEXT("A_DG_RHBH_Prototype skeleton mismatch");
        return false;
    }
    const TScriptInterface<IAnimationDataModel> DataModel = Sequence->GetDataModelInterface();
    const IAnimationDataModel* Model = DataModel.GetInterface();
    if (!Model)
    {
        Error = TEXT("A_DG_RHBH_Prototype has no animation data model");
        return false;
    }
    const FFrameRate ActualRate = Model->GetFrameRate();
    if (ActualRate.Numerator != FrameRate || ActualRate.Denominator != 1 ||
        Model->GetNumberOfFrames() != FrameCount || !NearlyEqual(Model->GetPlayLength(), DurationSeconds))
    {
        Error = FString::Printf(TEXT("RHBH sequence timing mismatch: %d/%d, frames=%d, duration=%.6f"),
            ActualRate.Numerator, ActualRate.Denominator, Model->GetNumberOfFrames(), Model->GetPlayLength());
        return false;
    }
    if (Sequence->bLoop || !NearlyEqual(Sequence->RateScale, 1.f))
    {
        Error = TEXT("RHBH sequence must be non-looping at rate scale 1");
        return false;
    }

    TArray<FName> TrackNames;
    Model->GetBoneTrackNames(TrackNames);
    TSet<FName> ActualTracks;
    for (const FName TrackName : TrackNames)
    {
        ActualTracks.Add(TrackName);
    }
    if (ActualTracks.Num() != AnimatedBones().Num())
    {
        Error = TEXT("RHBH sequence animated-bone track set mismatch");
        return false;
    }
    for (const FName ExpectedTrack : AnimatedBones())
    {
        if (!ActualTracks.Contains(ExpectedTrack))
        {
            Error = FString::Printf(TEXT("RHBH sequence is missing track %s"), *ExpectedTrack.ToString());
            return false;
        }
    }
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
    const TArray<FPoseKey> Poses = BuildPoseKeys();
    TArray<TArray<FQuat>> ExpectedLocalRotations;
    BuildAuthoredLocalRotations(RefSkeleton, RefPose, Poses, ExpectedLocalRotations);
    TMap<FName, TArray<FTransform>> TrackTransformsByName;
    for (const FName TrackName : TrackNames)
    {
        TArray<FTransform> TrackTransforms;
        Model->GetBoneTrackTransforms(TrackName, TrackTransforms);
        if (TrackTransforms.Num() != FrameCount + 1)
        {
            Error = FString::Printf(TEXT("Animation track %s does not contain 169 deterministic keys"), *TrackName.ToString());
            return false;
        }
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(TrackName);
        if (!RefPose.IsValidIndex(BoneIndex))
        {
            Error = FString::Printf(TEXT("Animation track %s has no master-skeleton bone"), *TrackName.ToString());
            return false;
        }
        const FTransform& Reference = RefPose[BoneIndex];
        for (int32 Frame = 0; Frame < TrackTransforms.Num(); ++Frame)
        {
            const FTransform& Transform = TrackTransforms[Frame];
            if (Transform.ContainsNaN() || !Transform.IsValid())
            {
                Error = FString::Printf(TEXT("Animation track %s contains an invalid transform"), *TrackName.ToString());
                return false;
            }
            const FVector ExpectedPosition = Reference.GetTranslation()
                + PoseTranslationAtFrame(Poses, TrackName, Frame);
            const float RotationErrorDegrees = FMath::RadiansToDegrees(static_cast<float>(
                Transform.GetRotation().AngularDistance(ExpectedLocalRotations[Frame][BoneIndex])));
            if (!Transform.GetTranslation().Equals(ExpectedPosition, 0.0002) ||
                !Transform.GetScale3D().Equals(Reference.GetScale3D(), 0.0002) ||
                RotationErrorDegrees > 0.01f)
            {
                Error = FString::Printf(
                    TEXT("Animation track %s frame %d does not use the component-intent-to-local pose basis "
                         "(rotation error %.4f degrees)"),
                    *TrackName.ToString(), Frame, RotationErrorDegrees);
                return false;
            }
        }
        TrackTransformsByName.Add(TrackName, MoveTemp(TrackTransforms));
    }
    for (const FCurveSpec& Curve : BuildCurveSpecs())
    {
        const FAnimationCurveIdentifier Id(Curve.Name, ERawCurveTrackTypes::RCT_Float);
        const FRichCurve* RichCurve = Model->FindRichCurve(Id);
        if (!RichCurve || RichCurve->GetNumKeys() != Curve.Keys.Num())
        {
            Error = FString::Printf(TEXT("Animation curve %s is missing or has the wrong key count"), *Curve.Name.ToString());
            return false;
        }
        for (const TPair<int32, float>& Key : Curve.Keys)
        {
            if (!NearlyEqual(RichCurve->Eval(FrameToTime(Key.Key)), Key.Value, 0.001f))
            {
                Error = FString::Printf(TEXT("Animation curve %s value mismatch at frame %d"), *Curve.Name.ToString(), Key.Key);
                return false;
            }
        }
    }

    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefPose, RefComponentPose);
    struct FPoseSampleSpec
    {
        const TCHAR* Label;
        int32 Frame;
    };
    constexpr FPoseSampleSpec PoseSamples[] = {
        {TEXT("Setup"), 0},
        {TEXT("ReachBack"), 54},
        {TEXT("Release"), ReleaseFrame},
        {TEXT("FollowThrough"), 112},
        {TEXT("Recovery"), 138},
        {TEXT("ThrowFinished"), FinishFrame},
    };

    TMap<int32, TArray<FTransform>> ComponentPoseByFrame;
    TArray<TSharedPtr<FJsonValue>> PoseSampleJson;
    float OverallMinSegmentRatio = TNumericLimits<float>::Max();
    float OverallMaxSegmentRatio = 0.f;
    float OverallMaxComponentDeltaDegrees = 0.f;
    float OverallMaxRootDistanceCm = 0.f;
    for (const FPoseSampleSpec& Spec : PoseSamples)
    {
        TArray<FTransform> LocalPose = RefPose;
        for (const TPair<FName, TArray<FTransform>>& TrackPair : TrackTransformsByName)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(TrackPair.Key);
            if (LocalPose.IsValidIndex(BoneIndex) && TrackPair.Value.IsValidIndex(Spec.Frame))
            {
                LocalPose[BoneIndex] = TrackPair.Value[Spec.Frame];
            }
        }
        TArray<FTransform> ComponentPose;
        BuildComponentPose(RefSkeleton, LocalPose, ComponentPose);

        float MinSegmentRatio = TNumericLimits<float>::Max();
        float MaxSegmentRatio = 0.f;
        float MaxComponentDeltaDegrees = 0.f;
        float MaxRootDistanceCm = 0.f;
        for (int32 BoneIndex = 0; BoneIndex < ComponentPose.Num(); ++BoneIndex)
        {
            const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
            if (ParentIndex != INDEX_NONE)
            {
                const float ReferenceLength = FVector::Distance(
                    RefComponentPose[BoneIndex].GetTranslation(),
                    RefComponentPose[ParentIndex].GetTranslation());
                if (ReferenceLength > 0.01f)
                {
                    const float PosedLength = FVector::Distance(
                        ComponentPose[BoneIndex].GetTranslation(),
                        ComponentPose[ParentIndex].GetTranslation());
                    const float SegmentRatio = PosedLength / ReferenceLength;
                    MinSegmentRatio = FMath::Min(MinSegmentRatio, SegmentRatio);
                    MaxSegmentRatio = FMath::Max(MaxSegmentRatio, SegmentRatio);
                }
            }
            const float ComponentDeltaDegrees = FMath::RadiansToDegrees(static_cast<float>(
                ComponentPose[BoneIndex].GetRotation().AngularDistance(
                    RefComponentPose[BoneIndex].GetRotation())));
            MaxComponentDeltaDegrees = FMath::Max(MaxComponentDeltaDegrees, ComponentDeltaDegrees);
            MaxRootDistanceCm = FMath::Max(MaxRootDistanceCm, FVector::Distance(
                ComponentPose[BoneIndex].GetTranslation(), ComponentPose[0].GetTranslation()));
        }
        if (MinSegmentRatio == TNumericLimits<float>::Max())
        {
            Error = FString::Printf(TEXT("RHBH %s pose has no measurable skeleton segments"), Spec.Label);
            return false;
        }
        if (MinSegmentRatio < 0.998f || MaxSegmentRatio > 1.002f)
        {
            Error = FString::Printf(TEXT("RHBH %s pose changes joint connectivity: ratios %.6f..%.6f"),
                Spec.Label, MinSegmentRatio, MaxSegmentRatio);
            return false;
        }
        if (MaxComponentDeltaDegrees > 105.f)
        {
            Error = FString::Printf(
                TEXT("RHBH %s pose has a %.3f degree component rotation; local rotations may be stacking"),
                Spec.Label, MaxComponentDeltaDegrees);
            return false;
        }
        if (MaxRootDistanceCm > 225.f)
        {
            Error = FString::Printf(TEXT("RHBH %s pose skeleton extent is %.3f cm from root"),
                Spec.Label, MaxRootDistanceCm);
            return false;
        }

        OverallMinSegmentRatio = FMath::Min(OverallMinSegmentRatio, MinSegmentRatio);
        OverallMaxSegmentRatio = FMath::Max(OverallMaxSegmentRatio, MaxSegmentRatio);
        OverallMaxComponentDeltaDegrees = FMath::Max(
            OverallMaxComponentDeltaDegrees, MaxComponentDeltaDegrees);
        OverallMaxRootDistanceCm = FMath::Max(OverallMaxRootDistanceCm, MaxRootDistanceCm);
        ComponentPoseByFrame.Add(Spec.Frame, ComponentPose);

        const TSharedRef<FJsonObject> SampleObject = MakeShared<FJsonObject>();
        SampleObject->SetStringField(TEXT("phase"), Spec.Label);
        SampleObject->SetNumberField(TEXT("frame"), Spec.Frame);
        SampleObject->SetNumberField(TEXT("time_seconds"), FrameToTime(Spec.Frame));
        SampleObject->SetNumberField(TEXT("minimum_segment_ratio"), MinSegmentRatio);
        SampleObject->SetNumberField(TEXT("maximum_segment_ratio"), MaxSegmentRatio);
        SampleObject->SetNumberField(TEXT("maximum_component_delta_degrees"), MaxComponentDeltaDegrees);
        SampleObject->SetNumberField(TEXT("maximum_root_distance_cm"), MaxRootDistanceCm);
        PoseSampleJson.Add(MakeShared<FJsonValueObject>(SampleObject));
    }

    const int32 ThrowingArmIndex = RefSkeleton.FindBoneIndex(TEXT("upperarm_r"));
    const int32 GripIndex = RefSkeleton.FindBoneIndex(TEXT("disc_grip_r"));
    if (ThrowingArmIndex == INDEX_NONE || GripIndex == INDEX_NONE ||
        !ComponentPoseByFrame.Contains(54) || !ComponentPoseByFrame.Contains(ReleaseFrame))
    {
        Error = TEXT("RHBH component-space sweep fixtures are incomplete");
        return false;
    }
    const TArray<FTransform>& ReachbackPose = ComponentPoseByFrame.FindChecked(54);
    const TArray<FTransform>& ReleasePose = ComponentPoseByFrame.FindChecked(ReleaseFrame);
    const float ThrowingArmSweepDegrees = FMath::RadiansToDegrees(
        static_cast<float>(ReachbackPose[ThrowingArmIndex].GetRotation().AngularDistance(
            ReleasePose[ThrowingArmIndex].GetRotation())));
    const float ThrowingHandSweepCm = FVector::Distance(
        ReachbackPose[GripIndex].GetTranslation(), ReleasePose[GripIndex].GetTranslation());
    if (ThrowingArmSweepDegrees < 65.f || ThrowingHandSweepCm < 35.f)
    {
        Error = FString::Printf(
            TEXT("RHBH component-space throw sweep is insufficient: arm %.3f degrees, grip %.3f cm"),
            ThrowingArmSweepDegrees, ThrowingHandSweepCm);
        return false;
    }

    OutObject->SetStringField(TEXT("status"), TEXT("PASS"));
    OutObject->SetStringField(TEXT("path"), Sequence->GetPathName());
    OutObject->SetNumberField(TEXT("frame_rate"), FrameRate);
    OutObject->SetNumberField(TEXT("frame_count"), FrameCount);
    OutObject->SetNumberField(TEXT("key_count_per_track"), FrameCount + 1);
    OutObject->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
    OutObject->SetNumberField(TEXT("animated_bone_tracks"), TrackNames.Num());
    OutObject->SetNumberField(TEXT("motion_curves"), BuildCurveSpecs().Num());
    OutObject->SetNumberField(TEXT("throwing_arm_sweep_degrees"), ThrowingArmSweepDegrees);
    OutObject->SetNumberField(TEXT("throwing_hand_sweep_cm"), ThrowingHandSweepCm);
    OutObject->SetStringField(TEXT("pose_rotation_basis"),
        TEXT("COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"));
    OutObject->SetNumberField(TEXT("minimum_joint_segment_ratio"), OverallMinSegmentRatio);
    OutObject->SetNumberField(TEXT("maximum_joint_segment_ratio"), OverallMaxSegmentRatio);
    OutObject->SetNumberField(TEXT("maximum_component_delta_degrees"),
        OverallMaxComponentDeltaDegrees);
    OutObject->SetNumberField(TEXT("maximum_root_distance_cm"), OverallMaxRootDistanceCm);
    OutObject->SetArrayField(TEXT("pose_connectivity_samples"), PoseSampleJson);
    OutObject->SetStringField(TEXT("root_motion_policy"), TEXT("IN_PLACE_SMALL_VALIDATION_SHIFTS_ONLY"));
    return true;
}

bool ValidateMontageAsset(UAnimMontage* Montage, UAnimSequence* Sequence, USkeleton* Skeleton,
    TSharedPtr<FJsonObject>& OutObject, FString& Error)
{
    OutObject = MakeShared<FJsonObject>();
    if (!Montage)
    {
        Error = TEXT("AM_DG_RHBH_Prototype is missing");
        return false;
    }
    if (Montage->GetSkeleton() != Skeleton || !NearlyEqual(Montage->GetPlayLength(), DurationSeconds))
    {
        Error = TEXT("RHBH montage skeleton or duration mismatch");
        return false;
    }
    if (Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].SlotName != DefaultSlot ||
        Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1 ||
        Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference() != Sequence)
    {
        Error = TEXT("RHBH montage must contain exactly one A_DG_RHBH_Prototype segment on DefaultSlot");
        return false;
    }

    int32 ReleaseCount = 0;
    int32 FinishCount = 0;
    TMap<FString, int32> PhaseCounts;
    TMap<FString, float> PhaseTimes;
    float ReleaseTime = -1.f;
    float FinishTime = -1.f;
    TArray<TSharedPtr<FJsonValue>> NotifyJson;
    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    {
        if (!Event.Notify)
        {
            Error = TEXT("RHBH montage contains a named/state notify instead of the approved native notify classes");
            return false;
        }
        const FString ClassPath = Event.Notify->GetClass()->GetPathName();
        const float Time = Event.GetTriggerTime();
        FString Label;
        if (ClassPath == PhaseNotifyClassPath)
        {
            Label = PhaseNameFromNotify(Event.Notify);
            ++PhaseCounts.FindOrAdd(Label);
            PhaseTimes.Add(Label, Time);
        }
        else if (ClassPath == ReleaseNotifyClassPath)
        {
            Label = TEXT("DG Release Disc");
            ++ReleaseCount;
            ReleaseTime = Time;
            if (Event.MontageTickType != EMontageNotifyTickType::BranchingPoint)
            {
                Error = TEXT("DG Release Disc must be a montage branching point");
                return false;
            }
        }
        else if (ClassPath == FinishNotifyClassPath)
        {
            Label = TEXT("DG Throw Finished");
            ++FinishCount;
            FinishTime = Time;
            if (Event.MontageTickType != EMontageNotifyTickType::BranchingPoint)
            {
                Error = TEXT("DG Throw Finished must be a montage branching point");
                return false;
            }
        }
        else
        {
            Error = FString::Printf(TEXT("Unexpected montage notify class: %s"), *ClassPath);
            return false;
        }
        const TSharedRef<FJsonObject> EventObject = MakeShared<FJsonObject>();
        EventObject->SetStringField(TEXT("event"), Label);
        EventObject->SetStringField(TEXT("class"), ClassPath);
        EventObject->SetNumberField(TEXT("time_seconds"), Time);
        EventObject->SetNumberField(TEXT("frame"), FMath::RoundToInt(Time * FrameRate));
        NotifyJson.Add(MakeShared<FJsonValueObject>(EventObject));
    }
    if (ReleaseCount != 1 || FinishCount != 1 || Montage->Notifies.Num() != UE_ARRAY_COUNT(PhaseEvents) + 2 ||
        !NearlyEqual(ReleaseTime, FrameToTime(ReleaseFrame)) ||
        !NearlyEqual(FinishTime, FrameToTime(FinishFrame)))
    {
        Error = FString::Printf(TEXT("Release/finish notify cardinality or timing mismatch: release=%d finish=%d total=%d"),
            ReleaseCount, FinishCount, Montage->Notifies.Num());
        return false;
    }
    for (const FPhaseEventSpec& Spec : PhaseEvents)
    {
        const FString Name(Spec.Phase);
        if (PhaseCounts.FindRef(Name) != 1 || !NearlyEqual(PhaseTimes.FindRef(Name), FrameToTime(Spec.Frame)))
        {
            Error = FString::Printf(TEXT("Phase event %s is missing, duplicated, or mistimed"), Spec.Phase);
            return false;
        }
    }

    OutObject->SetStringField(TEXT("status"), TEXT("PASS"));
    OutObject->SetStringField(TEXT("path"), Montage->GetPathName());
    OutObject->SetStringField(TEXT("slot"), DefaultSlot.ToString());
    OutObject->SetNumberField(TEXT("duration_seconds"), Montage->GetPlayLength());
    OutObject->SetNumberField(TEXT("release_frame"), ReleaseFrame);
    OutObject->SetNumberField(TEXT("release_time_seconds"), ReleaseTime);
    OutObject->SetNumberField(TEXT("finish_frame"), FinishFrame);
    OutObject->SetNumberField(TEXT("finish_time_seconds"), FinishTime);
    OutObject->SetNumberField(TEXT("release_notify_count"), ReleaseCount);
    OutObject->SetNumberField(TEXT("finish_notify_count"), FinishCount);
    OutObject->SetArrayField(TEXT("events"), NotifyJson);
    return true;
}

bool ValidateAnimGraphAsset(UAnimBlueprint* Blueprint, USkeleton* Skeleton,
    TSharedPtr<FJsonObject>& OutObject, FString& Error)
{
    OutObject = MakeShared<FJsonObject>();
    if (!Blueprint)
    {
        Error = TEXT("ABP_DG_Player is missing");
        return false;
    }
    if (Blueprint->TargetSkeleton != Skeleton)
    {
        Error = TEXT("ABP_DG_Player target skeleton mismatch");
        return false;
    }
    if (!Blueprint->ParentClass || Blueprint->ParentClass->GetPathName() != AnimInstanceParentPath)
    {
        Error = FString::Printf(TEXT("ABP_DG_Player parent mismatch: %s"),
            Blueprint->ParentClass ? *Blueprint->ParentClass->GetPathName() : TEXT("None"));
        return false;
    }
    if (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings)
    {
        Error = FString::Printf(TEXT("ABP_DG_Player is not compiled: status=%d"), static_cast<int32>(Blueprint->Status));
        return false;
    }
    UEdGraph* Graph = FindAnimGraph(Blueprint);
    if (!Graph || (Graph->Nodes.Num() != 3 && Graph->Nodes.Num() != 4))
    {
        Error = TEXT("ABP_DG_Player AnimGraph must match the exact Session 3 or Session 4 pose-node contract");
        return false;
    }
    const bool bSession4Graph = Graph->Nodes.Num() == 4;

    UEdGraphNode* RefNode = nullptr;
    UEdGraphNode* SlotNode = nullptr;
    UEdGraphNode* ControlRigNode = nullptr;
    UEdGraphNode* RootNode = nullptr;
    TArray<TSharedPtr<FJsonValue>> NodeJson;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        const FString Path = Node ? Node->GetClass()->GetPathName() : FString();
        if (Path == LocalRefPoseClassPath) RefNode = Node;
        else if (Path == SlotClassPath) SlotNode = Node;
        else if (bSession4Graph && Path == ControlRigNodeClassPath) ControlRigNode = Node;
        else if (Path == RootClassPath) RootNode = Node;
        else
        {
            Error = FString::Printf(TEXT("Unexpected ABP AnimGraph node class: %s"), *Path);
            return false;
        }
        NodeJson.Add(MakeShared<FJsonValueString>(Path));
    }
    if (!RefNode || !SlotNode || !RootNode || ReadSlotName(SlotNode) != DefaultSlot)
    {
        Error = TEXT("ABP_DG_Player node set or slot name is invalid");
        return false;
    }
    if (bSession4Graph)
    {
        if (!ControlRigNode)
        {
            Error = TEXT("Session 4 ABP graph is missing its post-montage Control Rig node");
            return false;
        }
        const FString Session4Validation = UDiscGolfSession4AssetUtility::ValidateSession4Assets();
        TSharedPtr<FJsonObject> Session4Json;
        const TSharedRef<TJsonReader<>> Session4Reader = TJsonReaderFactory<>::Create(Session4Validation);
        FString Session4Status;
        if (!FJsonSerializer::Deserialize(Session4Reader, Session4Json) || !Session4Json.IsValid() ||
            !Session4Json->TryGetStringField(TEXT("status"), Session4Status) ||
            !Session4Status.StartsWith(TEXT("PASS")))
        {
            Error = TEXT("Session 4 ABP graph failed its strict Control Rig class, mapping, transfer, or pose-flow contract");
            return false;
        }

        OutObject->SetStringField(TEXT("status"), TEXT("PASS"));
        OutObject->SetStringField(TEXT("path"), Blueprint->GetPathName());
        OutObject->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetPathName());
        OutObject->SetStringField(TEXT("slot"), DefaultSlot.ToString());
        OutObject->SetStringField(TEXT("pose_flow"), TEXT("LocalRefPose -> DefaultSlot -> ControlRig -> Root"));
        OutObject->SetStringField(TEXT("session4_contract"), Session4Status);
        OutObject->SetArrayField(TEXT("node_classes"), NodeJson);
        return true;
    }
    UEdGraphPin* RefOut = FindPosePin(RefNode, EGPD_Output);
    UEdGraphPin* SlotIn = FindPosePin(SlotNode, EGPD_Input);
    UEdGraphPin* SlotOut = FindPosePin(SlotNode, EGPD_Output);
    UEdGraphPin* RootIn = FindPosePin(RootNode, EGPD_Input);
    if (!RefOut || !SlotIn || !SlotOut || !RootIn ||
        !RefOut->LinkedTo.Contains(SlotIn) || !SlotIn->LinkedTo.Contains(RefOut) ||
        !SlotOut->LinkedTo.Contains(RootIn) || !RootIn->LinkedTo.Contains(SlotOut))
    {
        Error = TEXT("ABP_DG_Player pose graph is not RefPose -> DefaultSlot -> Root");
        return false;
    }

    OutObject->SetStringField(TEXT("status"), TEXT("PASS"));
    OutObject->SetStringField(TEXT("path"), Blueprint->GetPathName());
    OutObject->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetPathName());
    OutObject->SetStringField(TEXT("slot"), DefaultSlot.ToString());
    OutObject->SetStringField(TEXT("pose_flow"), TEXT("LocalRefPose -> DefaultSlot -> Root"));
    OutObject->SetArrayField(TEXT("node_classes"), NodeJson);
    return true;
}

FString BuildValidationReport(FString* OutError = nullptr)
{
    FString Error;
    USkeleton* Skeleton = LoadChecked<USkeleton>(SkeletonPath, Error);
    UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, SequencePath);
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, MontagePath);
    UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, AnimBlueprintPath);
    if (!Skeleton)
    {
        if (OutError) *OutError = Error;
        return FailureJson(Error);
    }

    TSharedPtr<FJsonObject> SequenceJson;
    TSharedPtr<FJsonObject> MontageJson;
    TSharedPtr<FJsonObject> BlueprintJson;
    if (!ValidateSequenceAsset(Sequence, Skeleton, SequenceJson, Error) ||
        !ValidateMontageAsset(Montage, Sequence, Skeleton, MontageJson, Error) ||
        !ValidateAnimGraphAsset(Blueprint, Skeleton, BlueprintJson, Error))
    {
        if (OutError) *OutError = Error;
        return FailureJson(Error);
    }

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), TEXT("PASS_STRICT_SESSION3_ASSET_CONTRACT"));
    Root->SetStringField(TEXT("scope"), TEXT("SESSION_3_RHBH_ANIMATION_MONTAGE_AND_ABP"));
    Root->SetObjectField(TEXT("animation_sequence"), SequenceJson);
    Root->SetObjectField(TEXT("montage"), MontageJson);
    Root->SetObjectField(TEXT("animation_blueprint"), BlueprintJson);
    Root->SetStringField(TEXT("flight_authority"), TEXT("NOT_PRESENT_IN_EDITOR_ASSET_UTILITY"));
    Root->SetStringField(TEXT("body_profile_compatibility"), TEXT("SHARED_SK_DG_MASTER_SKELETON_SHORT_BASELINE_TALL"));
    if (OutError) OutError->Reset();
    return JsonString(Root);
}
} // namespace DiscGolfSession3Assets

FString UDiscGolfSession3AssetUtility::ValidateSession3Assets()
{
    return DiscGolfSession3Assets::BuildValidationReport();
}

FString UDiscGolfSession3AssetUtility::AuthorSession3Assets()
{
    using namespace DiscGolfSession3Assets;
    FString Error;
    USkeleton* Skeleton = LoadChecked<USkeleton>(SkeletonPath, Error);
    USkeletalMesh* Mesh = LoadChecked<USkeletalMesh>(MeshPath, Error);
    UAnimBlueprint* Blueprint = LoadChecked<UAnimBlueprint>(AnimBlueprintPath, Error);
    if (!Skeleton || !Mesh || !Blueprint)
    {
        return FailureJson(Error);
    }

    TArray<FString> Writes;
    TSharedPtr<FJsonObject> Ignored;
    UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, SequencePath);
    if (!ValidateSequenceAsset(Sequence, Skeleton, Ignored, Error))
    {
        bool bCreated = false;
        if (!AuthorSequence(Sequence, Skeleton, Mesh, bCreated, Error))
        {
            return FailureJson(Error);
        }
        Writes.Add(Sequence->GetPathName());
    }

    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, MontagePath);
    if (!ValidateMontageAsset(Montage, Sequence, Skeleton, Ignored, Error))
    {
        bool bCreated = false;
        if (!AuthorMontage(Montage, Sequence, Skeleton, Mesh, bCreated, Error))
        {
            return FailureJson(Error);
        }
        Writes.Add(Montage->GetPathName());
    }

    if (!ValidateAnimGraphAsset(Blueprint, Skeleton, Ignored, Error))
    {
        if (!AuthorAnimGraph(Blueprint, Error))
        {
            return FailureJson(Error);
        }
        Writes.Add(Blueprint->GetPathName());
    }

    FString ValidationError;
    const FString Validation = BuildValidationReport(&ValidationError);
    if (!ValidationError.IsEmpty())
    {
        return FailureJson(FString::Printf(TEXT("Post-author validation failed: %s"), *ValidationError));
    }
    TSharedPtr<FJsonObject> ValidationObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Validation);
    if (!FJsonSerializer::Deserialize(Reader, ValidationObject) || !ValidationObject.IsValid())
    {
        return FailureJson(TEXT("Could not parse the strict post-author validation report"));
    }
    ValidationObject->SetStringField(TEXT("authoring_status"),
        Writes.IsEmpty() ? TEXT("PASS_ALREADY_CURRENT_NO_ASSET_WRITES") : TEXT("PASS_AUTHORED_AND_VALIDATED"));
    TArray<TSharedPtr<FJsonValue>> WriteJson;
    for (const FString& Path : Writes)
    {
        WriteJson.Add(MakeShared<FJsonValueString>(Path));
    }
    ValidationObject->SetArrayField(TEXT("asset_writes"), WriteJson);
    return JsonString(ValidationObject.ToSharedRef());
}
