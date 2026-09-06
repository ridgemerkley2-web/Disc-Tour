#include "DiscGolfSession5MocapUtility.h"

#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DiscGolfAnimationLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/DataAssetFactory.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargetFactory.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetOps.h"
#include "Retargeter/RetargetOps/CurveRemapOp.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigController.h"
#include "RigEditor/IKRigDefinitionFactory.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace DiscGolfSession5Mocap
{
constexpr int32 SchemaVersion = 1;
constexpr int32 FrameRate = 60;
constexpr int32 FrameCount = 168;
constexpr float DurationSeconds = 2.8f;
constexpr int32 ReleaseFrame = 96;
constexpr int32 FinishFrame = 162;
constexpr float MinimumBodyHeightCm = 80.0f;
constexpr float MaximumBodyHeightCm = 260.0f;
constexpr float MinimumPelvisToHeadCm = 25.0f;
constexpr float MaximumPelvisToHeadCm = 150.0f;
constexpr float MinimumRootToPelvisCm = 40.0f;
constexpr float MaximumRootToPelvisCm = 150.0f;
constexpr float MaximumKeyBoneRadiusCm = 300.0f;
constexpr float MaximumSegmentRatioError = 0.15f;
constexpr float MaximumComponentScaleRatioError = 0.01f;
constexpr int32 MinimumComparedSegments = 16;

const TCHAR* MasterMeshPath = TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master");
const TCHAR* MasterSkeletonPath = TEXT("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master");
const TCHAR* MasterIKRigPath = TEXT("/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master");
const TCHAR* PrototypeSequencePath = TEXT("/Game/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.A_DG_RHBH_Prototype");
const TCHAR* PrototypeMontagePath = TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype");

const TCHAR* SourceMeshPath = TEXT("/Game/DiscGolf/Animation/Mocap/Source/SK_DG_RHBH_SyntheticSource.SK_DG_RHBH_SyntheticSource");
const TCHAR* SourceSkeletonPath = TEXT("/Game/DiscGolf/Animation/Mocap/Source/SKEL_DG_RHBH_SyntheticSource.SKEL_DG_RHBH_SyntheticSource");
const TCHAR* RawSequencePath = TEXT("/Game/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW.A_DG_RHBH_SyntheticSource_RAW");
const TCHAR* SourceIKRigPath = TEXT("/Game/DiscGolf/Animation/Mocap/Rigs/IK_DG_RHBH_SyntheticSource.IK_DG_RHBH_SyntheticSource");
const TCHAR* RetargeterPath = TEXT("/Game/DiscGolf/Animation/Mocap/Rigs/RTG_DG_RHBH_Synthetic_To_Master.RTG_DG_RHBH_Synthetic_To_Master");
const TCHAR* RetargetedSequencePath = TEXT("/Game/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG.A_DG_RHBH_Synthetic_RTG");
const TCHAR* CleanedSequencePath = TEXT("/Game/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN.A_DG_RHBH_Synthetic_CLN");
const TCHAR* FinalSequencePath = TEXT("/Game/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001.A_DG_RHBH_SyntheticPipelineTest_v001");
const TCHAR* FinalMontagePath = TEXT("/Game/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001.AM_DG_RHBH_SyntheticPipelineTest_v001");
const TCHAR* LibraryPath = TEXT("/Game/DiscGolf/Animation/Mocap/Production/DA_DG_AnimationLibrary.DA_DG_AnimationLibrary");

const TCHAR* PhaseNotifyClassPath = TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_ThrowPhase");
const TCHAR* ReleaseNotifyClassPath = TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_DiscRelease");
const TCHAR* FinishNotifyClassPath = TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_ThrowFinished");
const FName DefaultSlot(TEXT("DefaultSlot"));
const FName SyntheticStyleId(TEXT("SyntheticTest_DO_NOT_SHIP"));

const TCHAR* MetadataSchema = TEXT("DG_MocapSchemaVersion");
const TCHAR* MetadataClassification = TEXT("DG_SourceClassification");
const TCHAR* MetadataShipping = TEXT("DG_ShippingPolicy");
const TCHAR* MetadataLicense = TEXT("DG_LicenseStatus");
const TCHAR* MetadataStage = TEXT("DG_PipelineStage");
const TCHAR* MetadataPredecessor = TEXT("DG_PipelinePredecessor");
const TCHAR* MetadataFixture = TEXT("DG_FixtureIdentity");
const TCHAR* MetadataCleanupRecipe = TEXT("DG_CleanupRecipe");
const TCHAR* MetadataRetargetUnitNormalization = TEXT("DG_RetargetUnitNormalization");
const TCHAR* CleanupRecipe = TEXT("ROOT_XY_LOCK_TO_FRAME0;ROOT_Z_DELTA_CLAMP_3CM;REMOVE_SEQUENCE_NOTIFIES;REMOVE_TRANSFORM_CURVES;REAUTHOR_6_DG_CURVES");
const TCHAR* RetargetUnitNormalization = TEXT("LEGACY_NON_UNIT_ROOT_SCALE_COMPATIBILITY_NORMALIZATION_V1");

struct FChainSpec
{
    const TCHAR* Chain;
    const TCHAR* StartBone;
    const TCHAR* EndBone;
};

constexpr FChainSpec ChainSpecs[] = {
    {TEXT("Root"), TEXT("root"), TEXT("root")},
    {TEXT("Spine"), TEXT("spine_01"), TEXT("spine_04")},
    {TEXT("Neck"), TEXT("neck_01"), TEXT("head")},
    {TEXT("Arm_L"), TEXT("upperarm_l"), TEXT("hand_l")},
    {TEXT("Arm_R"), TEXT("upperarm_r"), TEXT("hand_r")},
    {TEXT("Leg_L"), TEXT("thigh_l"), TEXT("foot_l")},
    {TEXT("Leg_R"), TEXT("thigh_r"), TEXT("foot_r")},
    {TEXT("Foot_L"), TEXT("foot_l"), TEXT("ball_l")},
    {TEXT("Foot_R"), TEXT("foot_r"), TEXT("ball_r")},
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

constexpr int32 EvaluatedPoseFrames[] = {0, 54, 96, 112, 138, 162};

const TCHAR* EvaluatedSegmentBones[] = {
    TEXT("pelvis"),
    TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
    TEXT("neck_01"), TEXT("head"),
    TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
    TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
    TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
    TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"),
};

const TCHAR* UnitProbeBones[] = {
    TEXT("root"), TEXT("pelvis"), TEXT("spine_01"), TEXT("head"),
    TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r"),
};

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

float FrameToTime(const int32 Frame)
{
    return static_cast<float>(Frame) / static_cast<float>(FrameRate);
}

bool NearlyEqual(const float A, const float B, const float Tolerance = 0.001f)
{
    return FMath::Abs(A - B) <= Tolerance;
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
    Root->SetStringField(TEXT("source_classification"), TEXT("SYNTHETIC_TEST"));
    Root->SetStringField(TEXT("shipping_policy"), TEXT("DO_NOT_SHIP"));
    return JsonString(Root);
}

template <typename TObjectType>
TObjectType* LoadChecked(const TCHAR* Path, FString& Error)
{
    TObjectType* Asset = LoadObject<TObjectType>(nullptr, Path);
    if (!Asset)
    {
        Error = FString::Printf(TEXT("Required asset did not load with expected class: %s"), Path);
    }
    return Asset;
}

bool RefuseObjectPathCollision(const TCHAR* ObjectPath, FString& Error)
{
    if (UObject* Existing = LoadObject<UObject>(nullptr, ObjectPath))
    {
        Error = FString::Printf(TEXT("Object path already exists with unexpected state or class: %s (%s)"),
            ObjectPath, *Existing->GetClass()->GetPathName());
        return false;
    }
    return true;
}

bool SaveAsset(UObject* Asset, FString& Error)
{
    if (!Asset)
    {
        Error = TEXT("Attempted to save a null Session 5 asset");
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

void RecordWrite(TArray<FString>& Writes, const UObject* Asset)
{
    if (Asset)
    {
        Writes.AddUnique(Asset->GetPathName());
    }
}

void SetSyntheticMetadata(UObject* Asset, const TCHAR* Stage, const TCHAR* Predecessor)
{
    FMetaData& Metadata = Asset->GetOutermost()->GetMetaData();
    Metadata.SetValue(Asset, MetadataSchema, TEXT("1"));
    Metadata.SetValue(Asset, MetadataClassification, TEXT("SYNTHETIC_TEST"));
    Metadata.SetValue(Asset, MetadataShipping, TEXT("DO_NOT_SHIP"));
    Metadata.SetValue(Asset, MetadataLicense, TEXT("PROJECT_OWNED_SYNTHETIC"));
    Metadata.SetValue(Asset, MetadataStage, Stage);
    Metadata.SetValue(Asset, MetadataPredecessor, Predecessor);
    Metadata.SetValue(Asset, MetadataFixture, TEXT("DG_SESSION5_RHBH_SYNTHETIC_PIPELINE_V1"));
}

void SetCleanupMetadata(UObject* Asset)
{
    Asset->GetOutermost()->GetMetaData().SetValue(
        Asset, MetadataCleanupRecipe, CleanupRecipe);
}

void SetRetargetUnitNormalizationMetadata(UObject* Asset)
{
    Asset->GetOutermost()->GetMetaData().SetValue(
        Asset, MetadataRetargetUnitNormalization, RetargetUnitNormalization);
}

bool ValidateSyntheticMetadata(UObject* Asset, const TCHAR* Stage, const TCHAR* Predecessor,
    FString& Error)
{
    if (!Asset)
    {
        Error = TEXT("Cannot validate metadata on a null asset");
        return false;
    }
    FMetaData& Metadata = Asset->GetOutermost()->GetMetaData();
    struct FExpectedValue
    {
        const TCHAR* Key;
        const TCHAR* Value;
    };
    const FExpectedValue Expected[] = {
        {MetadataSchema, TEXT("1")},
        {MetadataClassification, TEXT("SYNTHETIC_TEST")},
        {MetadataShipping, TEXT("DO_NOT_SHIP")},
        {MetadataLicense, TEXT("PROJECT_OWNED_SYNTHETIC")},
        {MetadataStage, Stage},
        {MetadataPredecessor, Predecessor},
        {MetadataFixture, TEXT("DG_SESSION5_RHBH_SYNTHETIC_PIPELINE_V1")},
    };
    for (const FExpectedValue& Pair : Expected)
    {
        const FString Actual = Metadata.GetValue(Asset, Pair.Key);
        if (Actual != Pair.Value)
        {
            Error = FString::Printf(TEXT("%s metadata %s is '%s', expected '%s'"),
                *Asset->GetPathName(), Pair.Key, *Actual, Pair.Value);
            return false;
        }
    }
    return true;
}

template <typename TObjectType>
TObjectType* DuplicateAssetAt(const TCHAR* ObjectPath, UObject* Source, FString& Error)
{
    if (!Source || !RefuseObjectPathCollision(ObjectPath, Error))
    {
        if (!Source && Error.IsEmpty())
        {
            Error = FString::Printf(TEXT("Cannot duplicate a null source to %s"), ObjectPath);
        }
        return nullptr;
    }
    const FString PackageName = FPackageName::ObjectPathToPackageName(FString(ObjectPath));
    const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
    const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(
        TEXT("AssetTools")).Get();
    TObjectType* Duplicate = Cast<TObjectType>(AssetTools.DuplicateAsset(
        AssetName, PackagePath, Source));
    if (!Duplicate)
    {
        Error = FString::Printf(TEXT("AssetTools failed to duplicate %s to %s"),
            *Source->GetPathName(), ObjectPath);
    }
    return Duplicate;
}

template <typename TObjectType, typename TFactoryType>
TObjectType* CreateFactoryAsset(const TCHAR* ObjectPath, FString& Error)
{
    if (!RefuseObjectPathCollision(ObjectPath, Error))
    {
        return nullptr;
    }
    const FString PackageName = FPackageName::ObjectPathToPackageName(FString(ObjectPath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(FString(ObjectPath));
    UPackage* Package = CreatePackage(*PackageName);
    TFactoryType* Factory = NewObject<TFactoryType>();
    TObjectType* Asset = Cast<TObjectType>(Factory->FactoryCreateNew(
        TObjectType::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!Asset)
    {
        Error = FString::Printf(TEXT("Factory failed to create %s"), ObjectPath);
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Asset);
    return Asset;
}

bool ReferenceSkeletonsMatch(const USkeleton* A, const USkeleton* B, FString& Error)
{
    if (!A || !B)
    {
        Error = TEXT("Cannot compare null skeletons");
        return false;
    }
    const FReferenceSkeleton& RefA = A->GetReferenceSkeleton();
    const FReferenceSkeleton& RefB = B->GetReferenceSkeleton();
    if (RefA.GetNum() != RefB.GetNum())
    {
        Error = FString::Printf(TEXT("Synthetic source skeleton bone count %d differs from master %d"),
            RefA.GetNum(), RefB.GetNum());
        return false;
    }
    for (int32 Index = 0; Index < RefA.GetNum(); ++Index)
    {
        if (RefA.GetBoneName(Index) != RefB.GetBoneName(Index) ||
            RefA.GetParentIndex(Index) != RefB.GetParentIndex(Index))
        {
            Error = FString::Printf(TEXT("Synthetic source hierarchy differs at bone index %d"), Index);
            return false;
        }
    }
    return true;
}

bool ValidateSourceAssets(USkeletalMesh* SourceMesh, USkeleton* SourceSkeleton,
    USkeletalMesh* MasterMesh, USkeleton* MasterSkeleton, FString& Error)
{
    if (!SourceMesh || !SourceSkeleton)
    {
        Error = TEXT("Synthetic source mesh or skeleton is missing");
        return false;
    }
    if (SourceMesh == MasterMesh || SourceSkeleton == MasterSkeleton ||
        SourceMesh->GetOutermost() == MasterMesh->GetOutermost() ||
        SourceSkeleton->GetOutermost() == MasterSkeleton->GetOutermost())
    {
        Error = TEXT("Session 5 source mesh/skeleton must be distinct project-owned duplicates");
        return false;
    }
    if (SourceMesh->GetSkeleton() != SourceSkeleton)
    {
        Error = TEXT("Synthetic source mesh is not bound to its distinct source skeleton");
        return false;
    }
    if (!ReferenceSkeletonsMatch(SourceSkeleton, MasterSkeleton, Error))
    {
        return false;
    }
    return ValidateSyntheticMetadata(SourceSkeleton, TEXT("SOURCE_SKELETON"), MasterSkeletonPath, Error) &&
        ValidateSyntheticMetadata(SourceMesh, TEXT("SOURCE_MESH"), MasterMeshPath, Error);
}

bool EnsureSourceAssets(USkeletalMesh* MasterMesh, USkeleton* MasterSkeleton,
    USkeletalMesh*& OutSourceMesh, USkeleton*& OutSourceSkeleton,
    TArray<FString>& Writes, FString& Error)
{
    OutSourceSkeleton = LoadObject<USkeleton>(nullptr, SourceSkeletonPath);
    OutSourceMesh = LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
    bool bSkeletonCreated = false;
    bool bMeshCreated = false;

    if (!OutSourceSkeleton)
    {
        if (LoadObject<UObject>(nullptr, SourceSkeletonPath))
        {
            Error = TEXT("Synthetic source skeleton path contains an unexpected class");
            return false;
        }
        OutSourceSkeleton = DuplicateAssetAt<USkeleton>(SourceSkeletonPath, MasterSkeleton, Error);
        if (!OutSourceSkeleton)
        {
            return false;
        }
        bSkeletonCreated = true;
        SetSyntheticMetadata(OutSourceSkeleton, TEXT("SOURCE_SKELETON"), MasterSkeletonPath);
    }
    else if (!ValidateSyntheticMetadata(OutSourceSkeleton, TEXT("SOURCE_SKELETON"), MasterSkeletonPath, Error))
    {
        return false;
    }

    if (!OutSourceMesh)
    {
        if (LoadObject<UObject>(nullptr, SourceMeshPath))
        {
            Error = TEXT("Synthetic source mesh path contains an unexpected class");
            return false;
        }
        OutSourceMesh = DuplicateAssetAt<USkeletalMesh>(SourceMeshPath, MasterMesh, Error);
        if (!OutSourceMesh)
        {
            return false;
        }
        bMeshCreated = true;
        OutSourceMesh->Modify();
        OutSourceMesh->SetSkeleton(OutSourceSkeleton);
        SetSyntheticMetadata(OutSourceMesh, TEXT("SOURCE_MESH"), MasterMeshPath);
        OutSourceMesh->PostEditChange();
    }
    else if (!ValidateSyntheticMetadata(OutSourceMesh, TEXT("SOURCE_MESH"), MasterMeshPath, Error))
    {
        return false;
    }

    if (bSkeletonCreated)
    {
        OutSourceSkeleton->Modify();
        OutSourceSkeleton->SetPreviewMesh(OutSourceMesh);
        OutSourceSkeleton->PostEditChange();
    }
    if (bMeshCreated)
    {
        if (!SaveAsset(OutSourceMesh, Error))
        {
            return false;
        }
        RecordWrite(Writes, OutSourceMesh);
    }
    if (bSkeletonCreated)
    {
        if (!SaveAsset(OutSourceSkeleton, Error))
        {
            return false;
        }
        RecordWrite(Writes, OutSourceSkeleton);
    }
    return ValidateSourceAssets(OutSourceMesh, OutSourceSkeleton, MasterMesh, MasterSkeleton, Error);
}

TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
{
    return {
        MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y),
        MakeShared<FJsonValueNumber>(Value.Z),
    };
}

TArray<TSharedPtr<FJsonValue>> QuatJson(const FQuat& Value)
{
    return {
        MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y),
        MakeShared<FJsonValueNumber>(Value.Z),
        MakeShared<FJsonValueNumber>(Value.W),
    };
}

TSharedRef<FJsonObject> TransformJson(const FTransform& Transform)
{
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("translation"), VectorJson(Transform.GetTranslation()));
    Result->SetArrayField(TEXT("rotation_quaternion"), QuatJson(Transform.GetRotation()));
    Result->SetArrayField(TEXT("scale"), VectorJson(Transform.GetScale3D()));
    return Result;
}

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

bool GetReferenceRootScale(USkeletalMesh* Mesh, FVector& OutScale, FString& Error)
{
    if (!Mesh)
    {
        Error = TEXT("Cannot resolve retarget unit scale from a null skeletal mesh");
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    const int32 RootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("root")));
    if (RootIndex == INDEX_NONE || RefSkeleton.GetParentIndex(RootIndex) != INDEX_NONE)
    {
        Error = FString::Printf(TEXT("%s does not have the expected root reference bone"),
            *Mesh->GetPathName());
        return false;
    }
    OutScale = RefSkeleton.GetRefBonePose()[RootIndex].GetScale3D();
    if (!IsFiniteVector(OutScale) || OutScale.GetAbsMin() <= UE_SMALL_NUMBER)
    {
        Error = FString::Printf(TEXT("%s has an invalid reference root scale"),
            *Mesh->GetPathName());
        return false;
    }
    return true;
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

bool LoadSequenceTracks(UAnimSequence* Sequence, const FReferenceSkeleton& RefSkeleton,
    TMap<FName, TArray<FTransform>>& OutTracks, FString& Error)
{
    OutTracks.Reset();
    if (!Sequence)
    {
        Error = TEXT("Cannot inspect animation tracks on a null sequence");
        return false;
    }
    const IAnimationDataModel* Model = Sequence->GetDataModelInterface().GetInterface();
    if (!Model)
    {
        Error = FString::Printf(TEXT("%s has no animation data model for track inspection"),
            *Sequence->GetPathName());
        return false;
    }
    TArray<FName> TrackNames;
    Model->GetBoneTrackNames(TrackNames);
    for (const FName TrackName : TrackNames)
    {
        if (RefSkeleton.FindBoneIndex(TrackName) == INDEX_NONE)
        {
            continue;
        }
        TArray<FTransform> TrackTransforms;
        Model->GetBoneTrackTransforms(TrackName, TrackTransforms);
        if (TrackTransforms.Num() != FrameCount + 1)
        {
            Error = FString::Printf(TEXT("%s track %s has %d keys, expected 169"),
                *Sequence->GetPathName(), *TrackName.ToString(), TrackTransforms.Num());
            return false;
        }
        OutTracks.Add(TrackName, MoveTemp(TrackTransforms));
    }
    return true;
}

bool BuildLocalPoseAtFrame(const FString& SequencePath, const FReferenceSkeleton& RefSkeleton,
    const TMap<FName, TArray<FTransform>>& Tracks, const int32 Frame,
    TArray<FTransform>& OutLocalPose, FString& Error)
{
    OutLocalPose = RefSkeleton.GetRefBonePose();
    for (const TPair<FName, TArray<FTransform>>& Pair : Tracks)
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(Pair.Key);
        if (BoneIndex == INDEX_NONE || !Pair.Value.IsValidIndex(Frame) ||
            Pair.Value[Frame].ContainsNaN())
        {
            Error = FString::Printf(TEXT("%s has an invalid local transform at frame %d"),
                *SequencePath, Frame);
            return false;
        }
        OutLocalPose[BoneIndex] = Pair.Value[Frame];
    }
    return true;
}

bool BuildUnitScaleReferenceLocalPose(const FReferenceSkeleton& RefSkeleton,
    TArray<FTransform>& OutLocalPose, FString& Error)
{
    const TArray<FTransform>& RefLocalPose = RefSkeleton.GetRefBonePose();
    if (RefLocalPose.Num() != RefSkeleton.GetNum() || RefLocalPose.IsEmpty())
    {
        Error = TEXT("Target mesh has an invalid reference pose");
        return false;
    }
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefLocalPose, RefComponentPose);
    TArray<FTransform> UnitComponentPose = RefComponentPose;
    for (FTransform& Transform : UnitComponentPose)
    {
        Transform.SetScale3D(FVector::OneVector);
    }
    OutLocalPose.SetNum(RefSkeleton.GetNum());
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        OutLocalPose[BoneIndex] = ParentIndex == INDEX_NONE
            ? UnitComponentPose[BoneIndex]
            : UnitComponentPose[BoneIndex].GetRelativeTransform(UnitComponentPose[ParentIndex]);
        OutLocalPose[BoneIndex].SetScale3D(FVector::OneVector);
        if (OutLocalPose[BoneIndex].ContainsNaN())
        {
            Error = FString::Printf(TEXT("Could not derive unit-scale local reference transform for %s"),
                *RefSkeleton.GetBoneName(BoneIndex).ToString());
            return false;
        }
    }
    return true;
}

bool ValidateTrackMapComponentSpacePose(const FString& SequencePath,
    USkeletalMesh* ExpectedMesh, const TMap<FName, TArray<FTransform>>& Tracks,
    const FVector& ExpectedRootScale, const bool bCompatibilityPolicy,
    TSharedPtr<FJsonObject>& OutJson, FString& Error)
{
    OutJson = MakeShared<FJsonObject>();
    if (!ExpectedMesh)
    {
        Error = TEXT("Cannot evaluate a Session 5 track map without its expected mesh");
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = ExpectedMesh->GetRefSkeleton();
    const TArray<FTransform>& RefLocalPose = RefSkeleton.GetRefBonePose();
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefLocalPose, RefComponentPose);
    const int32 RootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("root")));
    const int32 HeadIndex = RefSkeleton.FindBoneIndex(FName(TEXT("head")));
    const int32 PelvisIndex = RefSkeleton.FindBoneIndex(FName(TEXT("pelvis")));
    const int32 LeftFootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("foot_l")));
    const int32 RightFootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("foot_r")));
    if (RootIndex == INDEX_NONE || HeadIndex == INDEX_NONE || PelvisIndex == INDEX_NONE ||
        LeftFootIndex == INDEX_NONE || RightFootIndex == INDEX_NONE)
    {
        Error = FString::Printf(TEXT("%s is missing component-space validation bones"),
            *ExpectedMesh->GetPathName());
        return false;
    }
    const TArray<FTransform>* RootTrack = Tracks.Find(FName(TEXT("root")));
    if (!RootTrack)
    {
        Error = FString::Printf(TEXT("%s has no root track for unit validation"), *SequencePath);
        return false;
    }
    for (int32 Frame = 0; Frame < RootTrack->Num(); ++Frame)
    {
        if (!(*RootTrack)[Frame].GetScale3D().Equals(ExpectedRootScale, 0.01))
        {
            Error = FString::Printf(
                TEXT("%s root scale at frame %d is %s, expected %s under the Session 5 compatibility policy"),
                *SequencePath, Frame, *(*RootTrack)[Frame].GetScale3D().ToString(),
                *ExpectedRootScale.ToString());
            return false;
        }
    }

    TSet<int32> SegmentBoneIndices;
    for (const TCHAR* BoneNameText : EvaluatedSegmentBones)
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(BoneNameText));
        if (BoneIndex <= 0)
        {
            Error = FString::Printf(TEXT("%s is missing evaluated segment bone %s"),
                *ExpectedMesh->GetPathName(), BoneNameText);
            return false;
        }
        SegmentBoneIndices.Add(BoneIndex);
    }
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        if (RefSkeleton.GetParentIndex(BoneIndex) == RootIndex)
        {
            SegmentBoneIndices.Add(BoneIndex);
        }
    }

    const double ReferenceRootToPelvisCm = FVector::Dist(
        RefComponentPose[RootIndex].GetTranslation(),
        RefComponentPose[PelvisIndex].GetTranslation());
    TArray<TSharedPtr<FJsonValue>> SamplesJson;
    for (const int32 Frame : EvaluatedPoseFrames)
    {
        TArray<FTransform> LocalPose;
        if (!BuildLocalPoseAtFrame(SequencePath, RefSkeleton, Tracks, Frame,
            LocalPose, Error))
        {
            return false;
        }
        TArray<FTransform> ComponentPose;
        BuildComponentPose(RefSkeleton, LocalPose, ComponentPose);
        const FVector Root = ComponentPose[RootIndex].GetTranslation();
        const FVector Head = ComponentPose[HeadIndex].GetTranslation();
        const FVector Pelvis = ComponentPose[PelvisIndex].GetTranslation();
        const FVector Feet = (ComponentPose[LeftFootIndex].GetTranslation() +
            ComponentPose[RightFootIndex].GetTranslation()) * 0.5;
        const FVector PelvisFromRoot = Pelvis - Root;
        const double BodyHeightCm = FVector::Dist(Head, Feet);
        const double PelvisToHeadCm = FVector::Dist(Pelvis, Head);
        const double RootToPelvisCm = PelvisFromRoot.Size();
        const double RootToPelvisRatio = ReferenceRootToPelvisCm > UE_SMALL_NUMBER
            ? RootToPelvisCm / ReferenceRootToPelvisCm : 0.0;
        double MaximumKeyBoneRadius = 0.0;
        double MaxObservedComponentScaleRatioError = 0.0;
        for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
        {
            const FVector ReferenceScale = RefComponentPose[BoneIndex].GetScale3D();
            const FVector EvaluatedScale = ComponentPose[BoneIndex].GetScale3D();
            if (!IsFiniteVector(ReferenceScale) || !IsFiniteVector(EvaluatedScale) ||
                ReferenceScale.GetAbsMin() <= UE_SMALL_NUMBER)
            {
                Error = FString::Printf(TEXT("%s has an invalid component scale at frame %d bone %s"),
                    *SequencePath, Frame, *RefSkeleton.GetBoneName(BoneIndex).ToString());
                return false;
            }
            const FVector ScaleRatio(
                EvaluatedScale.X / ReferenceScale.X,
                EvaluatedScale.Y / ReferenceScale.Y,
                EvaluatedScale.Z / ReferenceScale.Z);
            MaxObservedComponentScaleRatioError = FMath::Max(
                MaxObservedComponentScaleRatioError,
                static_cast<double>((ScaleRatio - FVector::OneVector).GetAbsMax()));
        }
        for (const TCHAR* BoneNameText : EvaluatedSegmentBones)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(BoneNameText));
            const FVector Position = ComponentPose[BoneIndex].GetTranslation();
            if (!IsFiniteVector(Position))
            {
                Error = FString::Printf(TEXT("%s has a non-finite component position for %s at frame %d"),
                    *SequencePath, BoneNameText, Frame);
                return false;
            }
            MaximumKeyBoneRadius = FMath::Max(MaximumKeyBoneRadius,
                static_cast<double>(FVector::Dist(Root, Position)));
        }
        if (!FMath::IsFinite(BodyHeightCm) || !FMath::IsFinite(PelvisToHeadCm) ||
            !FMath::IsFinite(RootToPelvisCm) || !FMath::IsFinite(MaximumKeyBoneRadius) ||
            BodyHeightCm < MinimumBodyHeightCm || BodyHeightCm > MaximumBodyHeightCm ||
            PelvisToHeadCm < MinimumPelvisToHeadCm || PelvisToHeadCm > MaximumPelvisToHeadCm ||
            RootToPelvisCm < MinimumRootToPelvisCm || RootToPelvisCm > MaximumRootToPelvisCm ||
            FMath::Abs(RootToPelvisRatio - 1.0) > MaximumSegmentRatioError ||
            MaximumKeyBoneRadius > MaximumKeyBoneRadiusCm ||
            MaxObservedComponentScaleRatioError > MaximumComponentScaleRatioError)
        {
            Error = FString::Printf(
                TEXT("%s evaluated component-space body is implausible at frame %d: height=%.3fcm pelvis_to_head=%.3fcm root_to_pelvis=%.3fcm root_to_pelvis_ratio=%.4f max_root_extent=%.3fcm component_scale_ratio_error=%.4f"),
                *SequencePath, Frame, BodyHeightCm, PelvisToHeadCm, RootToPelvisCm,
                RootToPelvisRatio, MaximumKeyBoneRadius, MaxObservedComponentScaleRatioError);
            return false;
        }

        double MinimumSegmentRatio = TNumericLimits<double>::Max();
        double MaximumSegmentRatio = 0.0;
        double MaximumRatioError = 0.0;
        int32 ComparedSegments = 0;
        for (const int32 BoneIndex : SegmentBoneIndices)
        {
            const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
            if (ParentIndex == INDEX_NONE)
            {
                continue;
            }
            const double ReferenceLength = FVector::Dist(
                RefComponentPose[BoneIndex].GetTranslation(),
                RefComponentPose[ParentIndex].GetTranslation());
            if (ReferenceLength < 0.5)
            {
                continue;
            }
            const double EvaluatedLength = FVector::Dist(
                ComponentPose[BoneIndex].GetTranslation(),
                ComponentPose[ParentIndex].GetTranslation());
            const double Ratio = EvaluatedLength / ReferenceLength;
            if (!FMath::IsFinite(Ratio))
            {
                Error = FString::Printf(TEXT("%s has a non-finite segment ratio at frame %d"),
                    *SequencePath, Frame);
                return false;
            }
            MinimumSegmentRatio = FMath::Min(MinimumSegmentRatio, Ratio);
            MaximumSegmentRatio = FMath::Max(MaximumSegmentRatio, Ratio);
            MaximumRatioError = FMath::Max(MaximumRatioError, FMath::Abs(Ratio - 1.0));
            ++ComparedSegments;
        }
        if (ComparedSegments < MinimumComparedSegments ||
            MaximumRatioError > MaximumSegmentRatioError)
        {
            Error = FString::Printf(
                TEXT("%s evaluated segment contract failed at frame %d: compared=%d min_ratio=%.4f max_ratio=%.4f max_error=%.4f"),
                *SequencePath, Frame, ComparedSegments,
                MinimumSegmentRatio, MaximumSegmentRatio, MaximumRatioError);
            return false;
        }

        const TSharedRef<FJsonObject> SampleJson = MakeShared<FJsonObject>();
        SampleJson->SetNumberField(TEXT("frame"), Frame);
        SampleJson->SetNumberField(TEXT("body_height_cm"), BodyHeightCm);
        SampleJson->SetNumberField(TEXT("pelvis_to_head_cm"), PelvisToHeadCm);
        SampleJson->SetArrayField(TEXT("pelvis_from_root_cm"), VectorJson(PelvisFromRoot));
        SampleJson->SetNumberField(TEXT("root_to_pelvis_cm"), RootToPelvisCm);
        SampleJson->SetNumberField(TEXT("root_to_pelvis_reference_ratio"), RootToPelvisRatio);
        SampleJson->SetNumberField(TEXT("maximum_key_bone_radius_from_root_cm"), MaximumKeyBoneRadius);
        SampleJson->SetNumberField(TEXT("maximum_component_scale_ratio_error"),
            MaxObservedComponentScaleRatioError);
        SampleJson->SetNumberField(TEXT("compared_segments"), ComparedSegments);
        SampleJson->SetNumberField(TEXT("minimum_segment_ratio"), MinimumSegmentRatio);
        SampleJson->SetNumberField(TEXT("maximum_segment_ratio"), MaximumSegmentRatio);
        SampleJson->SetNumberField(TEXT("maximum_segment_ratio_error"), MaximumRatioError);
        SamplesJson.Add(MakeShared<FJsonValueObject>(SampleJson));
    }

    FVector ReferenceRootScale;
    if (!GetReferenceRootScale(ExpectedMesh, ReferenceRootScale, Error))
    {
        return false;
    }
    OutJson->SetStringField(TEXT("status"), TEXT("PASS_EVALUATED_COMPONENT_SPACE_CM"));
    OutJson->SetStringField(TEXT("mesh"), ExpectedMesh->GetPathName());
    OutJson->SetArrayField(TEXT("reference_root_scale"), VectorJson(ReferenceRootScale));
    OutJson->SetArrayField(TEXT("required_sequence_root_scale"), VectorJson(ExpectedRootScale));
    OutJson->SetStringField(TEXT("root_scale_policy"),
        bCompatibilityPolicy ? RetargetUnitNormalization : TEXT("SOURCE_REFERENCE_ROOT_SCALE_PRESERVED"));
    OutJson->SetNumberField(TEXT("minimum_body_height_cm"), MinimumBodyHeightCm);
    OutJson->SetNumberField(TEXT("maximum_body_height_cm"), MaximumBodyHeightCm);
    OutJson->SetNumberField(TEXT("minimum_root_to_pelvis_cm"), MinimumRootToPelvisCm);
    OutJson->SetNumberField(TEXT("maximum_root_to_pelvis_cm"), MaximumRootToPelvisCm);
    OutJson->SetNumberField(TEXT("maximum_key_bone_radius_from_root_cm"), MaximumKeyBoneRadiusCm);
    OutJson->SetNumberField(TEXT("maximum_segment_ratio_error"), MaximumSegmentRatioError);
    OutJson->SetNumberField(TEXT("maximum_component_scale_ratio_error"),
        MaximumComponentScaleRatioError);
    OutJson->SetArrayField(TEXT("samples"), SamplesJson);
    return true;
}

bool ValidateEvaluatedComponentSpacePose(UAnimSequence* Sequence, USkeletalMesh* ExpectedMesh,
    const bool bRequireRetargetCompatibility, TSharedPtr<FJsonObject>& OutJson, FString& Error)
{
    if (!Sequence || !ExpectedMesh)
    {
        Error = TEXT("Cannot evaluate a Session 5 sequence without its expected mesh");
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = ExpectedMesh->GetRefSkeleton();
    TMap<FName, TArray<FTransform>> Tracks;
    if (!LoadSequenceTracks(Sequence, RefSkeleton, Tracks, Error))
    {
        return false;
    }
    FVector ExpectedRootScale;
    if (!GetReferenceRootScale(ExpectedMesh, ExpectedRootScale, Error))
    {
        return false;
    }
    return ValidateTrackMapComponentSpacePose(Sequence->GetPathName(), ExpectedMesh,
        Tracks, ExpectedRootScale, bRequireRetargetCompatibility, OutJson, Error);
}

bool ValidateRawNonRootTranslationsAreStatic(FString& Error)
{
    UAnimSequence* Raw = LoadObject<UAnimSequence>(nullptr, RawSequencePath);
    USkeletalMesh* SourceMesh = LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
    if (!Raw || !SourceMesh)
    {
        Error = TEXT("Cannot prove the Session 5 source translation contract before compatibility normalization");
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = SourceMesh->GetRefSkeleton();
    TMap<FName, TArray<FTransform>> Tracks;
    if (!LoadSequenceTracks(Raw, RefSkeleton, Tracks, Error))
    {
        return false;
    }
    for (const TPair<FName, TArray<FTransform>>& Pair : Tracks)
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(Pair.Key);
        if (BoneIndex == INDEX_NONE || RefSkeleton.GetParentIndex(BoneIndex) == INDEX_NONE)
        {
            continue;
        }
        const FVector ReferenceTranslation =
            RefSkeleton.GetRefBonePose()[BoneIndex].GetTranslation();
        for (int32 Frame = 0; Frame < Pair.Value.Num(); ++Frame)
        {
            if (!Pair.Value[Frame].GetTranslation().Equals(
                ReferenceTranslation, 0.001))
            {
                Error = FString::Printf(
                    TEXT("Refusing fixture-only compatibility normalization because RAW non-root translation differs from the source reference local: bone=%s frame=%d actual=%s expected=%s"),
                    *Pair.Key.ToString(), Frame,
                    *Pair.Value[Frame].GetTranslation().ToString(),
                    *ReferenceTranslation.ToString());
                return false;
            }
        }
    }
    return true;
}

bool HasLegacyNonUnitRootScaleCompatibilityState(UAnimSequence* Sequence,
    USkeletalMesh* TargetMesh)
{
    if (!Sequence || !TargetMesh)
    {
        return false;
    }
    FVector ReferenceRootScale;
    FString IgnoredError;
    if (!GetReferenceRootScale(TargetMesh, ReferenceRootScale, IgnoredError) ||
        ReferenceRootScale.Equals(FVector::OneVector, 0.01))
    {
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    TMap<FName, TArray<FTransform>> Tracks;
    if (!LoadSequenceTracks(Sequence, RefSkeleton, Tracks, IgnoredError) ||
        Tracks.Num() != RefSkeleton.GetNum())
    {
        return false;
    }
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const TArray<FTransform>* Track = Tracks.Find(RefSkeleton.GetBoneName(BoneIndex));
        if (!Track)
        {
            return false;
        }
        for (const FTransform& Transform : *Track)
        {
            if (!Transform.GetScale3D().Equals(FVector::OneVector, 0.001))
            {
                return false;
            }
        }
    }
    return Sequence->GetOutermost()->GetMetaData().GetValue(
        Sequence, MetadataRetargetUnitNormalization).IsEmpty();
}

bool NormalizeLegacyNonUnitRootScaleCompatibility(UAnimSequence* Sequence,
    USkeletalMesh* TargetMesh, FString& Error)
{
    if (!Sequence || !TargetMesh)
    {
        Error = TEXT("Cannot normalize legacy import-scale compatibility on a null sequence or target mesh");
        return false;
    }
    if (!ValidateRawNonRootTranslationsAreStatic(Error))
    {
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    const TArray<FTransform>& ReferenceLocalPose = RefSkeleton.GetRefBonePose();
    FVector ReferenceRootScale;
    if (!GetReferenceRootScale(TargetMesh, ReferenceRootScale, Error))
    {
        return false;
    }
    TMap<FName, TArray<FTransform>> NormalizedTracks;
    if (!LoadSequenceTracks(Sequence, RefSkeleton, NormalizedTracks, Error) ||
        NormalizedTracks.Num() != RefSkeleton.GetNum())
    {
        Error = FString::Printf(TEXT("Cannot normalize %s: the target sequence must contain all %d target tracks"),
            *Sequence->GetPathName(), RefSkeleton.GetNum());
        return false;
    }
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
        TArray<FTransform>* Track = NormalizedTracks.Find(BoneName);
        if (!Track)
        {
            Error = FString::Printf(TEXT("Cannot normalize %s: missing target track %s"),
                *Sequence->GetPathName(), *BoneName.ToString());
            return false;
        }
        for (FTransform& Transform : *Track)
        {
            // The protected mesh's inverse bind matrices include its legacy root scale.
            // Reconstruct the original target locals so both component positions and
            // component scales match that bind pose; root-scale-only would amplify the
            // batch pelvis offset, while unit-scale/cm locals would shrink skinned parts.
            if (RefSkeleton.GetParentIndex(BoneIndex) != INDEX_NONE)
            {
                Transform.SetTranslation(ReferenceLocalPose[BoneIndex].GetTranslation());
            }
            Transform.SetScale3D(ReferenceLocalPose[BoneIndex].GetScale3D());
        }
    }

    TSharedPtr<FJsonObject> PreflightJson;
    FString PreflightError;
    if (!ValidateTrackMapComponentSpacePose(Sequence->GetPathName(), TargetMesh,
        NormalizedTracks, ReferenceRootScale, true, PreflightJson, PreflightError))
    {
        Error = FString::Printf(TEXT("Compatibility normalization preflight failed without mutation: %s"),
            *PreflightError);
        return false;
    }

    Sequence->Modify();
    IAnimationDataController& Controller = Sequence->GetController();
    Controller.OpenBracket(NSLOCTEXT("DiscGolfSession5", "NormalizeLegacyImportScaleCompatibility",
        "Normalize legacy non-unit root scale compatibility"), false);
    bool bAllKeysSet = true;
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
        const TArray<FTransform>& Track = NormalizedTracks.FindChecked(BoneName);
        TArray<FVector3f> Positions;
        TArray<FQuat4f> Rotations;
        TArray<FVector3f> Scales;
        Positions.Reserve(Track.Num());
        Rotations.Reserve(Track.Num());
        Scales.Reserve(Track.Num());
        for (const FTransform& Transform : Track)
        {
            const FVector Position = Transform.GetTranslation();
            const FQuat Rotation = Transform.GetRotation().GetNormalized();
            Positions.Emplace(static_cast<float>(Position.X), static_cast<float>(Position.Y),
                static_cast<float>(Position.Z));
            Rotations.Emplace(static_cast<float>(Rotation.X), static_cast<float>(Rotation.Y),
                static_cast<float>(Rotation.Z), static_cast<float>(Rotation.W));
            const FVector Scale = Transform.GetScale3D();
            Scales.Emplace(static_cast<float>(Scale.X), static_cast<float>(Scale.Y),
                static_cast<float>(Scale.Z));
        }
        bAllKeysSet = Controller.SetBoneTrackKeys(
            BoneName, Positions, Rotations, Scales, false) && bAllKeysSet;
    }
    Controller.CloseBracket(false);
    if (!bAllKeysSet)
    {
        Error = FString::Printf(TEXT("Could not write all compatibility-normalized target tracks on %s"),
            *Sequence->GetPathName());
        return false;
    }
    SetRetargetUnitNormalizationMetadata(Sequence);
    Sequence->RefreshCacheData();
    return true;
}

bool ValidateLegacyImportScaleCompatibilityTrackContract(UAnimSequence* Sequence,
    USkeletalMesh* TargetMesh, FString& Error)
{
    if (!Sequence || !TargetMesh)
    {
        Error = TEXT("Cannot validate the compatibility track contract on a null asset");
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    const TArray<FTransform>& ReferenceLocalPose = RefSkeleton.GetRefBonePose();
    TMap<FName, TArray<FTransform>> Tracks;
    if (!LoadSequenceTracks(Sequence, RefSkeleton, Tracks, Error) ||
        Tracks.Num() != RefSkeleton.GetNum())
    {
        Error = FString::Printf(
            TEXT("%s must contain all %d target tracks after compatibility normalization"),
            *Sequence->GetPathName(), RefSkeleton.GetNum());
        return false;
    }
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
        const TArray<FTransform>* Track = Tracks.Find(BoneName);
        if (!Track)
        {
            Error = FString::Printf(TEXT("%s is missing compatibility track %s"),
                *Sequence->GetPathName(), *BoneName.ToString());
            return false;
        }
        for (int32 Frame = 0; Frame < Track->Num(); ++Frame)
        {
            const FTransform& Transform = (*Track)[Frame];
            if (!Transform.GetScale3D().Equals(
                ReferenceLocalPose[BoneIndex].GetScale3D(), 0.001))
            {
                Error = FString::Printf(
                    TEXT("%s track %s frame %d must preserve the target-reference local scale after compatibility normalization"),
                    *Sequence->GetPathName(), *BoneName.ToString(), Frame);
                return false;
            }
            if (RefSkeleton.GetParentIndex(BoneIndex) != INDEX_NONE &&
                !Transform.GetTranslation().Equals(
                    ReferenceLocalPose[BoneIndex].GetTranslation(), 0.001))
            {
                Error = FString::Printf(
                    TEXT("%s track %s frame %d differs from the target-reference local translation"),
                    *Sequence->GetPathName(), *BoneName.ToString(), Frame);
                return false;
            }
        }
    }
    return true;
}

TSharedRef<FJsonObject> BuildMeshReferenceUnitProbe(USkeletalMesh* Mesh, FString& Error)
{
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Mesh)
    {
        Error = TEXT("Cannot probe a null skeletal mesh reference pose");
        return Result;
    }
    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    const TArray<FTransform>& RefLocalPose = RefSkeleton.GetRefBonePose();
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefLocalPose, RefComponentPose);
    TArray<FTransform> UnitScaleReferenceLocalPose;
    if (!BuildUnitScaleReferenceLocalPose(RefSkeleton, UnitScaleReferenceLocalPose, Error))
    {
        return Result;
    }
    const int32 RootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("root")));
    Result->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Result->SetNumberField(TEXT("bone_count"), RefSkeleton.GetNum());
    TArray<TSharedPtr<FJsonValue>> BonesJson;
    for (const TCHAR* BoneNameText : UnitProbeBones)
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(BoneNameText));
        if (BoneIndex == INDEX_NONE)
        {
            Error = FString::Printf(TEXT("Reference probe mesh %s is missing %s"),
                *Mesh->GetPathName(), BoneNameText);
            return Result;
        }
        const TSharedRef<FJsonObject> BoneJson = MakeShared<FJsonObject>();
        BoneJson->SetStringField(TEXT("bone"), BoneNameText);
        BoneJson->SetStringField(TEXT("parent"), RefSkeleton.GetParentIndex(BoneIndex) == INDEX_NONE
            ? TEXT("") : RefSkeleton.GetBoneName(RefSkeleton.GetParentIndex(BoneIndex)).ToString());
        BoneJson->SetObjectField(TEXT("reference_local"), TransformJson(RefLocalPose[BoneIndex]));
        BoneJson->SetObjectField(TEXT("reference_component"), TransformJson(RefComponentPose[BoneIndex]));
        BoneJson->SetObjectField(TEXT("unit_scale_reference_local"),
            TransformJson(UnitScaleReferenceLocalPose[BoneIndex]));
        BonesJson.Add(MakeShared<FJsonValueObject>(BoneJson));
    }
    Result->SetArrayField(TEXT("probe_bones"), BonesJson);
    TArray<TSharedPtr<FJsonValue>> DirectChildrenJson;
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        if (RefSkeleton.GetParentIndex(BoneIndex) != RootIndex)
        {
            continue;
        }
        const TSharedRef<FJsonObject> ChildJson = MakeShared<FJsonObject>();
        ChildJson->SetStringField(TEXT("bone"), RefSkeleton.GetBoneName(BoneIndex).ToString());
        ChildJson->SetObjectField(TEXT("reference_local"), TransformJson(RefLocalPose[BoneIndex]));
        ChildJson->SetObjectField(TEXT("unit_scale_reference_local"),
            TransformJson(UnitScaleReferenceLocalPose[BoneIndex]));
        DirectChildrenJson.Add(MakeShared<FJsonValueObject>(ChildJson));
    }
    Result->SetArrayField(TEXT("direct_children_of_root"), DirectChildrenJson);
    return Result;
}

TSharedRef<FJsonObject> BuildSequenceUnitProbe(UAnimSequence* Sequence,
    USkeletalMesh* Mesh, FString& Error)
{
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Sequence || !Mesh)
    {
        Error = TEXT("Cannot probe a missing Session 5 sequence or evaluation mesh");
        return Result;
    }
    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    TMap<FName, TArray<FTransform>> Tracks;
    if (!LoadSequenceTracks(Sequence, RefSkeleton, Tracks, Error))
    {
        return Result;
    }
    Result->SetStringField(TEXT("path"), Sequence->GetPathName());
    Result->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Result->SetNumberField(TEXT("track_count"), Tracks.Num());
    Result->SetStringField(TEXT("normalization_metadata"),
        Sequence->GetOutermost()->GetMetaData().GetValue(
            Sequence, MetadataRetargetUnitNormalization));
    bool bAllTrackScalesOne = true;
    for (const TPair<FName, TArray<FTransform>>& Pair : Tracks)
    {
        for (const FTransform& Transform : Pair.Value)
        {
            bAllTrackScalesOne = bAllTrackScalesOne &&
                Transform.GetScale3D().Equals(FVector::OneVector, 0.001);
        }
    }
    Result->SetBoolField(TEXT("all_track_scales_one"), bAllTrackScalesOne);

    const int32 RootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("root")));
    const int32 PelvisIndex = RefSkeleton.FindBoneIndex(FName(TEXT("pelvis")));
    const int32 HeadIndex = RefSkeleton.FindBoneIndex(FName(TEXT("head")));
    const int32 LeftFootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("foot_l")));
    const int32 RightFootIndex = RefSkeleton.FindBoneIndex(FName(TEXT("foot_r")));
    if (RootIndex == INDEX_NONE || PelvisIndex == INDEX_NONE || HeadIndex == INDEX_NONE ||
        LeftFootIndex == INDEX_NONE || RightFootIndex == INDEX_NONE)
    {
        Error = FString::Printf(TEXT("Probe mesh %s is missing required body bones"),
            *Mesh->GetPathName());
        return Result;
    }
    TArray<TSharedPtr<FJsonValue>> SamplesJson;
    for (const int32 Frame : EvaluatedPoseFrames)
    {
        TArray<FTransform> LocalPose;
        if (!BuildLocalPoseAtFrame(Sequence->GetPathName(), RefSkeleton, Tracks,
            Frame, LocalPose, Error))
        {
            return Result;
        }
        TArray<FTransform> ComponentPose;
        BuildComponentPose(RefSkeleton, LocalPose, ComponentPose);
        const FVector Root = ComponentPose[RootIndex].GetTranslation();
        const FVector Pelvis = ComponentPose[PelvisIndex].GetTranslation();
        const FVector Head = ComponentPose[HeadIndex].GetTranslation();
        const FVector Feet = (ComponentPose[LeftFootIndex].GetTranslation() +
            ComponentPose[RightFootIndex].GetTranslation()) * 0.5;
        double MaximumKeyBoneRadius = 0.0;
        TArray<TSharedPtr<FJsonValue>> BonesJson;
        for (const TCHAR* BoneNameText : UnitProbeBones)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(BoneNameText));
            if (BoneIndex == INDEX_NONE)
            {
                Error = FString::Printf(TEXT("Probe mesh %s is missing %s"),
                    *Mesh->GetPathName(), BoneNameText);
                return Result;
            }
            MaximumKeyBoneRadius = FMath::Max(MaximumKeyBoneRadius,
                static_cast<double>(FVector::Dist(Root,
                    ComponentPose[BoneIndex].GetTranslation())));
            const TSharedRef<FJsonObject> BoneJson = MakeShared<FJsonObject>();
            BoneJson->SetStringField(TEXT("bone"), BoneNameText);
            BoneJson->SetBoolField(TEXT("has_animation_track"),
                Tracks.Contains(FName(BoneNameText)));
            BoneJson->SetObjectField(TEXT("local"), TransformJson(LocalPose[BoneIndex]));
            BoneJson->SetObjectField(TEXT("component"), TransformJson(ComponentPose[BoneIndex]));
            BonesJson.Add(MakeShared<FJsonValueObject>(BoneJson));
        }
        const TSharedRef<FJsonObject> SampleJson = MakeShared<FJsonObject>();
        SampleJson->SetNumberField(TEXT("frame"), Frame);
        SampleJson->SetNumberField(TEXT("body_height_cm"), FVector::Dist(Head, Feet));
        SampleJson->SetNumberField(TEXT("pelvis_to_head_cm"), FVector::Dist(Pelvis, Head));
        SampleJson->SetArrayField(TEXT("pelvis_from_root_cm"), VectorJson(Pelvis - Root));
        SampleJson->SetNumberField(TEXT("root_to_pelvis_cm"), FVector::Dist(Root, Pelvis));
        SampleJson->SetNumberField(TEXT("maximum_key_bone_radius_from_root_cm"),
            MaximumKeyBoneRadius);
        SampleJson->SetArrayField(TEXT("bones"), BonesJson);
        SamplesJson.Add(MakeShared<FJsonValueObject>(SampleJson));
    }
    Result->SetArrayField(TEXT("samples"), SamplesJson);
    return Result;
}

FString BuildSession5UnitCompatibilityProbe()
{
    FString Error;
    USkeletalMesh* SourceMesh = LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
    USkeletalMesh* MasterMesh = LoadObject<USkeletalMesh>(nullptr, MasterMeshPath);
    UAnimSequence* Raw = LoadObject<UAnimSequence>(nullptr, RawSequencePath);
    UAnimSequence* Retargeted = LoadObject<UAnimSequence>(nullptr, RetargetedSequencePath);
    UAnimSequence* Cleaned = LoadObject<UAnimSequence>(nullptr, CleanedSequencePath);
    UAnimSequence* Final = LoadObject<UAnimSequence>(nullptr, FinalSequencePath);
    if (!SourceMesh || !MasterMesh || !Raw || !Retargeted || !Cleaned || !Final)
    {
        return FailureJson(TEXT("Session 5 unit probe is missing a required mesh or sequence"));
    }
    const TSharedRef<FJsonObject> SourceReference = BuildMeshReferenceUnitProbe(SourceMesh, Error);
    if (!Error.IsEmpty()) return FailureJson(Error);
    const TSharedRef<FJsonObject> TargetReference = BuildMeshReferenceUnitProbe(MasterMesh, Error);
    if (!Error.IsEmpty()) return FailureJson(Error);
    TArray<TPair<UAnimSequence*, USkeletalMesh*>> SequencePairs;
    SequencePairs.Emplace(Raw, SourceMesh);
    SequencePairs.Emplace(Retargeted, MasterMesh);
    SequencePairs.Emplace(Cleaned, MasterMesh);
    SequencePairs.Emplace(Final, MasterMesh);
    TArray<TSharedPtr<FJsonValue>> SequenceJson;
    for (const TPair<UAnimSequence*, USkeletalMesh*>& Pair : SequencePairs)
    {
        const TSharedRef<FJsonObject> Entry = BuildSequenceUnitProbe(Pair.Key, Pair.Value, Error);
        if (!Error.IsEmpty()) return FailureJson(Error);
        SequenceJson.Add(MakeShared<FJsonValueObject>(Entry));
    }
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), TEXT("PASS_READ_ONLY_SESSION5_UNIT_COMPATIBILITY_PROBE"));
    Root->SetStringField(TEXT("classification"), TEXT("SYNTHETIC_TEST"));
    Root->SetStringField(TEXT("shipping_policy"), TEXT("DO_NOT_SHIP"));
    Root->SetStringField(TEXT("mutation_policy"), TEXT("READ_ONLY_NO_MODIFY_NO_SAVE"));
    Root->SetStringField(TEXT("compatibility_policy"), RetargetUnitNormalization);
    Root->SetObjectField(TEXT("source_reference"), SourceReference);
    Root->SetObjectField(TEXT("target_reference"), TargetReference);
    Root->SetArrayField(TEXT("sequences"), SequenceJson);
    return JsonString(Root);
}

bool ValidateSequence(UAnimSequence* Sequence, USkeleton* ExpectedSkeleton,
    USkeletalMesh* ExpectedMesh, const TCHAR* Stage, const TCHAR* Predecessor,
    const bool bRequireProductionCurves, const bool bRequireRetargetUnitNormalization,
    const bool bValidateEvaluatedPose, TSharedPtr<FJsonObject>& OutJson, FString& Error)
{
    OutJson = MakeShared<FJsonObject>();
    if (!Sequence)
    {
        Error = FString::Printf(TEXT("Session 5 %s sequence is missing"), Stage);
        return false;
    }
    if (Sequence->GetSkeleton() != ExpectedSkeleton || !ExpectedMesh)
    {
        Error = FString::Printf(TEXT("%s uses the wrong skeleton or has no expected evaluation mesh"),
            *Sequence->GetPathName());
        return false;
    }
    const TScriptInterface<IAnimationDataModel> DataModel = Sequence->GetDataModelInterface();
    const IAnimationDataModel* Model = DataModel.GetInterface();
    if (!Model)
    {
        Error = FString::Printf(TEXT("%s has no animation data model"), *Sequence->GetPathName());
        return false;
    }
    const FFrameRate ActualRate = Model->GetFrameRate();
    if (ActualRate.Numerator != FrameRate || ActualRate.Denominator != 1 ||
        Model->GetNumberOfFrames() != FrameCount ||
        !NearlyEqual(Model->GetPlayLength(), DurationSeconds) || Sequence->bLoop ||
        !NearlyEqual(Sequence->RateScale, 1.f))
    {
        Error = FString::Printf(TEXT("%s timing/playback contract differs from 168 frames at 60 fps"),
            *Sequence->GetPathName());
        return false;
    }
    if (!Sequence->Notifies.IsEmpty() || Sequence->AnimNotifyTracks.Num() != 1 ||
        Sequence->AnimNotifyTracks[0].TrackName != FName(TEXT("1")) ||
        !Sequence->AnimNotifyTracks[0].Notifies.IsEmpty())
    {
        Error = FString::Printf(TEXT("%s must contain zero sequence events and only UE 5.8's mandatory empty notify track"),
            *Sequence->GetPathName());
        return false;
    }
    TArray<FName> TrackNames;
    Model->GetBoneTrackNames(TrackNames);
    if (TrackNames.IsEmpty())
    {
        Error = FString::Printf(TEXT("%s contains no animated bone tracks"), *Sequence->GetPathName());
        return false;
    }
    if (bRequireProductionCurves)
    {
        if (Model->GetNumberOfFloatCurves() != BuildCurveSpecs().Num() ||
            Model->GetNumberOfTransformCurves() != 0)
        {
            Error = FString::Printf(TEXT("%s must contain exactly the six approved float curves and no transform curves"),
                *Sequence->GetPathName());
            return false;
        }
        TArray<FTransform> RootTransforms;
        Model->GetBoneTrackTransforms(TEXT("root"), RootTransforms);
        if (RootTransforms.Num() != FrameCount + 1)
        {
            Error = FString::Printf(TEXT("%s cleaned root track does not contain 169 keys"),
                *Sequence->GetPathName());
            return false;
        }
        const FVector RootOrigin = RootTransforms[0].GetTranslation();
        for (int32 Index = 0; Index < RootTransforms.Num(); ++Index)
        {
            const FVector RootPosition = RootTransforms[Index].GetTranslation();
            if (!NearlyEqual(static_cast<float>(RootPosition.X), static_cast<float>(RootOrigin.X), 0.001f) ||
                !NearlyEqual(static_cast<float>(RootPosition.Y), static_cast<float>(RootOrigin.Y), 0.001f) ||
                FMath::Abs(RootPosition.Z - RootOrigin.Z) > 3.001)
            {
                Error = FString::Printf(TEXT("%s violates the bounded in-place root cleanup at frame %d"),
                    *Sequence->GetPathName(), Index);
                return false;
            }
        }
        for (const FCurveSpec& Curve : BuildCurveSpecs())
        {
            const FAnimationCurveIdentifier Id(Curve.Name, ERawCurveTrackTypes::RCT_Float);
            const FRichCurve* RichCurve = Model->FindRichCurve(Id);
            if (!RichCurve || RichCurve->GetNumKeys() != Curve.Keys.Num())
            {
                Error = FString::Printf(TEXT("%s curve %s is missing or has the wrong key count"),
                    *Sequence->GetPathName(), *Curve.Name.ToString());
                return false;
            }
            for (const TPair<int32, float>& Key : Curve.Keys)
            {
                if (!NearlyEqual(RichCurve->Eval(FrameToTime(Key.Key)), Key.Value))
                {
                    Error = FString::Printf(TEXT("%s curve %s differs at frame %d"),
                        *Sequence->GetPathName(), *Curve.Name.ToString(), Key.Key);
                    return false;
                }
            }
        }
        FMetaData& Metadata = Sequence->GetOutermost()->GetMetaData();
        if (Metadata.GetValue(Sequence, MetadataCleanupRecipe) != CleanupRecipe)
        {
            Error = FString::Printf(TEXT("%s does not record the exact cleanup recipe"),
                *Sequence->GetPathName());
            return false;
        }
    }
    if (bRequireRetargetUnitNormalization)
    {
        FMetaData& Metadata = Sequence->GetOutermost()->GetMetaData();
        if (Metadata.GetValue(Sequence, MetadataRetargetUnitNormalization) !=
            RetargetUnitNormalization)
        {
            Error = FString::Printf(TEXT("%s does not record the exact legacy import-scale compatibility normalization"),
                *Sequence->GetPathName());
            return false;
        }
        if (!ValidateLegacyImportScaleCompatibilityTrackContract(Sequence, ExpectedMesh, Error))
        {
            return false;
        }
    }
    if (!ValidateSyntheticMetadata(Sequence, Stage, Predecessor, Error))
    {
        return false;
    }
    TSharedPtr<FJsonObject> EvaluatedPoseJson;
    if (bValidateEvaluatedPose &&
        !ValidateEvaluatedComponentSpacePose(Sequence, ExpectedMesh,
            bRequireRetargetUnitNormalization, EvaluatedPoseJson, Error))
    {
        return false;
    }
    OutJson->SetStringField(TEXT("status"), TEXT("PASS"));
    OutJson->SetStringField(TEXT("path"), Sequence->GetPathName());
    OutJson->SetStringField(TEXT("stage"), Stage);
    OutJson->SetStringField(TEXT("predecessor"), Predecessor);
    OutJson->SetStringField(TEXT("skeleton"), ExpectedSkeleton->GetPathName());
    OutJson->SetStringField(TEXT("evaluation_mesh"), ExpectedMesh->GetPathName());
    OutJson->SetNumberField(TEXT("frame_rate"), FrameRate);
    OutJson->SetNumberField(TEXT("frame_count"), FrameCount);
    OutJson->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
    OutJson->SetNumberField(TEXT("animated_bone_tracks"), TrackNames.Num());
    OutJson->SetNumberField(TEXT("sequence_event_count"), 0);
    OutJson->SetStringField(TEXT("notify_track_policy"),
        TEXT("ONE_UE5_8_MANDATORY_EMPTY_TRACK_NO_SEQUENCE_EVENTS"));
    OutJson->SetNumberField(TEXT("required_curve_count"),
        bRequireProductionCurves ? BuildCurveSpecs().Num() : 0);
    if (bValidateEvaluatedPose)
    {
        OutJson->SetObjectField(TEXT("evaluated_component_space"), EvaluatedPoseJson);
    }
    if (bRequireRetargetUnitNormalization)
    {
        OutJson->SetStringField(TEXT("retarget_unit_normalization"),
            RetargetUnitNormalization);
        OutJson->SetStringField(TEXT("normalized_track_contract"),
            TEXT("ROOT_PRESERVES_BATCH_TRANSLATION_ROTATION;ALL_LOCAL_SCALES_EQUAL_TARGET_REFERENCE;ALL_NON_ROOT_TRANSLATIONS_EQUAL_TARGET_REFERENCE_LOCALS;COMPONENT_SCALE_MATCHES_BIND_REFERENCE"));
    }
    if (bRequireProductionCurves)
    {
        OutJson->SetStringField(TEXT("cleanup_recipe"), CleanupRecipe);
        OutJson->SetStringField(TEXT("root_drift_policy"),
            TEXT("LOCK_XY_TO_FRAME0_CLAMP_Z_DELTA_TO_PLUS_MINUS_3CM"));
    }
    return true;
}

bool RepairLegacyTargetSequenceUnitScale(UAnimSequence* Sequence, USkeleton* Skeleton,
    USkeletalMesh* Mesh, const TCHAR* Stage, const TCHAR* Predecessor,
    const bool bRequireProductionCurves, TArray<FString>& Writes, FString& Error)
{
    TSharedPtr<FJsonObject> LegacyStructureJson;
    FString LegacyStructureError;
    if (!ValidateSequence(Sequence, Skeleton, Mesh, Stage, Predecessor,
        bRequireProductionCurves, false, false, LegacyStructureJson, LegacyStructureError))
    {
        Error = FString::Printf(
            TEXT("Refusing to repair %s because it differs beyond the exact legacy import-scale compatibility state: %s"),
            *Sequence->GetPathName(), *LegacyStructureError);
        return false;
    }
    if (!HasLegacyNonUnitRootScaleCompatibilityState(Sequence, Mesh))
    {
        Error = FString::Printf(
            TEXT("Refusing to repair %s because its scale/track state is not the exact legacy non-unit-root compatibility input"),
            *Sequence->GetPathName());
        return false;
    }

    if (!NormalizeLegacyNonUnitRootScaleCompatibility(Sequence, Mesh, Error))
    {
        return false;
    }
    Sequence->PostEditChange();
    if (!SaveAsset(Sequence, Error))
    {
        return false;
    }
    RecordWrite(Writes, Sequence);

    TSharedPtr<FJsonObject> RepairedJson;
    return ValidateSequence(Sequence, Skeleton, Mesh, Stage, Predecessor,
        bRequireProductionCurves, true, true, RepairedJson, Error);
}

UAnimSequence* DuplicateSequence(const TCHAR* DestinationPath, UAnimSequence* Source,
    USkeleton* Skeleton, USkeletalMesh* PreviewMesh, const TCHAR* Stage,
    const TCHAR* Predecessor, const bool bAuthorProductionCurves, FString& Error)
{
    UAnimSequence* Sequence = DuplicateAssetAt<UAnimSequence>(DestinationPath, Source, Error);
    if (!Sequence)
    {
        return nullptr;
    }
    Sequence->Modify();
    if (Sequence->GetSkeleton() != Skeleton && !Sequence->ReplaceSkeleton(Skeleton, false))
    {
        Error = FString::Printf(TEXT("Could not replace skeleton on %s"), DestinationPath);
        return nullptr;
    }
    Sequence->SetPreviewMesh(PreviewMesh);
    Sequence->bLoop = false;
    Sequence->RateScale = 1.f;
    Sequence->Notifies.Reset();
    Sequence->AnimNotifyTracks.Reset();
    Sequence->InitializeNotifyTrack();
    Sequence->RefreshCacheData();

    if (bAuthorProductionCurves)
    {
        const IAnimationDataModel* Model = Sequence->GetDataModelInterface().GetInterface();
        TArray<FTransform> RootTransforms;
        if (Model)
        {
            Model->GetBoneTrackTransforms(TEXT("root"), RootTransforms);
        }
        if (RootTransforms.Num() != FrameCount + 1)
        {
            Error = FString::Printf(TEXT("Cannot clean %s: root track does not contain 169 keys"),
                DestinationPath);
            return nullptr;
        }
        IAnimationDataController& Controller = Sequence->GetController();
        Controller.OpenBracket(NSLOCTEXT("DiscGolfSession5", "CleanSyntheticCurves",
            "Clean Session 5 synthetic motion curves"), false);
        const FVector RootOrigin = RootTransforms[0].GetTranslation();
        TArray<FVector3f> RootPositions;
        TArray<FQuat4f> RootRotations;
        TArray<FVector3f> RootScales;
        RootPositions.Reserve(RootTransforms.Num());
        RootRotations.Reserve(RootTransforms.Num());
        RootScales.Reserve(RootTransforms.Num());
        for (const FTransform& Transform : RootTransforms)
        {
            const FVector Original = Transform.GetTranslation();
            const FVector Normalized(
                RootOrigin.X,
                RootOrigin.Y,
                RootOrigin.Z + FMath::Clamp(Original.Z - RootOrigin.Z, -3.0, 3.0));
            const FQuat Rotation = Transform.GetRotation().GetNormalized();
            const FVector Scale = Transform.GetScale3D();
            RootPositions.Emplace(static_cast<float>(Normalized.X),
                static_cast<float>(Normalized.Y), static_cast<float>(Normalized.Z));
            RootRotations.Emplace(static_cast<float>(Rotation.X), static_cast<float>(Rotation.Y),
                static_cast<float>(Rotation.Z), static_cast<float>(Rotation.W));
            RootScales.Emplace(static_cast<float>(Scale.X),
                static_cast<float>(Scale.Y), static_cast<float>(Scale.Z));
        }
        if (!Controller.SetBoneTrackKeys(TEXT("root"), RootPositions, RootRotations,
            RootScales, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not normalize root drift on %s"), DestinationPath);
            return nullptr;
        }
        Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, false);
        Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform, false);
        for (const FCurveSpec& Curve : BuildCurveSpecs())
        {
            const FAnimationCurveIdentifier CurveId(Curve.Name, ERawCurveTrackTypes::RCT_Float);
            if (!Controller.AddCurve(CurveId, AACF_DefaultCurve, false))
            {
                Controller.CloseBracket(false);
                Error = FString::Printf(TEXT("Could not add cleaned curve %s"), *Curve.Name.ToString());
                return nullptr;
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
                Error = FString::Printf(TEXT("Could not write cleaned curve %s"), *Curve.Name.ToString());
                return nullptr;
            }
        }
        Controller.CloseBracket(false);
    }
    SetSyntheticMetadata(Sequence, Stage, Predecessor);
    if (FCString::Strcmp(Stage, TEXT("RAW_SOURCE")) != 0)
    {
        SetRetargetUnitNormalizationMetadata(Sequence);
    }
    if (bAuthorProductionCurves || FCString::Strcmp(Stage, TEXT("PRODUCTION_TEST")) == 0)
    {
        SetCleanupMetadata(Sequence);
    }
    Sequence->PostEditChange();
    if (!SaveAsset(Sequence, Error))
    {
        return nullptr;
    }
    return Sequence;
}

bool EnsureRawSequence(UAnimSequence* Prototype, USkeleton* SourceSkeleton,
    USkeletalMesh* SourceMesh, UAnimSequence*& OutRaw, TArray<FString>& Writes,
    FString& Error)
{
    OutRaw = LoadObject<UAnimSequence>(nullptr, RawSequencePath);
    TSharedPtr<FJsonObject> Ignored;
    if (OutRaw)
    {
        return ValidateSequence(OutRaw, SourceSkeleton, SourceMesh, TEXT("RAW_SOURCE"),
            PrototypeSequencePath, false, false, true, Ignored, Error);
    }
    if (LoadObject<UObject>(nullptr, RawSequencePath))
    {
        Error = TEXT("Raw sequence path contains an unexpected class");
        return false;
    }
    OutRaw = DuplicateSequence(RawSequencePath, Prototype, SourceSkeleton, SourceMesh,
        TEXT("RAW_SOURCE"), PrototypeSequencePath, false, Error);
    if (!OutRaw)
    {
        return false;
    }
    RecordWrite(Writes, OutRaw);
    return ValidateSequence(OutRaw, SourceSkeleton, SourceMesh, TEXT("RAW_SOURCE"),
        PrototypeSequencePath, false, false, true, Ignored, Error);
}

bool ValidateSourceIKRig(UIKRigDefinition* Rig, USkeletalMesh* SourceMesh,
    TSharedPtr<FJsonObject>& OutJson, FString& Error)
{
    OutJson = MakeShared<FJsonObject>();
    if (!Rig)
    {
        Error = TEXT("Source IK Rig is missing");
        return false;
    }
    UIKRigController* Controller = UIKRigController::GetController(Rig);
    if (!Controller || Controller->GetSkeletalMesh() != SourceMesh)
    {
        Error = TEXT("Source IK Rig is not bound to the synthetic source mesh");
        return false;
    }
    if (Controller->GetNumSolvers() != 0 ||
        Controller->GetRetargetRoot() != FName(TEXT("pelvis")) ||
        Controller->GetRootMotionBone() != FName(TEXT("root")) ||
        Controller->GetRetargetChains().Num() != UE_ARRAY_COUNT(ChainSpecs))
    {
        Error = TEXT("Source IK Rig solver/root/chain cardinality contract differs");
        return false;
    }
    TArray<TSharedPtr<FJsonValue>> ChainsJson;
    for (const FChainSpec& Spec : ChainSpecs)
    {
        const FName Chain(Spec.Chain);
        if (Controller->GetRetargetChainStartBone(Chain) != FName(Spec.StartBone) ||
            Controller->GetRetargetChainEndBone(Chain) != FName(Spec.EndBone) ||
            !Controller->GetRetargetChainGoal(Chain).IsNone())
        {
            Error = FString::Printf(TEXT("Source IK Rig chain %s differs"), Spec.Chain);
            return false;
        }
        const TSharedRef<FJsonObject> ChainJson = MakeShared<FJsonObject>();
        ChainJson->SetStringField(TEXT("name"), Spec.Chain);
        ChainJson->SetStringField(TEXT("start_bone"), Spec.StartBone);
        ChainJson->SetStringField(TEXT("end_bone"), Spec.EndBone);
        ChainsJson.Add(MakeShared<FJsonValueObject>(ChainJson));
    }
    if (!ValidateSyntheticMetadata(Rig, TEXT("SOURCE_IK_RIG"), SourceMeshPath, Error))
    {
        return false;
    }
    OutJson->SetStringField(TEXT("status"), TEXT("PASS"));
    OutJson->SetStringField(TEXT("path"), Rig->GetPathName());
    OutJson->SetStringField(TEXT("preview_mesh"), SourceMesh->GetPathName());
    OutJson->SetStringField(TEXT("retarget_root"), TEXT("pelvis"));
    OutJson->SetStringField(TEXT("root_motion_bone"), TEXT("root"));
    OutJson->SetNumberField(TEXT("solver_count"), 0);
    OutJson->SetArrayField(TEXT("chains"), ChainsJson);
    return true;
}

bool EnsureSourceIKRig(USkeletalMesh* SourceMesh, UIKRigDefinition*& OutRig,
    TArray<FString>& Writes, FString& Error)
{
    OutRig = LoadObject<UIKRigDefinition>(nullptr, SourceIKRigPath);
    TSharedPtr<FJsonObject> Ignored;
    if (OutRig)
    {
        return ValidateSourceIKRig(OutRig, SourceMesh, Ignored, Error);
    }
    if (LoadObject<UObject>(nullptr, SourceIKRigPath))
    {
        Error = TEXT("Source IK Rig path contains an unexpected class");
        return false;
    }
    OutRig = CreateFactoryAsset<UIKRigDefinition, UIKRigDefinitionFactory>(
        SourceIKRigPath, Error);
    if (!OutRig)
    {
        return false;
    }
    UIKRigController* Controller = UIKRigController::GetController(OutRig);
    if (!Controller || !Controller->SetSkeletalMesh(SourceMesh))
    {
        Error = TEXT("UE 5.8 IK Rig controller rejected the synthetic source mesh");
        return false;
    }
    for (const FChainSpec& Spec : ChainSpecs)
    {
        const FName Added = Controller->AddRetargetChain(
            FName(Spec.Chain), FName(Spec.StartBone), FName(Spec.EndBone), NAME_None);
        if (Added != FName(Spec.Chain))
        {
            Error = FString::Printf(TEXT("Could not create exact source IK chain %s"), Spec.Chain);
            return false;
        }
    }
    if (!Controller->SetRetargetRoot(FName(TEXT("pelvis"))) ||
        !Controller->SetRootMotionBone(FName(TEXT("root"))))
    {
        Error = TEXT("Could not set source IK retarget/root-motion bones");
        return false;
    }
    SetSyntheticMetadata(OutRig, TEXT("SOURCE_IK_RIG"), SourceMeshPath);
    OutRig->PostEditChange();
    if (!SaveAsset(OutRig, Error))
    {
        return false;
    }
    RecordWrite(Writes, OutRig);
    return ValidateSourceIKRig(OutRig, SourceMesh, Ignored, Error);
}

bool ValidateRetargeter(UIKRetargeter* Retargeter, UIKRigDefinition* SourceRig,
    UIKRigDefinition* TargetRig, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh,
    TSharedPtr<FJsonObject>& OutJson, FString& Error)
{
    OutJson = MakeShared<FJsonObject>();
    if (!Retargeter)
    {
        Error = TEXT("IK Retargeter is missing");
        return false;
    }
    UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
    if (!Controller ||
        Controller->GetIKRig(ERetargetSourceOrTarget::Source) != SourceRig ||
        Controller->GetIKRig(ERetargetSourceOrTarget::Target) != TargetRig ||
        Controller->GetPreviewMesh(ERetargetSourceOrTarget::Source) != SourceMesh ||
        Controller->GetPreviewMesh(ERetargetSourceOrTarget::Target) != TargetMesh)
    {
        Error = TEXT("IK Retargeter source/target rig or preview mesh differs");
        return false;
    }
    const UScriptStruct* ExpectedOps[] = {
        FIKRetargetPelvisMotionOp::StaticStruct(),
        FIKRetargetFKChainsOp::StaticStruct(),
        FIKRetargetRunIKRigOp::StaticStruct(),
        FIKRetargetRootMotionOp::StaticStruct(),
        FIKRetargetCurveRemapOp::StaticStruct(),
    };
    if (Controller->GetNumRetargetOps() != UE_ARRAY_COUNT(ExpectedOps))
    {
        Error = FString::Printf(TEXT("IK Retargeter has %d ops, expected 5"),
            Controller->GetNumRetargetOps());
        return false;
    }
    TArray<TSharedPtr<FJsonValue>> OpsJson;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedOps); ++Index)
    {
        const FIKRetargetOpBase* Op = Controller->GetRetargetOpByIndex(Index);
        if (!Op || Op->GetType() != ExpectedOps[Index] ||
            !Controller->GetRetargetOpEnabled(Index))
        {
            Error = FString::Printf(TEXT("IK Retargeter op %d type/enabled state differs"), Index);
            return false;
        }
        OpsJson.Add(MakeShared<FJsonValueString>(Op->GetType()->GetPathName()));
    }
    for (const FChainSpec& Spec : ChainSpecs)
    {
        if (Controller->GetSourceChain(FName(Spec.Chain)) != FName(Spec.Chain))
        {
            Error = FString::Printf(TEXT("IK Retargeter target chain %s is not explicitly mapped"),
                Spec.Chain);
            return false;
        }
    }
    for (UIKRigDefinition* AssignedTargetRig : Controller->GetAllTargetIKRigs())
    {
        if (AssignedTargetRig != TargetRig)
        {
            Error = TEXT("An IK Retargeter op retained a non-master target IK Rig");
            return false;
        }
    }
    if (!ValidateSyntheticMetadata(Retargeter, TEXT("IK_RETARGETER"),
        SourceIKRigPath, Error))
    {
        return false;
    }
    OutJson->SetStringField(TEXT("status"), TEXT("PASS"));
    OutJson->SetStringField(TEXT("path"), Retargeter->GetPathName());
    OutJson->SetStringField(TEXT("source_ik_rig"), SourceRig->GetPathName());
    OutJson->SetStringField(TEXT("target_ik_rig"), TargetRig->GetPathName());
    OutJson->SetStringField(TEXT("source_mesh"), SourceMesh->GetPathName());
    OutJson->SetStringField(TEXT("target_mesh"), TargetMesh->GetPathName());
    OutJson->SetStringField(TEXT("chain_mapping_policy"), TEXT("EXPLICIT_EXACT_NAME_NO_FUZZY_AUTOMAP"));
    OutJson->SetArrayField(TEXT("op_stack"), OpsJson);
    return true;
}

bool EnsureRetargeter(UIKRigDefinition* SourceRig, UIKRigDefinition* TargetRig,
    USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UIKRetargeter*& OutRetargeter,
    TArray<FString>& Writes, FString& Error)
{
    OutRetargeter = LoadObject<UIKRetargeter>(nullptr, RetargeterPath);
    TSharedPtr<FJsonObject> Ignored;
    if (OutRetargeter)
    {
        return ValidateRetargeter(OutRetargeter, SourceRig, TargetRig, SourceMesh,
            TargetMesh, Ignored, Error);
    }
    if (LoadObject<UObject>(nullptr, RetargeterPath))
    {
        Error = TEXT("IK Retargeter path contains an unexpected class");
        return false;
    }
    OutRetargeter = CreateFactoryAsset<UIKRetargeter, UIKRetargetFactory>(
        RetargeterPath, Error);
    if (!OutRetargeter)
    {
        return false;
    }
    UIKRetargeterController* Controller = UIKRetargeterController::GetController(OutRetargeter);
    if (!Controller)
    {
        Error = TEXT("Could not acquire the UE 5.8 IK Retargeter controller");
        return false;
    }
    Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceRig);
    Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetRig);
    Controller->SetPreviewMesh(ERetargetSourceOrTarget::Source, SourceMesh);
    Controller->SetPreviewMesh(ERetargetSourceOrTarget::Target, TargetMesh);
    Controller->AddDefaultOps();
    Controller->AssignIKRigToAllOps(ERetargetSourceOrTarget::Source, SourceRig);
    Controller->AssignIKRigToAllOps(ERetargetSourceOrTarget::Target, TargetRig);
    for (const FChainSpec& Spec : ChainSpecs)
    {
        if (!Controller->SetSourceChain(FName(Spec.Chain), FName(Spec.Chain)))
        {
            Error = FString::Printf(TEXT("Could not explicitly map IK chain %s"), Spec.Chain);
            return false;
        }
    }
    SetSyntheticMetadata(OutRetargeter, TEXT("IK_RETARGETER"), SourceIKRigPath);
    OutRetargeter->PostEditChange();
    if (!SaveAsset(OutRetargeter, Error))
    {
        return false;
    }
    RecordWrite(Writes, OutRetargeter);
    return ValidateRetargeter(OutRetargeter, SourceRig, TargetRig, SourceMesh,
        TargetMesh, Ignored, Error);
}

bool EnsureRetargetedSequence(UAnimSequence* Raw, UIKRetargeter* Retargeter,
    USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, USkeleton* TargetSkeleton,
    UAnimSequence*& OutSequence, TArray<FString>& Writes, FString& Error)
{
    OutSequence = LoadObject<UAnimSequence>(nullptr, RetargetedSequencePath);
    TSharedPtr<FJsonObject> Ignored;
    if (OutSequence)
    {
        FString ValidationError;
        if (ValidateSequence(OutSequence, TargetSkeleton, TargetMesh, TEXT("RETARGETED"),
            RawSequencePath, false, true, true, Ignored, ValidationError))
        {
            return true;
        }
        return RepairLegacyTargetSequenceUnitScale(OutSequence, TargetSkeleton, TargetMesh,
            TEXT("RETARGETED"), RawSequencePath, false, Writes, Error);
    }
    if (LoadObject<UObject>(nullptr, RetargetedSequencePath))
    {
        Error = TEXT("Retargeted sequence path contains an unexpected class");
        return false;
    }

    FIKRetargetBatchOperationInputs Inputs;
    Inputs.AssetsToRetarget.Add(FAssetData(Raw));
    Inputs.SourceMesh = SourceMesh;
    Inputs.TargetMesh = TargetMesh;
    Inputs.IKRetargetAsset = Retargeter;
    Inputs.Search = FPackageName::ObjectPathToObjectName(FString(RawSequencePath));
    Inputs.Replace = FPackageName::ObjectPathToObjectName(FString(RetargetedSequencePath));
    Inputs.TargetPath = FPackageName::GetLongPackagePath(
        FPackageName::ObjectPathToPackageName(FString(RetargetedSequencePath)));
    Inputs.bUseSourcePath = false;
    Inputs.bIncludeReferencedAssets = false;
    Inputs.bOverwriteExistingFiles = false;
    Inputs.bRetainAdditiveFlags = true;
    const TArray<FAssetData> Outputs = UIKRetargetBatchOperation::RunBatchRetarget(Inputs);
    for (const FAssetData& Output : Outputs)
    {
        UAnimSequence* Candidate = Cast<UAnimSequence>(Output.GetAsset());
        if (Candidate && Candidate->GetPathName() == RetargetedSequencePath)
        {
            OutSequence = Candidate;
            break;
        }
    }
    if (!OutSequence)
    {
        OutSequence = LoadObject<UAnimSequence>(nullptr, RetargetedSequencePath);
    }
    if (!OutSequence)
    {
        Error = FString::Printf(TEXT("UE 5.8 RunBatchRetarget did not produce the exact output %s (outputs=%d)"),
            RetargetedSequencePath, Outputs.Num());
        return false;
    }
    OutSequence->Modify();
    OutSequence->SetPreviewMesh(TargetMesh);
    OutSequence->bLoop = false;
    OutSequence->RateScale = 1.f;
    OutSequence->Notifies.Reset();
    OutSequence->AnimNotifyTracks.Reset();
    OutSequence->InitializeNotifyTrack();
    if (!NormalizeLegacyNonUnitRootScaleCompatibility(OutSequence, TargetMesh, Error))
    {
        return false;
    }
    OutSequence->RefreshCacheData();
    SetSyntheticMetadata(OutSequence, TEXT("RETARGETED"), RawSequencePath);
    OutSequence->PostEditChange();
    if (!SaveAsset(OutSequence, Error))
    {
        return false;
    }
    RecordWrite(Writes, OutSequence);
    return ValidateSequence(OutSequence, TargetSkeleton, TargetMesh, TEXT("RETARGETED"),
        RawSequencePath, false, true, true, Ignored, Error);
}

bool EnsureDerivedSequence(const TCHAR* Path, UAnimSequence* Source, USkeleton* Skeleton,
    USkeletalMesh* Mesh, const TCHAR* Stage, const TCHAR* Predecessor,
    const bool bAuthorProductionCurves, UAnimSequence*& OutSequence,
    TArray<FString>& Writes, FString& Error)
{
    OutSequence = LoadObject<UAnimSequence>(nullptr, Path);
    TSharedPtr<FJsonObject> Ignored;
    if (OutSequence)
    {
        FString ValidationError;
        if (ValidateSequence(OutSequence, Skeleton, Mesh, Stage, Predecessor,
            true, true, true, Ignored, ValidationError))
        {
            return true;
        }
        return RepairLegacyTargetSequenceUnitScale(OutSequence, Skeleton, Mesh,
            Stage, Predecessor, true, Writes, Error);
    }
    if (LoadObject<UObject>(nullptr, Path))
    {
        Error = FString::Printf(TEXT("Derived sequence path contains an unexpected class: %s"), Path);
        return false;
    }
    OutSequence = DuplicateSequence(Path, Source, Skeleton, Mesh, Stage, Predecessor,
        bAuthorProductionCurves, Error);
    if (!OutSequence)
    {
        return false;
    }
    RecordWrite(Writes, OutSequence);
    return ValidateSequence(OutSequence, Skeleton, Mesh, Stage, Predecessor,
        true, true, true, Ignored, Error);
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
        Error = FString::Printf(TEXT("Could not resolve throw phase %s"), PhaseName);
        return false;
    }
    void* ValueAddress = PhaseProperty->ContainerPtrToValuePtr<void>(Notify);
    PhaseProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValueAddress, Value);
    return true;
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

bool AddNotify(UAnimMontage* Montage, UClass* NotifyClass, const float Time,
    const int32 TrackIndex, const FGuid& Guid, const TCHAR* PhaseName,
    const bool bBranchingPoint, FString& Error)
{
    UAnimNotify* Notify = NewObject<UAnimNotify>(Montage, NotifyClass, NAME_None, RF_Transactional);
    if (!Notify)
    {
        Error = FString::Printf(TEXT("Could not instantiate %s"), *NotifyClass->GetPathName());
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

UAnimMontage* CreateMontage(UAnimSequence* Sequence, USkeleton* Skeleton,
    USkeletalMesh* Mesh, FString& Error)
{
    if (!RefuseObjectPathCollision(FinalMontagePath, Error))
    {
        return nullptr;
    }
    const FString PackageName = FPackageName::ObjectPathToPackageName(FString(FinalMontagePath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(FString(FinalMontagePath));
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
        Error = TEXT("AnimMontageFactory failed to create the Session 5 test montage");
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Montage);
    return Montage;
}

bool ValidateMontage(UAnimMontage* Montage, UAnimSequence* Sequence, USkeleton* Skeleton,
    TSharedPtr<FJsonObject>& OutJson, FString& Error)
{
    OutJson = MakeShared<FJsonObject>();
    if (!Montage || Montage->GetSkeleton() != Skeleton ||
        !NearlyEqual(Montage->GetPlayLength(), DurationSeconds))
    {
        Error = TEXT("Session 5 final montage is missing or has the wrong skeleton/duration");
        return false;
    }
    if (Montage->SlotAnimTracks.Num() != 1 ||
        Montage->SlotAnimTracks[0].SlotName != DefaultSlot ||
        Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1 ||
        Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference() != Sequence)
    {
        Error = TEXT("Session 5 montage must contain exactly one final-sequence segment on DefaultSlot");
        return false;
    }
    if (Montage->CompositeSections.Num() != 1 ||
        Montage->CompositeSections[0].SectionName != FName(TEXT("Default")) ||
        !NearlyEqual(Montage->CompositeSections[0].GetTime(), 0.f) ||
        Montage->AnimNotifyTracks.Num() != 2 ||
        Montage->AnimNotifyTracks[0].TrackName != FName(TEXT("DG Phases")) ||
        Montage->AnimNotifyTracks[1].TrackName != FName(TEXT("DG Events")))
    {
        Error = TEXT("Session 5 montage section or notify-track layout differs");
        return false;
    }
    int32 ReleaseCount = 0;
    int32 FinishCount = 0;
    float ReleaseTime = -1.f;
    float FinishTime = -1.f;
    TMap<FString, int32> PhaseCounts;
    TMap<FString, float> PhaseTimes;
    TArray<TSharedPtr<FJsonValue>> EventsJson;
    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    {
        if (!Event.Notify)
        {
            Error = TEXT("Session 5 montage contains a named/state notify");
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
            if (Event.MontageTickType != EMontageNotifyTickType::Queued)
            {
                Error = TEXT("Throw phase events must be queued montage notifies");
                return false;
            }
            if (Event.TrackIndex != 0)
            {
                Error = TEXT("Throw phase events must live on the DG Phases track");
                return false;
            }
        }
        else if (ClassPath == ReleaseNotifyClassPath)
        {
            Label = TEXT("DG Release Disc");
            ++ReleaseCount;
            ReleaseTime = Time;
            if (Event.MontageTickType != EMontageNotifyTickType::BranchingPoint)
            {
                Error = TEXT("Disc release must be a montage branching point");
                return false;
            }
            if (Event.TrackIndex != 1)
            {
                Error = TEXT("Disc release must live on the DG Events track");
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
                Error = TEXT("Throw finish must be a montage branching point");
                return false;
            }
            if (Event.TrackIndex != 1)
            {
                Error = TEXT("Throw finish must live on the DG Events track");
                return false;
            }
        }
        else
        {
            Error = FString::Printf(TEXT("Unexpected Session 5 montage notify: %s"), *ClassPath);
            return false;
        }
        const TSharedRef<FJsonObject> EventJson = MakeShared<FJsonObject>();
        EventJson->SetStringField(TEXT("event"), Label);
        EventJson->SetStringField(TEXT("class"), ClassPath);
        EventJson->SetNumberField(TEXT("frame"), FMath::RoundToInt(Time * FrameRate));
        EventJson->SetNumberField(TEXT("time_seconds"), Time);
        EventsJson.Add(MakeShared<FJsonValueObject>(EventJson));
    }
    if (Montage->Notifies.Num() != UE_ARRAY_COUNT(PhaseEvents) + 2 ||
        ReleaseCount != 1 || FinishCount != 1 ||
        !NearlyEqual(ReleaseTime, FrameToTime(ReleaseFrame)) ||
        !NearlyEqual(FinishTime, FrameToTime(FinishFrame)))
    {
        Error = FString::Printf(TEXT("Session 5 release/finish cardinality or timing differs: release=%d finish=%d total=%d"),
            ReleaseCount, FinishCount, Montage->Notifies.Num());
        return false;
    }
    for (const FPhaseEventSpec& Spec : PhaseEvents)
    {
        const FString Name(Spec.Phase);
        if (PhaseCounts.FindRef(Name) != 1 ||
            !NearlyEqual(PhaseTimes.FindRef(Name), FrameToTime(Spec.Frame)))
        {
            Error = FString::Printf(TEXT("Phase %s is missing, duplicated, or mistimed"), Spec.Phase);
            return false;
        }
    }
    if (!ValidateSyntheticMetadata(Montage, TEXT("PRODUCTION_TEST_MONTAGE"),
        FinalSequencePath, Error))
    {
        return false;
    }
    OutJson->SetStringField(TEXT("status"), TEXT("PASS"));
    OutJson->SetStringField(TEXT("path"), Montage->GetPathName());
    OutJson->SetStringField(TEXT("sequence"), Sequence->GetPathName());
    OutJson->SetStringField(TEXT("slot"), DefaultSlot.ToString());
    OutJson->SetNumberField(TEXT("release_frame"), ReleaseFrame);
    OutJson->SetNumberField(TEXT("finish_frame"), FinishFrame);
    OutJson->SetNumberField(TEXT("release_notify_count"), ReleaseCount);
    OutJson->SetNumberField(TEXT("finish_notify_count"), FinishCount);
    OutJson->SetArrayField(TEXT("events"), EventsJson);
    return true;
}

bool EnsureMontage(UAnimSequence* Sequence, USkeleton* Skeleton, USkeletalMesh* Mesh,
    UAnimMontage*& OutMontage, TArray<FString>& Writes, FString& Error)
{
    OutMontage = LoadObject<UAnimMontage>(nullptr, FinalMontagePath);
    TSharedPtr<FJsonObject> Ignored;
    if (OutMontage)
    {
        return ValidateMontage(OutMontage, Sequence, Skeleton, Ignored, Error);
    }
    if (LoadObject<UObject>(nullptr, FinalMontagePath))
    {
        Error = TEXT("Final montage path contains an unexpected class");
        return false;
    }
    OutMontage = CreateMontage(Sequence, Skeleton, Mesh, Error);
    if (!OutMontage)
    {
        return false;
    }
    OutMontage->Modify();
    OutMontage->SlotAnimTracks.Reset();
    FSlotAnimationTrack& SlotTrack = OutMontage->SlotAnimTracks.AddDefaulted_GetRef();
    SlotTrack.SlotName = DefaultSlot;
    FAnimSegment& Segment = SlotTrack.AnimTrack.AnimSegments.AddDefaulted_GetRef();
    Segment.SetAnimReference(Sequence, true);
    Segment.StartPos = 0.f;
    Segment.AnimStartTime = 0.f;
    Segment.AnimEndTime = DurationSeconds;
    Segment.AnimPlayRate = 1.f;
    Segment.LoopingCount = 1;
    OutMontage->SetCompositeLength(DurationSeconds);

    OutMontage->CompositeSections.Reset();
    FCompositeSection& Section = OutMontage->CompositeSections.AddDefaulted_GetRef();
    Section.SectionName = TEXT("Default");
    Section.Link(OutMontage, 0.f, 0);

    OutMontage->Notifies.Reset();
    OutMontage->AnimNotifyTracks.Reset();
    FAnimNotifyTrack& PhaseTrack = OutMontage->AnimNotifyTracks.AddDefaulted_GetRef();
    PhaseTrack.TrackName = TEXT("DG Phases");
    PhaseTrack.TrackColor = FLinearColor(0.16f, 0.50f, 0.92f);
    FAnimNotifyTrack& EventTrack = OutMontage->AnimNotifyTracks.AddDefaulted_GetRef();
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
    int32 Ordinal = 1;
    for (const FPhaseEventSpec& Spec : PhaseEvents)
    {
        if (!AddNotify(OutMontage, PhaseClass, FrameToTime(Spec.Frame), 0,
            FGuid(0xD6155000u + Ordinal, 0xA11E0005u,
                0x50000000u + Ordinal, 0x00000005u), Spec.Phase, false, Error))
        {
            return false;
        }
        ++Ordinal;
    }
    if (!AddNotify(OutMontage, ReleaseClass, FrameToTime(ReleaseFrame), 1,
        FGuid(0xD61550F0u, 0xA11E0005u, 0x500000F0u, 0x00000005u),
        nullptr, true, Error) ||
        !AddNotify(OutMontage, FinishClass, FrameToTime(FinishFrame), 1,
        FGuid(0xD61550FFu, 0xA11E0005u, 0x500000FFu, 0x00000005u),
        nullptr, true, Error))
    {
        return false;
    }
    OutMontage->SortNotifies();
    OutMontage->RefreshCacheData();
    OutMontage->SetPreviewMesh(Mesh);
    OutMontage->bEnableAutoBlendOut = true;
    SetSyntheticMetadata(OutMontage, TEXT("PRODUCTION_TEST_MONTAGE"), FinalSequencePath);
    OutMontage->PostEditChange();
    if (!SaveAsset(OutMontage, Error))
    {
        return false;
    }
    RecordWrite(Writes, OutMontage);
    return ValidateMontage(OutMontage, Sequence, Skeleton, Ignored, Error);
}

UDiscGolfAnimationLibrary* CreateLibrary(FString& Error)
{
    if (!RefuseObjectPathCollision(LibraryPath, Error))
    {
        return nullptr;
    }
    const FString PackageName = FPackageName::ObjectPathToPackageName(FString(LibraryPath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(FString(LibraryPath));
    UPackage* Package = CreatePackage(*PackageName);
    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = UDiscGolfAnimationLibrary::StaticClass();
    UDiscGolfAnimationLibrary* Library = Cast<UDiscGolfAnimationLibrary>(
        Factory->FactoryCreateNew(UDiscGolfAnimationLibrary::StaticClass(), Package,
            FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional,
            nullptr, GWarn));
    if (!Library)
    {
        Error = TEXT("DataAssetFactory failed to create DA_DG_AnimationLibrary");
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Library);
    return Library;
}

bool ValidateLibrary(UDiscGolfAnimationLibrary* Library, UAnimMontage* Montage,
    TSharedPtr<FJsonObject>& OutJson, FString& Error)
{
    OutJson = MakeShared<FJsonObject>();
    if (!Library || Library->Entries.Num() != 1)
    {
        Error = TEXT("Session 5 animation library is missing or does not contain exactly one test entry");
        return false;
    }
    const FDGThrowAnimationEntry& Entry = Library->Entries[0];
    if (Entry.ThrowType != EDGThrowType::Backhand ||
        Entry.Handedness != EDGHandedness::Right || Entry.StyleId != SyntheticStyleId ||
        Entry.Montage.ToSoftObjectPath() != FSoftObjectPath(Montage) ||
        !NearlyEqual(Entry.RecommendedPowerMin, 0.f) ||
        !NearlyEqual(Entry.RecommendedPowerMax, 1.f))
    {
        Error = TEXT("DA_DG_AnimationLibrary test entry differs from the approved non-default fixture");
        return false;
    }
    if (!ValidateSyntheticMetadata(Library, TEXT("TEST_ANIMATION_LIBRARY"),
        FinalMontagePath, Error))
    {
        return false;
    }
    OutJson->SetStringField(TEXT("status"), TEXT("PASS"));
    OutJson->SetStringField(TEXT("path"), Library->GetPathName());
    OutJson->SetStringField(TEXT("style_id"), SyntheticStyleId.ToString());
    OutJson->SetStringField(TEXT("montage"), Montage->GetPathName());
    OutJson->SetBoolField(TEXT("runtime_default"), false);
    OutJson->SetBoolField(TEXT("shipping_allowed"), false);
    return true;
}

bool EnsureLibrary(UAnimMontage* Montage, UDiscGolfAnimationLibrary*& OutLibrary,
    TArray<FString>& Writes, FString& Error)
{
    OutLibrary = LoadObject<UDiscGolfAnimationLibrary>(nullptr, LibraryPath);
    TSharedPtr<FJsonObject> Ignored;
    if (OutLibrary)
    {
        return ValidateLibrary(OutLibrary, Montage, Ignored, Error);
    }
    if (LoadObject<UObject>(nullptr, LibraryPath))
    {
        Error = TEXT("Animation library path contains an unexpected class");
        return false;
    }
    OutLibrary = CreateLibrary(Error);
    if (!OutLibrary)
    {
        return false;
    }
    OutLibrary->Modify();
    OutLibrary->Entries.Reset();
    FDGThrowAnimationEntry& Entry = OutLibrary->Entries.AddDefaulted_GetRef();
    Entry.ThrowType = EDGThrowType::Backhand;
    Entry.Handedness = EDGHandedness::Right;
    Entry.StyleId = SyntheticStyleId;
    Entry.Montage = Montage;
    Entry.RecommendedPowerMin = 0.f;
    Entry.RecommendedPowerMax = 1.f;
    SetSyntheticMetadata(OutLibrary, TEXT("TEST_ANIMATION_LIBRARY"), FinalMontagePath);
    OutLibrary->PostEditChange();
    if (!SaveAsset(OutLibrary, Error))
    {
        return false;
    }
    RecordWrite(Writes, OutLibrary);
    return ValidateLibrary(OutLibrary, Montage, Ignored, Error);
}

FString BuildValidationReport(FString* OutError = nullptr)
{
    FString Error;
    USkeletalMesh* MasterMesh = LoadChecked<USkeletalMesh>(MasterMeshPath, Error);
    USkeleton* MasterSkeleton = LoadChecked<USkeleton>(MasterSkeletonPath, Error);
    UIKRigDefinition* MasterRig = LoadChecked<UIKRigDefinition>(MasterIKRigPath, Error);
    UAnimSequence* PrototypeSequence = LoadChecked<UAnimSequence>(PrototypeSequencePath, Error);
    UAnimMontage* PrototypeMontage = LoadChecked<UAnimMontage>(PrototypeMontagePath, Error);
    if (!MasterMesh || !MasterSkeleton || !MasterRig || !PrototypeSequence || !PrototypeMontage)
    {
        if (OutError) *OutError = Error;
        return FailureJson(Error);
    }

    USkeletalMesh* SourceMesh = LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
    USkeleton* SourceSkeleton = LoadObject<USkeleton>(nullptr, SourceSkeletonPath);
    UAnimSequence* Raw = LoadObject<UAnimSequence>(nullptr, RawSequencePath);
    UIKRigDefinition* SourceRig = LoadObject<UIKRigDefinition>(nullptr, SourceIKRigPath);
    UIKRetargeter* Retargeter = LoadObject<UIKRetargeter>(nullptr, RetargeterPath);
    UAnimSequence* Retargeted = LoadObject<UAnimSequence>(nullptr, RetargetedSequencePath);
    UAnimSequence* Cleaned = LoadObject<UAnimSequence>(nullptr, CleanedSequencePath);
    UAnimSequence* Final = LoadObject<UAnimSequence>(nullptr, FinalSequencePath);
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, FinalMontagePath);
    UDiscGolfAnimationLibrary* Library = LoadObject<UDiscGolfAnimationLibrary>(nullptr, LibraryPath);

    TSharedPtr<FJsonObject> RawJson;
    TSharedPtr<FJsonObject> SourceRigJson;
    TSharedPtr<FJsonObject> RetargeterJson;
    TSharedPtr<FJsonObject> RetargetedJson;
    TSharedPtr<FJsonObject> CleanedJson;
    TSharedPtr<FJsonObject> FinalJson;
    TSharedPtr<FJsonObject> MontageJson;
    TSharedPtr<FJsonObject> LibraryJson;
    if (!ValidateSourceAssets(SourceMesh, SourceSkeleton, MasterMesh, MasterSkeleton, Error) ||
        !ValidateSequence(Raw, SourceSkeleton, SourceMesh, TEXT("RAW_SOURCE"),
            PrototypeSequencePath, false, false, true, RawJson, Error) ||
        !ValidateSourceIKRig(SourceRig, SourceMesh, SourceRigJson, Error) ||
        !ValidateRetargeter(Retargeter, SourceRig, MasterRig, SourceMesh, MasterMesh,
            RetargeterJson, Error) ||
        !ValidateSequence(Retargeted, MasterSkeleton, MasterMesh, TEXT("RETARGETED"),
            RawSequencePath, false, true, true, RetargetedJson, Error) ||
        !ValidateSequence(Cleaned, MasterSkeleton, MasterMesh, TEXT("CLEANED"),
            RetargetedSequencePath, true, true, true, CleanedJson, Error) ||
        !ValidateSequence(Final, MasterSkeleton, MasterMesh, TEXT("PRODUCTION_TEST"),
            CleanedSequencePath, true, true, true, FinalJson, Error) ||
        !ValidateMontage(Montage, Final, MasterSkeleton, MontageJson, Error) ||
        !ValidateLibrary(Library, Montage, LibraryJson, Error))
    {
        if (OutError) *OutError = Error;
        return FailureJson(Error);
    }
    if (Raw == Retargeted || Raw == Cleaned || Raw == Final ||
        Retargeted == Cleaned || Retargeted == Final || Cleaned == Final)
    {
        Error = TEXT("Session 5 raw/retargeted/cleaned/production stages are not distinct assets");
        if (OutError) *OutError = Error;
        return FailureJson(Error);
    }

    const TCHAR* ProfilePaths[] = {
        TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact"),
        TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"),
        TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms"),
    };
    TArray<TSharedPtr<FJsonValue>> ProfilesJson;
    for (const TCHAR* ProfilePath : ProfilePaths)
    {
        if (!LoadObject<UObject>(nullptr, ProfilePath))
        {
            Error = FString::Printf(TEXT("Required profile smoke fixture is missing: %s"), ProfilePath);
            if (OutError) *OutError = Error;
            return FailureJson(Error);
        }
        ProfilesJson.Add(MakeShared<FJsonValueString>(ProfilePath));
    }

    const TSharedPtr<FJsonObject> SourceManifest = MakeShared<FJsonObject>();
    SourceManifest->SetNumberField(TEXT("schema_version"), SchemaVersion);
    SourceManifest->SetStringField(TEXT("classification"), TEXT("SYNTHETIC_TEST"));
    SourceManifest->SetStringField(TEXT("shipping_policy"), TEXT("DO_NOT_SHIP"));
    SourceManifest->SetStringField(TEXT("license_status"), TEXT("PROJECT_OWNED_SYNTHETIC"));
    SourceManifest->SetStringField(TEXT("motion_id"), TEXT("DG_RHBH_SYNTHETIC_PIPELINE"));
    SourceManifest->SetStringField(TEXT("source_id"), TEXT("PROJECT_SYNTHETIC_SESSION3"));
    SourceManifest->SetStringField(TEXT("take_id"), TEXT("SYNTHETIC_TAKE_001"));
    SourceManifest->SetStringField(TEXT("handedness"), TEXT("RIGHT"));
    SourceManifest->SetStringField(TEXT("throw_type"), TEXT("BACKHAND"));
    SourceManifest->SetNumberField(TEXT("source_frame_rate"), FrameRate);
    SourceManifest->SetStringField(TEXT("generation_source"), PrototypeSequencePath);
    SourceManifest->SetStringField(TEXT("authoring_method"),
        TEXT("PROJECT_OWNED_DUPLICATE_SOURCE_SKELETON_PLUS_UE5_8_IK_BATCH_RETARGET_PLUS_LEGACY_IMPORT_SCALE_COMPATIBILITY_NORMALIZATION"));
    SourceManifest->SetStringField(TEXT("retarget_unit_normalization"),
        RetargetUnitNormalization);
    SourceManifest->SetStringField(TEXT("compatibility_cause"),
        TEXT("PROJECT_LEGACY_ROOT_SCALE_100_WITH_METER_LOCAL_TRANSLATIONS_REQUIRES_REFERENCE_LOCAL_RECONSTRUCTION_AFTER_SCALE_STRIPPING"));
    SourceManifest->SetStringField(TEXT("compatibility_recipe"),
        TEXT("PRESERVE_BATCH_ROTATIONS_AND_ROOT_TRANSLATION;REBUILD_NON_ROOT_TRANSLATIONS_FROM_TARGET_REFERENCE_LOCALS;RESTORE_ALL_LOCAL_SCALES_FROM_TARGET_REFERENCE;REQUIRE_COMPONENT_SCALE_BIND_PARITY"));
    SourceManifest->SetBoolField(TEXT("external_capture"), false);
    SourceManifest->SetBoolField(TEXT("shipping_allowed"), false);

    const TSharedPtr<FJsonObject> PhaseManifest = MakeShared<FJsonObject>();
    PhaseManifest->SetNumberField(TEXT("schema_version"), SchemaVersion);
    PhaseManifest->SetStringField(TEXT("event_owner"), FinalMontagePath);
    PhaseManifest->SetStringField(TEXT("sequence_event_policy"),
        TEXT("ZERO_EVENTS_ONE_UE5_8_MANDATORY_EMPTY_TRACK"));
    TArray<TSharedPtr<FJsonValue>> PhaseEntries;
    for (const FPhaseEventSpec& Spec : PhaseEvents)
    {
        const TSharedRef<FJsonObject> Phase = MakeShared<FJsonObject>();
        Phase->SetStringField(TEXT("phase"), Spec.Phase);
        Phase->SetNumberField(TEXT("frame"), Spec.Frame);
        Phase->SetNumberField(TEXT("time_seconds"), FrameToTime(Spec.Frame));
        PhaseEntries.Add(MakeShared<FJsonValueObject>(Phase));
    }
    PhaseManifest->SetArrayField(TEXT("phases"), PhaseEntries);
    PhaseManifest->SetNumberField(TEXT("release_frame"), ReleaseFrame);
    PhaseManifest->SetNumberField(TEXT("finish_frame"), FinishFrame);
    PhaseManifest->SetNumberField(TEXT("release_count"), 1);
    PhaseManifest->SetNumberField(TEXT("finish_count"), 1);

    TArray<TSharedPtr<FJsonValue>> StagesJson;
    StagesJson.Add(MakeShared<FJsonValueObject>(RawJson.ToSharedRef()));
    StagesJson.Add(MakeShared<FJsonValueObject>(RetargetedJson.ToSharedRef()));
    StagesJson.Add(MakeShared<FJsonValueObject>(CleanedJson.ToSharedRef()));
    StagesJson.Add(MakeShared<FJsonValueObject>(FinalJson.ToSharedRef()));

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), TEXT("PASS_STRICT_SESSION5_SYNTHETIC_MOCAP_PIPELINE"));
    Root->SetStringField(TEXT("scope"), TEXT("SESSION_5_EDITOR_ONLY_SYNTHETIC_PIPELINE_FIXTURE"));
    Root->SetObjectField(TEXT("source_manifest"), SourceManifest);
    Root->SetObjectField(TEXT("phase_manifest"), PhaseManifest);
    Root->SetStringField(TEXT("source_mesh"), SourceMesh->GetPathName());
    Root->SetStringField(TEXT("source_skeleton"), SourceSkeleton->GetPathName());
    Root->SetObjectField(TEXT("source_ik_rig"), SourceRigJson);
    Root->SetObjectField(TEXT("ik_retargeter"), RetargeterJson);
    Root->SetArrayField(TEXT("pipeline_stages"), StagesJson);
    Root->SetObjectField(TEXT("production_test_montage"), MontageJson);
    Root->SetObjectField(TEXT("animation_library"), LibraryJson);
    Root->SetArrayField(TEXT("profile_smoke_assets"), ProfilesJson);
    Root->SetStringField(TEXT("accepted_prototype_sequence"), PrototypeSequence->GetPathName());
    Root->SetStringField(TEXT("accepted_prototype_montage"), PrototypeMontage->GetPathName());
    Root->SetStringField(TEXT("runtime_default_assignment"), TEXT("NONE"));
    Root->SetStringField(TEXT("throw_command_authority"), TEXT("UNCHANGED_RUNTIME_ADAPTER_AND_GAME_MODE"));
    Root->SetStringField(TEXT("release_transform_authority"), TEXT("UNCHANGED_FDGReleaseData_GRIP_WORLD_TRANSFORM"));
    Root->SetStringField(TEXT("validation_mutation_policy"), TEXT("READ_ONLY_NO_SAVE_NO_MODIFY"));
    if (OutError) OutError->Reset();
    return JsonString(Root);
}

bool ParseValidation(const FString& Validation, TSharedPtr<FJsonObject>& OutObject)
{
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Validation);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}
} // namespace DiscGolfSession5Mocap

FString UDiscGolfSession5MocapUtility::ValidateSession5Fixture()
{
    return DiscGolfSession5Mocap::BuildValidationReport();
}

FString UDiscGolfSession5MocapUtility::ProbeSession5UnitCompatibility()
{
    return DiscGolfSession5Mocap::BuildSession5UnitCompatibilityProbe();
}

FString UDiscGolfSession5MocapUtility::AuthorSession5Fixture()
{
    using namespace DiscGolfSession5Mocap;

    FString ExistingError;
    const FString ExistingValidation = BuildValidationReport(&ExistingError);
    if (ExistingError.IsEmpty())
    {
        TSharedPtr<FJsonObject> ExistingObject;
        if (!ParseValidation(ExistingValidation, ExistingObject))
        {
            return FailureJson(TEXT("Could not parse the current Session 5 validation report"));
        }
        ExistingObject->SetStringField(TEXT("authoring_status"),
            TEXT("PASS_ALREADY_CURRENT_NO_ASSET_WRITES"));
        ExistingObject->SetArrayField(TEXT("asset_writes"), TArray<TSharedPtr<FJsonValue>>());
        return JsonString(ExistingObject.ToSharedRef());
    }

    FString Error;
    USkeletalMesh* MasterMesh = LoadChecked<USkeletalMesh>(MasterMeshPath, Error);
    USkeleton* MasterSkeleton = LoadChecked<USkeleton>(MasterSkeletonPath, Error);
    UIKRigDefinition* MasterRig = LoadChecked<UIKRigDefinition>(MasterIKRigPath, Error);
    UAnimSequence* PrototypeSequence = LoadChecked<UAnimSequence>(PrototypeSequencePath, Error);
    UAnimMontage* PrototypeMontage = LoadChecked<UAnimMontage>(PrototypeMontagePath, Error);
    if (!MasterMesh || !MasterSkeleton || !MasterRig || !PrototypeSequence || !PrototypeMontage)
    {
        return FailureJson(Error);
    }

    TArray<FString> Writes;
    USkeletalMesh* SourceMesh = nullptr;
    USkeleton* SourceSkeleton = nullptr;
    UAnimSequence* Raw = nullptr;
    UIKRigDefinition* SourceRig = nullptr;
    UIKRetargeter* Retargeter = nullptr;
    UAnimSequence* Retargeted = nullptr;
    UAnimSequence* Cleaned = nullptr;
    UAnimSequence* Final = nullptr;
    UAnimMontage* Montage = nullptr;
    UDiscGolfAnimationLibrary* Library = nullptr;

    if (!EnsureSourceAssets(MasterMesh, MasterSkeleton, SourceMesh, SourceSkeleton,
            Writes, Error) ||
        !EnsureRawSequence(PrototypeSequence, SourceSkeleton, SourceMesh, Raw,
            Writes, Error) ||
        !EnsureSourceIKRig(SourceMesh, SourceRig, Writes, Error) ||
        !EnsureRetargeter(SourceRig, MasterRig, SourceMesh, MasterMesh, Retargeter,
            Writes, Error) ||
        !EnsureRetargetedSequence(Raw, Retargeter, SourceMesh, MasterMesh,
            MasterSkeleton, Retargeted, Writes, Error) ||
        !EnsureDerivedSequence(CleanedSequencePath, Retargeted, MasterSkeleton,
            MasterMesh, TEXT("CLEANED"), RetargetedSequencePath, true,
            Cleaned, Writes, Error) ||
        !EnsureDerivedSequence(FinalSequencePath, Cleaned, MasterSkeleton,
            MasterMesh, TEXT("PRODUCTION_TEST"), CleanedSequencePath, false,
            Final, Writes, Error) ||
        !EnsureMontage(Final, MasterSkeleton, MasterMesh, Montage, Writes, Error) ||
        !EnsureLibrary(Montage, Library, Writes, Error))
    {
        return FailureJson(Error);
    }

    FString ValidationError;
    const FString Validation = BuildValidationReport(&ValidationError);
    if (!ValidationError.IsEmpty())
    {
        return FailureJson(FString::Printf(TEXT("Post-author validation failed: %s"),
            *ValidationError));
    }
    TSharedPtr<FJsonObject> ValidationObject;
    if (!ParseValidation(Validation, ValidationObject))
    {
        return FailureJson(TEXT("Could not parse the Session 5 post-author validation report"));
    }
    ValidationObject->SetStringField(TEXT("authoring_status"),
        Writes.IsEmpty() ? TEXT("PASS_ALREADY_CURRENT_NO_ASSET_WRITES")
                         : TEXT("PASS_AUTHORED_AND_VALIDATED"));
    TArray<TSharedPtr<FJsonValue>> WriteJson;
    for (const FString& Path : Writes)
    {
        WriteJson.Add(MakeShared<FJsonValueString>(Path));
    }
    ValidationObject->SetArrayField(TEXT("asset_writes"), WriteJson);
    return JsonString(ValidationObject.ToSharedRef());
}
