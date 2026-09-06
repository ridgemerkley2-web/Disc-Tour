#include "DiscGolfAuthenticV008RetargetUtility.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Rig/IKRigDefinition.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargeter.h"
#include "RigEditor/IKRigController.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr const TCHAR* AuthorFlag = TEXT("DG_V008_RAW_RETARGET_AUTHORING");
constexpr const TCHAR* SourceSequencePath = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_Performer.AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_Performer");
constexpr const TCHAR* SourceMeshPath = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/SK_DG_RHBH_IMG2396_v008_DIAGNOSTIC_Performer.SK_DG_RHBH_IMG2396_v008_DIAGNOSTIC_Performer");
constexpr const TCHAR* SourceSkeletonPath = TEXT("/MetaHumanBodyTracker/metahuman_base_skel.metahuman_base_skel");
constexpr const TCHAR* SourceRigPath = TEXT("/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig");
constexpr const TCHAR* RetargeterPath = TEXT("/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster.RTG_MetaHuman_To_DGMaster");
constexpr const TCHAR* TargetRigPath = TEXT("/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master");
constexpr const TCHAR* TargetMeshPath = TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master");
constexpr const TCHAR* TargetSkeletonPath = TEXT("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master");
constexpr const TCHAR* OutputPackage = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW");
constexpr const TCHAR* OutputName = TEXT("AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW");
constexpr const TCHAR* OutputFolder = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted");
constexpr const TCHAR* OutputPath = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW.AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW");
constexpr const TCHAR* NormalizedAuthorFlag = TEXT("DG_V008_NORMALIZED_RETARGET_AUTHORING");
constexpr const TCHAR* NormalizedOutputPackage = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED");
constexpr const TCHAR* NormalizedOutputPath = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED.AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED");
constexpr const TCHAR* NormalizedOutputName = TEXT("AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED");
constexpr uint64 FrozenRawTrackDigest = 0x97541AC2B52744D3ull;
constexpr const TCHAR* CleanedAuthorFlag = TEXT("DG_V008_CLEANED_RETARGET_AUTHORING");
constexpr const TCHAR* CleanedOutputPackage = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_CLEANED");
constexpr const TCHAR* CleanedOutputPath = TEXT("/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_CLEANED.AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_CLEANED");
constexpr const TCHAR* CleanedOutputName = TEXT("AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_CLEANED");
constexpr int32 CleanedNativeSourceStartInclusive = 97;
constexpr int32 CleanedNativeSourceEndExclusive = 205;
constexpr int32 CleanedNativeSampleCount =
    CleanedNativeSourceEndExclusive - CleanedNativeSourceStartInclusive;
constexpr int32 CleanedNativeIntervalCount = CleanedNativeSampleCount - 1;
constexpr int32 CleanedEvaluatedSourceStartInclusive = 388;
constexpr int32 CleanedEvaluatedSourceEndExclusive = 817;
constexpr int32 CleanedEvaluatedSampleCount =
    CleanedEvaluatedSourceEndExclusive - CleanedEvaluatedSourceStartInclusive;
constexpr int32 CleanedEvaluatedIntervalCount = CleanedEvaluatedSampleCount - 1;
constexpr uint64 FrozenNormalizedTrackDigest = 0x4220EEE0AB54D010ull;
constexpr uint64 FrozenCleanedTrackDigest = 0x47CCCD2741D7B5A1ull;
// Probed from the frozen packages with ComputeFloatCurveSemanticDigest. These
// are intentionally independent of the bone-track digests above.
constexpr uint64 FrozenNormalizedFloatCurveDigest = 0x5A6BEB149B7218B0ull;
constexpr uint64 FrozenCleanedFloatCurveDigest = 0x37BBDC1EEA19FDBEull;

FString Payload(const FString& Status, const FString& Error = FString())
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("DiscGolfTour.AuthenticV008RawRetarget.v1"));
    Root->SetStringField(TEXT("status"), Status);
    Root->SetStringField(TEXT("source_sequence"), SourceSequencePath);
    Root->SetStringField(TEXT("source_mesh_override"), SourceMeshPath);
    Root->SetStringField(TEXT("retargeter"), RetargeterPath);
    Root->SetStringField(TEXT("target_mesh"), TargetMeshPath);
    Root->SetStringField(TEXT("output"), OutputPath);
    Root->SetBoolField(TEXT("diagnostic_only"), true);
    Root->SetBoolField(TEXT("runtime_authority_changed"), false);
    Root->SetBoolField(TEXT("production_promotion_allowed"), false);
    if (!Error.IsEmpty()) Root->SetStringField(TEXT("error"), Error);
    FString Out;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);
    return Out;
}

FString NormalizedPayload(const FString& Status, const FString& Error = FString())
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("DiscGolfTour.AuthenticV008NormalizedRetarget.v1"));
    Root->SetStringField(TEXT("status"), Status);
    Root->SetStringField(TEXT("predecessor"), OutputPath);
    Root->SetStringField(TEXT("output"), NormalizedOutputPath);
    Root->SetBoolField(TEXT("diagnostic_only"), true);
    Root->SetBoolField(TEXT("runtime_authority_changed"), false);
    Root->SetBoolField(TEXT("production_promotion_allowed"), false);
    if (!Error.IsEmpty()) Root->SetStringField(TEXT("error"), Error);
    FString Out;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);
    return Out;
}

FString CleanedPayload(const FString& Status, const FString& Error = FString())
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("DiscGolfTour.AuthenticV008CleanedRetarget.v1"));
    Root->SetStringField(TEXT("status"), Status);
    Root->SetStringField(TEXT("predecessor"), NormalizedOutputPath);
    Root->SetStringField(TEXT("output"), CleanedOutputPath);
    Root->SetNumberField(TEXT("native_sample_rate_hz"), 15);
    Root->SetNumberField(TEXT("native_source_start_inclusive"),
        CleanedNativeSourceStartInclusive);
    Root->SetNumberField(TEXT("native_source_end_exclusive"),
        CleanedNativeSourceEndExclusive);
    Root->SetNumberField(TEXT("native_output_sample_count"),
        CleanedNativeSampleCount);
    Root->SetNumberField(TEXT("native_output_interval_count"),
        CleanedNativeIntervalCount);
    Root->SetNumberField(TEXT("evaluated_sample_rate_hz"), 60);
    Root->SetNumberField(TEXT("evaluated_source_start_inclusive"),
        CleanedEvaluatedSourceStartInclusive);
    Root->SetNumberField(TEXT("evaluated_source_end_exclusive"),
        CleanedEvaluatedSourceEndExclusive);
    Root->SetNumberField(TEXT("evaluated_output_sample_count"),
        CleanedEvaluatedSampleCount);
    Root->SetNumberField(TEXT("evaluated_output_interval_count"),
        CleanedEvaluatedIntervalCount);
    Root->SetNumberField(TEXT("duration_seconds"),
        static_cast<double>(CleanedNativeIntervalCount) / 15.0);
    TSharedRef<FJsonObject> Phases = MakeShared<FJsonObject>();
    Phases->SetNumberField(TEXT("address"), 24);
    Phases->SetNumberField(TEXT("runup"), 60);
    Phases->SetNumberField(TEXT("reachback"), 120);
    Phases->SetNumberField(TEXT("plant"), 180);
    Phases->SetNumberField(TEXT("release"), 182);
    Phases->SetNumberField(TEXT("followthrough"), 216);
    Phases->SetNumberField(TEXT("recovery"), 268);
    Root->SetObjectField(TEXT("evaluated_phase_frames"), Phases);
    Root->SetBoolField(TEXT("crop_only"), true);
    Root->SetBoolField(TEXT("retimed"), false);
    Root->SetBoolField(TEXT("smoothed"), false);
    Root->SetBoolField(TEXT("interpolation_invented"), false);
    Root->SetBoolField(TEXT("diagnostic_only"), true);
    Root->SetBoolField(TEXT("runtime_authority_changed"), false);
    Root->SetBoolField(TEXT("production_promotion_allowed"), false);
    if (!Error.IsEmpty()) Root->SetStringField(TEXT("error"), Error);
    FString Out;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);
    return Out;
}

bool LoadContract(UAnimSequence*& Source, USkeletalMesh*& SourceMesh,
    UIKRetargeter*& Retargeter, USkeletalMesh*& TargetMesh, FString& Error)
{
    Source = LoadObject<UAnimSequence>(nullptr, SourceSequencePath);
    SourceMesh = LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
    Retargeter = LoadObject<UIKRetargeter>(nullptr, RetargeterPath);
    TargetMesh = LoadObject<USkeletalMesh>(nullptr, TargetMeshPath);
    UIKRigDefinition* SourceRig = LoadObject<UIKRigDefinition>(nullptr, SourceRigPath);
    UIKRigDefinition* TargetRig = LoadObject<UIKRigDefinition>(nullptr, TargetRigPath);
    if (!Source || !SourceMesh || !Retargeter || !TargetMesh || !SourceRig || !TargetRig)
    {
        Error = TEXT("An exact v008 retarget input is missing or has the wrong class.");
        return false;
    }
    if (!Source->GetSkeleton() || Source->GetSkeleton()->GetPathName() != SourceSkeletonPath
        || !SourceMesh->GetSkeleton() || SourceMesh->GetSkeleton()->GetPathName() != SourceSkeletonPath
        || !TargetMesh->GetSkeleton() || TargetMesh->GetSkeleton()->GetPathName() != TargetSkeletonPath
        || Source->GetPlayLength() < 17.06 || Source->GetPlayLength() > 17.08)
    {
        Error = TEXT("Source/target skeleton or source duration differs from the frozen contract.");
        return false;
    }
    UIKRigController* SourceRigController = UIKRigController::GetController(SourceRig);
    UIKRigController* TargetRigController = UIKRigController::GetController(TargetRig);
    UIKRetargeterController* RetargetController = UIKRetargeterController::GetController(Retargeter);
    if (!SourceRigController || !SourceRigController->IsSkeletalMeshCompatible(SourceMesh)
        || !TargetRigController || !TargetRigController->IsSkeletalMeshCompatible(TargetMesh)
        || !RetargetController
        || RetargetController->GetIKRig(ERetargetSourceOrTarget::Source) != SourceRig
        || RetargetController->GetIKRig(ERetargetSourceOrTarget::Target) != TargetRig
        || RetargetController->GetPreviewMesh(ERetargetSourceOrTarget::Target) != TargetMesh)
    {
        Error = TEXT("Performer mesh compatibility or exact retargeter rig contract failed.");
        return false;
    }
    const TPair<const TCHAR*, const TCHAR*> Mappings[] = {
        {TEXT("Root"), TEXT("Root")}, {TEXT("Spine"), TEXT("Spine")},
        {TEXT("Neck"), TEXT("Neck")}, {TEXT("LeftArm"), TEXT("Arm_L")},
        {TEXT("RightArm"), TEXT("Arm_R")}, {TEXT("LeftLeg"), TEXT("Leg_L")},
        {TEXT("RightLeg"), TEXT("Leg_R")}};
    if (RetargetController->GetNumRetargetOps() != 5)
    { Error = TEXT("Offline retarget operation count differs from five."); return false; }
    for (int32 Index = 0; Index < 5; ++Index)
        if (!RetargetController->GetRetargetOpEnabled(Index))
        { Error = TEXT("An offline retarget operation is unexpectedly disabled."); return false; }
    for (const auto& Mapping : Mappings)
        if (RetargetController->GetSourceChain(FName(Mapping.Value)) != FName(Mapping.Key))
        { Error = TEXT("An explicit source/target retarget chain mapping differs."); return false; }
    if (!RetargetController->GetSourceChain(TEXT("Foot_L")).IsNone()
        || !RetargetController->GetSourceChain(TEXT("Foot_R")).IsNone()
        || RetargetController->GetCurrentRetargetPoseName(
            ERetargetSourceOrTarget::Target) != FName(TEXT("MetaHumanAligned")))
    { Error = TEXT("Unmapped foot or target retarget-pose policy differs."); return false; }
    return true;
}

bool ValidateOutput(UAnimSequence* Output, UAnimSequence* Source,
    USkeletalMesh* TargetMesh, FString& Error)
{
    const TScriptInterface<IAnimationDataModel> OutputData = Output
        ? Output->GetDataModelInterface() : TScriptInterface<IAnimationDataModel>();
    const TScriptInterface<IAnimationDataModel> SourceData = Source
        ? Source->GetDataModelInterface() : TScriptInterface<IAnimationDataModel>();
    const IAnimationDataModel* OutputModel = OutputData.GetInterface();
    const IAnimationDataModel* SourceModel = SourceData.GetInterface();
    FMetaData* Metadata = Output ? &Output->GetOutermost()->GetMetaData() : nullptr;
    if (!Output || Output->GetPathName() != OutputPath || !Output->GetSkeleton()
        || Output->GetSkeleton()->GetPathName() != TargetSkeletonPath
        || Output->GetPreviewMesh() != TargetMesh
        || !FMath::IsNearlyEqual(Output->GetPlayLength(), Source->GetPlayLength(), 0.02f)
        || Output->bLoop || Output->bEnableRootMotion
        || Output->GetAdditiveAnimType() != AAT_None
        || !FMath::IsNearlyEqual(Output->RateScale, 1.0f)
        || !Output->Notifies.IsEmpty()
        || Output->AnimNotifyTracks.Num() != 1
        || !Output->AnimNotifyTracks[0].Notifies.IsEmpty()
        || !OutputModel || !SourceModel
        || OutputModel->GetNumberOfFrames() <= 0
        || OutputModel->GetNumberOfFrames() != SourceModel->GetNumberOfFrames()
        || OutputModel->GetFrameRate() != SourceModel->GetFrameRate()
        || !OutputModel->GetFrameRate().IsValid()
        || !Metadata
        || Metadata->GetValue(Output, TEXT("DG_Classification"))
            != FString(TEXT("AUTHENTIC_DIAGNOSTIC_RAW_RETARGET"))
        || Metadata->GetValue(Output, TEXT("DG_Predecessor")) != FString(SourceSequencePath)
        || Metadata->GetValue(Output, TEXT("DG_ProductionPromotionAllowed")) != FString(TEXT("false")))
    {
        Error = TEXT("RAW output path, target skeleton/mesh, duration, or playback policy differs.");
        return false;
    }

    // Metadata alone is insufficient: a legacy 0.01 root-scale mismatch can produce
    // a structurally valid sequence whose visible body is only a few centimetres.
    // Evaluate one real keyed pose in component space and reject both unit collapse
    // and a capture-space body axis that was never corrected into UE Z-up space.
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    TArray<FTransform> ComponentPose;
    ComponentPose.SetNum(RefSkeleton.GetNum());
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        TArray<FTransform> Track;
        OutputModel->GetBoneTrackTransforms(RefSkeleton.GetBoneName(BoneIndex), Track);
        if (Track.IsEmpty())
        {
            Error = FString::Printf(TEXT("RAW output is missing target track %s."),
                *RefSkeleton.GetBoneName(BoneIndex).ToString());
            return false;
        }
        const FTransform Local = Track[0];
        const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        ComponentPose[BoneIndex] = ParentIndex == INDEX_NONE
            ? Local : Local * ComponentPose[ParentIndex];
    }
    const int32 HeadIndex = RefSkeleton.FindBoneIndex(TEXT("head"));
    const int32 LeftFootIndex = RefSkeleton.FindBoneIndex(TEXT("foot_l"));
    const int32 RightFootIndex = RefSkeleton.FindBoneIndex(TEXT("foot_r"));
    if (HeadIndex == INDEX_NONE || LeftFootIndex == INDEX_NONE || RightFootIndex == INDEX_NONE)
    {
        Error = TEXT("RAW output target skeleton lacks head/foot plausibility landmarks.");
        return false;
    }
    const FVector Head = ComponentPose[HeadIndex].GetLocation();
    const FVector FootCenter = (ComponentPose[LeftFootIndex].GetLocation()
        + ComponentPose[RightFootIndex].GetLocation()) * 0.5;
    const FVector HeadToFoot = Head - FootCenter;
    const double BodyLengthCm = HeadToFoot.Length();
    const double ZUpRatio = BodyLengthCm > UE_SMALL_NUMBER
        ? FMath::Abs(HeadToFoot.Z) / BodyLengthCm : 0.0;
    if (BodyLengthCm < 100.0 || BodyLengthCm > 240.0 || ZUpRatio < 0.55)
    {
        Error = FString::Printf(
            TEXT("RAW pose is not production-plausible (head_to_foot_cm=%.4f,z_up_ratio=%.4f); legacy unit normalization and explicit axis correction are required."),
            BodyLengthCm, ZUpRatio);
        return false;
    }
    return true;
}

bool LoadTargetTracks(UAnimSequence* Sequence, const FReferenceSkeleton& RefSkeleton,
    TMap<FName, TArray<FTransform>>& OutTracks, FString& Error)
{
    OutTracks.Reset();
    const IAnimationDataModel* Model = Sequence
        ? Sequence->GetDataModelInterface().GetInterface() : nullptr;
    if (!Model || Model->GetNumberOfFrames() <= 0)
    { Error = TEXT("Derivative predecessor has no real animation data model."); return false; }
    const int32 ExpectedKeys = Model->GetNumberOfFrames() + 1;
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName Bone = RefSkeleton.GetBoneName(BoneIndex);
        TArray<FTransform> Keys;
        Model->GetBoneTrackTransforms(Bone, Keys);
        if (Keys.Num() != ExpectedKeys)
        {
            Error = FString::Printf(TEXT("Track %s has %d keys, expected %d."),
                *Bone.ToString(), Keys.Num(), ExpectedKeys);
            return false;
        }
        OutTracks.Add(Bone, MoveTemp(Keys));
    }
    return true;
}

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

bool IsValidUnitRotation(const FQuat& Value)
{
    const double SizeSquared = Value.SizeSquared();
    return !Value.ContainsNaN() && FMath::IsFinite(SizeSquared)
        && SizeSquared > UE_SMALL_NUMBER && FMath::Abs(SizeSquared - 1.0) <= 0.001;
}

bool ValidateTrackTransformsFiniteAndUnit(
    const TMap<FName, TArray<FTransform>>& Tracks, FString& Error)
{
    for (const TPair<FName, TArray<FTransform>>& Pair : Tracks)
        for (const FTransform& Key : Pair.Value)
            if (Key.ContainsNaN() || !IsFiniteVector(Key.GetTranslation())
                || !IsFiniteVector(Key.GetScale3D()) || !IsValidUnitRotation(Key.GetRotation()))
            {
                Error = FString::Printf(TEXT("Track %s contains a non-finite transform or non-unit/degenerate quaternion."),
                    *Pair.Key.ToString());
                return false;
            }
    return true;
}

uint64 ComputeRawTrackDigest(const TMap<FName, TArray<FTransform>>& Tracks,
    const FReferenceSkeleton& RefSkeleton)
{
    uint64 Hash = 1469598103934665603ull;
    const auto Feed = [&Hash](const void* Data, const SIZE_T Size)
    {
        const uint8* Bytes = static_cast<const uint8*>(Data);
        for (SIZE_T Index = 0; Index < Size; ++Index)
        { Hash ^= Bytes[Index]; Hash *= 1099511628211ull; }
    };
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FString Name = RefSkeleton.GetBoneName(BoneIndex).ToString();
        for (const TCHAR Character : Name) Feed(&Character, sizeof(Character));
        const TArray<FTransform>& Keys = Tracks.FindChecked(RefSkeleton.GetBoneName(BoneIndex));
        const int32 KeyCount = Keys.Num(); Feed(&KeyCount, sizeof(KeyCount));
        for (const FTransform& Key : Keys)
        {
            const FVector Translation = Key.GetTranslation();
            const FQuat Rotation = Key.GetRotation();
            const FVector Scale = Key.GetScale3D();
            Feed(&Translation, sizeof(Translation)); Feed(&Rotation, sizeof(Rotation));
            Feed(&Scale, sizeof(Scale));
        }
    }
    return Hash;
}

uint64 ComputeFloatCurveSemanticDigest(const IAnimationDataModel* Model)
{
    uint64 Hash = 1469598103934665603ull;
    const auto Feed = [&Hash](const void* Data, const SIZE_T Size)
    {
        const uint8* Bytes = static_cast<const uint8*>(Data);
        for (SIZE_T Index = 0; Index < Size; ++Index)
        { Hash ^= Bytes[Index]; Hash *= 1099511628211ull; }
    };
    const TCHAR Domain[] = TEXT("DG_V008_FLOAT_CURVE_SEMANTIC_V1");
    Feed(Domain, sizeof(Domain));
    if (!Model)
    {
        const int32 MissingModel = -1;
        Feed(&MissingModel, sizeof(MissingModel));
        return Hash;
    }
    TArray<FName> CurveNames;
    CurveNames.Reserve(Model->GetNumberOfFloatCurves());
    for (const FFloatCurve& Curve : Model->GetFloatCurves())
        CurveNames.Add(Curve.GetName());
    CurveNames.Sort([](const FName& A, const FName& B)
    {
        return A.LexicalLess(B);
    });
    const int32 CurveCount = CurveNames.Num();
    Feed(&CurveCount, sizeof(CurveCount));
    for (const FName CurveName : CurveNames)
    {
        const FString Name = CurveName.ToString();
        const int32 NameLength = Name.Len();
        Feed(&NameLength, sizeof(NameLength));
        Feed(*Name, NameLength * sizeof(TCHAR));
        const FAnimationCurveIdentifier Id(CurveName,
            ERawCurveTrackTypes::RCT_Float);
        const FFloatCurve* Curve = Model->FindFloatCurve(Id);
        const uint8 bCurvePresent = Curve ? 1 : 0;
        Feed(&bCurvePresent, sizeof(bCurvePresent));
        if (!Curve) continue;
        const int32 Flags = Curve->GetCurveTypeFlags();
        const int32 PreInfinity = static_cast<int32>(
            Curve->FloatCurve.PreInfinityExtrap.GetValue());
        const int32 PostInfinity = static_cast<int32>(
            Curve->FloatCurve.PostInfinityExtrap.GetValue());
        Feed(&Flags, sizeof(Flags));
        Feed(&PreInfinity, sizeof(PreInfinity));
        Feed(&PostInfinity, sizeof(PostInfinity));
        const uint8 bHasDefaultValue =
            Curve->FloatCurve.DefaultValue != MAX_flt ? 1 : 0;
        Feed(&bHasDefaultValue, sizeof(bHasDefaultValue));
        if (bHasDefaultValue)
            Feed(&Curve->FloatCurve.DefaultValue,
                sizeof(Curve->FloatCurve.DefaultValue));
        const TArray<FRichCurveKey>& Keys =
            Curve->FloatCurve.GetConstRefOfKeys();
        const int32 KeyCount = Keys.Num();
        Feed(&KeyCount, sizeof(KeyCount));
        for (const FRichCurveKey& Key : Keys)
        {
            const int32 InterpMode = static_cast<int32>(Key.InterpMode.GetValue());
            const int32 TangentMode = static_cast<int32>(Key.TangentMode.GetValue());
            const int32 TangentWeightMode = static_cast<int32>(
                Key.TangentWeightMode.GetValue());
            Feed(&Key.Time, sizeof(Key.Time));
            Feed(&Key.Value, sizeof(Key.Value));
            Feed(&InterpMode, sizeof(InterpMode));
            Feed(&TangentMode, sizeof(TangentMode));
            Feed(&TangentWeightMode, sizeof(TangentWeightMode));
            Feed(&Key.ArriveTangent, sizeof(Key.ArriveTangent));
            Feed(&Key.LeaveTangent, sizeof(Key.LeaveTangent));
            Feed(&Key.ArriveTangentWeight, sizeof(Key.ArriveTangentWeight));
            Feed(&Key.LeaveTangentWeight, sizeof(Key.LeaveTangentWeight));
        }
    }
    return Hash;
}

bool ValidateRejectedRawPredecessor(UAnimSequence* Raw, UAnimSequence* Source,
    USkeletalMesh* TargetMesh, FString& Error)
{
    const IAnimationDataModel* RawModel = Raw
        ? Raw->GetDataModelInterface().GetInterface() : nullptr;
    const IAnimationDataModel* SourceModel = Source
        ? Source->GetDataModelInterface().GetInterface() : nullptr;
    FMetaData* Metadata = Raw ? &Raw->GetOutermost()->GetMetaData() : nullptr;
    if (!Raw || Raw->GetPathName() != OutputPath || !Raw->GetSkeleton()
        || Raw->GetSkeleton()->GetPathName() != TargetSkeletonPath
        || Raw->GetPreviewMesh() != TargetMesh || Raw->bLoop || Raw->bEnableRootMotion
        || Raw->GetAdditiveAnimType() != AAT_None || !FMath::IsNearlyEqual(Raw->RateScale, 1.0f)
        || !RawModel || !SourceModel || RawModel->GetNumBoneTracks() != 69
        || RawModel->GetNumberOfFrames() <= 0
        || RawModel->GetNumberOfFrames() != SourceModel->GetNumberOfFrames()
        || RawModel->GetFrameRate() != SourceModel->GetFrameRate()
        || !FMath::IsNearlyEqual(Raw->GetPlayLength(), Source->GetPlayLength(), 0.001f)
        || !Raw->Notifies.IsEmpty() || Raw->AnimNotifyTracks.Num() != 1
        || !Raw->AnimNotifyTracks[0].Notifies.IsEmpty() || !Metadata
        || Metadata->GetValue(Raw, TEXT("DG_Classification"))
            != FString(TEXT("AUTHENTIC_DIAGNOSTIC_RAW_RETARGET"))
        || Metadata->GetValue(Raw, TEXT("DG_Predecessor")) != FString(SourceSequencePath)
        || Metadata->GetValue(Raw, TEXT("DG_ProductionPromotionAllowed")) != FString(TEXT("false")))
    { Error = TEXT("Rejected RAW predecessor provenance, timing, or playback contract differs."); return false; }
    TMap<FName, TArray<FTransform>> Tracks;
    if (!LoadTargetTracks(Raw, TargetMesh->GetRefSkeleton(), Tracks, Error)
        || Tracks.Num() != TargetMesh->GetRefSkeleton().GetNum()
        || !ValidateTrackTransformsFiniteAndUnit(Tracks, Error))
    { Error = TEXT("Rejected RAW predecessor target-track contract differs."); return false; }
    const uint64 TrackDigest = ComputeRawTrackDigest(Tracks, TargetMesh->GetRefSkeleton());
    if (TrackDigest != FrozenRawTrackDigest)
    {
        Error = FString::Printf(TEXT("Rejected RAW full-track digest differs (actual=%016llX)."),
            TrackDigest);
        return false;
    }
    return true;
}

void BuildComponentPose(const FReferenceSkeleton& RefSkeleton,
    const TMap<FName, TArray<FTransform>>& Tracks, const int32 Frame,
    TArray<FTransform>& OutPose)
{
    OutPose.SetNum(RefSkeleton.GetNum());
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FTransform& Local = Tracks.FindChecked(
            RefSkeleton.GetBoneName(BoneIndex))[Frame];
        const int32 Parent = RefSkeleton.GetParentIndex(BoneIndex);
        OutPose[BoneIndex] = Parent == INDEX_NONE ? Local : Local * OutPose[Parent];
    }
}

bool BuildNormalizedTracks(UAnimSequence* Raw, USkeletalMesh* TargetMesh,
    TMap<FName, TArray<FTransform>>& OutTracks, FString& Error)
{
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    if (!LoadTargetTracks(Raw, RefSkeleton, OutTracks, Error)) return false;
    const TArray<FTransform>& RefLocals = RefSkeleton.GetRefBonePose();
    const int32 RootIndex = RefSkeleton.FindBoneIndex(TEXT("root"));
    const int32 PelvisIndex = RefSkeleton.FindBoneIndex(TEXT("pelvis"));
    if (RootIndex == INDEX_NONE || PelvisIndex == INDEX_NONE
        || RefSkeleton.GetParentIndex(PelvisIndex) != RootIndex)
    { Error = TEXT("DGMaster root/pelvis reference hierarchy differs."); return false; }
    TArray<FTransform> RefComponents;
    RefComponents.SetNum(RefSkeleton.GetNum());
    for (int32 Index = 0; Index < RefSkeleton.GetNum(); ++Index)
    {
        const int32 Parent = RefSkeleton.GetParentIndex(Index);
        RefComponents[Index] = Parent == INDEX_NONE ? RefLocals[Index]
            : RefLocals[Index] * RefComponents[Parent];
    }
    TArray<FTransform>& RootTrack = OutTracks.FindChecked(TEXT("root"));
    const FVector RawOrigin = RootTrack[0].GetTranslation();
    const FVector RefPelvis = RefComponents[PelvisIndex].GetTranslation();
    const FVector ScaledPelvisLocal = RefLocals[PelvisIndex].GetTranslation()
        * RefLocals[RootIndex].GetScale3D();
    const FQuat AxisCorrection(FVector::YAxisVector, UE_HALF_PI);
    for (int32 Frame = 0; Frame < RootTrack.Num(); ++Frame)
    {
        const FVector DesiredPelvis = RefPelvis
            + AxisCorrection.RotateVector(RootTrack[Frame].GetTranslation() - RawOrigin);
        const FQuat CorrectedRootRotation =
            (AxisCorrection * RootTrack[Frame].GetRotation()).GetNormalized();
        RootTrack[Frame].SetRotation(CorrectedRootRotation);
        RootTrack[Frame].SetScale3D(RefLocals[RootIndex].GetScale3D());
        RootTrack[Frame].SetTranslation(DesiredPelvis
            - CorrectedRootRotation.RotateVector(ScaledPelvisLocal));
    }
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        if (BoneIndex == RootIndex) continue;
        TArray<FTransform>& Track = OutTracks.FindChecked(RefSkeleton.GetBoneName(BoneIndex));
        for (FTransform& Key : Track)
        {
            Key.SetTranslation(RefLocals[BoneIndex].GetTranslation());
            Key.SetScale3D(RefLocals[BoneIndex].GetScale3D());
        }
    }
    return true;
}

bool WriteTracks(UAnimSequence* Sequence,
    const TMap<FName, TArray<FTransform>>& Tracks, FString& Error)
{
    IAnimationDataController& Controller = Sequence->GetController();
    Controller.OpenBracket(NSLOCTEXT("DiscGolfV008", "NormalizeAxisRootPolicy",
        "Apply frozen v008 DGMaster normalization and axis/root policy"), false);
    bool bOkay = true;
    for (const TPair<FName, TArray<FTransform>>& Pair : Tracks)
    {
        TArray<FVector3f> Positions; TArray<FQuat4f> Rotations; TArray<FVector3f> Scales;
        Positions.Reserve(Pair.Value.Num()); Rotations.Reserve(Pair.Value.Num());
        Scales.Reserve(Pair.Value.Num());
        for (const FTransform& Key : Pair.Value)
        {
            Positions.Add(FVector3f(Key.GetTranslation()));
            Rotations.Add(FQuat4f(Key.GetRotation().GetNormalized()));
            Scales.Add(FVector3f(Key.GetScale3D()));
        }
        bOkay = Controller.SetBoneTrackKeys(Pair.Key, Positions, Rotations, Scales, false)
            && bOkay;
    }
    Controller.CloseBracket(false);
    if (!bOkay) Error = TEXT("Could not write every normalized DGMaster track.");
    return bOkay;
}

bool ValidateNormalizedOutput(UAnimSequence* Output, UAnimSequence* Raw,
    USkeletalMesh* TargetMesh, FString& Error)
{
    const IAnimationDataModel* OutputModel = Output
        ? Output->GetDataModelInterface().GetInterface() : nullptr;
    const IAnimationDataModel* RawModel = Raw
        ? Raw->GetDataModelInterface().GetInterface() : nullptr;
    if (!Output || Output->GetPathName() != NormalizedOutputPath
        || Output->GetSkeleton() != Raw->GetSkeleton() || Output->GetPreviewMesh() != TargetMesh
        || Output->bEnableRootMotion || Output->bLoop || Output->GetAdditiveAnimType() != AAT_None
        || !FMath::IsNearlyEqual(Output->RateScale, 1.0f)
        || !FMath::IsNearlyEqual(Output->GetPlayLength(), Raw->GetPlayLength(), 0.001f)
        || !OutputModel || !RawModel
        || OutputModel->GetNumBoneTracks() != 69 || RawModel->GetNumBoneTracks() != 69
        || OutputModel->GetNumberOfFrames() != RawModel->GetNumberOfFrames()
        || OutputModel->GetFrameRate() != RawModel->GetFrameRate()
        || !OutputModel->GetFrameRate().IsValid()
        || !Output->Notifies.IsEmpty() || Output->AnimNotifyTracks.Num() != 1
        || !Output->AnimNotifyTracks[0].Notifies.IsEmpty())
    { Error = TEXT("Normalized derivative identity or playback policy differs."); return false; }
    FMetaData& Metadata = Output->GetOutermost()->GetMetaData();
    if (Metadata.GetValue(Output, TEXT("DG_Classification"))
            != FString(TEXT("AUTHENTIC_DIAGNOSTIC_NORMALIZED_RETARGET"))
        || Metadata.GetValue(Output, TEXT("DG_Predecessor")) != FString(OutputPath)
        || Metadata.GetValue(Output, TEXT("DG_AxisPolicy")) != FString(TEXT("Q_Y_POS_90_PREMULTIPLY"))
        || Metadata.GetValue(Output, TEXT("DG_RootTranslationPolicy"))
            != FString(TEXT("SINGLE_AUTHORITY_RAW_ROOT_AS_SOURCE_PELVIS"))
        || Metadata.GetValue(Output, TEXT("DG_ProductionPromotionAllowed")) != FString(TEXT("false")))
    { Error = TEXT("Normalized derivative provenance/policy metadata differs."); return false; }
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    TMap<FName, TArray<FTransform>> Tracks, RawTracks;
    if (!LoadTargetTracks(Output, RefSkeleton, Tracks, Error)
        || !LoadTargetTracks(Raw, RefSkeleton, RawTracks, Error)
        || !ValidateTrackTransformsFiniteAndUnit(Tracks, Error)
        || !ValidateTrackTransformsFiniteAndUnit(RawTracks, Error)) return false;
    const TArray<FTransform>& RefLocals = RefSkeleton.GetRefBonePose();
    TArray<FTransform> RefComponents;
    RefComponents.SetNum(RefSkeleton.GetNum());
    for (int32 Index = 0; Index < RefSkeleton.GetNum(); ++Index)
    {
        const int32 Parent = RefSkeleton.GetParentIndex(Index);
        RefComponents[Index] = Parent == INDEX_NONE ? RefLocals[Index]
            : RefLocals[Index] * RefComponents[Parent];
    }
    const int32 RootIndex = RefSkeleton.FindBoneIndex(TEXT("root"));
    const int32 PelvisIndex = RefSkeleton.FindBoneIndex(TEXT("pelvis"));
    const int32 HeadIndex = RefSkeleton.FindBoneIndex(TEXT("head"));
    const int32 LF = RefSkeleton.FindBoneIndex(TEXT("foot_l"));
    const int32 RF = RefSkeleton.FindBoneIndex(TEXT("foot_r"));
    if (RootIndex == INDEX_NONE || PelvisIndex == INDEX_NONE || HeadIndex == INDEX_NONE
        || LF == INDEX_NONE || RF == INDEX_NONE) return false;
    const TArray<FTransform>& RootTrack = Tracks.FindChecked(TEXT("root"));
    const TArray<FTransform>& RawRoot = RawTracks.FindChecked(TEXT("root"));
    const FVector RawOrigin = RawRoot[0].GetTranslation();
    const FVector RefPelvis = RefComponents[PelvisIndex].GetTranslation();
    const FQuat AxisCorrection(FVector::YAxisVector, UE_HALF_PI);
    for (int32 Frame = 0; Frame < RootTrack.Num(); ++Frame)
    {
        for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
        {
            const FTransform& Key = Tracks.FindChecked(RefSkeleton.GetBoneName(BoneIndex))[Frame];
            const FQuat ExpectedRotation = BoneIndex == RootIndex
                ? (AxisCorrection * RawTracks.FindChecked(TEXT("root"))[Frame].GetRotation()).GetNormalized()
                : RawTracks.FindChecked(RefSkeleton.GetBoneName(BoneIndex))[Frame].GetRotation().GetNormalized();
            const double RotationDot = FMath::Abs(Key.GetRotation().GetNormalized() | ExpectedRotation);
            if (RotationDot < 0.999999)
            { Error = TEXT("Normalized rotation inheritance differs from the frozen sign-insensitive quaternion policy."); return false; }
            if (!Key.GetScale3D().Equals(RefLocals[BoneIndex].GetScale3D(), 0.01)
                || (BoneIndex != RootIndex && !Key.GetTranslation().Equals(
                    RefLocals[BoneIndex].GetTranslation(), 0.001)))
            { Error = TEXT("Normalized local translation/scale policy differs."); return false; }
        }
        TArray<FTransform> Pose;
        BuildComponentPose(RefSkeleton, Tracks, Frame, Pose);
        const FVector DesiredPelvis = RefPelvis + AxisCorrection.RotateVector(
            RawRoot[Frame].GetTranslation() - RawOrigin);
        if (!Pose[PelvisIndex].GetTranslation().Equals(DesiredPelvis, 0.1))
        { Error = TEXT("Normalized component pelvis violates single-authority policy."); return false; }
        const FVector Feet = (Pose[LF].GetTranslation() + Pose[RF].GetTranslation()) * 0.5;
        const FVector HeadToFeet = Pose[HeadIndex].GetTranslation() - Feet;
        const double Length = HeadToFeet.Length();
        if (Length < 100.0 || Length > 240.0 || HeadToFeet.Z / Length < 0.80)
        { Error = TEXT("Normalized body dimensions or signed Z-up ratio are implausible."); return false; }
        for (int32 BoneIndex = 1; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
        {
            const int32 Parent = RefSkeleton.GetParentIndex(BoneIndex);
            const double RefLength = FVector::Dist(RefComponents[BoneIndex].GetTranslation(),
                RefComponents[Parent].GetTranslation());
            const double ActualLength = FVector::Dist(Pose[BoneIndex].GetTranslation(),
                Pose[Parent].GetTranslation());
            if (RefLength > 0.01 && FMath::Abs(ActualLength / RefLength - 1.0) > 0.02)
            { Error = TEXT("Normalized component segment ratio exceeds two percent."); return false; }
            const FVector RefScale = RefComponents[BoneIndex].GetScale3D();
            const FVector ActualScale = Pose[BoneIndex].GetScale3D();
            if (!ActualScale.Equals(RefScale, FMath::Max(0.01, RefScale.GetAbsMax() * 0.01)))
            { Error = TEXT("Normalized component scale error exceeds one percent."); return false; }
        }
    }
    if (!Tracks.FindChecked(TEXT("pelvis"))[0].GetTranslation().Equals(
        RefLocals[PelvisIndex].GetTranslation(), 0.1))
    { Error = TEXT("Frame-zero pelvis local translation differs from DG reference."); return false; }
    return true;
}

bool ValidateFrozenNormalizedPredecessor(UAnimSequence* Normalized,
    UAnimSequence* Raw, USkeletalMesh* TargetMesh, FString& Error)
{
    if (!ValidateNormalizedOutput(Normalized, Raw, TargetMesh, Error))
        return false;
    const IAnimationDataModel* Model = Normalized
        ? Normalized->GetDataModelInterface().GetInterface() : nullptr;
    if (!Model || Model->GetNumBoneTracks() != 69
        || Model->GetNumberOfFloatCurves() != 3
        || Model->GetNumberOfTransformCurves() != 0)
    {
        Error = FString::Printf(
            TEXT("Frozen NORMALIZED predecessor track/curve contract differs (bones=%d,float_curves=%d,transform_curves=%d)."),
            Model ? Model->GetNumBoneTracks() : -1,
            Model ? Model->GetNumberOfFloatCurves() : -1,
            Model ? Model->GetNumberOfTransformCurves() : -1);
        return false;
    }
    const TMap<FName, int32> FrozenCurveKeyCounts = {
        {TEXT("bodyframesolved"), 257},
        {TEXT("mhfdsversion"), 1},
        {TEXT("disablefaceoverride"), 1}};
    for (const FFloatCurve& Curve : Model->GetFloatCurves())
    {
        const int32* ExpectedCount = FrozenCurveKeyCounts.Find(Curve.GetName());
        const TArray<FRichCurveKey>& Keys = Curve.FloatCurve.GetConstRefOfKeys();
        if (!ExpectedCount || Keys.Num() != *ExpectedCount || Keys.IsEmpty()
            || Curve.GetCurveTypeFlags() != 16
            || !FMath::IsNearlyZero(Keys[0].Time, 0.00001f)
            || (Curve.GetName() == FName(TEXT("bodyframesolved"))
                && !FMath::IsNearlyEqual(Keys.Last().Time, 1024.0f / 60.0f,
                    0.0001f)))
        {
            Error = TEXT("Frozen NORMALIZED float-curve names, key counts, or timing differ.");
            return false;
        }
        if (Curve.GetName() == FName(TEXT("bodyframesolved")))
        {
            for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
                if (!FMath::IsFinite(Keys[KeyIndex].Value)
                    || !FMath::IsNearlyEqual(Keys[KeyIndex].Time,
                        static_cast<float>(KeyIndex) / 15.0f, 0.0001f))
                {
                    Error = TEXT("Frozen bodyframesolved curve is not the exact finite 15 Hz key series.");
                    return false;
                }
        }
        else if (Keys[0].Value != 1.0f || Keys[0].Time != 0.0f)
        {
            Error = FString::Printf(
                TEXT("Frozen constant NORMALIZED curve %s is not value 1 at time zero."),
                *Curve.GetName().ToString());
            return false;
        }
    }
    const uint64 CurveDigest = ComputeFloatCurveSemanticDigest(Model);
    if (CurveDigest != FrozenNormalizedFloatCurveDigest)
    {
        Error = FString::Printf(
            TEXT("Frozen NORMALIZED full float-curve semantic digest differs (actual=%016llX,expected=%016llX)."),
            CurveDigest, FrozenNormalizedFloatCurveDigest);
        return false;
    }
    TMap<FName, TArray<FTransform>> Tracks;
    if (!LoadTargetTracks(Normalized, TargetMesh->GetRefSkeleton(), Tracks, Error)
        || !ValidateTrackTransformsFiniteAndUnit(Tracks, Error))
        return false;
    const uint64 TrackDigest = ComputeRawTrackDigest(Tracks,
        TargetMesh->GetRefSkeleton());
    if (TrackDigest != FrozenNormalizedTrackDigest)
    {
        FString CurveSummary;
        for (const FFloatCurve& Curve : Model->GetFloatCurves())
        {
            const TArray<FRichCurveKey>& Keys = Curve.FloatCurve.GetConstRefOfKeys();
            CurveSummary += FString::Printf(TEXT(" %s:%d[%g,%g]"),
                *Curve.GetName().ToString(), Keys.Num(),
                Keys.IsEmpty() ? -1.0 : Keys[0].Time,
                Keys.IsEmpty() ? -1.0 : Keys.Last().Time);
        }
        Error = FString::Printf(
            TEXT("Frozen NORMALIZED full-track digest differs (actual=%016llX); curves:%s."),
            TrackDigest, *CurveSummary);
        return false;
    }
    return true;
}

bool BuildCleanedCurveKeys(const IAnimationDataModel* SourceModel,
    TMap<FName, TArray<FRichCurveKey>>& OutCurves,
    TMap<FName, int32>& OutFlags, FString& Error)
{
    if (!SourceModel || SourceModel->GetNumberOfFloatCurves() != 3)
    {
        Error = TEXT("CLEANED curve crop requires the three frozen NORMALIZED float curves.");
        return false;
    }
    constexpr float SourceStartSeconds =
        static_cast<float>(CleanedNativeSourceStartInclusive) / 15.0f;
    constexpr float SourceEndSeconds =
        static_cast<float>(CleanedNativeSourceEndExclusive - 1) / 15.0f;
    constexpr float OutputDurationSeconds =
        static_cast<float>(CleanedNativeIntervalCount) / 15.0f;
    OutCurves.Reset();
    OutFlags.Reset();
    for (const FFloatCurve& Curve : SourceModel->GetFloatCurves())
    {
        TArray<FRichCurveKey> Cropped;
        const TArray<FRichCurveKey>& SourceKeys =
            Curve.FloatCurve.GetConstRefOfKeys();
        if (Curve.GetName() == FName(TEXT("bodyframesolved")))
        {
            for (const FRichCurveKey& SourceKey : SourceKeys)
                if (SourceKey.Time >= SourceStartSeconds - 0.0001f
                    && SourceKey.Time <= SourceEndSeconds + 0.0001f)
                {
                    FRichCurveKey Key = SourceKey;
                    Key.Time -= SourceStartSeconds;
                    if (FMath::IsNearlyZero(Key.Time, 0.0001f)) Key.Time = 0.0f;
                    if (FMath::IsNearlyEqual(Key.Time, OutputDurationSeconds, 0.0001f))
                        Key.Time = OutputDurationSeconds;
                    Cropped.Add(Key);
                }
            if (Cropped.Num() != 108 || Cropped[0].Time != 0.0f
                || Cropped.Last().Time != OutputDurationSeconds)
            {
                Error = TEXT("bodyframesolved curve does not map exactly from NORMALIZED keys 97..204.");
                return false;
            }
        }
        else
        {
            if (SourceKeys.Num() != 1)
            {
                Error = FString::Printf(TEXT("Frozen constant curve %s is not single-key."),
                    *Curve.GetName().ToString());
                return false;
            }
            FRichCurveKey Key = SourceKeys[0];
            Key.Time = 0.0f;
            Cropped.Add(Key);
        }
        OutFlags.Add(Curve.GetName(), Curve.GetCurveTypeFlags());
        OutCurves.Add(Curve.GetName(), MoveTemp(Cropped));
    }
    return OutCurves.Num() == 3;
}

bool BuildCleanedTracks(UAnimSequence* Normalized, USkeletalMesh* TargetMesh,
    TMap<FName, TArray<FTransform>>& OutTracks, FString& Error)
{
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    TMap<FName, TArray<FTransform>> SourceTracks;
    if (!LoadTargetTracks(Normalized, RefSkeleton, SourceTracks, Error)
        || !ValidateTrackTransformsFiniteAndUnit(SourceTracks, Error)
        || SourceTracks.FindChecked(TEXT("root")).Num()
            < CleanedNativeSourceEndExclusive)
    {
        Error = TEXT("NORMALIZED predecessor is too short for the frozen crop window.");
        return false;
    }
    OutTracks.Reset();
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName Bone = RefSkeleton.GetBoneName(BoneIndex);
        const TArray<FTransform>& SourceKeys = SourceTracks.FindChecked(Bone);
        TArray<FTransform> CroppedKeys;
        CroppedKeys.Reserve(CleanedNativeSampleCount);
        for (int32 SourceKey = CleanedNativeSourceStartInclusive;
            SourceKey < CleanedNativeSourceEndExclusive; ++SourceKey)
            CroppedKeys.Add(SourceKeys[SourceKey]);
        OutTracks.Add(Bone, MoveTemp(CroppedKeys));
    }
    if (!ValidateTrackTransformsFiniteAndUnit(OutTracks, Error))
        return false;
    return true;
}

bool BuildEvaluatedTracks60Hz(UAnimSequence* Sequence,
    USkeletalMesh* TargetMesh, const int32 EvaluatedSourceStart,
    TMap<FName, TArray<FTransform>>& OutTracks, FString& Error)
{
    if (!Sequence || !TargetMesh)
    {
        Error = TEXT("60 Hz evaluated crop validation input is missing.");
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    OutTracks.Reset();
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName Bone = RefSkeleton.GetBoneName(BoneIndex);
        TArray<FTransform> Samples;
        Samples.Reserve(CleanedEvaluatedSampleCount);
        for (int32 OutputFrame = 0; OutputFrame < CleanedEvaluatedSampleCount;
            ++OutputFrame)
        {
            FTransform Sample;
            const FAnimExtractContext Context(
                static_cast<double>(EvaluatedSourceStart + OutputFrame) / 60.0,
                false);
            Sequence->GetBoneTransform(Sample,
                FSkeletonPoseBoneIndex(BoneIndex), Context, true);
            Samples.Add(Sample);
        }
        OutTracks.Add(Bone, MoveTemp(Samples));
    }
    return ValidateTrackTransformsFiniteAndUnit(OutTracks, Error);
}

bool IsSameCroppedTransform(const FTransform& A, const FTransform& B)
{
    const double RotationDot = FMath::Abs(
        A.GetRotation().GetNormalized() | B.GetRotation().GetNormalized());
    return A.GetTranslation().Equals(B.GetTranslation(), 0.0001)
        && A.GetScale3D().Equals(B.GetScale3D(), 0.0001)
        && RotationDot >= 0.99999999;
}

bool ValidateNoLongFullPoseHold(const TMap<FName, TArray<FTransform>>& Tracks,
    const FReferenceSkeleton& RefSkeleton, FString& Error)
{
    int32 ConsecutiveHeldTransitions = 0;
    for (int32 Frame = 1; Frame < CleanedEvaluatedSampleCount; ++Frame)
    {
        bool bFullPoseHeld = true;
        for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
        {
            const TArray<FTransform>& Keys = Tracks.FindChecked(
                RefSkeleton.GetBoneName(BoneIndex));
            const FTransform& Previous = Keys[Frame - 1];
            const FTransform& Current = Keys[Frame];
            const double Dot = FMath::Clamp(FMath::Abs(
                Previous.GetRotation().GetNormalized()
                    | Current.GetRotation().GetNormalized()), 0.0, 1.0);
            const double RotationDegrees = FMath::RadiansToDegrees(2.0 * FMath::Acos(Dot));
            if (FVector::Dist(Previous.GetTranslation(), Current.GetTranslation()) > 0.01
                || RotationDegrees > 0.01)
            {
                bFullPoseHeld = false;
                break;
            }
        }
        ConsecutiveHeldTransitions = bFullPoseHeld
            ? ConsecutiveHeldTransitions + 1 : 0;
        if (ConsecutiveHeldTransitions >= 4)
        {
            Error = FString::Printf(
                TEXT("CLEANED crop contains at least four consecutive full-pose holds ending at output frame %d."),
                Frame);
            return false;
        }
    }
    return true;
}

bool WriteCleanedTracks(UAnimSequence* Sequence,
    const TMap<FName, TArray<FTransform>>& Tracks,
    const FReferenceSkeleton& RefSkeleton, const IAnimationDataModel* SourceModel,
    FString& Error)
{
    TMap<FName, TArray<FRichCurveKey>> CroppedCurves;
    TMap<FName, int32> CurveFlags;
    if (!BuildCleanedCurveKeys(SourceModel, CroppedCurves, CurveFlags, Error))
        return false;
    IAnimationDataController& Controller = Sequence->GetController();
    Controller.OpenBracket(NSLOCTEXT("DiscGolfV008", "CropNormalizedTiming",
        "Crop frozen v008 NORMALIZED samples without retiming"), false);
    Controller.RemoveAllBoneTracks(false);
    Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, false);
    Controller.SetFrameRate(SourceModel->GetFrameRate(), false);
    Controller.SetNumberOfFrames(FFrameNumber(CleanedNativeIntervalCount), false);
    bool bOkay = true;
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName Bone = RefSkeleton.GetBoneName(BoneIndex);
        const TArray<FTransform>& Keys = Tracks.FindChecked(Bone);
        if (!Controller.AddBoneCurve(Bone, false))
        {
            bOkay = false;
            Error = FString::Printf(TEXT("Could not add CLEANED track %s."),
                *Bone.ToString());
            break;
        }
        TArray<FVector3f> Positions;
        TArray<FQuat4f> Rotations;
        TArray<FVector3f> Scales;
        Positions.Reserve(Keys.Num());
        Rotations.Reserve(Keys.Num());
        Scales.Reserve(Keys.Num());
        for (const FTransform& Key : Keys)
        {
            Positions.Add(FVector3f(Key.GetTranslation()));
            Rotations.Add(FQuat4f(Key.GetRotation()));
            Scales.Add(FVector3f(Key.GetScale3D()));
        }
        if (!Controller.SetBoneTrackKeys(Bone, Positions, Rotations, Scales, false))
        {
            bOkay = false;
            Error = FString::Printf(TEXT("Could not write CLEANED track %s."),
                *Bone.ToString());
            break;
        }
    }
    if (bOkay)
        for (const FFloatCurve& SourceCurve : SourceModel->GetFloatCurves())
        {
            const FName CurveName = SourceCurve.GetName();
            const FAnimationCurveIdentifier Id(CurveName,
                ERawCurveTrackTypes::RCT_Float);
            if (!Controller.AddCurve(Id, CurveFlags.FindChecked(CurveName), false)
                || !Controller.SetCurveKeys(Id,
                    CroppedCurves.FindChecked(CurveName), false))
            {
                bOkay = false;
                Error = FString::Printf(TEXT("Could not write CLEANED curve %s."),
                    *CurveName.ToString());
                break;
            }
        }
    Controller.CloseBracket(false);
    return bOkay;
}

bool ValidateCleanedOutput(UAnimSequence* Output, UAnimSequence* Normalized,
    USkeletalMesh* TargetMesh, FString& Error)
{
    const IAnimationDataModel* OutputModel = Output
        ? Output->GetDataModelInterface().GetInterface() : nullptr;
    const IAnimationDataModel* SourceModel = Normalized
        ? Normalized->GetDataModelInterface().GetInterface() : nullptr;
    if (!Output || Output->GetPathName() != CleanedOutputPath
        || Output->GetSkeleton() != Normalized->GetSkeleton()
        || Output->GetPreviewMesh() != TargetMesh
        || Output->bEnableRootMotion || Output->bLoop
        || Output->GetAdditiveAnimType() != AAT_None
        || !FMath::IsNearlyEqual(Output->RateScale, 1.0f)
        || !OutputModel || !SourceModel
        || OutputModel->GetNumBoneTracks() != 69
        || OutputModel->GetNumberOfFrames() != CleanedNativeIntervalCount
        || OutputModel->GetFrameRate() != SourceModel->GetFrameRate()
        || !FMath::IsNearlyEqual(OutputModel->GetFrameRate().AsDecimal(),
            15.0, 0.000001)
        || OutputModel->GetNumberOfFloatCurves() != 3
        || OutputModel->GetNumberOfTransformCurves() != 0
        || !FMath::IsNearlyEqual(Output->GetPlayLength(),
            static_cast<double>(CleanedNativeIntervalCount) / 15.0, 0.0001)
        || !Output->Notifies.IsEmpty() || Output->AnimNotifyTracks.Num() != 1
        || !Output->AnimNotifyTracks[0].Notifies.IsEmpty())
    {
        Error = FString::Printf(
            TEXT("CLEANED derivative identity/native timing/playback differs (path=%s,skeleton_match=%s,mesh_match=%s,root_motion=%s,loop=%s,additive=%d,rate=%g,bones=%d,frames=%d,fps=%d/%d,float_curves=%d,transform_curves=%d,length=%g,notifies=%d,notify_tracks=%d)."),
            Output ? *Output->GetPathName() : TEXT("missing"),
            Output && Normalized && Output->GetSkeleton() == Normalized->GetSkeleton()
                ? TEXT("true") : TEXT("false"),
            Output && Output->GetPreviewMesh() == TargetMesh ? TEXT("true") : TEXT("false"),
            Output && Output->bEnableRootMotion ? TEXT("true") : TEXT("false"),
            Output && Output->bLoop ? TEXT("true") : TEXT("false"),
            Output ? static_cast<int32>(Output->GetAdditiveAnimType()) : -1,
            Output ? Output->RateScale : -1.0,
            OutputModel ? OutputModel->GetNumBoneTracks() : -1,
            OutputModel ? OutputModel->GetNumberOfFrames() : -1,
            OutputModel ? OutputModel->GetFrameRate().Numerator : -1,
            OutputModel ? OutputModel->GetFrameRate().Denominator : -1,
            OutputModel ? OutputModel->GetNumberOfFloatCurves() : -1,
            OutputModel ? OutputModel->GetNumberOfTransformCurves() : -1,
            Output ? Output->GetPlayLength() : -1.0,
            Output ? Output->Notifies.Num() : -1,
            Output ? Output->AnimNotifyTracks.Num() : -1);
        return false;
    }
    FMetaData& Metadata = Output->GetOutermost()->GetMetaData();
    const TPair<const TCHAR*, const TCHAR*> RequiredMetadata[] = {
        {TEXT("DG_Classification"), TEXT("AUTHENTIC_DIAGNOSTIC_CLEANED_CROP")},
        {TEXT("DG_Predecessor"), NormalizedOutputPath},
        {TEXT("DG_CropPolicy"), TEXT("NATIVE_KEYS_[97,205)_NO_RETIME_NO_SMOOTHING_NO_NEW_INTERPOLATION")},
        {TEXT("DG_NativeSourceKeyMap"), TEXT("OUTPUT_KEY_N_EQUALS_NORMALIZED_KEY_97_PLUS_N")},
        {TEXT("DG_NativeSampleRateHz"), TEXT("15")},
        {TEXT("DG_NativeSourceStartInclusive"), TEXT("97")},
        {TEXT("DG_NativeSourceEndExclusive"), TEXT("205")},
        {TEXT("DG_NativeOutputSampleCount"), TEXT("108")},
        {TEXT("DG_NativeOutputIntervalCount"), TEXT("107")},
        {TEXT("DG_EvaluatedSourceFrameMap"), TEXT("OUTPUT_FRAME_N_EQUALS_NORMALIZED_FRAME_388_PLUS_N")},
        {TEXT("DG_EvaluatedSampleRateHz"), TEXT("60")},
        {TEXT("DG_EvaluatedSourceStartInclusive"), TEXT("388")},
        {TEXT("DG_EvaluatedSourceEndExclusive"), TEXT("817")},
        {TEXT("DG_EvaluatedOutputSampleCount"), TEXT("429")},
        {TEXT("DG_EvaluatedOutputIntervalCount"), TEXT("428")},
        {TEXT("DG_CurveCropPolicy"), TEXT("BODYFRAMESOLVED_KEYS_97_TO_204_SHIFTED;CONSTANT_CURVES_REBASED_AT_ZERO")},
        {TEXT("DG_EvaluatedPhaseFrameMap"), TEXT("address=24;runup=60;reachback=120;plant=180;release=182;followthrough=216;recovery=268")},
        {TEXT("DG_AxisPolicy"), TEXT("Q_Y_POS_90_PREMULTIPLY")},
        {TEXT("DG_RootTranslationPolicy"), TEXT("SINGLE_AUTHORITY_RAW_ROOT_AS_SOURCE_PELVIS")},
        {TEXT("DG_ProductionPromotionAllowed"), TEXT("false")}};
    for (const TPair<const TCHAR*, const TCHAR*>& Pair : RequiredMetadata)
        if (Metadata.GetValue(Output, Pair.Key) != FString(Pair.Value))
        {
            Error = FString::Printf(TEXT("CLEANED metadata differs for %s."), Pair.Key);
            return false;
        }
    const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
    TMap<FName, TArray<FTransform>> OutputTracks;
    TMap<FName, TArray<FTransform>> ExpectedTracks;
    if (!LoadTargetTracks(Output, RefSkeleton, OutputTracks, Error)
        || !ValidateTrackTransformsFiniteAndUnit(OutputTracks, Error)
        || !BuildCleanedTracks(Normalized, TargetMesh, ExpectedTracks, Error))
        return false;
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName Bone = RefSkeleton.GetBoneName(BoneIndex);
        const TArray<FTransform>& OutputKeys = OutputTracks.FindChecked(Bone);
        const TArray<FTransform>& ExpectedKeys = ExpectedTracks.FindChecked(Bone);
        if (OutputKeys.Num() != CleanedNativeSampleCount)
        {
            Error = FString::Printf(TEXT("CLEANED track %s does not contain 108 native samples."),
                *Bone.ToString());
            return false;
        }
        if (!IsSameCroppedTransform(OutputKeys[0], ExpectedKeys[0])
            || !IsSameCroppedTransform(OutputKeys.Last(), ExpectedKeys.Last()))
        {
            Error = FString::Printf(TEXT("CLEANED boundary equality differs for %s."),
                *Bone.ToString());
            return false;
        }
        for (int32 OutputKey = 0; OutputKey < CleanedNativeSampleCount; ++OutputKey)
            if (!IsSameCroppedTransform(OutputKeys[OutputKey],
                    ExpectedKeys[OutputKey]))
            {
                Error = FString::Printf(
                    TEXT("CLEANED native output-to-source key map differs for %s at output key %d/source key %d."),
                    *Bone.ToString(), OutputKey,
                    CleanedNativeSourceStartInclusive + OutputKey);
                return false;
            }
    }
    TMap<FName, TArray<FTransform>> EvaluatedOutputTracks;
    TMap<FName, TArray<FTransform>> EvaluatedSourceTracks;
    if (!BuildEvaluatedTracks60Hz(Output, TargetMesh, 0,
            EvaluatedOutputTracks, Error)
        || !BuildEvaluatedTracks60Hz(Normalized, TargetMesh,
            CleanedEvaluatedSourceStartInclusive, EvaluatedSourceTracks, Error))
        return false;
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName Bone = RefSkeleton.GetBoneName(BoneIndex);
        const TArray<FTransform>& OutputSamples =
            EvaluatedOutputTracks.FindChecked(Bone);
        const TArray<FTransform>& SourceSamples =
            EvaluatedSourceTracks.FindChecked(Bone);
        for (int32 Frame = 0; Frame < CleanedEvaluatedSampleCount; ++Frame)
            if (!IsSameCroppedTransform(OutputSamples[Frame], SourceSamples[Frame]))
            {
                Error = FString::Printf(
                    TEXT("CLEANED evaluated 60 Hz mapping differs for %s at output frame %d/source frame %d."),
                    *Bone.ToString(), Frame,
                    CleanedEvaluatedSourceStartInclusive + Frame);
                return false;
            }
    }
    if (!ValidateNoLongFullPoseHold(EvaluatedOutputTracks, RefSkeleton, Error))
        return false;
    TMap<FName, TArray<FRichCurveKey>> ExpectedCurves;
    TMap<FName, int32> ExpectedCurveFlags;
    if (!BuildCleanedCurveKeys(SourceModel, ExpectedCurves, ExpectedCurveFlags, Error))
        return false;
    for (const TPair<FName, TArray<FRichCurveKey>>& Pair : ExpectedCurves)
    {
        const FAnimationCurveIdentifier Id(Pair.Key,
            ERawCurveTrackTypes::RCT_Float);
        const FFloatCurve* ActualCurve = OutputModel->FindFloatCurve(Id);
        int32 FirstMismatchedKey = INDEX_NONE;
        bool bKeysSemanticallyEqual = false;
        if (ActualCurve)
        {
            const TArray<FRichCurveKey>& ActualKeys =
                ActualCurve->FloatCurve.GetConstRefOfKeys();
            if (ActualKeys.Num() == Pair.Value.Num())
            {
                bKeysSemanticallyEqual = true;
                for (int32 KeyIndex = 0; KeyIndex < ActualKeys.Num(); ++KeyIndex)
                {
                    const FRichCurveKey& Actual = ActualKeys[KeyIndex];
                    const FRichCurveKey& Expected = Pair.Value[KeyIndex];
                    const bool bSame = FMath::IsNearlyEqual(
                            Actual.Time, Expected.Time, 0.00001f)
                        && FMath::IsNearlyEqual(Actual.Value, Expected.Value,
                            0.000001f)
                        && Actual.InterpMode == Expected.InterpMode
                        && Actual.TangentMode == Expected.TangentMode
                        && Actual.TangentWeightMode == Expected.TangentWeightMode
                        && FMath::IsNearlyEqual(Actual.ArriveTangent,
                            Expected.ArriveTangent, 0.000001f)
                        && FMath::IsNearlyEqual(Actual.LeaveTangent,
                            Expected.LeaveTangent, 0.000001f)
                        && FMath::IsNearlyEqual(Actual.ArriveTangentWeight,
                            Expected.ArriveTangentWeight, 0.000001f)
                        && FMath::IsNearlyEqual(Actual.LeaveTangentWeight,
                            Expected.LeaveTangentWeight, 0.000001f);
                    if (!bSame)
                    {
                        bKeysSemanticallyEqual = false;
                        FirstMismatchedKey = KeyIndex;
                        break;
                    }
                }
            }
        }
        if (!ActualCurve
            || ActualCurve->GetCurveTypeFlags()
                != ExpectedCurveFlags.FindChecked(Pair.Key)
            || !bKeysSemanticallyEqual)
        {
            const TArray<FRichCurveKey>* ActualKeys = ActualCurve
                ? &ActualCurve->FloatCurve.GetConstRefOfKeys() : nullptr;
            const FRichCurveKey* ActualKey = ActualKeys
                && ActualKeys->IsValidIndex(FirstMismatchedKey)
                ? &(*ActualKeys)[FirstMismatchedKey] : nullptr;
            const FRichCurveKey* ExpectedKey =
                Pair.Value.IsValidIndex(FirstMismatchedKey)
                ? &Pair.Value[FirstMismatchedKey] : nullptr;
            Error = FString::Printf(
                TEXT("CLEANED exact float-curve crop differs for %s (actual_count=%d,expected_count=%d,first_mismatch=%d,actual_time=%g,expected_time=%g,actual_value=%g,expected_value=%g,actual_interp=%d,expected_interp=%d,actual_tangent=%d,expected_tangent=%d,actual_arrive=%g,expected_arrive=%g,actual_leave=%g,expected_leave=%g)."),
                *Pair.Key.ToString(), ActualKeys ? ActualKeys->Num() : -1,
                Pair.Value.Num(), FirstMismatchedKey,
                ActualKey ? ActualKey->Time : -1.0,
                ExpectedKey ? ExpectedKey->Time : -1.0,
                ActualKey ? ActualKey->Value : -1.0,
                ExpectedKey ? ExpectedKey->Value : -1.0,
                ActualKey ? static_cast<int32>(ActualKey->InterpMode) : -1,
                ExpectedKey ? static_cast<int32>(ExpectedKey->InterpMode) : -1,
                ActualKey ? static_cast<int32>(ActualKey->TangentMode) : -1,
                ExpectedKey ? static_cast<int32>(ExpectedKey->TangentMode) : -1,
                ActualKey ? ActualKey->ArriveTangent : -1.0,
                ExpectedKey ? ExpectedKey->ArriveTangent : -1.0,
                ActualKey ? ActualKey->LeaveTangent : -1.0,
                ExpectedKey ? ExpectedKey->LeaveTangent : -1.0);
            return false;
        }
    }
    const uint64 OutputCurveDigest = ComputeFloatCurveSemanticDigest(OutputModel);
    if (OutputCurveDigest != FrozenCleanedFloatCurveDigest)
    {
        Error = FString::Printf(
            TEXT("CLEANED deterministic full float-curve semantic digest differs (actual=%016llX,expected=%016llX)."),
            OutputCurveDigest, FrozenCleanedFloatCurveDigest);
        return false;
    }
    const uint64 OutputTrackDigest = ComputeRawTrackDigest(OutputTracks, RefSkeleton);
    if (OutputTrackDigest != FrozenCleanedTrackDigest)
    {
        Error = FString::Printf(
            TEXT("CLEANED deterministic full-track digest differs (actual=%016llX,expected=%016llX,ideal_unsaved_crop=%016llX)."),
            OutputTrackDigest, FrozenCleanedTrackDigest,
            ComputeRawTrackDigest(ExpectedTracks, RefSkeleton));
        return false;
    }
    return true;
}

TSet<FName> SnapshotTargetFolder()
{
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
        .Get().GetAssetsByPath(FName(OutputFolder), Assets, true, false);
    TSet<FName> Result;
    for (const FAssetData& Asset : Assets) Result.Add(Asset.PackageName);
    return Result;
}

bool RollBackNewFolderAssets(const TSet<FName>& Before,
    const TArray<FAssetData>& Returned, FString& Error)
{
    TSet<FName> OwnedPackages;
    for (const FAssetData& Data : Returned)
        if (Data.PackageName.ToString().StartsWith(OutputFolder)
            && !Before.Contains(Data.PackageName))
            OwnedPackages.Add(Data.PackageName);
    const TSet<FName> After = SnapshotTargetFolder();
    for (const FName Package : After)
        if (!Before.Contains(Package))
            if (!OwnedPackages.Contains(Package))
            {
                Error += FString::Printf(TEXT(" Unexplained concurrent target-folder delta %s; refusing deletion."),
                    *Package.ToString());
                return false;
            }
    TArray<UObject*> NewObjects;
    for (const FName Package : OwnedPackages)
        if (UObject* Asset = LoadObject<UObject>(nullptr,
            *(Package.ToString() + TEXT(".") + FPackageName::GetShortName(Package))))
            NewObjects.AddUnique(Asset);
    if (!NewObjects.IsEmpty()) ObjectTools::DeleteObjectsUnchecked(NewObjects);
    const TSet<FName> Final = SnapshotTargetFolder();
    for (const FName Package : OwnedPackages)
        if (Final.Contains(Package))
        { Error += TEXT(" Transaction-owned target remains after rollback."); return false; }
    return true;
}

bool Save(UObject* Asset, FString& Error)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    if (!Package) { Error = TEXT("RAW output package is unavailable."); return false; }
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    Args.Error = GError;
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
    { Error = TEXT("Failed to save exact RAW output package."); return false; }
    return true;
}

}

FString UDiscGolfAuthenticV008RetargetUtility::ValidateImg2396RawRetarget()
{
    UAnimSequence* Source = nullptr; USkeletalMesh* SourceMesh = nullptr;
    UIKRetargeter* Retargeter = nullptr; USkeletalMesh* TargetMesh = nullptr;
    FString Error;
    if (!LoadContract(Source, SourceMesh, Retargeter, TargetMesh, Error))
        return Payload(TEXT("BLOCKED_INPUT_CONTRACT"), Error);
    UAnimSequence* Output = LoadObject<UAnimSequence>(nullptr, OutputPath);
    if (!ValidateOutput(Output, Source, TargetMesh, Error))
        return Payload(TEXT("BLOCKED_OUTPUT_CONTRACT"), Error);
    return Payload(TEXT("PASS_RAW_RETARGET_VALID"));
}

FString UDiscGolfAuthenticV008RetargetUtility::AuthorImg2396RawRetarget()
{
    if (FPlatformMisc::GetEnvironmentVariable(AuthorFlag) != TEXT("1"))
        return Payload(TEXT("BLOCKED_EXPLICIT_AUTHORING_FLAG_REQUIRED"), AuthorFlag);
    FString ExistingFilename;
    if (LoadObject<UObject>(nullptr, OutputPath)
        || FPackageName::DoesPackageExist(OutputPackage, &ExistingFilename))
        return Payload(TEXT("BLOCKED_APPEND_ONLY_TARGET_EXISTS"));
    UAnimSequence* Source = nullptr; USkeletalMesh* SourceMesh = nullptr;
    UIKRetargeter* Retargeter = nullptr; USkeletalMesh* TargetMesh = nullptr;
    FString Error;
    if (!LoadContract(Source, SourceMesh, Retargeter, TargetMesh, Error))
        return Payload(TEXT("BLOCKED_INPUT_CONTRACT"), Error);

    FIKRetargetBatchOperationInputs Inputs;
    Inputs.AssetsToRetarget.Add(FAssetData(Source));
    Inputs.SourceMesh = SourceMesh;
    Inputs.TargetMesh = TargetMesh;
    Inputs.IKRetargetAsset = Retargeter;
    Inputs.Search = FPackageName::ObjectPathToObjectName(FString(SourceSequencePath));
    Inputs.Replace = OutputName;
    Inputs.TargetPath = FPackageName::GetLongPackagePath(OutputPackage);
    Inputs.bUseSourcePath = false;
    Inputs.bIncludeReferencedAssets = false;
    Inputs.bOverwriteExistingFiles = false;
    Inputs.bRetainAdditiveFlags = true;
    const TSet<FName> FolderBefore = SnapshotTargetFolder();
    const TArray<FAssetData> Outputs = UIKRetargetBatchOperation::RunBatchRetarget(Inputs);
    UAnimSequence* Output = nullptr;
    for (const FAssetData& Data : Outputs)
        if (UAnimSequence* Candidate = Cast<UAnimSequence>(Data.GetAsset());
            Candidate && Candidate->GetPathName() == OutputPath) Output = Candidate;
    if (!Output) Output = LoadObject<UAnimSequence>(nullptr, OutputPath);
    const TSet<FName> FolderAfter = SnapshotTargetFolder();
    const bool bExactFolderDelta = FolderAfter.Num() == FolderBefore.Num() + 1
        && FolderAfter.Contains(FName(OutputPackage));
    if (!Output || Outputs.Num() != 1 || !bExactFolderDelta)
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Outputs, Error);
        return Payload(bRolledBack ? TEXT("FAILED_BATCH_RETARGET_ROLLED_BACK_ALL_NEW_TARGETS")
            : TEXT("FAILED_BATCH_RETARGET_ROLLBACK_BLOCKED"), Error);
    }

    Output->Modify();
    Output->SetPreviewMesh(TargetMesh);
    Output->bLoop = false;
    Output->RateScale = 1.0f;
    Output->Notifies.Reset();
    Output->AnimNotifyTracks.Reset();
    Output->InitializeNotifyTrack();
    FMetaData& Metadata = Output->GetOutermost()->GetMetaData();
    Metadata.SetValue(Output, TEXT("DG_Classification"), TEXT("AUTHENTIC_DIAGNOSTIC_RAW_RETARGET"));
    Metadata.SetValue(Output, TEXT("DG_Predecessor"), SourceSequencePath);
    Metadata.SetValue(Output, TEXT("DG_ProductionPromotionAllowed"), TEXT("false"));
    Output->RefreshCacheData();
    Output->PostEditChange();
    if (!ValidateOutput(Output, Source, TargetMesh, Error) || !Save(Output, Error))
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Outputs, Error);
        return Payload(bRolledBack
            ? TEXT("FAILED_VALIDATION_ROLLED_BACK_NEW_EXACT_TARGET")
            : TEXT("FAILED_VALIDATION_ROLLBACK_BLOCKED"), Error);
    }
    return Payload(TEXT("PASS_AUTHORED_EXACTLY_ONE_RAW_RETARGET"));
}

FString UDiscGolfAuthenticV008RetargetUtility::AuthorImg2396NormalizedRetarget()
{
    if (FPlatformMisc::GetEnvironmentVariable(NormalizedAuthorFlag) != TEXT("1"))
        return NormalizedPayload(TEXT("BLOCKED_EXPLICIT_AUTHORING_FLAG_REQUIRED"),
            NormalizedAuthorFlag);
    FString ExistingFilename;
    if (FindPackage(nullptr, NormalizedOutputPackage)
        || LoadObject<UObject>(nullptr, NormalizedOutputPath)
        || FPackageName::DoesPackageExist(NormalizedOutputPackage, &ExistingFilename))
        return NormalizedPayload(TEXT("BLOCKED_APPEND_ONLY_TARGET_EXISTS"));
    UAnimSequence* Source = nullptr; USkeletalMesh* SourceMesh = nullptr;
    UIKRetargeter* Retargeter = nullptr; USkeletalMesh* TargetMesh = nullptr;
    FString Error;
    if (!LoadContract(Source, SourceMesh, Retargeter, TargetMesh, Error))
        return NormalizedPayload(TEXT("BLOCKED_INPUT_CONTRACT"), Error);
    UAnimSequence* Raw = LoadObject<UAnimSequence>(nullptr, OutputPath);
    if (!ValidateRejectedRawPredecessor(Raw, Source, TargetMesh, Error))
        return NormalizedPayload(TEXT("BLOCKED_REJECTED_RAW_CONTRACT"), Error);
    TMap<FName, TArray<FTransform>> CorrectedTracks;
    if (!BuildNormalizedTracks(Raw, TargetMesh, CorrectedTracks, Error))
        return NormalizedPayload(TEXT("BLOCKED_NORMALIZATION_PREFLIGHT"), Error);

    const TSet<FName> FolderBefore = SnapshotTargetFolder();
    UPackage* Package = CreatePackage(NormalizedOutputPackage);
    UAnimSequence* Output = Cast<UAnimSequence>(StaticDuplicateObject(
        Raw, Package, NormalizedOutputName, RF_Public | RF_Standalone));
    TArray<FAssetData> Created;
    if (Output)
    {
        FAssetRegistryModule::AssetCreated(Output);
        Created.Add(FAssetData(Output));
    }
    const TSet<FName> FolderAfter = SnapshotTargetFolder();
    if (!Output || FolderAfter.Num() != FolderBefore.Num() + 1
        || !FolderAfter.Contains(FName(NormalizedOutputPackage)))
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Created, Error);
        return NormalizedPayload(bRolledBack
            ? TEXT("FAILED_DUPLICATE_ROLLED_BACK_ALL_NEW_TARGETS")
            : TEXT("FAILED_DUPLICATE_ROLLBACK_BLOCKED"), Error);
    }
    Output->Modify();
    if (!WriteTracks(Output, CorrectedTracks, Error))
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Created, Error);
        return NormalizedPayload(bRolledBack
            ? TEXT("FAILED_TRACK_WRITE_ROLLED_BACK") : TEXT("FAILED_TRACK_WRITE_ROLLBACK_BLOCKED"), Error);
    }
    Output->SetPreviewMesh(TargetMesh);
    Output->bLoop = false;
    Output->bEnableRootMotion = false;
    Output->RateScale = 1.0f;
    Output->Notifies.Reset();
    Output->AnimNotifyTracks.Reset();
    Output->InitializeNotifyTrack();
    FMetaData& Metadata = Output->GetOutermost()->GetMetaData();
    Metadata.SetValue(Output, TEXT("DG_Classification"),
        TEXT("AUTHENTIC_DIAGNOSTIC_NORMALIZED_RETARGET"));
    Metadata.SetValue(Output, TEXT("DG_Predecessor"), OutputPath);
    Metadata.SetValue(Output, TEXT("DG_AxisPolicy"), TEXT("Q_Y_POS_90_PREMULTIPLY"));
    Metadata.SetValue(Output, TEXT("DG_RootTranslationPolicy"),
        TEXT("SINGLE_AUTHORITY_RAW_ROOT_AS_SOURCE_PELVIS"));
    Metadata.SetValue(Output, TEXT("DG_ProductionPromotionAllowed"), TEXT("false"));
    Output->RefreshCacheData();
    Output->PostEditChange();
    if (!ValidateNormalizedOutput(Output, Raw, TargetMesh, Error) || !Save(Output, Error))
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Created, Error);
        return NormalizedPayload(bRolledBack
            ? TEXT("FAILED_VALIDATION_ROLLED_BACK_NEW_NORMALIZED_TARGET")
            : TEXT("FAILED_VALIDATION_ROLLBACK_BLOCKED"), Error);
    }
    return NormalizedPayload(TEXT("PASS_AUTHORED_EXACTLY_ONE_NORMALIZED_RETARGET"));
}

FString UDiscGolfAuthenticV008RetargetUtility::ValidateImg2396NormalizedRetarget()
{
    UAnimSequence* Source = nullptr; USkeletalMesh* SourceMesh = nullptr;
    UIKRetargeter* Retargeter = nullptr; USkeletalMesh* TargetMesh = nullptr;
    FString Error;
    if (!LoadContract(Source, SourceMesh, Retargeter, TargetMesh, Error))
        return NormalizedPayload(TEXT("BLOCKED_INPUT_CONTRACT"), Error);
    UAnimSequence* Raw = LoadObject<UAnimSequence>(nullptr, OutputPath);
    UAnimSequence* Output = LoadObject<UAnimSequence>(nullptr, NormalizedOutputPath);
    if (!Raw || !Output)
        return NormalizedPayload(TEXT("BLOCKED_OUTPUT_MISSING"));
    if (!ValidateRejectedRawPredecessor(Raw, Source, TargetMesh, Error))
        return NormalizedPayload(TEXT("BLOCKED_REJECTED_RAW_CONTRACT"), Error);
    if (!ValidateNormalizedOutput(Output, Raw, TargetMesh, Error))
        return NormalizedPayload(TEXT("BLOCKED_OUTPUT_CONTRACT"), Error);
    return NormalizedPayload(TEXT("PASS_NORMALIZED_RETARGET_VALID"));
}

FString UDiscGolfAuthenticV008RetargetUtility::AuthorImg2396CleanedRetarget()
{
    if (FPlatformMisc::GetEnvironmentVariable(CleanedAuthorFlag) != TEXT("1"))
        return CleanedPayload(TEXT("BLOCKED_EXPLICIT_AUTHORING_FLAG_REQUIRED"),
            CleanedAuthorFlag);
    FString ExistingFilename;
    if (FindPackage(nullptr, CleanedOutputPackage)
        || LoadObject<UObject>(nullptr, CleanedOutputPath)
        || FPackageName::DoesPackageExist(CleanedOutputPackage, &ExistingFilename))
        return CleanedPayload(TEXT("BLOCKED_APPEND_ONLY_TARGET_EXISTS"));

    UAnimSequence* Source = nullptr;
    USkeletalMesh* SourceMesh = nullptr;
    UIKRetargeter* Retargeter = nullptr;
    USkeletalMesh* TargetMesh = nullptr;
    FString Error;
    if (!LoadContract(Source, SourceMesh, Retargeter, TargetMesh, Error))
        return CleanedPayload(TEXT("BLOCKED_INPUT_CONTRACT"), Error);
    UAnimSequence* Raw = LoadObject<UAnimSequence>(nullptr, OutputPath);
    UAnimSequence* Normalized = LoadObject<UAnimSequence>(nullptr, NormalizedOutputPath);
    if (!Raw || !Normalized)
        return CleanedPayload(TEXT("BLOCKED_PREDECESSOR_MISSING"));
    if (!ValidateRejectedRawPredecessor(Raw, Source, TargetMesh, Error))
        return CleanedPayload(TEXT("BLOCKED_REJECTED_RAW_CONTRACT"), Error);
    if (!ValidateFrozenNormalizedPredecessor(Normalized, Raw, TargetMesh, Error))
        return CleanedPayload(TEXT("BLOCKED_NORMALIZED_PREDECESSOR_CONTRACT"), Error);

    TMap<FName, TArray<FTransform>> CroppedTracks;
    TMap<FName, TArray<FTransform>> EvaluatedCropTracks;
    if (!BuildCleanedTracks(Normalized, TargetMesh, CroppedTracks, Error)
        || !BuildEvaluatedTracks60Hz(Normalized, TargetMesh,
            CleanedEvaluatedSourceStartInclusive, EvaluatedCropTracks, Error)
        || !ValidateNoLongFullPoseHold(EvaluatedCropTracks,
            TargetMesh->GetRefSkeleton(), Error))
        return CleanedPayload(TEXT("BLOCKED_CROP_PREFLIGHT"), Error);

    const TSet<FName> FolderBefore = SnapshotTargetFolder();
    UPackage* Package = CreatePackage(CleanedOutputPackage);
    UAnimSequence* Output = Cast<UAnimSequence>(StaticDuplicateObject(
        Normalized, Package, CleanedOutputName, RF_Public | RF_Standalone));
    TArray<FAssetData> Created;
    if (Output)
    {
        FAssetRegistryModule::AssetCreated(Output);
        Created.Add(FAssetData(Output));
    }
    const TSet<FName> FolderAfter = SnapshotTargetFolder();
    if (!Output || FolderAfter.Num() != FolderBefore.Num() + 1
        || !FolderAfter.Contains(FName(CleanedOutputPackage)))
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Created, Error);
        return CleanedPayload(bRolledBack
            ? TEXT("FAILED_DUPLICATE_ROLLED_BACK_ALL_NEW_TARGETS")
            : TEXT("FAILED_DUPLICATE_ROLLBACK_BLOCKED"), Error);
    }

    Output->Modify();
    const IAnimationDataModel* NormalizedModel =
        Normalized->GetDataModelInterface().GetInterface();
    if (!NormalizedModel
        || !WriteCleanedTracks(Output, CroppedTracks, TargetMesh->GetRefSkeleton(),
            NormalizedModel, Error))
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Created, Error);
        return CleanedPayload(bRolledBack
            ? TEXT("FAILED_TRACK_WRITE_ROLLED_BACK")
            : TEXT("FAILED_TRACK_WRITE_ROLLBACK_BLOCKED"), Error);
    }
    Output->SetPreviewMesh(TargetMesh);
    Output->bLoop = false;
    Output->bEnableRootMotion = false;
    Output->RateScale = 1.0f;
    Output->Notifies.Reset();
    Output->AnimNotifyTracks.Reset();
    Output->InitializeNotifyTrack();
    FMetaData& Metadata = Output->GetOutermost()->GetMetaData();
    Metadata.SetValue(Output, TEXT("DG_Classification"),
        TEXT("AUTHENTIC_DIAGNOSTIC_CLEANED_CROP"));
    Metadata.SetValue(Output, TEXT("DG_Predecessor"), NormalizedOutputPath);
    Metadata.SetValue(Output, TEXT("DG_CropPolicy"),
        TEXT("NATIVE_KEYS_[97,205)_NO_RETIME_NO_SMOOTHING_NO_NEW_INTERPOLATION"));
    Metadata.SetValue(Output, TEXT("DG_NativeSourceKeyMap"),
        TEXT("OUTPUT_KEY_N_EQUALS_NORMALIZED_KEY_97_PLUS_N"));
    Metadata.SetValue(Output, TEXT("DG_NativeSampleRateHz"), TEXT("15"));
    Metadata.SetValue(Output, TEXT("DG_NativeSourceStartInclusive"), TEXT("97"));
    Metadata.SetValue(Output, TEXT("DG_NativeSourceEndExclusive"), TEXT("205"));
    Metadata.SetValue(Output, TEXT("DG_NativeOutputSampleCount"), TEXT("108"));
    Metadata.SetValue(Output, TEXT("DG_NativeOutputIntervalCount"), TEXT("107"));
    Metadata.SetValue(Output, TEXT("DG_EvaluatedSourceFrameMap"),
        TEXT("OUTPUT_FRAME_N_EQUALS_NORMALIZED_FRAME_388_PLUS_N"));
    Metadata.SetValue(Output, TEXT("DG_EvaluatedSampleRateHz"), TEXT("60"));
    Metadata.SetValue(Output, TEXT("DG_EvaluatedSourceStartInclusive"), TEXT("388"));
    Metadata.SetValue(Output, TEXT("DG_EvaluatedSourceEndExclusive"), TEXT("817"));
    Metadata.SetValue(Output, TEXT("DG_EvaluatedOutputSampleCount"), TEXT("429"));
    Metadata.SetValue(Output, TEXT("DG_EvaluatedOutputIntervalCount"), TEXT("428"));
    Metadata.SetValue(Output, TEXT("DG_CurveCropPolicy"),
        TEXT("BODYFRAMESOLVED_KEYS_97_TO_204_SHIFTED;CONSTANT_CURVES_REBASED_AT_ZERO"));
    Metadata.SetValue(Output, TEXT("DG_EvaluatedPhaseFrameMap"),
        TEXT("address=24;runup=60;reachback=120;plant=180;release=182;followthrough=216;recovery=268"));
    Metadata.SetValue(Output, TEXT("DG_AxisPolicy"), TEXT("Q_Y_POS_90_PREMULTIPLY"));
    Metadata.SetValue(Output, TEXT("DG_RootTranslationPolicy"),
        TEXT("SINGLE_AUTHORITY_RAW_ROOT_AS_SOURCE_PELVIS"));
    Metadata.SetValue(Output, TEXT("DG_ProductionPromotionAllowed"), TEXT("false"));
    Output->RefreshCacheData();
    Output->PostEditChange();
    if (!ValidateCleanedOutput(Output, Normalized, TargetMesh, Error)
        || !Save(Output, Error))
    {
        const bool bRolledBack = RollBackNewFolderAssets(FolderBefore, Created, Error);
        return CleanedPayload(bRolledBack
            ? TEXT("FAILED_VALIDATION_ROLLED_BACK_NEW_CLEANED_TARGET")
            : TEXT("FAILED_VALIDATION_ROLLBACK_BLOCKED"), Error);
    }
    return CleanedPayload(TEXT("PASS_AUTHORED_EXACTLY_ONE_CLEANED_RETARGET"));
}

FString UDiscGolfAuthenticV008RetargetUtility::ValidateImg2396CleanedRetarget()
{
    UAnimSequence* Source = nullptr;
    USkeletalMesh* SourceMesh = nullptr;
    UIKRetargeter* Retargeter = nullptr;
    USkeletalMesh* TargetMesh = nullptr;
    FString Error;
    if (!LoadContract(Source, SourceMesh, Retargeter, TargetMesh, Error))
        return CleanedPayload(TEXT("BLOCKED_INPUT_CONTRACT"), Error);
    UAnimSequence* Raw = LoadObject<UAnimSequence>(nullptr, OutputPath);
    UAnimSequence* Normalized = LoadObject<UAnimSequence>(nullptr, NormalizedOutputPath);
    UAnimSequence* Output = LoadObject<UAnimSequence>(nullptr, CleanedOutputPath);
    if (!Raw || !Normalized || !Output)
        return CleanedPayload(TEXT("BLOCKED_OUTPUT_OR_PREDECESSOR_MISSING"));
    if (!ValidateRejectedRawPredecessor(Raw, Source, TargetMesh, Error))
        return CleanedPayload(TEXT("BLOCKED_REJECTED_RAW_CONTRACT"), Error);
    if (!ValidateFrozenNormalizedPredecessor(Normalized, Raw, TargetMesh, Error))
        return CleanedPayload(TEXT("BLOCKED_NORMALIZED_PREDECESSOR_CONTRACT"), Error);
    if (!ValidateCleanedOutput(Output, Normalized, TargetMesh, Error))
        return CleanedPayload(TEXT("BLOCKED_OUTPUT_CONTRACT"), Error);
    return CleanedPayload(TEXT("PASS_CLEANED_RETARGET_VALID"));
}
