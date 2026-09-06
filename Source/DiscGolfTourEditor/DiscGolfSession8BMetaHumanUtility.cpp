#include "DiscGolfSession8BMetaHumanUtility.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/AssetManager.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/DataAssetFactory.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "MetaHumanComponentUE.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "RetargetEditor/IKRetargetFactory.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargetOps.h"
#include "Retargeter/IKRetargetProfile.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/CurveRemapOp.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rig/IKRigDefinition.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

#include "DiscGolfAvatarBackendProfile.h"
#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfMetaHumanVisualContract.h"

namespace DiscGolfSession8BMetaHuman
{
constexpr int32 SchemaVersion = 2;

const TCHAR* GeneratedBlueprintPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/BP_MHC_DG_Golfer_Default.BP_MHC_DG_Golfer_Default");
const TCHAR* WrapperBlueprintPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.BP_DG_MetaHuman_Default");
const TCHAR* RetargeterPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.RTG_DGMaster_To_MetaHuman");
const TCHAR* BackendProfilePath =
    TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default.DA_DG_AvatarBackend_MetaHuman_Default");
const TCHAR* SourceMeshPath =
    TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master");
const TCHAR* SourceIKRigPath =
    TEXT("/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master");
const TCHAR* TargetIKRigPath =
    TEXT("/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig");
const TCHAR* SourceMHCPackageName =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default");
const TCHAR* ExpectedHairGroomPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/Grooms/Hair_S_Clean.Hair_S_Clean");
const TCHAR* ExpectedOutfitMeshPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/Clothing/MHC_DG_Golfer_Default_Outfits.MHC_DG_Golfer_Default_Outfits");

const FName BodyVariableName(TEXT("Body"));
const FName FaceVariableName(TEXT("Face"));
const FName BodyTag(TEXT("DGVisualBody"));
const FName HeadTag(TEXT("DGVisualHead"));
const FName TargetRetargetPoseName(TEXT("DGMasterAligned"));
const FName ExpectedPrimaryAssetType(TEXT("DiscGolfAvatarBackendProfile"));
const FName CharacterInstancePropertyName(TEXT("CharacterInstance"));
const FName ExpectedQualityProfileId(TEXT("GameplayPerformance"));

const TCHAR* ConfigureFunctionName = TEXT("ConfigureFromDGAnimationSource");
const TCHAR* ApplyFunctionName = TEXT("ApplyMappedCustomization");
const TCHAR* ConfigureStatus =
    TEXT("Fixed assembled MetaHuman preset accepted; the project adapter owns final tagged-mesh and retarget verification.");
const TCHAR* ApplyStatus =
    TEXT("Fixed assembled MetaHuman preset retained; unsupported proxy-only controls are intentionally not mapped.");
const TCHAR* FixedPresetSemantics =
    TEXT("CURATED_FIXED_PRESET;CONFIGURE_ACCEPTS_REQUEST;APPLY_RETAINS_PRESET;ADAPTER_PROVES_RUNTIME_READY");
const TCHAR* LegacyOrientationPolicy =
    TEXT("DG_SOURCE_MINUS_90_YAW_COMPENSATED_BY_WRAPPER_PLUS_90_YAW_V1");
const TCHAR* OrientationPolicy =
    TEXT("DG_SOURCE_AUTHORITY_WRAPPER_IDENTITY_RELATIVE_TRANSFORM_V2");
const TCHAR* OrientationCanonicalSemantics =
    TEXT("WRAPPER_COLOCATED_WITH_DG_ANIMATION_SOURCE;NO_RELATIVE_YAW;SOURCE_DISC_GRIP_REMAINS_AUTHORITATIVE");
const TCHAR* RetargetPolicyRunIKAndRootMotionEnabled =
    TEXT("LEGACY_ALL_DEFAULT_OPS_ENABLED");
const TCHAR* RetargetPolicyRunIKDisabledRootMotionEnabled =
    TEXT("PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED");
const TCHAR* RetargetPolicyRunIKAndRootMotionDisabled =
    TEXT("PELVIS_FK_CURVES_ENABLED_RUN_IK_AND_ROOT_MOTION_DISABLED");

constexpr double RetargetPelvisExpectedZCm = 93.164;
constexpr double RetargetPelvisExpectedZToleranceCm = 0.25;
constexpr double RetargetPelvisHeightToleranceCm = 0.10;
constexpr double RetargetPelvisMotionToleranceCm = 0.10;

const FTransform LegacyWrapperRelativeTransform(
    FRotator(0.0, 90.0, 0.0), FVector::ZeroVector, FVector::OneVector);
const FTransform WrapperRelativeTransform(
    FRotator::ZeroRotator, FVector::ZeroVector, FVector::OneVector);

struct FChainMapping
{
    const TCHAR* Target;
    const TCHAR* Source;
};

const FChainMapping ChainMappings[] = {
    {TEXT("Root"), TEXT("Root")},
    {TEXT("Spine"), TEXT("Spine")},
    {TEXT("Neck"), TEXT("Neck")},
    {TEXT("Head"), TEXT("Neck")},
    {TEXT("LeftArm"), TEXT("Arm_L")},
    {TEXT("RightArm"), TEXT("Arm_R")},
    {TEXT("LeftLeg"), TEXT("Leg_L")},
    {TEXT("RightLeg"), TEXT("Leg_R")},
};

struct FSourceChainContract
{
    const TCHAR* Chain;
    const TCHAR* StartBone;
    const TCHAR* EndBone;
};

const FSourceChainContract SourceChainContracts[] = {
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

struct FFileSnapshot
{
    FString Filename;
    TArray<uint8> Bytes;
};

struct FPackageSnapshot
{
    FString ObjectPath;
    FString PackageName;
    FString BaseFilename;
    TArray<FFileSnapshot> Files;
};

struct FNodeReference
{
    TObjectPtr<UBlueprint> OwnerBlueprint = nullptr;
    TObjectPtr<USCS_Node> Node = nullptr;
};

struct FAssetContext
{
    TObjectPtr<UBlueprint> GeneratedBlueprint = nullptr;
    TObjectPtr<UBlueprint> WrapperBlueprint = nullptr;
    TObjectPtr<UIKRetargeter> Retargeter = nullptr;
    TObjectPtr<UDiscGolfAvatarBackendProfile> BackendProfile = nullptr;
    TObjectPtr<USkeletalMesh> SourceMesh = nullptr;
    TObjectPtr<USkeletalMesh> TargetMesh = nullptr;
    TObjectPtr<UIKRigDefinition> SourceIKRig = nullptr;
    TObjectPtr<UIKRigDefinition> TargetIKRig = nullptr;
};

TSharedRef<FJsonObject> TransformToJson(const FTransform& Transform)
{
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    const FVector Translation = Transform.GetTranslation();
    const FRotator Rotation = Transform.Rotator();
    const FVector Scale = Transform.GetScale3D();
    Result->SetNumberField(TEXT("translation_x_cm"), Translation.X);
    Result->SetNumberField(TEXT("translation_y_cm"), Translation.Y);
    Result->SetNumberField(TEXT("translation_z_cm"), Translation.Z);
    Result->SetNumberField(TEXT("rotation_pitch_degrees"), Rotation.Pitch);
    Result->SetNumberField(TEXT("rotation_yaw_degrees"), Rotation.Yaw);
    Result->SetNumberField(TEXT("rotation_roll_degrees"), Rotation.Roll);
    Result->SetNumberField(TEXT("scale_x"), Scale.X);
    Result->SetNumberField(TEXT("scale_y"), Scale.Y);
    Result->SetNumberField(TEXT("scale_z"), Scale.Z);
    return Result;
}

TSharedRef<FJsonObject> ExactTransformToJson(const FTransform& Transform)
{
    TSharedRef<FJsonObject> Result = TransformToJson(Transform);
    const FQuat Rotation = Transform.GetRotation();
    Result->SetNumberField(TEXT("rotation_quaternion_x"), Rotation.X);
    Result->SetNumberField(TEXT("rotation_quaternion_y"), Rotation.Y);
    Result->SetNumberField(TEXT("rotation_quaternion_z"), Rotation.Z);
    Result->SetNumberField(TEXT("rotation_quaternion_w"), Rotation.W);
    return Result;
}

struct FRunReport
{
    explicit FRunReport(const TCHAR* InOperation)
        : Root(MakeShared<FJsonObject>())
    {
        Root->SetNumberField(TEXT("schema_version"), SchemaVersion);
        Root->SetStringField(TEXT("session"), TEXT("8B"));
        Root->SetStringField(TEXT("operation"), InOperation);
        Root->SetStringField(TEXT("status"), TEXT("FAIL"));
        Root->SetStringField(TEXT("disk_mutation"), TEXT("NONE"));
        Root->SetStringField(TEXT("generated_blueprint"), GeneratedBlueprintPath);
        Root->SetStringField(TEXT("wrapper_blueprint"), WrapperBlueprintPath);
        Root->SetStringField(TEXT("retargeter"), RetargeterPath);
        Root->SetStringField(TEXT("backend_profile"), BackendProfilePath);
        Root->SetStringField(TEXT("source_mesh"), SourceMeshPath);
        Root->SetStringField(TEXT("source_ik_rig"), SourceIKRigPath);
        Root->SetStringField(TEXT("target_ik_rig"), TargetIKRigPath);
        Root->SetBoolField(TEXT("fixed_preset_semantics"), true);
        Root->SetStringField(TEXT("fixed_preset_policy"), FixedPresetSemantics);
        Root->SetStringField(TEXT("orientation_policy"), OrientationPolicy);
        Root->SetStringField(
            TEXT("orientation_canonical_semantics"),
            OrientationCanonicalSemantics);
        Root->SetBoolField(TEXT("orientation_requires_runtime_visual_validation"), true);
        Root->SetBoolField(TEXT("wrapper_attachment_relative_transform_is_identity"), true);
        Root->SetObjectField(
            TEXT("wrapper_attachment_relative_transform"),
            TransformToJson(WrapperRelativeTransform));
        Root->SetBoolField(TEXT("rollback_verified"), false);
        Root->SetBoolField(TEXT("rollback_performed"), false);
        Root->SetBoolField(TEXT("reload_from_disk_verified"), false);
        Root->SetBoolField(TEXT("clean_shaven"), false);
        Root->SetNumberField(TEXT("beard_groom_asset_count"), -1);
        Root->SetNumberField(TEXT("mustache_groom_asset_count"), -1);
        Root->SetBoolField(TEXT("hair_s_clean_present"), false);
        Root->SetBoolField(TEXT("outfit_present"), false);
        Root->SetBoolField(TEXT("source_mhc_editor_only"), false);

        TSharedRef<FJsonObject> Translation = MakeShared<FJsonObject>();
        Translation->SetNumberField(TEXT("x"), 0.0);
        Translation->SetNumberField(TEXT("y"), 0.0);
        Translation->SetNumberField(TEXT("z"), 0.0);
        Root->SetObjectField(TEXT("wrapper_relative_translation_cm"), Translation);

        TSharedRef<FJsonObject> Rotation = MakeShared<FJsonObject>();
        Rotation->SetNumberField(TEXT("pitch"), 0.0);
        Rotation->SetNumberField(TEXT("yaw"), 0.0);
        Rotation->SetNumberField(TEXT("roll"), 0.0);
        Root->SetObjectField(TEXT("wrapper_relative_rotation_degrees"), Rotation);

        TSharedRef<FJsonObject> Scale = MakeShared<FJsonObject>();
        Scale->SetNumberField(TEXT("x"), 1.0);
        Scale->SetNumberField(TEXT("y"), 1.0);
        Scale->SetNumberField(TEXT("z"), 1.0);
        Root->SetObjectField(TEXT("wrapper_relative_scale"), Scale);

        TSharedRef<FJsonObject> Mapping = MakeShared<FJsonObject>();
        for (const FChainMapping& Pair : ChainMappings)
        {
            Mapping->SetStringField(Pair.Target, Pair.Source);
        }
        Root->SetObjectField(TEXT("chain_mapping"), Mapping);
        SyncArrays();
    }

    void Fail(const FString& Error)
    {
        if (!Error.IsEmpty())
        {
            Errors.Add(Error);
            Root->SetStringField(TEXT("summary"), Error);
        }
        SyncArrays();
    }

    void Pass(const FString& Summary, const FString& DiskMutation)
    {
        Root->SetStringField(TEXT("status"), TEXT("PASS"));
        Root->SetStringField(TEXT("summary"), Summary);
        Root->SetStringField(TEXT("disk_mutation"), DiskMutation);
        SyncArrays();
    }

    void SyncArrays()
    {
        TArray<TSharedPtr<FJsonValue>> ErrorValues;
        for (const FString& Error : Errors)
        {
            ErrorValues.Add(MakeShared<FJsonValueString>(Error));
        }
        Root->SetArrayField(TEXT("errors"), ErrorValues);

        TArray<TSharedPtr<FJsonValue>> WriteValues;
        for (const FString& Write : Writes)
        {
            WriteValues.Add(MakeShared<FJsonValueString>(Write));
        }
        Root->SetArrayField(TEXT("writes"), WriteValues);

        TArray<TSharedPtr<FJsonValue>> AttemptedValues;
        for (const FString& Write : AttemptedWrites)
        {
            AttemptedValues.Add(MakeShared<FJsonValueString>(Write));
        }
        Root->SetArrayField(TEXT("attempted_writes"), AttemptedValues);
    }

    FString Serialize() const
    {
        FString Result;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
        FJsonSerializer::Serialize(Root, Writer);
        return Result;
    }

    TSharedRef<FJsonObject> Root;
    TArray<FString> Errors;
    TArray<FString> Writes;
    TArray<FString> AttemptedWrites;
};

bool VectorPinValueMatches(const FString& Value, const FVector& Expected)
{
    FVector ParsedVector;
    return FDefaultValueHelper::ParseVector(Value, ParsedVector)
        && ParsedVector.Equals(Expected, UE_KINDA_SMALL_NUMBER);
}

bool RotatorPinValueMatches(const FString& Value, const FRotator& Expected)
{
    FRotator ParsedRotator;
    return FDefaultValueHelper::ParseRotator(Value, ParsedRotator)
        && ParsedRotator.Equals(Expected, UE_KINDA_SMALL_NUMBER);
}

FString VectorPinDefault(const FVector& Value)
{
    return FString::Printf(
        TEXT("%.17g,%.17g,%.17g"), Value.X, Value.Y, Value.Z);
}

FString RotatorPinDefault(const FRotator& Value)
{
    return FString::Printf(
        TEXT("%.17g,%.17g,%.17g"), Value.Pitch, Value.Yaw, Value.Roll);
}

FString ObjectPathToFilename(const TCHAR* ObjectPath)
{
    return FPackageName::LongPackageNameToFilename(
        FPackageName::ObjectPathToPackageName(FString(ObjectPath)),
        FPackageName::GetAssetPackageExtension());
}

bool SnapshotPackage(const TCHAR* ObjectPath, FPackageSnapshot& OutSnapshot, FString& Error)
{
    OutSnapshot.ObjectPath = ObjectPath;
    OutSnapshot.PackageName = FPackageName::ObjectPathToPackageName(OutSnapshot.ObjectPath);
    OutSnapshot.BaseFilename = FPaths::ConvertRelativePathToFull(
        FPackageName::LongPackageNameToFilename(
            OutSnapshot.PackageName, TEXT("")));
    OutSnapshot.Files.Reset();

    TArray<FString> ExistingNames;
    IFileManager::Get().FindFiles(
        ExistingNames, *(OutSnapshot.BaseFilename + TEXT(".*")), true, false);
    ExistingNames.Sort();
    for (const FString& ExistingName : ExistingNames)
    {
        FFileSnapshot& File = OutSnapshot.Files.AddDefaulted_GetRef();
        File.Filename = FPaths::Combine(
            FPaths::GetPath(OutSnapshot.BaseFilename), ExistingName);
        if (!FFileHelper::LoadFileToArray(File.Bytes, *File.Filename))
        {
            Error = FString::Printf(
                TEXT("Could not snapshot canonical package file %s"), *File.Filename);
            return false;
        }
    }
    return true;
}

bool RestorePackageSnapshots(
    const TArray<FPackageSnapshot>& Snapshots,
    FString& Error)
{
    bool bRestored = true;
    TArray<FString> Failures;
    for (const FPackageSnapshot& Snapshot : Snapshots)
    {
        TSet<FString> ExpectedFiles;
        for (const FFileSnapshot& File : Snapshot.Files)
        {
            ExpectedFiles.Add(FPaths::ConvertRelativePathToFull(File.Filename));
        }

        TArray<FString> CurrentNames;
        IFileManager::Get().FindFiles(
            CurrentNames, *(Snapshot.BaseFilename + TEXT(".*")), true, false);
        for (const FString& CurrentName : CurrentNames)
        {
            const FString CurrentFile = FPaths::ConvertRelativePathToFull(
                FPaths::Combine(FPaths::GetPath(Snapshot.BaseFilename), CurrentName));
            if (!ExpectedFiles.Contains(CurrentFile)
                && !IFileManager::Get().Delete(*CurrentFile, false, true, true))
            {
                bRestored = false;
                Failures.Add(FString::Printf(TEXT("delete %s"), *CurrentFile));
            }
        }

        for (const FFileSnapshot& File : Snapshot.Files)
        {
            if (!FFileHelper::SaveArrayToFile(File.Bytes, *File.Filename))
            {
                bRestored = false;
                Failures.Add(FString::Printf(TEXT("restore %s"), *File.Filename));
            }
        }
    }

    for (const FPackageSnapshot& Snapshot : Snapshots)
    {
        TArray<FString> CurrentNames;
        IFileManager::Get().FindFiles(
            CurrentNames, *(Snapshot.BaseFilename + TEXT(".*")), true, false);
        if (CurrentNames.Num() != Snapshot.Files.Num())
        {
            bRestored = false;
            Failures.Add(FString::Printf(
                TEXT("sidecar count mismatch for %s"), *Snapshot.PackageName));
        }
        for (const FFileSnapshot& File : Snapshot.Files)
        {
            TArray<uint8> CurrentBytes;
            if (!FFileHelper::LoadFileToArray(CurrentBytes, *File.Filename)
                || CurrentBytes != File.Bytes)
            {
                bRestored = false;
                Failures.Add(FString::Printf(TEXT("byte mismatch %s"), *File.Filename));
            }
        }
    }

    if (!bRestored)
    {
        Error = FString::Join(Failures, TEXT("; "));
    }
    return bRestored;
}

TSet<FName> CaptureDirtyPackages()
{
    TSet<FName> Result;
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        if (It->IsDirty() && !It->HasAnyFlags(RF_Transient))
        {
            Result.Add(It->GetFName());
        }
    }
    return Result;
}

TSet<FName> CaptureSavableDirtyPackages()
{
    TArray<UPackage*> DirtyPackages;
    FEditorFileUtils::GetDirtyPackages(DirtyPackages);

    TSet<FName> Result;
    for (const UPackage* Package : DirtyPackages)
    {
        if (Package)
        {
            Result.Add(Package->GetFName());
        }
    }
    return Result;
}

bool ValidateNoUnexpectedNewDirtyPackages(
    const TSet<FName>& Before,
    const TSet<FName>& Allowed,
    FString& Error)
{
    TArray<FString> Unexpected;
    for (const FName Current : CaptureDirtyPackages())
    {
        if (!Before.Contains(Current) && !Allowed.Contains(Current))
        {
            Unexpected.Add(Current.ToString());
        }
    }
    Unexpected.Sort();
    if (!Unexpected.IsEmpty())
    {
        Error = FString::Printf(
            TEXT("Session 8B authoring dirtied packages outside the exact three-package scope: %s"),
            *FString::Join(Unexpected, TEXT(", ")));
        return false;
    }
    return true;
}

template <typename TObjectType>
TObjectType* LoadExact(const TCHAR* ObjectPath, FString& Error)
{
    TObjectType* Result = LoadObject<TObjectType>(nullptr, ObjectPath, nullptr, LOAD_NoWarn);
    if (!Result)
    {
        Error = FString::Printf(
            TEXT("Required asset did not load with its exact expected class: %s"), ObjectPath);
    }
    return Result;
}

UObject* LoadAny(const TCHAR* ObjectPath)
{
    return LoadObject<UObject>(nullptr, ObjectPath, nullptr, LOAD_NoWarn);
}

void GatherSCSNodes(UBlueprint* Blueprint, TArray<FNodeReference>& OutNodes)
{
    OutNodes.Reset();
    if (!Blueprint || !Blueprint->GeneratedClass)
    {
        return;
    }

    TSet<UBlueprint*> Visited;
    for (UClass* Class = Blueprint->GeneratedClass; Class; Class = Class->GetSuperClass())
    {
        UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
        UBlueprint* OwnerBlueprint = BlueprintClass
            ? Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy) : nullptr;
        if (!OwnerBlueprint || Visited.Contains(OwnerBlueprint)
            || !OwnerBlueprint->SimpleConstructionScript)
        {
            continue;
        }
        Visited.Add(OwnerBlueprint);
        for (USCS_Node* Node : OwnerBlueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node)
            {
                FNodeReference& Reference = OutNodes.AddDefaulted_GetRef();
                Reference.OwnerBlueprint = OwnerBlueprint;
                Reference.Node = Node;
            }
        }
    }
}

bool FindUniqueSCSNode(
    UBlueprint* Blueprint,
    FName VariableName,
    FNodeReference& OutReference,
    FString& Error)
{
    TArray<FNodeReference> Nodes;
    GatherSCSNodes(Blueprint, Nodes);
    int32 MatchCount = 0;
    for (const FNodeReference& Reference : Nodes)
    {
        if (Reference.Node && Reference.Node->GetVariableName() == VariableName)
        {
            OutReference = Reference;
            ++MatchCount;
        }
    }
    if (MatchCount != 1)
    {
        Error = FString::Printf(
            TEXT("Blueprint %s exposes %d SCS nodes named %s; exactly one is required"),
            Blueprint ? *Blueprint->GetPathName() : TEXT("<null>"),
            MatchCount,
            *VariableName.ToString());
        return false;
    }
    return true;
}

UActorComponent* GetActualTemplate(
    UBlueprint* Blueprint,
    const FNodeReference& Reference)
{
    UBlueprintGeneratedClass* ActualClass = Blueprint
        ? Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass) : nullptr;
    return ActualClass && Reference.Node
        ? Reference.Node->GetActualComponentTemplate(ActualClass) : nullptr;
}

bool CreateOrGetComponentOverride(
    UBlueprint* Wrapper,
    const FNodeReference& Reference,
    UActorComponent*& OutTemplate,
    FString& Error)
{
    OutTemplate = nullptr;
    if (!Wrapper || !Reference.OwnerBlueprint || !Reference.Node)
    {
        Error = TEXT("Cannot create a component override from an invalid SCS reference");
        return false;
    }

    if (Reference.OwnerBlueprint == Wrapper)
    {
        OutTemplate = Reference.Node->ComponentTemplate;
    }
    else
    {
        UInheritableComponentHandler* Handler =
            Wrapper->GetInheritableComponentHandler(true);
        const FComponentKey Key(Reference.Node);
        if (!Handler || !Key.IsValid())
        {
            Error = FString::Printf(
                TEXT("Could not acquire an inherited-component override key for %s"),
                *Reference.Node->GetVariableName().ToString());
            return false;
        }
        Handler->Modify();
        OutTemplate = Handler->GetOverridenComponentTemplate(Key);
        if (!OutTemplate)
        {
            OutTemplate = Handler->CreateOverridenComponentTemplate(Key);
        }
        Handler->ValidateTemplates();
    }

    if (!OutTemplate)
    {
        Error = FString::Printf(
            TEXT("Could not create the wrapper component template for %s"),
            *Reference.Node->GetVariableName().ToString());
        return false;
    }
    OutTemplate->Modify();
    return true;
}

bool IsFiniteTransform(const FTransform& Transform)
{
    const FVector Translation = Transform.GetTranslation();
    const FVector Scale = Transform.GetScale3D();
    const FQuat Rotation = Transform.GetRotation();
    return FMath::IsFinite(Translation.X)
        && FMath::IsFinite(Translation.Y)
        && FMath::IsFinite(Translation.Z)
        && FMath::IsFinite(Scale.X)
        && FMath::IsFinite(Scale.Y)
        && FMath::IsFinite(Scale.Z)
        && FMath::IsFinite(Rotation.X)
        && FMath::IsFinite(Rotation.Y)
        && FMath::IsFinite(Rotation.Z)
        && FMath::IsFinite(Rotation.W);
}

bool ValidateAndReportComponentTransform(
    const TCHAR* FieldName,
    const USceneComponent* Component,
    FRunReport& Report,
    FString& Error)
{
    if (!Component)
    {
        Error = FString::Printf(TEXT("%s component is absent"), FieldName);
        return false;
    }
    const FTransform Transform = Component->GetRelativeTransform();
    const FVector Translation = Transform.GetTranslation();
    const FVector Scale = Transform.GetScale3D();
    if (!IsFiniteTransform(Transform)
        || Translation.SizeSquared() > FMath::Square(10000.0)
        || FMath::Abs(Scale.X) < KINDA_SMALL_NUMBER
        || FMath::Abs(Scale.Y) < KINDA_SMALL_NUMBER
        || FMath::Abs(Scale.Z) < KINDA_SMALL_NUMBER
        || FMath::Abs(Scale.X) > 100.0
        || FMath::Abs(Scale.Y) > 100.0
        || FMath::Abs(Scale.Z) > 100.0)
    {
        Error = FString::Printf(
            TEXT("%s relative transform is non-finite or outside the bounded authoring envelope"),
            FieldName);
        return false;
    }
    Report.Root->SetObjectField(FieldName, TransformToJson(Transform));
    return true;
}

bool ValidateBoneChain(
    const UIKRigDefinition* Rig,
    const USkeletalMesh* Mesh,
    FName ChainName,
    FString& Error)
{
    const FBoneChain* Chain = Rig ? Rig->GetRetargetChainByName(ChainName) : nullptr;
    if (!Chain || !Mesh)
    {
        Error = FString::Printf(TEXT("IK chain %s or its skeletal mesh is missing"),
            *ChainName.ToString());
        return false;
    }
    const FReferenceSkeleton& ReferenceSkeleton = Mesh->GetRefSkeleton();
    const int32 StartIndex = ReferenceSkeleton.FindBoneIndex(Chain->StartBone.BoneName);
    int32 CurrentIndex = ReferenceSkeleton.FindBoneIndex(Chain->EndBone.BoneName);
    if (StartIndex == INDEX_NONE || CurrentIndex == INDEX_NONE)
    {
        Error = FString::Printf(
            TEXT("IK chain %s endpoints %s -> %s are absent from %s"),
            *ChainName.ToString(),
            *Chain->StartBone.BoneName.ToString(),
            *Chain->EndBone.BoneName.ToString(),
            *Mesh->GetPathName());
        return false;
    }
    while (CurrentIndex != INDEX_NONE && CurrentIndex != StartIndex)
    {
        CurrentIndex = ReferenceSkeleton.GetParentIndex(CurrentIndex);
    }
    if (CurrentIndex != StartIndex)
    {
        Error = FString::Printf(
            TEXT("IK chain %s endpoints are not ancestor-compatible on %s"),
            *ChainName.ToString(), *Mesh->GetPathName());
        return false;
    }
    return true;
}

bool ValidateSourceRigContract(
    UIKRigDefinition* SourceRig,
    USkeletalMesh* SourceMesh,
    FString& Error)
{
    if (!SourceRig || SourceRig->GetPreviewMesh() != SourceMesh
        || SourceRig->GetRoot() != FName(TEXT("root"))
        || SourceRig->GetPelvis() != FName(TEXT("pelvis")))
    {
        Error = TEXT("IK_DG_Master preview mesh, root, or pelvis differs from the accepted contract");
        return false;
    }
    for (const FSourceChainContract& Expected : SourceChainContracts)
    {
        const FBoneChain* Chain = SourceRig->GetRetargetChainByName(FName(Expected.Chain));
        if (!Chain
            || Chain->StartBone.BoneName != FName(Expected.StartBone)
            || Chain->EndBone.BoneName != FName(Expected.EndBone)
            || !ValidateBoneChain(SourceRig, SourceMesh, FName(Expected.Chain), Error))
        {
            if (Error.IsEmpty())
            {
                Error = FString::Printf(
                    TEXT("IK_DG_Master chain %s differs from %s -> %s"),
                    Expected.Chain, Expected.StartBone, Expected.EndBone);
            }
            return false;
        }
    }
    return true;
}

bool LoadDependencies(FAssetContext& Context, FString& Error)
{
    Context.GeneratedBlueprint = LoadExact<UBlueprint>(GeneratedBlueprintPath, Error);
    if (!Context.GeneratedBlueprint || !Context.GeneratedBlueprint->GeneratedClass
        || !Context.GeneratedBlueprint->IsUpToDate())
    {
        if (Error.IsEmpty())
        {
            Error = TEXT("The assembled generated MetaHuman Blueprint is not compile-current");
        }
        return false;
    }
    if (!Context.GeneratedBlueprint->GeneratedClass->IsChildOf(AActor::StaticClass())
        || Context.GeneratedBlueprint->GeneratedClass->IsChildOf(APawn::StaticClass())
        || Context.GeneratedBlueprint->GeneratedClass->IsChildOf(AController::StaticClass()))
    {
        Error = TEXT("The generated MetaHuman Blueprint is not a presentation-only actor class");
        return false;
    }

    Context.SourceMesh = LoadExact<USkeletalMesh>(SourceMeshPath, Error);
    Context.SourceIKRig = LoadExact<UIKRigDefinition>(SourceIKRigPath, Error);
    Context.TargetIKRig = LoadExact<UIKRigDefinition>(TargetIKRigPath, Error);
    if (!Context.SourceMesh || !Context.SourceIKRig || !Context.TargetIKRig)
    {
        return false;
    }
    if (!ValidateSourceRigContract(Context.SourceIKRig, Context.SourceMesh, Error))
    {
        return false;
    }

    FNodeReference BodyReference;
    FNodeReference FaceReference;
    if (!FindUniqueSCSNode(
            Context.GeneratedBlueprint, BodyVariableName, BodyReference, Error)
        || !FindUniqueSCSNode(
            Context.GeneratedBlueprint, FaceVariableName, FaceReference, Error))
    {
        return false;
    }
    const USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(
        GetActualTemplate(Context.GeneratedBlueprint, BodyReference));
    const USkeletalMeshComponent* Face = Cast<USkeletalMeshComponent>(
        GetActualTemplate(Context.GeneratedBlueprint, FaceReference));
    if (!Body || !Face || Body == Face
        || !Body->GetSkeletalMeshAsset() || !Face->GetSkeletalMeshAsset())
    {
        Error = TEXT("The generated MetaHuman does not expose distinct ready Body and Face meshes");
        return false;
    }
    Context.TargetMesh = Body->GetSkeletalMeshAsset();
    return true;
}

UEdGraph* FindInterfaceGraph(UBlueprint* Blueprint, FName FunctionName)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
    {
        if (Interface.Interface != UDiscGolfMetaHumanVisualContract::StaticClass())
        {
            continue;
        }
        for (UEdGraph* Graph : Interface.Graphs)
        {
            if (Graph && Graph->GetFName() == FunctionName)
            {
                return Graph;
            }
        }
    }
    return FindObject<UEdGraph>(Blueprint, *FunctionName.ToString());
}

bool SetPinDefault(
    const UEdGraphSchema_K2* Schema,
    UEdGraphPin* Pin,
    const FString& Value,
    const TCHAR* Description,
    FString& Error)
{
    if (!Schema || !Pin)
    {
        Error = FString::Printf(TEXT("Interface graph is missing its %s pin"), Description);
        return false;
    }
    Schema->TrySetDefaultValue(*Pin, Value);
    if (Pin->DefaultValue != Value)
    {
        Error = FString::Printf(
            TEXT("Interface graph rejected the deterministic %s default"), Description);
        return false;
    }
    return true;
}

bool SetVectorPinDefault(
    const UEdGraphSchema_K2* Schema,
    UEdGraphPin* Pin,
    const FVector& Value,
    const TCHAR* Description,
    FString& Error)
{
    if (!Schema || !Pin)
    {
        Error = FString::Printf(TEXT("Interface graph is missing its %s pin"), Description);
        return false;
    }
    Schema->TrySetDefaultValue(*Pin, VectorPinDefault(Value));
    if (!VectorPinValueMatches(Pin->DefaultValue, Value))
    {
        Error = FString::Printf(
            TEXT("Interface graph rejected the deterministic %s default"), Description);
        return false;
    }
    return true;
}

bool SetRotatorPinDefault(
    const UEdGraphSchema_K2* Schema,
    UEdGraphPin* Pin,
    const FRotator& Value,
    const TCHAR* Description,
    FString& Error)
{
    if (!Schema || !Pin)
    {
        Error = FString::Printf(TEXT("Interface graph is missing its %s pin"), Description);
        return false;
    }
    Schema->TrySetDefaultValue(*Pin, RotatorPinDefault(Value));
    if (!RotatorPinValueMatches(Pin->DefaultValue, Value))
    {
        Error = FString::Printf(
            TEXT("Interface graph rejected the deterministic %s default"), Description);
        return false;
    }
    return true;
}

bool AuthorInterfaceGraph(
    UBlueprint* Wrapper,
    FName FunctionName,
    const FString& Status,
    FString& Error)
{
    UEdGraph* Graph = FindInterfaceGraph(Wrapper, FunctionName);
    const UEdGraphSchema_K2* Schema = Graph
        ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
    if (!Graph || !Schema)
    {
        Error = FString::Printf(
            TEXT("Project visual-contract graph %s was not generated"),
            *FunctionName.ToString());
        return false;
    }

    UK2Node_FunctionEntry* Entry = nullptr;
    UK2Node_FunctionResult* Result = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UK2Node_FunctionEntry* EntryCandidate = Cast<UK2Node_FunctionEntry>(Node))
        {
            if (Entry)
            {
                Error = FString::Printf(TEXT("Graph %s has duplicate entry nodes"),
                    *FunctionName.ToString());
                return false;
            }
            Entry = EntryCandidate;
        }
        else if (UK2Node_FunctionResult* ResultCandidate =
                     Cast<UK2Node_FunctionResult>(Node))
        {
            if (Result)
            {
                Error = FString::Printf(TEXT("Graph %s has duplicate result nodes"),
                    *FunctionName.ToString());
                return false;
            }
            Result = ResultCandidate;
        }
        else if (Node)
        {
            Error = FString::Printf(
                TEXT("New interface graph %s contains an unexpected node"),
                *FunctionName.ToString());
            return false;
        }
    }
    if (!Entry || !Result || Graph->Nodes.Num() != 2)
    {
        Error = FString::Printf(
            TEXT("New interface graph %s does not contain exactly entry and result nodes"),
            *FunctionName.ToString());
        return false;
    }

    UFunction* TransformFunction = AActor::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorRelativeTransform));
    UFunction* MakeTransformFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, MakeTransform));
    if (!TransformFunction || !MakeTransformFunction)
    {
        Error = TEXT("UE 5.8 transform Blueprint functions are unavailable");
        return false;
    }
    UK2Node_CallFunction* MakeTransformCall = NewObject<UK2Node_CallFunction>(Graph);
    MakeTransformCall->SetFlags(RF_Transactional);
    Graph->AddNode(MakeTransformCall, false, false);
    MakeTransformCall->SetFromFunction(MakeTransformFunction);
    MakeTransformCall->CreateNewGuid();
    MakeTransformCall->PostPlacedNewNode();
    MakeTransformCall->AllocateDefaultPins();
    UK2Node_CallFunction* TransformCall = NewObject<UK2Node_CallFunction>(Graph);
    TransformCall->SetFlags(RF_Transactional);
    Graph->AddNode(TransformCall, false, false);
    TransformCall->SetFromFunction(TransformFunction);
    TransformCall->CreateNewGuid();
    TransformCall->PostPlacedNewNode();
    TransformCall->AllocateDefaultPins();
    MakeTransformCall->NodePosX = 80;
    MakeTransformCall->NodePosY = 180;
    TransformCall->NodePosX = 400;
    TransformCall->NodePosY = 0;
    Entry->NodePosX = 0;
    Entry->NodePosY = 0;
    Result->NodePosX = 700;
    Result->NodePosY = 0;

    UEdGraphPin* EntryExec = Entry->FindPin(UEdGraphSchema_K2::PN_Then);
    UEdGraphPin* CallExec = TransformCall->FindPin(UEdGraphSchema_K2::PN_Execute);
    UEdGraphPin* CallThen = TransformCall->FindPin(UEdGraphSchema_K2::PN_Then);
    UEdGraphPin* ResultExec = Result->FindPin(UEdGraphSchema_K2::PN_Execute);
    UEdGraphPin* MakeTransformResult =
        MakeTransformCall->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
    UEdGraphPin* TransformInput = TransformCall->FindPin(TEXT("NewRelativeTransform"));
    if (!EntryExec || !CallExec || !CallThen || !ResultExec
        || !MakeTransformResult || !TransformInput
        || !Schema->TryCreateConnection(EntryExec, CallExec)
        || !Schema->TryCreateConnection(CallThen, ResultExec)
        || !Schema->TryCreateConnection(MakeTransformResult, TransformInput))
    {
        Error = FString::Printf(
            TEXT("Could not wire deterministic execution for graph %s"),
            *FunctionName.ToString());
        return false;
    }

    if (!SetVectorPinDefault(
            Schema,
            MakeTransformCall->FindPin(TEXT("Location")),
            WrapperRelativeTransform.GetTranslation(),
            TEXT("wrapper relative location"),
            Error)
        || !SetRotatorPinDefault(
            Schema,
            MakeTransformCall->FindPin(TEXT("Rotation")),
            WrapperRelativeTransform.Rotator(),
            TEXT("wrapper relative rotation"),
            Error)
        || !SetVectorPinDefault(
            Schema,
            MakeTransformCall->FindPin(TEXT("Scale")),
            WrapperRelativeTransform.GetScale3D(),
            TEXT("wrapper relative scale"),
            Error)
        || !SetPinDefault(
            Schema,
            TransformCall->FindPin(TEXT("bSweep")),
            TEXT("false"),
            TEXT("sweep policy"),
            Error)
        || !SetPinDefault(
            Schema,
            TransformCall->FindPin(TEXT("bTeleport")),
            TEXT("true"),
            TEXT("teleport policy"),
            Error)
        || !SetPinDefault(
            Schema,
            Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue),
            TEXT("true"),
            TEXT("success result"),
            Error)
        || !SetPinDefault(
            Schema,
            Result->FindPin(TEXT("OutStatus")),
            Status,
            TEXT("status result"),
            Error))
    {
        return false;
    }
    return true;
}

bool ValidateInterfaceGraph(
    UBlueprint* Wrapper,
    FName FunctionName,
    const FString& ExpectedStatus,
    const FTransform& ExpectedRelativeTransform,
    FString& Error)
{
    UEdGraph* Graph = FindInterfaceGraph(Wrapper, FunctionName);
    if (!Graph || Graph->Nodes.Num() != 4)
    {
        Error = FString::Printf(
            TEXT("Wrapper graph %s is missing or is not the frozen four-node graph"),
            *FunctionName.ToString());
        return false;
    }
    UFunction* ExpectedTransformFunction = AActor::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorRelativeTransform));
    UFunction* ExpectedMakeTransformFunction =
        UKismetMathLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, MakeTransform));
    UK2Node_FunctionEntry* Entry = nullptr;
    UK2Node_FunctionResult* Result = nullptr;
    UK2Node_CallFunction* TransformCall = nullptr;
    UK2Node_CallFunction* MakeTransformCall = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UK2Node_FunctionEntry* EntryCandidate = Cast<UK2Node_FunctionEntry>(Node))
        {
            if (Entry)
            {
                return false;
            }
            Entry = EntryCandidate;
        }
        else if (UK2Node_FunctionResult* ResultCandidate =
                     Cast<UK2Node_FunctionResult>(Node))
        {
            if (Result)
            {
                return false;
            }
            Result = ResultCandidate;
        }
        else if (UK2Node_CallFunction* CallCandidate =
                     Cast<UK2Node_CallFunction>(Node))
        {
            if (CallCandidate->GetTargetFunction() == ExpectedTransformFunction)
            {
                if (TransformCall)
                {
                    return false;
                }
                TransformCall = CallCandidate;
            }
            else if (CallCandidate->GetTargetFunction() == ExpectedMakeTransformFunction)
            {
                if (MakeTransformCall)
                {
                    return false;
                }
                MakeTransformCall = CallCandidate;
            }
            else
            {
                Error = FString::Printf(
                    TEXT("Wrapper graph %s contains an unexpected function call"),
                    *FunctionName.ToString());
                return false;
            }
        }
        else if (Node)
        {
            Error = FString::Printf(TEXT("Wrapper graph %s contains an unexpected node"),
                *FunctionName.ToString());
            return false;
        }
    }
    if (!Entry || !Result || !TransformCall || !MakeTransformCall
        || !ExpectedTransformFunction || !ExpectedMakeTransformFunction)
    {
        Error = FString::Printf(
            TEXT("Wrapper graph %s does not contain the exact transform calls"),
            *FunctionName.ToString());
        return false;
    }

    const UEdGraphPin* EntryExec = Entry->FindPin(UEdGraphSchema_K2::PN_Then);
    const UEdGraphPin* CallExec = TransformCall->FindPin(UEdGraphSchema_K2::PN_Execute);
    const UEdGraphPin* CallThen = TransformCall->FindPin(UEdGraphSchema_K2::PN_Then);
    const UEdGraphPin* ResultExec = Result->FindPin(UEdGraphSchema_K2::PN_Execute);
    const UEdGraphPin* TransformPin = TransformCall->FindPin(TEXT("NewRelativeTransform"));
    const UEdGraphPin* MakeTransformResult =
        MakeTransformCall->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
    const UEdGraphPin* LocationPin = MakeTransformCall->FindPin(TEXT("Location"));
    const UEdGraphPin* RotationPin = MakeTransformCall->FindPin(TEXT("Rotation"));
    const UEdGraphPin* ScalePin = MakeTransformCall->FindPin(TEXT("Scale"));
    const UEdGraphPin* SweepPin = TransformCall->FindPin(TEXT("bSweep"));
    const UEdGraphPin* TeleportPin = TransformCall->FindPin(TEXT("bTeleport"));
    const UEdGraphPin* ReturnPin = Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
    const UEdGraphPin* StatusPin = Result->FindPin(TEXT("OutStatus"));
    if (!EntryExec || !CallExec || !CallThen || !ResultExec
        || EntryExec->LinkedTo.Num() != 1 || EntryExec->LinkedTo[0] != CallExec
        || CallExec->LinkedTo.Num() != 1 || CallExec->LinkedTo[0] != EntryExec
        || CallThen->LinkedTo.Num() != 1 || CallThen->LinkedTo[0] != ResultExec
        || ResultExec->LinkedTo.Num() != 1 || ResultExec->LinkedTo[0] != CallThen
        || !TransformPin || !MakeTransformResult
        || TransformPin->LinkedTo.Num() != 1
        || TransformPin->LinkedTo[0] != MakeTransformResult
        || MakeTransformResult->LinkedTo.Num() != 1
        || MakeTransformResult->LinkedTo[0] != TransformPin
        || !LocationPin || LocationPin->LinkedTo.Num() != 0
        || !VectorPinValueMatches(
            LocationPin->DefaultValue, ExpectedRelativeTransform.GetTranslation())
        || !RotationPin || RotationPin->LinkedTo.Num() != 0
        || !RotatorPinValueMatches(
            RotationPin->DefaultValue, ExpectedRelativeTransform.Rotator())
        || !ScalePin || ScalePin->LinkedTo.Num() != 0
        || !VectorPinValueMatches(
            ScalePin->DefaultValue, ExpectedRelativeTransform.GetScale3D())
        || !SweepPin || SweepPin->DefaultValue != TEXT("false")
        || !TeleportPin || TeleportPin->DefaultValue != TEXT("true")
        || !ReturnPin || ReturnPin->DefaultValue != TEXT("true")
        || !StatusPin || StatusPin->DefaultValue != ExpectedStatus)
    {
        Error = FString::Printf(
            TEXT("Wrapper graph %s differs from the fixed-preset/orientation contract"),
            *FunctionName.ToString());
        return false;
    }
    return true;
}

bool SetInterfaceGraphRotationDefault(
    UBlueprint* Wrapper,
    FName FunctionName,
    const FRotator& Rotation,
    FString& Error)
{
    UEdGraph* Graph = FindInterfaceGraph(Wrapper, FunctionName);
    const UEdGraphSchema_K2* Schema = Graph
        ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
    UFunction* ExpectedMakeTransformFunction =
        UKismetMathLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, MakeTransform));
    UK2Node_CallFunction* MakeTransformCall = nullptr;
    if (Graph && Schema && ExpectedMakeTransformFunction)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            UK2Node_CallFunction* Candidate = Cast<UK2Node_CallFunction>(Node);
            if (Candidate
                && Candidate->GetTargetFunction() == ExpectedMakeTransformFunction)
            {
                if (MakeTransformCall)
                {
                    Error = FString::Printf(
                        TEXT("Wrapper graph %s has duplicate MakeTransform calls"),
                        *FunctionName.ToString());
                    return false;
                }
                MakeTransformCall = Candidate;
            }
        }
    }
    UEdGraphPin* RotationPin = MakeTransformCall
        ? MakeTransformCall->FindPin(TEXT("Rotation")) : nullptr;
    if (!Wrapper || !Graph || !Schema || !ExpectedMakeTransformFunction
        || !MakeTransformCall || !RotationPin || !RotationPin->LinkedTo.IsEmpty())
    {
        Error = FString::Printf(
            TEXT("Wrapper graph %s cannot expose its exact unlinked MakeTransform rotation default"),
            *FunctionName.ToString());
        return false;
    }

    Wrapper->Modify();
    Graph->Modify();
    MakeTransformCall->Modify();
    RotationPin->Modify();
    return SetRotatorPinDefault(
        Schema,
        RotationPin,
        Rotation,
        TEXT("wrapper relative rotation"),
        Error);
}

bool CharacterInstanceMatches(
    const UBlueprint* Generated,
    const UBlueprint* Wrapper,
    bool bRequireDiskEvidence,
    FRunReport& Report,
    FString& Error)
{
    if (!Generated || !Wrapper || !Generated->GeneratedClass || !Wrapper->GeneratedClass)
    {
        Error = TEXT("Cannot compare assembled CharacterInstance on invalid Blueprints");
        return false;
    }
    const FObjectPropertyBase* GeneratedProperty = FindFProperty<FObjectPropertyBase>(
        Generated->GeneratedClass, CharacterInstancePropertyName);
    const FObjectPropertyBase* WrapperProperty = FindFProperty<FObjectPropertyBase>(
        Wrapper->GeneratedClass, CharacterInstancePropertyName);
    const UObject* GeneratedCDO = Generated->GeneratedClass->GetDefaultObject(false);
    const UObject* WrapperCDO = Wrapper->GeneratedClass->GetDefaultObject(false);
    if (!GeneratedCDO || !WrapperCDO)
    {
        Error = FString::Printf(
            TEXT("CharacterInstance CDO is absent: generated=%s wrapper=%s"),
            GeneratedCDO ? TEXT("present") : TEXT("absent"),
            WrapperCDO ? TEXT("present") : TEXT("absent"));
        return false;
    }
    if (GeneratedProperty || WrapperProperty)
    {
        Error = FString::Printf(
            TEXT("Fixed assembled runtime Blueprints must not expose CharacterInstance: generated=%s wrapper=%s"),
            GeneratedProperty ? TEXT("present") : TEXT("absent"),
            WrapperProperty ? TEXT("present") : TEXT("absent"));
        return false;
    }

    // UMetaHumanDefaultEditorPipeline::UpdateActorBlueprint deliberately treats
    // CharacterInstance as optional, and the accepted Optimized assembly's flattened
    // Actor class does not expose it.  Preserve that exact symmetric contract instead
    // of synthesizing a persistent editor-only dependency on the canonical wrapper.
    Report.Root->SetStringField(
        TEXT("character_instance_storage_contract"),
        TEXT("NO_REFLECTED_PROPERTY_FIXED_ASSEMBLED_ACTOR"));
    Report.Root->SetBoolField(TEXT("character_instance_property_absent_on_both"), true);
    Report.Root->SetBoolField(TEXT("character_instance_contract_verified"), true);
    Report.Root->SetBoolField(
        TEXT("generated_transient_character_instance_present"), false);
    if (!bRequireDiskEvidence)
    {
        Report.Root->SetBoolField(
            TEXT("wrapper_transient_character_instance_present_during_authoring"), false);
    }
    Report.Root->SetStringField(TEXT("generated_character_instance_path"), FString());
    Report.Root->SetStringField(
        TEXT("generated_character_instance_outer_package"), FString());
    Report.Root->SetStringField(TEXT("wrapper_character_instance_path"), FString());
    Report.Root->SetStringField(
        TEXT("wrapper_character_instance_outer_package"), FString());
    if (bRequireDiskEvidence)
    {
        Report.Root->SetBoolField(
            TEXT("wrapper_character_instance_property_absent_after_disk_reload"), true);
        Report.Root->SetBoolField(TEXT("runtime_character_instance_property_absent"), true);
    }
    return true;
}

bool ValidateWrapper(
    FAssetContext& Context,
    FRunReport& Report,
    bool bRequireDiskEvidence,
    const FTransform& ExpectedRelativeTransform,
    FString& Error)
{
    UBlueprint* Wrapper = Context.WrapperBlueprint;
    UBlueprint* Generated = Context.GeneratedBlueprint;
    if (!Wrapper || !Generated || Wrapper->GetPathName() != WrapperBlueprintPath
        || !Wrapper->GeneratedClass || !Wrapper->IsUpToDate()
        || Wrapper->ParentClass != Generated->ParentClass
        || !Wrapper->GeneratedClass->IsChildOf(AActor::StaticClass())
        || Wrapper->GeneratedClass->IsChildOf(APawn::StaticClass())
        || Wrapper->GeneratedClass->IsChildOf(AController::StaticClass())
        || !Wrapper->GeneratedClass->ImplementsInterface(
            UDiscGolfMetaHumanVisualContract::StaticClass()))
    {
        Error = TEXT("Canonical wrapper class, parent, compile state, or project interface differs");
        return false;
    }
    if (!CharacterInstanceMatches(
            Generated, Wrapper, bRequireDiskEvidence, Report, Error)
        || !ValidateInterfaceGraph(
            Wrapper,
            FName(ConfigureFunctionName),
            ConfigureStatus,
            ExpectedRelativeTransform,
            Error)
        || !ValidateInterfaceGraph(
            Wrapper,
            FName(ApplyFunctionName),
            ApplyStatus,
            ExpectedRelativeTransform,
            Error))
    {
        return false;
    }

    FNodeReference BodyReference;
    FNodeReference FaceReference;
    if (!FindUniqueSCSNode(Wrapper, BodyVariableName, BodyReference, Error)
        || !FindUniqueSCSNode(Wrapper, FaceVariableName, FaceReference, Error))
    {
        return false;
    }
    USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(
        GetActualTemplate(Wrapper, BodyReference));
    USkeletalMeshComponent* Face = Cast<USkeletalMeshComponent>(
        GetActualTemplate(Wrapper, FaceReference));
    if (!Body || !Face || Body == Face
        || !Body->GetSkeletalMeshAsset() || !Face->GetSkeletalMeshAsset())
    {
        Error = TEXT("Canonical wrapper Body and Face templates are not distinct ready skeletal meshes");
        return false;
    }

    FNodeReference GeneratedBodyReference;
    FNodeReference GeneratedFaceReference;
    const USkeletalMeshComponent* GeneratedBody = nullptr;
    const USkeletalMeshComponent* GeneratedFace = nullptr;
    if (!FindUniqueSCSNode(
            Generated, BodyVariableName, GeneratedBodyReference, Error)
        || !FindUniqueSCSNode(
            Generated, FaceVariableName, GeneratedFaceReference, Error))
    {
        return false;
    }
    GeneratedBody = Cast<USkeletalMeshComponent>(
        GetActualTemplate(Generated, GeneratedBodyReference));
    GeneratedFace = Cast<USkeletalMeshComponent>(
        GetActualTemplate(Generated, GeneratedFaceReference));
    if (!ValidateAndReportComponentTransform(
            TEXT("generated_body_relative_transform"), GeneratedBody, Report, Error)
        || !ValidateAndReportComponentTransform(
            TEXT("generated_face_relative_transform"), GeneratedFace, Report, Error)
        || !ValidateAndReportComponentTransform(
            TEXT("wrapper_body_relative_transform"), Body, Report, Error)
        || !ValidateAndReportComponentTransform(
            TEXT("wrapper_face_relative_transform"), Face, Report, Error))
    {
        return false;
    }

    int32 TaggedBodyCount = 0;
    int32 TaggedHeadCount = 0;
    int32 MetaHumanComponentCount = 0;
    TArray<FNodeReference> Nodes;
    GatherSCSNodes(Wrapper, Nodes);
    for (const FNodeReference& Reference : Nodes)
    {
        UActorComponent* ActualTemplate = GetActualTemplate(Wrapper, Reference);
        if (USkeletalMeshComponent* SkeletalTemplate =
                Cast<USkeletalMeshComponent>(ActualTemplate))
        {
            TaggedBodyCount += SkeletalTemplate->ComponentHasTag(BodyTag) ? 1 : 0;
            TaggedHeadCount += SkeletalTemplate->ComponentHasTag(HeadTag) ? 1 : 0;
        }
        MetaHumanComponentCount +=
            Cast<UMetaHumanComponentUE>(ActualTemplate) ? 1 : 0;
    }
    if (TaggedBodyCount != 1 || TaggedHeadCount != 1
        || !Body->ComponentHasTag(BodyTag) || Body->ComponentHasTag(HeadTag)
        || !Face->ComponentHasTag(HeadTag) || Face->ComponentHasTag(BodyTag)
        || MetaHumanComponentCount != 1)
    {
        Error = FString::Printf(
            TEXT("Wrapper component contract differs: body tags=%d head tags=%d MetaHuman components=%d"),
            TaggedBodyCount, TaggedHeadCount, MetaHumanComponentCount);
        return false;
    }
    Context.TargetMesh = Body->GetSkeletalMeshAsset();
    Report.Root->SetStringField(TEXT("target_mesh"), Context.TargetMesh->GetPathName());
    Report.Root->SetNumberField(TEXT("tagged_body_count"), TaggedBodyCount);
    Report.Root->SetNumberField(TEXT("tagged_head_count"), TaggedHeadCount);
    Report.Root->SetNumberField(TEXT("metahuman_component_count"), MetaHumanComponentCount);
    Report.Root->SetBoolField(TEXT("wrapper_contract_verified"), true);
    return true;
}

bool AuthorWrapper(FAssetContext& Context, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(
        FString(WrapperBlueprintPath));
    const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
    const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(
        TEXT("AssetTools")).Get();
    Context.WrapperBlueprint = Cast<UBlueprint>(AssetTools.DuplicateAsset(
        AssetName, PackagePath, Context.GeneratedBlueprint));
    if (!Context.WrapperBlueprint
        || Context.WrapperBlueprint->GetPathName() != WrapperBlueprintPath)
    {
        Error = TEXT("AssetTools could not duplicate the generated actor to the canonical wrapper path");
        return false;
    }

    FNodeReference BodyReference;
    FNodeReference FaceReference;
    if (!FindUniqueSCSNode(
            Context.WrapperBlueprint, BodyVariableName, BodyReference, Error)
        || !FindUniqueSCSNode(
            Context.WrapperBlueprint, FaceVariableName, FaceReference, Error))
    {
        return false;
    }
    UActorComponent* BodyTemplateObject = nullptr;
    UActorComponent* FaceTemplateObject = nullptr;
    if (!CreateOrGetComponentOverride(
            Context.WrapperBlueprint, BodyReference, BodyTemplateObject, Error)
        || !CreateOrGetComponentOverride(
            Context.WrapperBlueprint, FaceReference, FaceTemplateObject, Error))
    {
        return false;
    }
    USkeletalMeshComponent* BodyTemplate = Cast<USkeletalMeshComponent>(BodyTemplateObject);
    USkeletalMeshComponent* FaceTemplate = Cast<USkeletalMeshComponent>(FaceTemplateObject);
    if (!BodyTemplate || !FaceTemplate || BodyTemplate == FaceTemplate)
    {
        Error = TEXT("Inherited Body/Face overrides are not distinct skeletal-mesh templates");
        return false;
    }
    BodyTemplate->ComponentTags.Remove(BodyTag);
    BodyTemplate->ComponentTags.Remove(HeadTag);
    BodyTemplate->ComponentTags.Add(BodyTag);
    FaceTemplate->ComponentTags.Remove(BodyTag);
    FaceTemplate->ComponentTags.Remove(HeadTag);
    FaceTemplate->ComponentTags.Add(HeadTag);

    if (!FBlueprintEditorUtils::ImplementNewInterface(
            Context.WrapperBlueprint,
            UDiscGolfMetaHumanVisualContract::StaticClass()->GetClassPathName()))
    {
        Error = TEXT("Could not add the project MetaHuman visual contract to the wrapper");
        return false;
    }
    if (!AuthorInterfaceGraph(
            Context.WrapperBlueprint,
            FName(ConfigureFunctionName),
            ConfigureStatus,
            Error)
        || !AuthorInterfaceGraph(
            Context.WrapperBlueprint,
            FName(ApplyFunctionName),
            ApplyStatus,
            Error))
    {
        return false;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.WrapperBlueprint);
    FKismetEditorUtilities::CompileBlueprint(
        Context.WrapperBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    if (!Context.WrapperBlueprint->IsUpToDate()
        || !Context.WrapperBlueprint->GeneratedClass)
    {
        Error = TEXT("Canonical MetaHuman wrapper failed to compile");
        return false;
    }
    Context.WrapperBlueprint->MarkPackageDirty();
    return true;
}

bool ValidateRetargeter(
    FAssetContext& Context,
    FRunReport& Report,
    bool bExpectRunIKEnabled,
    bool bExpectRootMotionEnabled,
    FString& Error)
{
    UIKRetargeter* Retargeter = Context.Retargeter;
    UIKRetargeterController* Controller = Retargeter
        ? UIKRetargeterController::GetController(Retargeter) : nullptr;
    if (!Retargeter || Retargeter->GetPathName() != RetargeterPath
        || !Controller
        || Controller->GetIKRig(ERetargetSourceOrTarget::Source) != Context.SourceIKRig
        || Controller->GetIKRig(ERetargetSourceOrTarget::Target) != Context.TargetIKRig
        || Controller->GetPreviewMesh(ERetargetSourceOrTarget::Source) != Context.SourceMesh
        || Controller->GetPreviewMesh(ERetargetSourceOrTarget::Target) != Context.TargetMesh)
    {
        Error = TEXT("Canonical retargeter rigs, preview meshes, or op stack differ");
        return false;
    }

    const UScriptStruct* ExpectedOpTypes[] = {
        FIKRetargetPelvisMotionOp::StaticStruct(),
        FIKRetargetFKChainsOp::StaticStruct(),
        FIKRetargetRunIKRigOp::StaticStruct(),
        FIKRetargetRootMotionOp::StaticStruct(),
        FIKRetargetCurveRemapOp::StaticStruct(),
    };
    if (Controller->GetNumRetargetOps() != UE_ARRAY_COUNT(ExpectedOpTypes))
    {
        Error = FString::Printf(
            TEXT("Canonical retarget op count is %d, expected %d"),
            Controller->GetNumRetargetOps(),
            UE_ARRAY_COUNT(ExpectedOpTypes));
        return false;
    }
    int32 RunIKOpCount = 0;
    int32 RootMotionOpCount = 0;
    int32 RootMotionOpIndex = INDEX_NONE;
    const FIKRetargetPelvisMotionOpSettings* PelvisMotionSettings = nullptr;
    const FIKRetargetRootMotionOpSettings* RootMotionSettings = nullptr;
    TArray<TSharedPtr<FJsonValue>> OpTypeValues;
    TArray<TSharedPtr<FJsonValue>> OpEnabledValues;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedOpTypes); ++Index)
    {
        FIKRetargetOpBase* Op = Controller->GetRetargetOpByIndex(Index);
        const bool bExpectedEnabled = Index == 2
            ? bExpectRunIKEnabled
            : (Index == 3 ? bExpectRootMotionEnabled : true);
        if (!Op || Op->GetType() != ExpectedOpTypes[Index])
        {
            Error = FString::Printf(
                TEXT("Canonical retarget op %d has an unexpected type"), Index);
            return false;
        }
        if (Controller->GetRetargetOpEnabled(Index) != bExpectedEnabled)
        {
            Error = FString::Printf(
                TEXT("Canonical retarget op %d enabled state differs"), Index);
            return false;
        }
        if (Op->GetType() == FIKRetargetRunIKRigOp::StaticStruct())
        {
            ++RunIKOpCount;
        }
        else if (Op->GetType() == FIKRetargetRootMotionOp::StaticStruct())
        {
            ++RootMotionOpCount;
            RootMotionOpIndex = Index;
            RootMotionSettings = static_cast<
                const FIKRetargetRootMotionOpSettings*>(
                    Op->GetSettingsConst());
        }
        else if (Op->GetType() == FIKRetargetPelvisMotionOp::StaticStruct())
        {
            PelvisMotionSettings = static_cast<
                const FIKRetargetPelvisMotionOpSettings*>(
                    Op->GetSettingsConst());
        }
        OpTypeValues.Add(MakeShared<FJsonValueString>(Op->GetType()->GetName()));
        OpEnabledValues.Add(MakeShared<FJsonValueBoolean>(bExpectedEnabled));
    }
    if (RunIKOpCount != 1)
    {
        Error = FString::Printf(
            TEXT("Canonical retargeter has %d Run IK Rig ops, expected 1"),
            RunIKOpCount);
        return false;
    }
    if (RootMotionOpCount != 1 || RootMotionOpIndex != 3
        || !RootMotionSettings || !PelvisMotionSettings
        || Context.SourceIKRig->GetRoot() != FName(TEXT("root"))
        || Context.SourceIKRig->GetPelvis() != FName(TEXT("pelvis"))
        || Context.TargetIKRig->GetRoot() != FName(TEXT("pelvis"))
        || Context.TargetIKRig->GetPelvis() != FName(TEXT("pelvis"))
        || RootMotionSettings->bEnabled != bExpectRootMotionEnabled
        || RootMotionSettings->SourceRoot.BoneName != FName(TEXT("root"))
        || RootMotionSettings->TargetRoot.BoneName != FName(TEXT("pelvis"))
        || RootMotionSettings->TargetPelvis.BoneName != FName(TEXT("pelvis"))
        || RootMotionSettings->RootMotionSource
            != ERootMotionSource::CopyFromSourceRoot)
    {
        Error = TEXT(
            "Canonical Root Motion op must be the unique index-3 root-to-pelvis/pelvis CopyFromSourceRoot operation with the expected enabled state");
        return false;
    }
    Report.Root->SetNumberField(
        TEXT("retarget_op_count"), UE_ARRAY_COUNT(ExpectedOpTypes));
    Report.Root->SetArrayField(TEXT("retarget_op_types"), OpTypeValues);
    Report.Root->SetArrayField(TEXT("retarget_op_enabled_mask"), OpEnabledValues);
    Report.Root->SetNumberField(TEXT("run_ik_rig_op_count"), RunIKOpCount);
    Report.Root->SetBoolField(TEXT("run_ik_rig_enabled"), bExpectRunIKEnabled);
    Report.Root->SetBoolField(
        TEXT("run_ik_rig_disabled_for_fixed_presentation"),
        !bExpectRunIKEnabled);
    Report.Root->SetNumberField(
        TEXT("root_motion_op_count"), RootMotionOpCount);
    Report.Root->SetNumberField(
        TEXT("root_motion_op_index"), RootMotionOpIndex);
    Report.Root->SetBoolField(
        TEXT("root_motion_enabled"), bExpectRootMotionEnabled);
    Report.Root->SetBoolField(
        TEXT("root_motion_disabled_for_fixed_presentation"),
        !bExpectRootMotionEnabled);
    Report.Root->SetStringField(
        TEXT("root_motion_source_root_bone"),
        RootMotionSettings->SourceRoot.BoneName.ToString());
    Report.Root->SetStringField(
        TEXT("root_motion_target_root_bone"),
        RootMotionSettings->TargetRoot.BoneName.ToString());
    Report.Root->SetStringField(
        TEXT("root_motion_target_pelvis_bone"),
        RootMotionSettings->TargetPelvis.BoneName.ToString());
    Report.Root->SetStringField(
        TEXT("root_motion_source"),
        StaticEnum<ERootMotionSource>()->GetNameStringByValue(
            static_cast<int64>(RootMotionSettings->RootMotionSource)));
    Report.Root->SetStringField(
        TEXT("retarget_runtime_policy"),
        bExpectRunIKEnabled
            ? (bExpectRootMotionEnabled
                ? RetargetPolicyRunIKAndRootMotionEnabled
                : TEXT("PELVIS_FK_CURVES_ENABLED_ROOT_MOTION_DISABLED"))
            : (bExpectRootMotionEnabled
                ? RetargetPolicyRunIKDisabledRootMotionEnabled
                : RetargetPolicyRunIKAndRootMotionDisabled));
    for (UIKRigDefinition* AssignedTargetRig : Controller->GetAllTargetIKRigs())
    {
        if (AssignedTargetRig != Context.TargetIKRig)
        {
            Error = TEXT("A retarget op retained an unexpected target IK Rig");
            return false;
        }
    }

    TMap<FName, FName> ExpectedMappings;
    for (const FChainMapping& Pair : ChainMappings)
    {
        const FName TargetName(Pair.Target);
        const FName SourceName(Pair.Source);
        ExpectedMappings.Add(TargetName, SourceName);
        if (!ValidateBoneChain(Context.SourceIKRig, Context.SourceMesh, SourceName, Error)
            || !ValidateBoneChain(Context.TargetIKRig, Context.TargetMesh, TargetName, Error)
            || Controller->GetSourceChain(TargetName) != SourceName)
        {
            if (Error.IsEmpty())
            {
                Error = FString::Printf(
                    TEXT("Target chain %s is not explicitly mapped from %s"),
                    Pair.Target, Pair.Source);
            }
            return false;
        }
    }
    for (const FBoneChain& TargetChain : Context.TargetIKRig->GetRetargetChains())
    {
        const FName ActualSource = Controller->GetSourceChain(TargetChain.ChainName);
        const FName* ExpectedSource = ExpectedMappings.Find(TargetChain.ChainName);
        if ((ExpectedSource && ActualSource != *ExpectedSource)
            || (!ExpectedSource && !ActualSource.IsNone()))
        {
            Error = FString::Printf(
                TEXT("Target chain %s has an unexpected source mapping %s"),
                *TargetChain.ChainName.ToString(), *ActualSource.ToString());
            return false;
        }
    }

    if (Controller->GetCurrentRetargetPoseName(ERetargetSourceOrTarget::Target)
        != TargetRetargetPoseName)
    {
        Error = TEXT("Canonical target retarget pose is not DGMasterAligned");
        return false;
    }
    const TMap<FName, FIKRetargetPose>& TargetPoses =
        Controller->GetRetargetPoses(ERetargetSourceOrTarget::Target);
    const FIKRetargetPose* TargetPose = TargetPoses.Find(TargetRetargetPoseName);
    if (!TargetPose)
    {
        Error = TEXT("DGMasterAligned target retarget pose is missing");
        return false;
    }
    const FVector RootOffset = TargetPose->GetRootTranslationDelta();
    if (!FMath::IsFinite(RootOffset.X)
        || !FMath::IsFinite(RootOffset.Y)
        || !FMath::IsFinite(RootOffset.Z))
    {
        Error = TEXT("DGMasterAligned root offset is non-finite");
        return false;
    }
    const FReferenceSkeleton& TargetReference = Context.TargetMesh->GetRefSkeleton();
    for (const TPair<FName, FQuat>& Pair : TargetPose->GetAllDeltaRotations())
    {
        const FQuat& Rotation = Pair.Value;
        if (TargetReference.FindBoneIndex(Pair.Key) == INDEX_NONE
            || !FMath::IsFinite(Rotation.X)
            || !FMath::IsFinite(Rotation.Y)
            || !FMath::IsFinite(Rotation.Z)
            || !FMath::IsFinite(Rotation.W))
        {
            Error = FString::Printf(
                TEXT("DGMasterAligned contains an invalid offset for bone %s"),
                *Pair.Key.ToString());
            return false;
        }
    }

    FIKRetargetProcessor Processor;
    FRetargetInitParameters InitParameters;
    InitParameters.SourceSkeletalMesh = Context.SourceMesh;
    InitParameters.TargetSkeletalMesh = Context.TargetMesh;
    InitParameters.RetargeterAsset = Retargeter;
    InitParameters.bSuppressWarnings = false;
    Processor.Initialize(InitParameters);
    if (!Processor.IsInitialized()
        || !Processor.WasInitializedWithTheseAssets(
            Context.SourceMesh, Context.TargetMesh, Retargeter))
    {
        Error = TEXT("DG-to-MetaHuman retarget processor did not initialize against exact assets");
        return false;
    }

    TArray<FTransform> SourcePose = Processor.GetSkeleton(
        ERetargetSourceOrTarget::Source).RetargetPoses.GetGlobalRetargetPose();
    const TArray<FTransform> SourceRetargetPose = SourcePose;
    const TArray<FTransform> TargetRetargetPose = Processor.GetSkeleton(
        ERetargetSourceOrTarget::Target).RetargetPoses.GetGlobalRetargetPose();
    Processor.ApplySourceScaleToPose(SourcePose);
    FRetargetRunParameters RunParameters;
    RunParameters.SourceGlobalPose = &SourcePose;
    RunParameters.DeltaTime = 0.0f;
    const TArray<FTransform>& OutputPose = Processor.RunRetargeter(RunParameters);
    if (OutputPose.Num() != TargetReference.GetNum())
    {
        Error = FString::Printf(
            TEXT("Retarget output has %d bones; target mesh requires %d"),
            OutputPose.Num(), TargetReference.GetNum());
        return false;
    }
    constexpr double MaxPlausibleTranslationCm = 1000.0;
    double MaxAbsTranslationCm = 0.0;
    int32 TranslationOutlierCount = 0;
    for (int32 Index = 0; Index < OutputPose.Num(); ++Index)
    {
        if (!IsFiniteTransform(OutputPose[Index]))
        {
            Error = FString::Printf(TEXT("Retarget output bone %d is non-finite"), Index);
            return false;
        }
        const FVector Translation = OutputPose[Index].GetTranslation();
        const double BoneMaxAbsTranslation = FMath::Max3(
            FMath::Abs(Translation.X),
            FMath::Abs(Translation.Y),
            FMath::Abs(Translation.Z));
        MaxAbsTranslationCm = FMath::Max(
            MaxAbsTranslationCm, BoneMaxAbsTranslation);
        if (BoneMaxAbsTranslation > MaxPlausibleTranslationCm)
        {
            ++TranslationOutlierCount;
        }
    }

    TSharedRef<FJsonObject> OutputAnchors = MakeShared<FJsonObject>();
    const FName RequiredAnchorBones[] = {
        FName(TEXT("root")),
        FName(TEXT("pelvis")),
        FName(TEXT("spine_01")),
        FName(TEXT("head")),
    };
    for (const FName BoneName : RequiredAnchorBones)
    {
        const int32 BoneIndex = TargetReference.FindBoneIndex(BoneName);
        if (!OutputPose.IsValidIndex(BoneIndex))
        {
            Error = FString::Printf(
                TEXT("Retarget output is missing required anchor bone %s"),
                *BoneName.ToString());
            return false;
        }
        OutputAnchors->SetObjectField(
            BoneName.ToString(), TransformToJson(OutputPose[BoneIndex]));
    }
    Report.Root->SetNumberField(
        TEXT("retarget_output_max_abs_translation_cm"),
        MaxAbsTranslationCm);
    Report.Root->SetNumberField(
        TEXT("retarget_output_translation_outlier_count"),
        TranslationOutlierCount);
    Report.Root->SetObjectField(
        TEXT("retarget_output_anchor_transforms"), OutputAnchors);
    Report.Root->SetBoolField(
        TEXT("retarget_output_pose_plausible"),
        TranslationOutlierCount == 0
            && MaxAbsTranslationCm <= MaxPlausibleTranslationCm);
    if (!bExpectRootMotionEnabled)
    {
        const FReferenceSkeleton& SourceReference =
            Context.SourceMesh->GetRefSkeleton();
        const int32 SourcePelvisIndex =
            SourceReference.FindBoneIndex(FName(TEXT("pelvis")));
        const int32 TargetPelvisIndex =
            TargetReference.FindBoneIndex(FName(TEXT("pelvis")));
        if (!SourceRetargetPose.IsValidIndex(SourcePelvisIndex)
            || !SourcePose.IsValidIndex(SourcePelvisIndex)
            || !TargetRetargetPose.IsValidIndex(TargetPelvisIndex)
            || !OutputPose.IsValidIndex(TargetPelvisIndex))
        {
            Error = TEXT(
                "Corrected retarget output lacks the source/target pelvis anchors required by the proven Root Motion invariant");
            return false;
        }

        const FVector SourceRetargetPelvisTranslation =
            SourceRetargetPose[SourcePelvisIndex].GetTranslation();
        const FVector SourceInputPelvisTranslation =
            SourcePose[SourcePelvisIndex].GetTranslation();
        const FVector TargetRetargetPelvisTranslation =
            TargetRetargetPose[TargetPelvisIndex].GetTranslation();
        const FVector OutputPelvisTranslation =
            OutputPose[TargetPelvisIndex].GetTranslation();
        const bool bNeutralPelvisMotionSettingsConfirmed =
            PelvisMotionSettings->BlendToAbsoluteOffset.Equals(
                FVector::ZeroVector, UE_KINDA_SMALL_NUMBER)
            && FMath::IsNearlyZero(
                PelvisMotionSettings->FloorConstraintWeight,
                UE_KINDA_SMALL_NUMBER)
            && FMath::IsNearlyZero(
                PelvisMotionSettings->BlendToSourceTranslation,
                UE_KINDA_SMALL_NUMBER)
            && FMath::IsNearlyEqual(
                PelvisMotionSettings->ScaleHorizontal,
                1.0,
                UE_KINDA_SMALL_NUMBER)
            && FMath::IsNearlyEqual(
                PelvisMotionSettings->ScaleVertical,
                1.0,
                UE_KINDA_SMALL_NUMBER)
            && PelvisMotionSettings->TranslationOffsetGlobal.Equals(
                FVector::ZeroVector, UE_KINDA_SMALL_NUMBER)
            && PelvisMotionSettings->TranslationOffsetLocal.Equals(
                FVector::ZeroVector, UE_KINDA_SMALL_NUMBER)
            && FMath::IsNearlyEqual(
                PelvisMotionSettings->TranslationAlpha,
                1.0,
                UE_KINDA_SMALL_NUMBER)
            && FMath::Abs(SourceRetargetPelvisTranslation.Z)
                > UE_KINDA_SMALL_NUMBER;
        const double PelvisHeightRatio = bNeutralPelvisMotionSettingsConfirmed
            ? TargetRetargetPelvisTranslation.Z
                / SourceRetargetPelvisTranslation.Z
            : 0.0;
        const FVector ExpectedPelvisMotionTranslation =
            SourceInputPelvisTranslation * PelvisHeightRatio;
        const double OutputPelvisHeightFromTargetPoseCm = FMath::Abs(
            OutputPelvisTranslation.Z - TargetRetargetPelvisTranslation.Z);
        const double OutputPelvisFromExpectedPelvisMotionCm = FVector::Distance(
            OutputPelvisTranslation, ExpectedPelvisMotionTranslation);
        const bool bTargetPosePelvisExpected = FMath::Abs(
            TargetRetargetPelvisTranslation.Z - RetargetPelvisExpectedZCm)
            <= RetargetPelvisExpectedZToleranceCm;
        const bool bOutputPelvisRestoresTargetPoseHeight =
            OutputPelvisHeightFromTargetPoseCm
                <= RetargetPelvisHeightToleranceCm;
        const bool bOutputPelvisMatchesExpectedPelvisMotion =
            bNeutralPelvisMotionSettingsConfirmed
            && OutputPelvisFromExpectedPelvisMotionCm
                <= RetargetPelvisMotionToleranceCm;

        Report.Root->SetNumberField(
            TEXT("retarget_target_pose_pelvis_z_cm"),
            TargetRetargetPelvisTranslation.Z);
        Report.Root->SetNumberField(
            TEXT("retarget_output_pelvis_z_cm"),
            OutputPelvisTranslation.Z);
        Report.Root->SetNumberField(
            TEXT("retarget_expected_pelvis_motion_z_cm"),
            ExpectedPelvisMotionTranslation.Z);
        Report.Root->SetNumberField(
            TEXT("retarget_output_pelvis_height_from_target_pose_cm"),
            OutputPelvisHeightFromTargetPoseCm);
        Report.Root->SetNumberField(
            TEXT("retarget_output_pelvis_from_expected_pelvis_motion_cm"),
            OutputPelvisFromExpectedPelvisMotionCm);
        Report.Root->SetNumberField(
            TEXT("retarget_target_pose_pelvis_expected_z_cm"),
            RetargetPelvisExpectedZCm);
        Report.Root->SetNumberField(
            TEXT("retarget_target_pose_pelvis_z_tolerance_cm"),
            RetargetPelvisExpectedZToleranceCm);
        Report.Root->SetNumberField(
            TEXT("retarget_output_pelvis_height_tolerance_cm"),
            RetargetPelvisHeightToleranceCm);
        Report.Root->SetNumberField(
            TEXT("retarget_output_pelvis_motion_tolerance_cm"),
            RetargetPelvisMotionToleranceCm);
        Report.Root->SetBoolField(
            TEXT("retarget_pelvis_motion_neutral_settings_confirmed"),
            bNeutralPelvisMotionSettingsConfirmed);
        Report.Root->SetBoolField(
            TEXT("retarget_target_pose_pelvis_near_93_164_cm"),
            bTargetPosePelvisExpected);
        Report.Root->SetBoolField(
            TEXT("retarget_output_pelvis_restores_target_pose_height"),
            bOutputPelvisRestoresTargetPoseHeight);
        Report.Root->SetBoolField(
            TEXT("retarget_output_pelvis_matches_expected_pelvis_motion"),
            bOutputPelvisMatchesExpectedPelvisMotion);
        if (!bTargetPosePelvisExpected
            || !bOutputPelvisRestoresTargetPoseHeight
            || !bOutputPelvisMatchesExpectedPelvisMotion)
        {
            Error = FString::Printf(
                TEXT("Root-Motion-disabled pelvis invariant failed: target retarget Z %.6f cm, output Z %.6f cm, height delta %.6f cm, expected Pelvis Motion delta %.6f cm"),
                TargetRetargetPelvisTranslation.Z,
                OutputPelvisTranslation.Z,
                OutputPelvisHeightFromTargetPoseCm,
                OutputPelvisFromExpectedPelvisMotionCm);
            return false;
        }
    }
    if (!bExpectRunIKEnabled
        && (TranslationOutlierCount != 0
            || MaxAbsTranslationCm > MaxPlausibleTranslationCm))
    {
        Error = FString::Printf(
            TEXT("Retarget output has %d translation outliers and max |translation| %.3f cm"),
            TranslationOutlierCount,
            MaxAbsTranslationCm);
        return false;
    }

    Report.Root->SetBoolField(TEXT("retarget_processor_initialized"), true);
    Report.Root->SetNumberField(TEXT("retarget_output_bone_count"), OutputPose.Num());
    Report.Root->SetNumberField(TEXT("explicit_chain_mapping_count"),
        UE_ARRAY_COUNT(ChainMappings));
    return true;
}

bool AuthorRetargeter(FAssetContext& Context, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(
        FString(RetargeterPath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(
        FString(RetargeterPath));
    UPackage* Package = CreatePackage(*PackageName);
    UIKRetargetFactory* Factory = NewObject<UIKRetargetFactory>();
    Context.Retargeter = Cast<UIKRetargeter>(Factory->FactoryCreateNew(
        UIKRetargeter::StaticClass(),
        Package,
        FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional,
        nullptr,
        GWarn));
    if (!Context.Retargeter)
    {
        Error = TEXT("IK Retargeter factory failed at the canonical path");
        return false;
    }
    FAssetRegistryModule::AssetCreated(Context.Retargeter);

    UIKRetargeterController* Controller =
        UIKRetargeterController::GetController(Context.Retargeter);
    if (!Controller)
    {
        Error = TEXT("Could not acquire the UE 5.8 IK Retargeter controller");
        return false;
    }
    Controller->SetIKRig(ERetargetSourceOrTarget::Source, Context.SourceIKRig);
    Controller->SetIKRig(ERetargetSourceOrTarget::Target, Context.TargetIKRig);
    Controller->SetPreviewMesh(ERetargetSourceOrTarget::Source, Context.SourceMesh);
    Controller->SetPreviewMesh(ERetargetSourceOrTarget::Target, Context.TargetMesh);
    Controller->AddDefaultOps();
    int32 RunIKOpIndex = INDEX_NONE;
    int32 RunIKOpCount = 0;
    int32 RootMotionOpIndex = INDEX_NONE;
    int32 RootMotionOpCount = 0;
    for (int32 Index = 0; Index < Controller->GetNumRetargetOps(); ++Index)
    {
        FIKRetargetOpBase* Op = Controller->GetRetargetOpByIndex(Index);
        if (Op && Op->GetType() == FIKRetargetRunIKRigOp::StaticStruct())
        {
            RunIKOpIndex = Index;
            ++RunIKOpCount;
        }
        else if (Op && Op->GetType() == FIKRetargetRootMotionOp::StaticStruct())
        {
            RootMotionOpIndex = Index;
            ++RootMotionOpCount;
        }
    }
    if (RunIKOpCount != 1 || RunIKOpIndex != 2
        || RootMotionOpCount != 1 || RootMotionOpIndex != 3)
    {
        Error = FString::Printf(
            TEXT("Default retarget stack has Run IK/Root Motion counts %d/%d and indices %d/%d, expected 1/1 at 2/3"),
            RunIKOpCount,
            RootMotionOpCount,
            RunIKOpIndex,
            RootMotionOpIndex);
        return false;
    }
    if (!Controller->SetRetargetOpEnabled(RunIKOpIndex, false)
        || Controller->GetRetargetOpEnabled(RunIKOpIndex))
    {
        Error = TEXT("Could not disable the exact Run IK Rig op for fixed presentation");
        return false;
    }
    if (!Controller->SetRetargetOpEnabled(RootMotionOpIndex, false)
        || Controller->GetRetargetOpEnabled(RootMotionOpIndex))
    {
        Error = TEXT("Could not disable the exact Root Motion op for fixed presentation");
        return false;
    }
    Controller->AssignIKRigToAllOps(
        ERetargetSourceOrTarget::Source, Context.SourceIKRig);
    Controller->AssignIKRigToAllOps(
        ERetargetSourceOrTarget::Target, Context.TargetIKRig);
    Controller->AutoMapChains(EAutoMapChainType::Clear, true);
    for (const FChainMapping& Pair : ChainMappings)
    {
        if (!Controller->SetSourceChain(FName(Pair.Source), FName(Pair.Target)))
        {
            Error = FString::Printf(
                TEXT("Could not explicitly map target chain %s from %s"),
                Pair.Target, Pair.Source);
            return false;
        }
    }
    const FName CreatedPose = Controller->CreateRetargetPose(
        TargetRetargetPoseName, ERetargetSourceOrTarget::Target);
    if (CreatedPose != TargetRetargetPoseName
        || !Controller->SetCurrentRetargetPose(
            TargetRetargetPoseName, ERetargetSourceOrTarget::Target))
    {
        Error = TEXT("Could not create/select the exact DGMasterAligned target retarget pose");
        return false;
    }
    Controller->AutoAlignAllBones(ERetargetSourceOrTarget::Target);
    Controller->CleanAsset();
    Context.Retargeter->PostEditChange();
    Context.Retargeter->MarkPackageDirty();
    return true;
}

bool ValidateProfile(
    FAssetContext& Context,
    FRunReport& Report,
    bool bRequireAssetManagerRegistration,
    FString& Error)
{
    UDiscGolfAvatarBackendProfile* Profile = Context.BackendProfile;
    if (!Profile || Profile->GetPathName() != BackendProfilePath
        || Profile->BackendId
            != FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId)
        || Profile->Backend != EDGAvatarBackend::MetaHumanPreset
        || Profile->MetaHumanRuntimeMode
            != EDGMetaHumanRuntimeMode::ShippingSafeAssembled
        || !Profile->bUseRuntimeRetargeting
        || Profile->VisualBodyComponentTag != BodyTag
        || Profile->VisualHeadComponentTag != HeadTag
        || Profile->PreferredQualityProfileId != ExpectedQualityProfileId
        || Profile->bAllowRuntimeFaceSculpting)
    {
        Error = TEXT("Canonical MetaHuman backend profile fields differ");
        return false;
    }
    const FString ExpectedWrapperClassPath = Context.WrapperBlueprint
        && Context.WrapperBlueprint->GeneratedClass
        ? Context.WrapperBlueprint->GeneratedClass->GetPathName() : FString();
    if (ExpectedWrapperClassPath.IsEmpty()
        || Profile->VisualActorClass.ToSoftObjectPath().ToString()
            != ExpectedWrapperClassPath
        || Profile->RetargetAsset.ToSoftObjectPath().ToString() != RetargeterPath
        || Profile->VisualActorClass.LoadSynchronous()
            != Context.WrapperBlueprint->GeneratedClass
        || Profile->RetargetAsset.LoadSynchronous() != Context.Retargeter)
    {
        Error = TEXT("Backend profile soft references do not reload the exact wrapper class and retargeter");
        return false;
    }
    FString ContractReason;
    if (!DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
            Profile, ContractReason))
    {
        Error = ContractReason.IsEmpty()
            ? TEXT("Project MetaHuman profile contract rejected the canonical profile")
            : ContractReason;
        return false;
    }
    const FPrimaryAssetId ExpectedId(
        FPrimaryAssetType(ExpectedPrimaryAssetType), Profile->GetFName());
    if (Profile->GetPrimaryAssetId() != ExpectedId)
    {
        Error = FString::Printf(
            TEXT("Backend profile primary asset ID is %s, expected %s"),
            *Profile->GetPrimaryAssetId().ToString(), *ExpectedId.ToString());
        return false;
    }
    if (bRequireAssetManagerRegistration)
    {
        const FPrimaryAssetId RegisteredId = UAssetManager::Get().GetPrimaryAssetIdForPath(
            FSoftObjectPath(Profile));
        if (RegisteredId != ExpectedId)
        {
            Error = FString::Printf(
                TEXT("Asset Manager registered %s for the backend profile, expected %s"),
                *RegisteredId.ToString(), *ExpectedId.ToString());
            return false;
        }
    }
    Report.Root->SetBoolField(TEXT("profile_contract_verified"), true);
    Report.Root->SetStringField(TEXT("profile_contract_status"), ContractReason);
    Report.Root->SetStringField(
        TEXT("preferred_quality_profile_id"),
        Profile->PreferredQualityProfileId.ToString());
    Report.Root->SetStringField(TEXT("assembly_pipeline"), TEXT("UE_OPTIMIZED"));
    Report.Root->SetStringField(
        TEXT("assembly_optimization_level"), TEXT("MEDIUM"));
    return true;
}

bool AuthorProfile(FAssetContext& Context, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(
        FString(BackendProfilePath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(
        FString(BackendProfilePath));
    UPackage* Package = CreatePackage(*PackageName);
    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = UDiscGolfAvatarBackendProfile::StaticClass();
    Context.BackendProfile = Cast<UDiscGolfAvatarBackendProfile>(
        Factory->FactoryCreateNew(
            UDiscGolfAvatarBackendProfile::StaticClass(),
            Package,
            FName(*AssetName),
            RF_Public | RF_Standalone | RF_Transactional,
            nullptr,
            GWarn));
    if (!Context.BackendProfile)
    {
        Error = TEXT("Data Asset factory failed at the canonical backend-profile path");
        return false;
    }
    FAssetRegistryModule::AssetCreated(Context.BackendProfile);
    Context.BackendProfile->BackendId =
        FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId);
    Context.BackendProfile->Backend = EDGAvatarBackend::MetaHumanPreset;
    Context.BackendProfile->VisualActorClass = TSoftClassPtr<AActor>(
        FSoftObjectPath(Context.WrapperBlueprint->GeneratedClass));
    Context.BackendProfile->MetaHumanRuntimeMode =
        EDGMetaHumanRuntimeMode::ShippingSafeAssembled;
    Context.BackendProfile->RetargetAsset = TSoftObjectPtr<UObject>(
        FSoftObjectPath(Context.Retargeter));
    Context.BackendProfile->bUseRuntimeRetargeting = true;
    Context.BackendProfile->VisualBodyComponentTag = BodyTag;
    Context.BackendProfile->VisualHeadComponentTag = HeadTag;
    Context.BackendProfile->PreferredQualityProfileId = ExpectedQualityProfileId;
    Context.BackendProfile->bAllowRuntimeFaceSculpting = false;
    Context.BackendProfile->PostEditChange();
    Context.BackendProfile->MarkPackageDirty();
    return true;
}

UObject* GetObjectPropertyValue(UObject* Object, FName PropertyName)
{
    if (!Object)
    {
        return nullptr;
    }
    const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
        Object->GetClass(), PropertyName);
    return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
}

void GatherDirectObjectPropertyValues(UObject* Object, TArray<UObject*>& OutValues)
{
    if (!Object)
    {
        return;
    }
    for (TFieldIterator<FObjectPropertyBase> It(
            Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        UObject* Value = It->GetObjectPropertyValue_InContainer(Object);
        if (Value)
        {
            OutValues.Add(Value);
        }
    }
}

bool IsFacialHairRuntimePath(const FString& Path)
{
    return Path.Contains(TEXT("Beard"), ESearchCase::IgnoreCase)
        || Path.Contains(TEXT("Mustache"), ESearchCase::IgnoreCase)
        || Path.Contains(TEXT("Stubble"), ESearchCase::IgnoreCase);
}

bool ValidateFixedPresetAppearance(
    FAssetContext& Context,
    FRunReport& Report,
    FString& Error)
{
    int32 BeardGroomCount = 0;
    int32 MustacheGroomCount = 0;
    int32 BeardComponentCount = 0;
    int32 MustacheComponentCount = 0;
    int32 HairComponentCount = 0;
    int32 OutfitComponentCount = 0;
    int32 RuntimeBeardReferenceCount = 0;
    bool bHairCleanPresent = false;
    bool bOutfitPresent = false;
    bool bDirectSourceMHCReference = false;

    TArray<FNodeReference> Nodes;
    GatherSCSNodes(Context.WrapperBlueprint, Nodes);
    for (const FNodeReference& Reference : Nodes)
    {
        UActorComponent* Component = GetActualTemplate(
            Context.WrapperBlueprint, Reference);
        if (!Component || !Reference.Node)
        {
            continue;
        }
        const FString Variable = Reference.Node->GetVariableName().ToString();
        UObject* GroomAsset = GetObjectPropertyValue(Component, TEXT("GroomAsset"));
        if (Variable.Contains(TEXT("Beard"), ESearchCase::IgnoreCase))
        {
            ++BeardComponentCount;
            BeardGroomCount += GroomAsset ? 1 : 0;
        }
        if (Variable.Contains(TEXT("Mustache"), ESearchCase::IgnoreCase))
        {
            ++MustacheComponentCount;
            MustacheGroomCount += GroomAsset ? 1 : 0;
        }
        if (Variable.Equals(TEXT("Hair"), ESearchCase::IgnoreCase))
        {
            ++HairComponentCount;
            bHairCleanPresent |= GroomAsset
                && GroomAsset->GetPathName() == ExpectedHairGroomPath;
        }
        if (const USkeletalMeshComponent* Skeletal =
                Cast<USkeletalMeshComponent>(Component))
        {
            const USkeletalMesh* SkeletalMesh = Skeletal->GetSkeletalMeshAsset();
            if (SkeletalMesh && SkeletalMesh->GetPathName() == ExpectedOutfitMeshPath)
            {
                ++OutfitComponentCount;
                bOutfitPresent = true;
            }
        }

        TArray<UObject*> DirectReferences;
        GatherDirectObjectPropertyValues(Component, DirectReferences);
        for (UObject* ReferenceObject : DirectReferences)
        {
            const FString Path = ReferenceObject->GetPathName();
            RuntimeBeardReferenceCount += IsFacialHairRuntimePath(Path) ? 1 : 0;
            bDirectSourceMHCReference |=
                ReferenceObject->GetOutermost()->GetName() == SourceMHCPackageName;
        }
    }

    if (BeardComponentCount != 1 || MustacheComponentCount != 1
        || HairComponentCount != 1 || OutfitComponentCount != 1
        || BeardGroomCount != 0 || MustacheGroomCount != 0
        || RuntimeBeardReferenceCount != 0
        || !bHairCleanPresent || !bOutfitPresent || bDirectSourceMHCReference)
    {
        Error = FString::Printf(
            TEXT("Fixed preset differs: beard nodes=%d mustache nodes=%d hair nodes=%d outfit nodes=%d beard grooms=%d mustache grooms=%d facial-hair refs=%d hair clean=%s outfit=%s source ref=%s"),
            BeardComponentCount,
            MustacheComponentCount,
            HairComponentCount,
            OutfitComponentCount,
            BeardGroomCount,
            MustacheGroomCount,
            RuntimeBeardReferenceCount,
            bHairCleanPresent ? TEXT("true") : TEXT("false"),
            bOutfitPresent ? TEXT("true") : TEXT("false"),
            bDirectSourceMHCReference ? TEXT("true") : TEXT("false"));
        return false;
    }

    Report.Root->SetBoolField(TEXT("clean_shaven"), true);
    Report.Root->SetNumberField(TEXT("beard_component_count"), BeardComponentCount);
    Report.Root->SetNumberField(TEXT("mustache_component_count"), MustacheComponentCount);
    Report.Root->SetNumberField(TEXT("hair_component_count"), HairComponentCount);
    Report.Root->SetNumberField(TEXT("outfit_component_count"), OutfitComponentCount);
    Report.Root->SetNumberField(TEXT("beard_groom_asset_count"), BeardGroomCount);
    Report.Root->SetNumberField(TEXT("mustache_groom_asset_count"), MustacheGroomCount);
    Report.Root->SetNumberField(TEXT("runtime_beard_reference_count"),
        RuntimeBeardReferenceCount);
    Report.Root->SetBoolField(TEXT("hair_s_clean_present"), true);
    Report.Root->SetBoolField(TEXT("outfit_present"), true);
    return true;
}

bool ValidateRuntimeDependencyClosure(FRunReport& Report, FString& Error)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
        TEXT("AssetRegistry")).Get();
    TArray<FString> FilesToScan = {
        ObjectPathToFilename(WrapperBlueprintPath),
        ObjectPathToFilename(RetargeterPath),
        ObjectPathToFilename(BackendProfilePath),
    };
    Registry.ScanFilesSynchronous(FilesToScan, true);

    TSet<FName> Visited;
    TArray<FName> Pending = {
        FName(FPackageName::ObjectPathToPackageName(FString(WrapperBlueprintPath))),
        FName(FPackageName::ObjectPathToPackageName(FString(RetargeterPath))),
        FName(FPackageName::ObjectPathToPackageName(FString(BackendProfilePath))),
    };
    int32 BeardPackageReferenceCount = 0;
    bool bHairCleanDependency = false;
    bool bOutfitDependency = false;
    bool bSourceMHCDependency = false;
    while (!Pending.IsEmpty())
    {
        const FName Current = Pending.Pop(EAllowShrinking::No);
        if (Visited.Contains(Current))
        {
            continue;
        }
        Visited.Add(Current);
        if (Visited.Num() > 4096)
        {
            Error = TEXT("MetaHuman runtime dependency closure exceeded the bounded 4096-package gate");
            return false;
        }
        const FString CurrentString = Current.ToString();
        BeardPackageReferenceCount += IsFacialHairRuntimePath(CurrentString) ? 1 : 0;
        bHairCleanDependency |= CurrentString.Contains(
            TEXT("Hair_S_Clean"), ESearchCase::IgnoreCase);
        bOutfitDependency |= CurrentString.Contains(
                TEXT("DefaultGarment"), ESearchCase::IgnoreCase)
            || CurrentString.Contains(TEXT("Outfit"), ESearchCase::IgnoreCase);
        bSourceMHCDependency |= CurrentString == SourceMHCPackageName;

        TArray<FName> Dependencies;
        Registry.GetDependencies(
            Current,
            Dependencies,
            UE::AssetRegistry::EDependencyCategory::Package);
        for (const FName Dependency : Dependencies)
        {
            if (!Visited.Contains(Dependency))
            {
                Pending.Add(Dependency);
            }
        }
    }

    // The component-template proof is authoritative for generated/baked asset
    // names. The registry closure independently rejects editor wardrobe/source
    // roots and records whether their original names survive assembly.
    if (BeardPackageReferenceCount != 0 || bSourceMHCDependency
        || !bHairCleanDependency || !bOutfitDependency)
    {
        Error = FString::Printf(
            TEXT("Runtime closure retained %d facial-hair packages; source MHC dependency=%s hair clean=%s outfit=%s"),
            BeardPackageReferenceCount,
            bSourceMHCDependency ? TEXT("true") : TEXT("false"),
            bHairCleanDependency ? TEXT("true") : TEXT("false"),
            bOutfitDependency ? TEXT("true") : TEXT("false"));
        return false;
    }
    Report.Root->SetNumberField(TEXT("runtime_dependency_package_count"), Visited.Num());
    Report.Root->SetNumberField(TEXT("beard_package_reference_count"),
        BeardPackageReferenceCount);
    Report.Root->SetBoolField(TEXT("hair_s_clean_dependency_name_present"),
        bHairCleanDependency);
    Report.Root->SetBoolField(TEXT("outfit_dependency_name_present"),
        bOutfitDependency);
    Report.Root->SetBoolField(TEXT("source_mhc_editor_only"), true);
    return true;
}

const TCHAR* const CanonicalObjectPaths[] = {
    WrapperBlueprintPath,
    RetargeterPath,
    BackendProfilePath,
};

void ResetCanonicalPointers(FAssetContext& Context)
{
    Context.WrapperBlueprint = nullptr;
    Context.Retargeter = nullptr;
    Context.BackendProfile = nullptr;
}

bool AreAllCanonicalFilesPresent(int32& OutPresentCount)
{
    OutPresentCount = 0;
    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        OutPresentCount += IFileManager::Get().FileExists(
            *ObjectPathToFilename(ObjectPath)) ? 1 : 0;
    }
    return OutPresentCount == UE_ARRAY_COUNT(CanonicalObjectPaths);
}

bool SnapshotCanonicalPackages(
    TArray<FPackageSnapshot>& OutSnapshots,
    FString& Error)
{
    OutSnapshots.Reset();
    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        FPackageSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
        if (!SnapshotPackage(ObjectPath, Snapshot, Error))
        {
            return false;
        }
    }
    return true;
}

bool VerifyPackageSnapshotsUnchanged(
    const TArray<FPackageSnapshot>& Expected,
    FString& Error)
{
    TArray<FPackageSnapshot> Current;
    if (!SnapshotCanonicalPackages(Current, Error)
        || Current.Num() != Expected.Num())
    {
        return false;
    }
    for (int32 PackageIndex = 0; PackageIndex < Expected.Num(); ++PackageIndex)
    {
        const FPackageSnapshot& ExpectedPackage = Expected[PackageIndex];
        const FPackageSnapshot& CurrentPackage = Current[PackageIndex];
        if (ExpectedPackage.PackageName != CurrentPackage.PackageName
            || ExpectedPackage.Files.Num() != CurrentPackage.Files.Num())
        {
            Error = FString::Printf(
                TEXT("Canonical package sidecar set changed during no-write validation: %s"),
                *ExpectedPackage.PackageName);
            return false;
        }
        for (int32 FileIndex = 0; FileIndex < ExpectedPackage.Files.Num(); ++FileIndex)
        {
            const FFileSnapshot& ExpectedFile = ExpectedPackage.Files[FileIndex];
            const FFileSnapshot& CurrentFile = CurrentPackage.Files[FileIndex];
            if (ExpectedFile.Filename != CurrentFile.Filename
                || ExpectedFile.Bytes != CurrentFile.Bytes)
            {
                Error = FString::Printf(
                    TEXT("Canonical package bytes changed during no-write validation: %s"),
                    *ExpectedFile.Filename);
                return false;
            }
        }
    }
    return true;
}

bool HasOrphanCanonicalSidecars(
    const TArray<FPackageSnapshot>& Snapshots,
    FString& Error)
{
    for (const FPackageSnapshot& Snapshot : Snapshots)
    {
        const FString MainFilename = FPaths::ConvertRelativePathToFull(
            ObjectPathToFilename(*Snapshot.ObjectPath));
        const bool bHasMainFile = IFileManager::Get().FileExists(*MainFilename);
        if (!bHasMainFile && !Snapshot.Files.IsEmpty())
        {
            Error = FString::Printf(
                TEXT("Canonical package has orphan sidecars without its .uasset: %s"),
                *Snapshot.PackageName);
            return true;
        }
    }
    return false;
}

void ScanCanonicalFilesFromDisk()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
        TEXT("AssetRegistry")).Get();
    TArray<FString> ExistingFiles;
    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        const FString Filename = ObjectPathToFilename(ObjectPath);
        if (IFileManager::Get().FileExists(*Filename))
        {
            ExistingFiles.Add(Filename);
        }
    }
    if (!ExistingFiles.IsEmpty())
    {
        Registry.ScanFilesSynchronous(ExistingFiles, true);
    }
    UAssetManager::Get().RefreshPrimaryAssetDirectory(true);
}

bool LoadCanonicalAssets(FAssetContext& Context, FString& Error)
{
    Context.WrapperBlueprint = LoadExact<UBlueprint>(WrapperBlueprintPath, Error);
    Context.Retargeter = LoadExact<UIKRetargeter>(RetargeterPath, Error);
    Context.BackendProfile = LoadExact<UDiscGolfAvatarBackendProfile>(
        BackendProfilePath, Error);
    return Context.WrapperBlueprint && Context.Retargeter && Context.BackendProfile;
}

bool ValidateCanonicalPackagesClean(FString& Error)
{
    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        const FString PackageName = FPackageName::ObjectPathToPackageName(
            FString(ObjectPath));
        UPackage* Package = FindPackage(nullptr, *PackageName);
        if (!Package || Package->IsDirty())
        {
            Error = FString::Printf(
                TEXT("Canonical package is absent or dirty after validation: %s"),
                *PackageName);
            return false;
        }
    }
    return true;
}

bool UnloadCanonicalPackages(
    FAssetContext& Context,
    bool bUnloadDirtyPackages,
    FString& Error)
{
    TArray<UPackage*> Packages;
    TArray<FString> PackageNames;
    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        const FString PackageName = FPackageName::ObjectPathToPackageName(
            FString(ObjectPath));
        PackageNames.Add(PackageName);
        if (UPackage* Package = FindPackage(nullptr, *PackageName))
        {
            if (Package->IsDirty() && !bUnloadDirtyPackages)
            {
                Error = FString::Printf(
                    TEXT("Canonical package is dirty and cannot be used as disk evidence: %s"),
                    *PackageName);
                return false;
            }
            Packages.AddUnique(Package);
        }
    }

    ResetCanonicalPointers(Context);
    if (!Packages.IsEmpty())
    {
        UPackageTools::FUnloadPackageParams Params(Packages);
        Params.bUnloadDirtyPackages = bUnloadDirtyPackages;
        Params.bResetTransBuffer = true;
        UPackageTools::UnloadPackages(Params);
        for (const FString& PackageName : PackageNames)
        {
            if (FindPackage(nullptr, *PackageName))
            {
                Error = Params.OutErrorMessage.IsEmpty()
                    ? FString::Printf(
                        TEXT("Canonical package remained loaded after unload: %s"),
                        *PackageName)
                    : Params.OutErrorMessage.ToString();
                return false;
            }
        }
    }
    return true;
}

bool ReloadCanonicalAssetsFromDisk(
    FAssetContext& Context,
    FRunReport& Report,
    FString& Error)
{
    Report.Root->SetBoolField(TEXT("reload_from_disk_verified"), false);
    if (!UnloadCanonicalPackages(Context, false, Error))
    {
        return false;
    }
    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        if (!IFileManager::Get().FileExists(*ObjectPathToFilename(ObjectPath)))
        {
            Error = FString::Printf(
                TEXT("Canonical package file is absent before disk reload: %s"),
                ObjectPath);
            return false;
        }
    }
    Report.Root->SetBoolField(TEXT("canonical_packages_absent_before_reload"), true);
    if (!LoadCanonicalAssets(Context, Error))
    {
        return false;
    }
    Report.Root->SetBoolField(TEXT("reload_from_disk_verified"), true);
    Report.Root->SetNumberField(TEXT("reloaded_canonical_package_count"),
        UE_ARRAY_COUNT(CanonicalObjectPaths));
    return true;
}

bool ValidateCanonicalRegistry(FRunReport& Report, FString& Error)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
        TEXT("AssetRegistry")).Get();
    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        const FSoftObjectPath SoftPath(ObjectPath);
        const FAssetData AssetData = Registry.GetAssetByObjectPath(
            SoftPath, true, true);
        if (!AssetData.IsValid() || AssetData.GetSoftObjectPath() != SoftPath)
        {
            Error = FString::Printf(
                TEXT("Canonical package is not visible in the on-disk Asset Registry: %s"),
                ObjectPath);
            return false;
        }
    }
    Report.Root->SetNumberField(TEXT("on_disk_registry_asset_count"),
        UE_ARRAY_COUNT(CanonicalObjectPaths));
    return true;
}

bool ValidateAllCanonicalAssets(
    FAssetContext& Context,
    FRunReport& Report,
    bool bRequireDiskEvidence,
    bool bExpectRunIKEnabled,
    bool bExpectRootMotionEnabled,
    FString& Error)
{
    if (!ValidateWrapper(
            Context,
            Report,
            bRequireDiskEvidence,
            WrapperRelativeTransform,
            Error)
        || !ValidateRetargeter(
            Context,
            Report,
            bExpectRunIKEnabled,
            bExpectRootMotionEnabled,
            Error)
        || !ValidateProfile(Context, Report, bRequireDiskEvidence, Error)
        || !ValidateFixedPresetAppearance(Context, Report, Error))
    {
        return false;
    }
    if (bRequireDiskEvidence
        && (!ValidateCanonicalRegistry(Report, Error)
            || !ValidateRuntimeDependencyClosure(Report, Error)))
    {
        return false;
    }
    return true;
}

bool SaveCanonicalAsset(
    UObject* Asset,
    const TCHAR* ExpectedObjectPath,
    FRunReport& Report,
    FString& Error)
{
    if (!Asset || Asset->GetPathName() != ExpectedObjectPath
        || !Asset->GetOutermost())
    {
        Error = FString::Printf(
            TEXT("Refusing to save an invalid or non-canonical object for %s"),
            ExpectedObjectPath);
        return false;
    }
    const FString ExpectedPackageName = FPackageName::ObjectPathToPackageName(
        FString(ExpectedObjectPath));
    UPackage* Package = Asset->GetOutermost();
    if (Package->GetName() != ExpectedPackageName)
    {
        Error = FString::Printf(
            TEXT("Object %s is not outered to its exact canonical package"),
            ExpectedObjectPath);
        return false;
    }
    const FString Filename = ObjectPathToFilename(ExpectedObjectPath);
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
    {
        Error = FString::Printf(TEXT("Could not create canonical directory for %s"),
            *Filename);
        return false;
    }

    Report.AttemptedWrites.Add(ExpectedObjectPath);
    Report.SyncArrays();
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_None;
    SaveArgs.bSlowTask = false;
    if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
    {
        Error = FString::Printf(TEXT("UE SavePackage failed for %s"),
            ExpectedObjectPath);
        return false;
    }
    UPackage::WaitForAsyncFileWrites();
    if (!IFileManager::Get().FileExists(*Filename) || Package->IsDirty())
    {
        Error = FString::Printf(
            TEXT("Canonical package save did not produce a clean file: %s"),
            *Filename);
        return false;
    }
    return true;
}

bool RollbackCanonicalTransaction(
    FAssetContext& Context,
    const TArray<FPackageSnapshot>& Snapshots,
    FRunReport& Report,
    FString& Error)
{
    Report.Root->SetBoolField(TEXT("rollback_performed"), true);
    UObject* Assets[] = {
        Context.BackendProfile,
        Context.Retargeter,
        Context.WrapperBlueprint,
    };
    for (UObject* Asset : Assets)
    {
        if (Asset)
        {
            FAssetRegistryModule::AssetDeleted(Asset);
        }
    }

    FString UnloadError;
    const bool bUnloaded = UnloadCanonicalPackages(Context, true, UnloadError);
    FString RestoreError;
    const bool bRestored = RestorePackageSnapshots(Snapshots, RestoreError);
    ScanCanonicalFilesFromDisk();
    const bool bVerified = bUnloaded && bRestored;
    Report.Root->SetBoolField(TEXT("rollback_verified"), bVerified);
    Report.Root->SetStringField(
        TEXT("rollback_scope"), TEXT("ALL_FILES_WITH_EXACT_CANONICAL_PACKAGE_BASENAMES"));
    Report.Root->SetStringField(
        TEXT("disk_mutation"),
        bVerified
            ? TEXT("ROLLED_BACK_NO_PERSISTENT_CANONICAL_WRITES")
            : TEXT("ROLLBACK_FAILED"));
    if (!bVerified)
    {
        TArray<FString> Problems;
        if (!bUnloaded)
        {
            Problems.Add(UnloadError);
        }
        if (!bRestored)
        {
            Problems.Add(RestoreError);
        }
        Error = FString::Join(Problems, TEXT("; "));
    }
    return bVerified;
}

FString FailAuthorTransaction(
    FAssetContext& Context,
    const TArray<FPackageSnapshot>& Snapshots,
    FRunReport& Report,
    const FString& Cause)
{
    FString RollbackError;
    if (!RollbackCanonicalTransaction(Context, Snapshots, Report, RollbackError))
    {
        Report.Fail(FString::Printf(
            TEXT("%s; rollback failed: %s"), *Cause, *RollbackError));
    }
    else
    {
        Report.Fail(Cause);
    }
    return Report.Serialize();
}

FString RunPrepareRunIKCorrection()
{
    FRunReport Report(TEXT("PREPARE_RETARGET_RUN_IK_CORRECTION"));
    TArray<FPackageSnapshot> Snapshots;
    FString Error;
    if (!SnapshotCanonicalPackages(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    if (HasOrphanCanonicalSidecars(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    int32 PresentCount = 0;
    if (!AreAllCanonicalFilesPresent(PresentCount))
    {
        Report.Fail(FString::Printf(
            TEXT("Canonical asset state is incomplete: %d of 3 package files exist"),
            PresentCount));
        return Report.Serialize();
    }
    const TSet<FName> SavableDirtyPackagesBefore =
        CaptureSavableDirtyPackages();
    if (!SavableDirtyPackagesBefore.IsEmpty())
    {
        Report.Fail(TEXT(
            "Run IK correction refuses to begin with savable dirty packages"));
        return Report.Serialize();
    }
    const TSet<FName> DirtyPackagesBefore = CaptureDirtyPackages();
    Report.Root->SetNumberField(
        TEXT("preexisting_nonsavable_dirty_package_count"),
        DirtyPackagesBefore.Num());

    FAssetContext Context;
    if (!LoadDependencies(Context, Error)
        || !LoadCanonicalAssets(Context, Error)
        || !ValidateWrapper(
            Context,
            Report,
            false,
            WrapperRelativeTransform,
            Error)
        || !ValidateRetargeter(Context, Report, true, true, Error)
        || !ValidateProfile(Context, Report, false, Error)
        || !ValidateFixedPresetAppearance(Context, Report, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    Report.Root->SetBoolField(TEXT("run_ik_rig_enabled_before_correction"), true);

    UIKRetargeterController* Controller =
        UIKRetargeterController::GetController(Context.Retargeter);
    int32 RunIKOpIndex = INDEX_NONE;
    int32 RunIKOpCount = 0;
    if (Controller)
    {
        for (int32 Index = 0; Index < Controller->GetNumRetargetOps(); ++Index)
        {
            FIKRetargetOpBase* Op = Controller->GetRetargetOpByIndex(Index);
            if (Op && Op->GetType() == FIKRetargetRunIKRigOp::StaticStruct())
            {
                RunIKOpIndex = Index;
                ++RunIKOpCount;
            }
        }
    }
    if (!Controller || RunIKOpCount != 1 || RunIKOpIndex == INDEX_NONE
        || !Controller->SetRetargetOpEnabled(RunIKOpIndex, false)
        || Controller->GetRetargetOpEnabled(RunIKOpIndex))
    {
        Report.Fail(TEXT("Could not disable the exact canonical Run IK Rig op"));
        return Report.Serialize();
    }
    Context.Retargeter->PostEditChange();
    Context.Retargeter->MarkPackageDirty();

    const FName RetargetPackageName(
        FPackageName::ObjectPathToPackageName(FString(RetargeterPath)));
    const TSet<FName> AllowedDirtyPackages = {RetargetPackageName};
    bool bPrepared =
        ValidateAllCanonicalAssets(
            Context, Report, false, false, true, Error)
        && ValidateNoUnexpectedNewDirtyPackages(
            DirtyPackagesBefore, AllowedDirtyPackages, Error)
        && VerifyPackageSnapshotsUnchanged(Snapshots, Error);
    if (bPrepared)
    {
        const TSet<FName> AllDirtyPackagesAfter = CaptureDirtyPackages();
        bool bPreservedBroadDirtyBaseline =
            AllDirtyPackagesAfter.Num() == DirtyPackagesBefore.Num() + 1
            && AllDirtyPackagesAfter.Contains(RetargetPackageName);
        for (const FName BaselinePackage : DirtyPackagesBefore)
        {
            bPreservedBroadDirtyBaseline =
                bPreservedBroadDirtyBaseline
                && AllDirtyPackagesAfter.Contains(BaselinePackage);
        }

        const TSet<FName> SavableDirtyPackagesAfter =
            CaptureSavableDirtyPackages();
        bPrepared = bPreservedBroadDirtyBaseline
            && SavableDirtyPackagesAfter.Num() == 1
            && SavableDirtyPackagesAfter.Contains(RetargetPackageName)
            && Context.Retargeter->GetOutermost()->IsDirty();
        if (!bPrepared)
        {
            Error = TEXT(
                "Run IK correction did not add exactly the retargeter to the "
                "preserved dirty-package baseline");
        }
    }
    if (!bPrepared)
    {
        const FString Cause = Error.IsEmpty()
            ? TEXT("Run IK correction did not leave exactly the retargeter dirty")
            : Error;
        const bool bRestored =
            Controller->SetRetargetOpEnabled(RunIKOpIndex, true)
            && Controller->GetRetargetOpEnabled(RunIKOpIndex);
        Context.Retargeter->GetOutermost()->SetDirtyFlag(false);
        Report.Root->SetBoolField(TEXT("in_memory_rollback_performed"), true);
        Report.Root->SetBoolField(
            TEXT("in_memory_rollback_verified"), bRestored);
        Report.Fail(
            bRestored
                ? Cause
                : Cause + TEXT("; in-memory Run IK rollback failed"));
        return Report.Serialize();
    }

    Report.Root->SetBoolField(TEXT("retarget_correction_prepared"), true);
    Report.Root->SetBoolField(TEXT("retargeter_dirty_for_exact_save"), true);
    Report.Root->SetBoolField(TEXT("run_ik_rig_enabled_after_correction"), false);
    Report.Pass(
        TEXT("Prepared exactly one canonical retargeter package with Run IK Rig disabled; caller owns save and rollback"),
        TEXT("NONE_IN_MEMORY_ONLY"));
    return Report.Serialize();
}

FString RunPrepareRootMotionCorrection()
{
    FRunReport Report(TEXT("PREPARE_RETARGET_ROOT_MOTION_CORRECTION"));
    Report.Root->SetStringField(
        TEXT("retarget_runtime_policy_before_correction"),
        RetargetPolicyRunIKDisabledRootMotionEnabled);
    Report.Root->SetStringField(
        TEXT("retarget_runtime_policy_after_correction"),
        RetargetPolicyRunIKAndRootMotionDisabled);
    Report.Root->SetStringField(
        TEXT("root_motion_correction_scope"),
        TEXT("UNIQUE_INDEX_3_ROOT_MOTION_OP_ENABLED_STATE_ONLY_VIA_CONTROLLER"));

    auto MakeEnabledMask = [](bool bRootMotionEnabled)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueBoolean>(true));
        Values.Add(MakeShared<FJsonValueBoolean>(true));
        Values.Add(MakeShared<FJsonValueBoolean>(false));
        Values.Add(MakeShared<FJsonValueBoolean>(bRootMotionEnabled));
        Values.Add(MakeShared<FJsonValueBoolean>(true));
        return Values;
    };
    Report.Root->SetArrayField(
        TEXT("retarget_op_enabled_mask_before_correction"),
        MakeEnabledMask(true));
    Report.Root->SetArrayField(
        TEXT("retarget_op_enabled_mask_after_correction"),
        MakeEnabledMask(false));

    TArray<FPackageSnapshot> Snapshots;
    FString Error;
    if (!SnapshotCanonicalPackages(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    if (HasOrphanCanonicalSidecars(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    int32 PresentCount = 0;
    if (!AreAllCanonicalFilesPresent(PresentCount))
    {
        Report.Fail(FString::Printf(
            TEXT("Canonical asset state is incomplete: %d of 3 package files exist"),
            PresentCount));
        return Report.Serialize();
    }
    const TSet<FName> SavableDirtyPackagesBefore =
        CaptureSavableDirtyPackages();
    if (!SavableDirtyPackagesBefore.IsEmpty())
    {
        Report.Fail(TEXT(
            "Root Motion correction refuses to begin with savable dirty packages"));
        return Report.Serialize();
    }
    const TSet<FName> DirtyPackagesBefore = CaptureDirtyPackages();
    Report.Root->SetNumberField(
        TEXT("preexisting_nonsavable_dirty_package_count"),
        DirtyPackagesBefore.Num());

    FAssetContext Context;
    if (!LoadDependencies(Context, Error)
        || !LoadCanonicalAssets(Context, Error)
        || !ValidateWrapper(
            Context,
            Report,
            false,
            WrapperRelativeTransform,
            Error)
        || !ValidateRetargeter(Context, Report, false, true, Error)
        || !ValidateProfile(Context, Report, false, Error)
        || !ValidateFixedPresetAppearance(Context, Report, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }

    UIKRetargeterController* Controller =
        UIKRetargeterController::GetController(Context.Retargeter);
    int32 RootMotionOpIndex = INDEX_NONE;
    int32 RootMotionOpCount = 0;
    const FIKRetargetRootMotionOpSettings* RootMotionSettings = nullptr;
    if (Controller)
    {
        for (int32 Index = 0; Index < Controller->GetNumRetargetOps(); ++Index)
        {
            FIKRetargetOpBase* Op = Controller->GetRetargetOpByIndex(Index);
            if (Op && Op->GetType() == FIKRetargetRootMotionOp::StaticStruct())
            {
                ++RootMotionOpCount;
                RootMotionOpIndex = Index;
                RootMotionSettings = static_cast<
                    const FIKRetargetRootMotionOpSettings*>(
                        Op->GetSettingsConst());
            }
        }
    }
    Report.Root->SetNumberField(
        TEXT("root_motion_op_count_before_correction"), RootMotionOpCount);
    Report.Root->SetNumberField(
        TEXT("root_motion_op_index_before_correction"), RootMotionOpIndex);
    Report.Root->SetBoolField(
        TEXT("root_motion_enabled_before_correction"),
        RootMotionSettings && RootMotionSettings->bEnabled);
    if (!Controller || Controller->GetNumRetargetOps() != 5
        || RootMotionOpCount != 1 || RootMotionOpIndex != 3
        || !RootMotionSettings || !RootMotionSettings->bEnabled
        || !Controller->GetRetargetOpEnabled(RootMotionOpIndex)
        || RootMotionSettings->SourceRoot.BoneName != FName(TEXT("root"))
        || RootMotionSettings->TargetRoot.BoneName != FName(TEXT("pelvis"))
        || RootMotionSettings->TargetPelvis.BoneName != FName(TEXT("pelvis"))
        || RootMotionSettings->RootMotionSource
            != ERootMotionSource::CopyFromSourceRoot)
    {
        Report.Fail(TEXT(
            "Root Motion correction requires the exact pre-mask [true,true,false,true,true] and one enabled index-3 root-to-pelvis/pelvis CopyFromSourceRoot op"));
        return Report.Serialize();
    }
    Report.Root->SetBoolField(
        TEXT("root_motion_settings_verified_before_correction"), true);

    auto SameNameSet = [](const TSet<FName>& A, const TSet<FName>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }
        for (const FName Name : A)
        {
            if (!B.Contains(Name))
            {
                return false;
            }
        }
        return true;
    };
    auto RollbackInMemory = [&]()
    {
        Controller->SetRetargetOpEnabled(RootMotionOpIndex, true);
        const bool bExactPreMaskRestored =
            Controller->GetNumRetargetOps() == 5
            && Controller->GetRetargetOpEnabled(0)
            && Controller->GetRetargetOpEnabled(1)
            && !Controller->GetRetargetOpEnabled(2)
            && Controller->GetRetargetOpEnabled(3)
            && Controller->GetRetargetOpEnabled(4);
        Context.Retargeter->PostEditChange();
        Context.Retargeter->GetOutermost()->SetDirtyFlag(false);
        FString SnapshotError;
        const bool bDiskPreserved = VerifyPackageSnapshotsUnchanged(
            Snapshots, SnapshotError);
        const bool bDirtyBaselineRestored = SameNameSet(
            DirtyPackagesBefore, CaptureDirtyPackages())
            && CaptureSavableDirtyPackages().IsEmpty();
        const bool bRestored = bExactPreMaskRestored
            && bDiskPreserved
            && bDirtyBaselineRestored;
        Report.Root->SetBoolField(
            TEXT("in_memory_rollback_performed"), true);
        Report.Root->SetBoolField(
            TEXT("in_memory_rollback_verified"), bRestored);
        if (!bDiskPreserved)
        {
            Report.Root->SetStringField(
                TEXT("in_memory_rollback_disk_error"), SnapshotError);
        }
        return bRestored;
    };

    const bool bControllerToggleSucceeded =
        Controller->SetRetargetOpEnabled(RootMotionOpIndex, false)
        && !Controller->GetRetargetOpEnabled(RootMotionOpIndex)
        && !RootMotionSettings->bEnabled;
    Report.Root->SetBoolField(
        TEXT("root_motion_toggle_via_controller_only"),
        bControllerToggleSucceeded);
    if (!bControllerToggleSucceeded)
    {
        const bool bRestored = RollbackInMemory();
        Report.Fail(bRestored
            ? TEXT("Could not disable the exact canonical Root Motion op via its controller")
            : TEXT("Could not disable the exact canonical Root Motion op via its controller; in-memory rollback failed"));
        return Report.Serialize();
    }
    Context.Retargeter->PostEditChange();
    Context.Retargeter->MarkPackageDirty();

    const FName RetargetPackageName(
        FPackageName::ObjectPathToPackageName(FString(RetargeterPath)));
    const TSet<FName> AllowedDirtyPackages = {RetargetPackageName};
    bool bPrepared = ValidateAllCanonicalAssets(
            Context, Report, false, false, false, Error)
        && ValidateNoUnexpectedNewDirtyPackages(
            DirtyPackagesBefore, AllowedDirtyPackages, Error)
        && VerifyPackageSnapshotsUnchanged(Snapshots, Error);
    int32 RootMotionOpCountAfter = 0;
    int32 RootMotionOpIndexAfter = INDEX_NONE;
    for (int32 Index = 0; Index < Controller->GetNumRetargetOps(); ++Index)
    {
        FIKRetargetOpBase* Op = Controller->GetRetargetOpByIndex(Index);
        if (Op && Op->GetType() == FIKRetargetRootMotionOp::StaticStruct())
        {
            ++RootMotionOpCountAfter;
            RootMotionOpIndexAfter = Index;
        }
    }
    Report.Root->SetNumberField(
        TEXT("root_motion_op_count_after_correction"),
        RootMotionOpCountAfter);
    Report.Root->SetNumberField(
        TEXT("root_motion_op_index_after_correction"),
        RootMotionOpIndexAfter);
    Report.Root->SetBoolField(
        TEXT("root_motion_enabled_after_correction"),
        Controller->GetRetargetOpEnabled(RootMotionOpIndex));
    bPrepared = bPrepared
        && RootMotionOpCountAfter == 1
        && RootMotionOpIndexAfter == 3
        && !Controller->GetRetargetOpEnabled(RootMotionOpIndexAfter);
    if (bPrepared)
    {
        const TSet<FName> AllDirtyPackagesAfter = CaptureDirtyPackages();
        bool bPreservedBroadDirtyBaseline =
            AllDirtyPackagesAfter.Num() == DirtyPackagesBefore.Num() + 1
            && AllDirtyPackagesAfter.Contains(RetargetPackageName);
        for (const FName BaselinePackage : DirtyPackagesBefore)
        {
            bPreservedBroadDirtyBaseline =
                bPreservedBroadDirtyBaseline
                && AllDirtyPackagesAfter.Contains(BaselinePackage);
        }

        const TSet<FName> SavableDirtyPackagesAfter =
            CaptureSavableDirtyPackages();
        bPrepared = bPreservedBroadDirtyBaseline
            && SavableDirtyPackagesAfter.Num() == 1
            && SavableDirtyPackagesAfter.Contains(RetargetPackageName)
            && Context.Retargeter->GetOutermost()->IsDirty();
        if (!bPrepared)
        {
            Error = TEXT(
                "Root Motion correction did not add exactly the retargeter to the preserved dirty-package baseline");
        }
    }
    if (!bPrepared)
    {
        const FString Cause = Error.IsEmpty()
            ? TEXT(
                "Root Motion correction did not produce the exact post-mask [true,true,false,false,true]")
            : Error;
        const bool bRestored = RollbackInMemory();
        Report.Fail(bRestored
            ? Cause
            : Cause + TEXT("; in-memory Root Motion rollback failed"));
        return Report.Serialize();
    }

    Report.Root->SetBoolField(
        TEXT("corrected_pelvis_numerical_invariants_verified"), true);
    Report.Root->SetBoolField(
        TEXT("root_motion_correction_prepared"), true);
    Report.Root->SetBoolField(
        TEXT("retarget_correction_prepared"), true);
    Report.Root->SetBoolField(
        TEXT("retargeter_dirty_for_exact_save"), true);
    Report.Pass(
        TEXT("Prepared exactly one canonical retargeter package with Root Motion disabled and the 93.164 cm corrected pelvis invariant verified; caller owns save and rollback"),
        TEXT("NONE_IN_MEMORY_ONLY"));
    return Report.Serialize();
}

FString RunPrepareWrapperOrientationCorrection()
{
    FRunReport Report(TEXT("PREPARE_WRAPPER_ORIENTATION_CORRECTION"));
    Report.Root->SetStringField(
        TEXT("orientation_policy_before_correction"),
        LegacyOrientationPolicy);
    Report.Root->SetStringField(
        TEXT("orientation_policy_after_correction"),
        OrientationPolicy);
    Report.Root->SetObjectField(
        TEXT("wrapper_attachment_relative_transform_before_correction"),
        TransformToJson(LegacyWrapperRelativeTransform));
    Report.Root->SetObjectField(
        TEXT("wrapper_attachment_relative_transform_after_correction"),
        TransformToJson(WrapperRelativeTransform));
    Report.Root->SetStringField(
        TEXT("orientation_correction_scope"),
        TEXT("CONFIGURE_AND_APPLY_MAKE_TRANSFORM_ROTATION_DEFAULTS_ONLY"));

    TArray<FPackageSnapshot> Snapshots;
    FString Error;
    if (!SnapshotCanonicalPackages(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    if (HasOrphanCanonicalSidecars(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    int32 PresentCount = 0;
    if (!AreAllCanonicalFilesPresent(PresentCount))
    {
        Report.Fail(FString::Printf(
            TEXT("Canonical asset state is incomplete: %d of 3 package files exist"),
            PresentCount));
        return Report.Serialize();
    }
    const TSet<FName> SavableDirtyPackagesBefore =
        CaptureSavableDirtyPackages();
    if (!SavableDirtyPackagesBefore.IsEmpty())
    {
        Report.Fail(TEXT(
            "Wrapper orientation correction refuses to begin with savable dirty packages"));
        return Report.Serialize();
    }
    const TSet<FName> DirtyPackagesBefore = CaptureDirtyPackages();
    Report.Root->SetNumberField(
        TEXT("preexisting_nonsavable_dirty_package_count"),
        DirtyPackagesBefore.Num());

    FAssetContext Context;
    if (!LoadDependencies(Context, Error)
        || !LoadCanonicalAssets(Context, Error)
        || !ValidateWrapper(
            Context,
            Report,
            false,
            LegacyWrapperRelativeTransform,
            Error)
        || !ValidateRetargeter(Context, Report, false, true, Error)
        || !ValidateProfile(Context, Report, false, Error)
        || !ValidateFixedPresetAppearance(Context, Report, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    Report.Root->SetBoolField(
        TEXT("exact_legacy_wrapper_orientation_verified"), true);
    TArray<TSharedPtr<FJsonValue>> LegacyGraphs;
    LegacyGraphs.Add(MakeShared<FJsonValueString>(ConfigureFunctionName));
    LegacyGraphs.Add(MakeShared<FJsonValueString>(ApplyFunctionName));
    Report.Root->SetArrayField(
        TEXT("exact_legacy_wrapper_orientation_graphs_verified"),
        LegacyGraphs);

    auto RestoreLegacyOrientationInMemory = [&]() -> bool
    {
        TArray<FString> RestoreErrors;
        FString RestoreError;
        const bool bConfigureRestored = SetInterfaceGraphRotationDefault(
            Context.WrapperBlueprint,
            FName(ConfigureFunctionName),
            LegacyWrapperRelativeTransform.Rotator(),
            RestoreError);
        if (!bConfigureRestored)
        {
            RestoreErrors.Add(RestoreError);
        }
        RestoreError.Reset();
        const bool bApplyRestored = SetInterfaceGraphRotationDefault(
            Context.WrapperBlueprint,
            FName(ApplyFunctionName),
            LegacyWrapperRelativeTransform.Rotator(),
            RestoreError);
        if (!bApplyRestored)
        {
            RestoreErrors.Add(RestoreError);
        }
        if (bConfigureRestored && bApplyRestored)
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(Context.WrapperBlueprint);
            FKismetEditorUtilities::CompileBlueprint(
                Context.WrapperBlueprint,
                EBlueprintCompileOptions::SkipGarbageCollection);
        }
        const bool bCompiled = Context.WrapperBlueprint
            && Context.WrapperBlueprint->IsUpToDate()
            && Context.WrapperBlueprint->GeneratedClass;
        RestoreError.Reset();
        const bool bLegacyValidated = bCompiled
            && ValidateWrapper(
                Context,
                Report,
                false,
                LegacyWrapperRelativeTransform,
                RestoreError);
        if (!bLegacyValidated && !RestoreError.IsEmpty())
        {
            RestoreErrors.Add(RestoreError);
        }
        if (Context.WrapperBlueprint && Context.WrapperBlueprint->GetOutermost())
        {
            Context.WrapperBlueprint->GetOutermost()->SetDirtyFlag(false);
        }
        const bool bDirtyBaselineRestored =
            CaptureSavableDirtyPackages().IsEmpty();
        if (!bDirtyBaselineRestored)
        {
            RestoreErrors.Add(TEXT(
                "savable dirty-package baseline was not restored"));
        }
        Report.Root->SetBoolField(TEXT("in_memory_rollback_performed"), true);
        Report.Root->SetBoolField(
            TEXT("in_memory_rollback_verified"),
            RestoreErrors.IsEmpty());
        if (!RestoreErrors.IsEmpty())
        {
            Report.Root->SetStringField(
                TEXT("in_memory_rollback_error"),
                FString::Join(RestoreErrors, TEXT("; ")));
        }
        return RestoreErrors.IsEmpty();
    };

    bool bChanged = SetInterfaceGraphRotationDefault(
        Context.WrapperBlueprint,
        FName(ConfigureFunctionName),
        WrapperRelativeTransform.Rotator(),
        Error);
    if (bChanged)
    {
        bChanged = SetInterfaceGraphRotationDefault(
            Context.WrapperBlueprint,
            FName(ApplyFunctionName),
            WrapperRelativeTransform.Rotator(),
            Error);
    }
    if (bChanged)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Context.WrapperBlueprint);
        FKismetEditorUtilities::CompileBlueprint(
            Context.WrapperBlueprint,
            EBlueprintCompileOptions::SkipGarbageCollection);
        bChanged = Context.WrapperBlueprint->IsUpToDate()
            && Context.WrapperBlueprint->GeneratedClass;
        if (!bChanged)
        {
            Error = TEXT(
                "Canonical MetaHuman wrapper failed to compile after the exact orientation-default correction");
        }
    }
    if (bChanged)
    {
        Context.WrapperBlueprint->MarkPackageDirty();
    }

    const FName WrapperPackageName(
        FPackageName::ObjectPathToPackageName(FString(WrapperBlueprintPath)));
    const TSet<FName> AllowedDirtyPackages = {WrapperPackageName};
    bool bPrepared = bChanged
        && ValidateAllCanonicalAssets(
            Context, Report, false, false, true, Error)
        && ValidateNoUnexpectedNewDirtyPackages(
            DirtyPackagesBefore, AllowedDirtyPackages, Error)
        && VerifyPackageSnapshotsUnchanged(Snapshots, Error);
    if (bPrepared)
    {
        const TSet<FName> AllDirtyPackagesAfter = CaptureDirtyPackages();
        bool bPreservedBroadDirtyBaseline =
            AllDirtyPackagesAfter.Num() == DirtyPackagesBefore.Num() + 1
            && AllDirtyPackagesAfter.Contains(WrapperPackageName);
        for (const FName BaselinePackage : DirtyPackagesBefore)
        {
            bPreservedBroadDirtyBaseline =
                bPreservedBroadDirtyBaseline
                && AllDirtyPackagesAfter.Contains(BaselinePackage);
        }

        const TSet<FName> SavableDirtyPackagesAfter =
            CaptureSavableDirtyPackages();
        bPrepared = bPreservedBroadDirtyBaseline
            && SavableDirtyPackagesAfter.Num() == 1
            && SavableDirtyPackagesAfter.Contains(WrapperPackageName)
            && Context.WrapperBlueprint->GetOutermost()->IsDirty();
        if (!bPrepared)
        {
            Error = TEXT(
                "Wrapper orientation correction did not add exactly the wrapper to the preserved dirty-package baseline");
        }
    }
    if (!bPrepared)
    {
        const FString Cause = Error.IsEmpty()
            ? TEXT(
                "Wrapper orientation correction did not leave exactly the wrapper dirty")
            : Error;
        const bool bRestored = RestoreLegacyOrientationInMemory();
        Report.Fail(
            bRestored
                ? Cause
                : Cause + TEXT("; in-memory wrapper-orientation rollback failed"));
        return Report.Serialize();
    }

    TArray<TSharedPtr<FJsonValue>> CorrectedGraphs;
    CorrectedGraphs.Add(MakeShared<FJsonValueString>(ConfigureFunctionName));
    CorrectedGraphs.Add(MakeShared<FJsonValueString>(ApplyFunctionName));
    Report.Root->SetArrayField(
        TEXT("wrapper_orientation_graphs_corrected"), CorrectedGraphs);
    Report.Root->SetNumberField(
        TEXT("wrapper_rotation_default_change_count"), 2);
    Report.Root->SetBoolField(
        TEXT("wrapper_orientation_correction_prepared"), true);
    Report.Root->SetBoolField(TEXT("wrapper_dirty_for_exact_save"), true);
    Report.Pass(
        TEXT("Prepared exactly one canonical wrapper package with Configure and Apply relative yaw corrected from +90 degrees to identity; caller owns save and rollback"),
        TEXT("NONE_IN_MEMORY_ONLY"));
    return Report.Serialize();
}

FString RunRootMotionCounterfactualDiagnostic()
{
    FRunReport Report(TEXT("ROOT_MOTION_COUNTERFACTUAL_DIAGNOSTIC"));
    Report.Root->SetStringField(
        TEXT("diagnostic_schema"),
        TEXT("DiscGolfTour.Session8BMetaHumanRootMotionCounterfactual.v1"));
    Report.Root->SetStringField(
        TEXT("hypothesis"),
        TEXT("TARGET_IK_ROOT_IS_PELVIS_AND_LATE_ROOT_MOTION_COPY_FROM_SOURCE_ROOT_OVERWRITES_RETARGETED_PELVIS"));
    Report.Root->SetStringField(
        TEXT("counterfactual_delta"),
        TEXT("TRANSIENT_PROFILE_ONLY_ROOT_MOTION_BENABLED_TRUE_TO_FALSE"));
    Report.Root->SetStringField(
        TEXT("processor_asset_scope"),
        TEXT("TWO_INDEPENDENT_TRANSIENT_DUPLICATES_OF_CANONICAL_RETARGETER"));
    Report.Root->SetStringField(
        TEXT("cleanup_contract"),
        TEXT("STACK_PROFILES_AND_PROCESSORS_RELEASE_ON_RETURN;TRANSIENT_DUPLICATES_GC_ELIGIBLE;NO_CANONICAL_RESTORE_OR_SAVE_REQUIRED"));
    Report.Root->SetBoolField(TEXT("asset_save_attempted"), false);
    Report.Root->SetBoolField(TEXT("canonical_asset_mutation_attempted"), false);
    Report.Root->SetBoolField(TEXT("editor_retargeter_controller_acquired"), false);
    Report.Root->SetBoolField(TEXT("canonical_disk_bytes_preserved"), false);
    Report.Root->SetBoolField(TEXT("canonical_in_memory_settings_preserved"), false);
    Report.Root->SetBoolField(TEXT("dirty_package_baseline_preserved"), false);
    Report.Root->SetBoolField(TEXT("savable_dirty_package_baseline_preserved"), false);
    Report.Root->SetBoolField(TEXT("cleanup_requires_asset_restore"), false);

    const TSet<FName> DirtyPackagesBefore = CaptureDirtyPackages();
    const TSet<FName> SavableDirtyPackagesBefore = CaptureSavableDirtyPackages();
    auto SameNameSet = [](const TSet<FName>& A, const TSet<FName>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }
        for (const FName Name : A)
        {
            if (!B.Contains(Name))
            {
                return false;
            }
        }
        return true;
    };

    FString Error;
    TArray<FPackageSnapshot> CanonicalSnapshots;
    if (!SnapshotCanonicalPackages(CanonicalSnapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    Report.Root->SetNumberField(
        TEXT("canonical_package_snapshot_count"), CanonicalSnapshots.Num());

    auto FinishFailure = [
        &Report,
        &CanonicalSnapshots,
        &DirtyPackagesBefore,
        &SavableDirtyPackagesBefore,
        &SameNameSet](const FString& Cause)
    {
        FString DiskError;
        const bool bDiskPreserved = VerifyPackageSnapshotsUnchanged(
            CanonicalSnapshots, DiskError);
        const bool bDirtyPreserved = SameNameSet(
            DirtyPackagesBefore, CaptureDirtyPackages());
        const bool bSavableDirtyPreserved = SameNameSet(
            SavableDirtyPackagesBefore, CaptureSavableDirtyPackages());
        Report.Root->SetBoolField(
            TEXT("canonical_disk_bytes_preserved"), bDiskPreserved);
        Report.Root->SetBoolField(
            TEXT("dirty_package_baseline_preserved"), bDirtyPreserved);
        Report.Root->SetBoolField(
            TEXT("savable_dirty_package_baseline_preserved"),
            bSavableDirtyPreserved);
        Report.Root->SetBoolField(TEXT("diagnostic_completed"), false);

        FString Failure = Cause;
        if (!bDiskPreserved)
        {
            Failure += TEXT("; canonical disk preservation failed: ") + DiskError;
        }
        if (!bDirtyPreserved || !bSavableDirtyPreserved)
        {
            Failure += TEXT("; diagnostic changed the dirty-package baseline");
        }
        Report.Fail(Failure);
        return Report.Serialize();
    };

    FAssetContext Context;
    if (!LoadDependencies(Context, Error)
        || !LoadCanonicalAssets(Context, Error))
    {
        return FinishFailure(Error);
    }
    if (!Context.Retargeter
        || Context.Retargeter->GetIKRig(ERetargetSourceOrTarget::Source)
            != Context.SourceIKRig
        || Context.Retargeter->GetIKRig(ERetargetSourceOrTarget::Target)
            != Context.TargetIKRig)
    {
        return FinishFailure(
            TEXT("Canonical retargeter does not reference the exact source and target IK rigs"));
    }

    const FName SourceRigRoot = Context.SourceIKRig->GetRoot();
    const FName SourceRigPelvis = Context.SourceIKRig->GetPelvis();
    const FName TargetRigRoot = Context.TargetIKRig->GetRoot();
    const FName TargetRigPelvis = Context.TargetIKRig->GetPelvis();
    TSharedRef<FJsonObject> RigIdentity = MakeShared<FJsonObject>();
    RigIdentity->SetStringField(TEXT("source_root"), SourceRigRoot.ToString());
    RigIdentity->SetStringField(TEXT("source_pelvis"), SourceRigPelvis.ToString());
    RigIdentity->SetStringField(TEXT("target_root"), TargetRigRoot.ToString());
    RigIdentity->SetStringField(TEXT("target_pelvis"), TargetRigPelvis.ToString());
    RigIdentity->SetBoolField(
        TEXT("target_root_equals_target_pelvis"),
        TargetRigRoot == TargetRigPelvis);
    Report.Root->SetObjectField(TEXT("ik_rig_retarget_definition"), RigIdentity);
    if (SourceRigRoot != FName(TEXT("root"))
        || SourceRigPelvis != FName(TEXT("pelvis"))
        || TargetRigRoot != FName(TEXT("pelvis"))
        || TargetRigPelvis != FName(TEXT("pelvis")))
    {
        return FinishFailure(FString::Printf(
            TEXT("Root-motion counterfactual requires source root/pelvis root,pelvis and target root/pelvis pelvis,pelvis; got %s,%s and %s,%s"),
            *SourceRigRoot.ToString(),
            *SourceRigPelvis.ToString(),
            *TargetRigRoot.ToString(),
            *TargetRigPelvis.ToString()));
    }

    auto VectorToJson = [](const FVector& Value)
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("x"), Value.X);
        Result->SetNumberField(TEXT("y"), Value.Y);
        Result->SetNumberField(TEXT("z"), Value.Z);
        return Result;
    };
    auto RootSettingsToJson = [](const FIKRetargetRootMotionOpSettings& Settings)
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(
            TEXT("settings_type"),
            FIKRetargetRootMotionOpSettings::StaticStruct()->GetPathName());
        Result->SetBoolField(TEXT("enabled"), Settings.bEnabled);
        Result->SetNumberField(TEXT("lod_threshold"), Settings.LODThreshold);
        Result->SetBoolField(TEXT("debug_draw"), Settings.bDebugDraw);
        Result->SetStringField(
            TEXT("source_root_bone"), Settings.SourceRoot.BoneName.ToString());
        Result->SetStringField(
            TEXT("target_root_bone"), Settings.TargetRoot.BoneName.ToString());
        Result->SetStringField(
            TEXT("target_pelvis_bone"), Settings.TargetPelvis.BoneName.ToString());
        Result->SetStringField(
            TEXT("root_motion_source"),
            StaticEnum<ERootMotionSource>()->GetNameStringByValue(
                static_cast<int64>(Settings.RootMotionSource)));
        Result->SetStringField(
            TEXT("root_height_source"),
            StaticEnum<ERootMotionHeightSource>()->GetNameStringByValue(
                static_cast<int64>(Settings.RootHeightSource)));
        Result->SetObjectField(
            TEXT("global_offset"), ExactTransformToJson(Settings.GlobalOffset));
        Result->SetBoolField(
            TEXT("maintain_offset_from_pelvis"),
            Settings.bMaintainOffsetFromPelvis);
        Result->SetBoolField(
            TEXT("rotate_with_pelvis"), Settings.bRotateWithPelvis);
        Result->SetBoolField(
            TEXT("propagate_to_non_retargeted_children"),
            Settings.bPropagateToNonRetargetedChildren);
        return Result;
    };
    auto PelvisSettingsToJson = [&VectorToJson](
        const FIKRetargetPelvisMotionOpSettings& Settings)
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(
            TEXT("settings_type"),
            FIKRetargetPelvisMotionOpSettings::StaticStruct()->GetPathName());
        Result->SetBoolField(TEXT("enabled"), Settings.bEnabled);
        Result->SetNumberField(TEXT("lod_threshold"), Settings.LODThreshold);
        Result->SetStringField(
            TEXT("source_pelvis_bone"),
            Settings.SourcePelvisBone.BoneName.ToString());
        Result->SetStringField(
            TEXT("target_pelvis_bone"),
            Settings.TargetPelvisBone.BoneName.ToString());
        Result->SetNumberField(
            TEXT("translation_alpha"), Settings.TranslationAlpha);
        Result->SetNumberField(
            TEXT("scale_horizontal"), Settings.ScaleHorizontal);
        Result->SetNumberField(
            TEXT("scale_vertical"), Settings.ScaleVertical);
        Result->SetObjectField(
            TEXT("blend_to_absolute_offset"),
            VectorToJson(Settings.BlendToAbsoluteOffset));
        Result->SetNumberField(
            TEXT("blend_to_source_translation"),
            Settings.BlendToSourceTranslation);
        Result->SetObjectField(
            TEXT("blend_to_source_translation_weights"),
            VectorToJson(Settings.BlendToSourceTranslationWeights));
        Result->SetObjectField(
            TEXT("translation_offset_local"),
            VectorToJson(Settings.TranslationOffsetLocal));
        Result->SetObjectField(
            TEXT("translation_offset_global"),
            VectorToJson(Settings.TranslationOffsetGlobal));
        Result->SetNumberField(
            TEXT("floor_constraint_weight"), Settings.FloorConstraintWeight);
        Result->SetNumberField(
            TEXT("source_crotch_offset"), Settings.SourceCrotchOffset);
        Result->SetNumberField(
            TEXT("target_crotch_offset"), Settings.TargetCrotchOffset);
        Result->SetBoolField(
            TEXT("use_ground_falloff"), Settings.bUseGroundFalloff);
        Result->SetNumberField(
            TEXT("ground_falloff_height_percent"),
            Settings.GroundFalloffHeightPercent);
        Result->SetNumberField(
            TEXT("rotation_alpha"), Settings.RotationAlpha);
        Result->SetNumberField(
            TEXT("affect_ik_horizontal"), Settings.AffectIKHorizontal);
        Result->SetNumberField(
            TEXT("affect_ik_vertical"), Settings.AffectIKVertical);
        return Result;
    };

    const TArray<FInstancedStruct>& AssetOps = Context.Retargeter->GetRetargetOps();
    const FIKRetargetRootMotionOpSettings* AssetRootSettings = nullptr;
    const FIKRetargetPelvisMotionOpSettings* AssetPelvisSettings = nullptr;
    FName RootOpName = NAME_None;
    int32 RootOpIndex = INDEX_NONE;
    int32 PelvisOpIndex = INDEX_NONE;
    int32 RootOpCount = 0;
    int32 PelvisOpCount = 0;
    TArray<TSharedPtr<FJsonValue>> OpIdentities;
    for (int32 OpIndex = 0; OpIndex < AssetOps.Num(); ++OpIndex)
    {
        const FIKRetargetOpBase* Op =
            AssetOps[OpIndex].GetPtr<FIKRetargetOpBase>();
        if (!Op || !Op->GetType()
            || !Op->GetSettingsConst() || !Op->GetSettingsType())
        {
            return FinishFailure(FString::Printf(
                TEXT("Canonical retarget op %d is invalid"), OpIndex));
        }
        TSharedRef<FJsonObject> Identity = MakeShared<FJsonObject>();
        Identity->SetNumberField(TEXT("index"), OpIndex);
        Identity->SetStringField(TEXT("op_name"), Op->GetName().ToString());
        Identity->SetStringField(TEXT("op_type"), Op->GetType()->GetPathName());
        Identity->SetStringField(
            TEXT("settings_type"), Op->GetSettingsType()->GetPathName());
        Identity->SetBoolField(TEXT("enabled"), Op->IsEnabled());
        Identity->SetNumberField(
            TEXT("lod_threshold"), Op->GetSettingsConst()->LODThreshold);
        OpIdentities.Add(MakeShared<FJsonValueObject>(Identity));

        if (Op->GetType() == FIKRetargetRootMotionOp::StaticStruct())
        {
            ++RootOpCount;
            RootOpIndex = OpIndex;
            RootOpName = Op->GetName();
            AssetRootSettings = static_cast<
                const FIKRetargetRootMotionOpSettings*>(
                    Op->GetSettingsConst());
        }
        else if (Op->GetType() == FIKRetargetPelvisMotionOp::StaticStruct())
        {
            ++PelvisOpCount;
            PelvisOpIndex = OpIndex;
            AssetPelvisSettings = static_cast<
                const FIKRetargetPelvisMotionOpSettings*>(
                    Op->GetSettingsConst());
        }
    }
    Report.Root->SetArrayField(TEXT("canonical_op_stack"), OpIdentities);
    Report.Root->SetNumberField(TEXT("canonical_op_count"), AssetOps.Num());
    Report.Root->SetNumberField(TEXT("pelvis_motion_op_index"), PelvisOpIndex);
    Report.Root->SetNumberField(TEXT("root_motion_op_index"), RootOpIndex);
    Report.Root->SetBoolField(
        TEXT("pelvis_motion_executes_before_root_motion"),
        PelvisOpIndex != INDEX_NONE && RootOpIndex > PelvisOpIndex);
    if (RootOpCount != 1 || PelvisOpCount != 1
        || !AssetRootSettings || !AssetPelvisSettings
        || PelvisOpIndex >= RootOpIndex)
    {
        return FinishFailure(FString::Printf(
            TEXT("Expected one Pelvis Motion op before one Root Motion op; got pelvis=%d root=%d"),
            PelvisOpCount, RootOpCount));
    }
    Report.Root->SetObjectField(
        TEXT("canonical_asset_root_motion_settings"),
        RootSettingsToJson(*AssetRootSettings));
    Report.Root->SetObjectField(
        TEXT("canonical_asset_pelvis_motion_settings"),
        PelvisSettingsToJson(*AssetPelvisSettings));
    Report.Root->SetBoolField(
        TEXT("pelvis_scale_vertical_precedes_late_root_overwrite"), true);

    FRetargetProfile CanonicalProfile;
    CanonicalProfile.FillProfileWithAssetSettings(Context.Retargeter);
    TArray<FIKRetargetRootMotionOpSettings*> CanonicalRootSettingsArray;
    TArray<FIKRetargetPelvisMotionOpSettings*> CanonicalPelvisSettingsArray;
    CanonicalProfile.GetOpSettingsByTypeInProfile(CanonicalRootSettingsArray);
    CanonicalProfile.GetOpSettingsByTypeInProfile(CanonicalPelvisSettingsArray);
    if (CanonicalRootSettingsArray.Num() != 1
        || CanonicalPelvisSettingsArray.Num() != 1)
    {
        return FinishFailure(FString::Printf(
            TEXT("Merged canonical profile has root/pelvis settings counts %d/%d, expected 1/1"),
            CanonicalRootSettingsArray.Num(),
            CanonicalPelvisSettingsArray.Num()));
    }
    FIKRetargetRootMotionOpSettings* const CanonicalRootSettings =
        CanonicalRootSettingsArray[0];
    FIKRetargetPelvisMotionOpSettings* const CanonicalPelvisSettings =
        CanonicalPelvisSettingsArray[0];
    int32 RootProfileIndex = INDEX_NONE;
    for (int32 ProfileIndex = 0;
         ProfileIndex < CanonicalProfile.RetargetOpProfiles.Num();
         ++ProfileIndex)
    {
        if (CanonicalProfile.RetargetOpProfiles[ProfileIndex]
                .SettingsToApply.GetScriptStruct()
            == FIKRetargetRootMotionOpSettings::StaticStruct())
        {
            RootProfileIndex = ProfileIndex;
            break;
        }
    }
    if (RootProfileIndex == INDEX_NONE
        || CanonicalProfile.RetargetOpProfiles[RootProfileIndex]
                .OpToApplySettingsTo
            != RootOpName)
    {
        return FinishFailure(
            TEXT("Merged canonical profile does not identify the exact Root Motion op by name"));
    }

    Report.Root->SetStringField(
        TEXT("canonical_source_retarget_pose_name"),
        CanonicalProfile.SourceRetargetPoseName.ToString());
    Report.Root->SetStringField(
        TEXT("canonical_target_retarget_pose_name"),
        CanonicalProfile.TargetRetargetPoseName.ToString());
    Report.Root->SetBoolField(
        TEXT("canonical_profile_applies_source_retarget_pose"),
        CanonicalProfile.bApplySourceRetargetPose);
    Report.Root->SetBoolField(
        TEXT("canonical_profile_applies_target_retarget_pose"),
        CanonicalProfile.bApplyTargetRetargetPose);
    Report.Root->SetNumberField(
        TEXT("canonical_profile_op_settings_count"),
        CanonicalProfile.RetargetOpProfiles.Num());
    Report.Root->SetNumberField(
        TEXT("canonical_root_motion_profile_index"), RootProfileIndex);
    Report.Root->SetStringField(
        TEXT("canonical_root_motion_profile_op_name"),
        CanonicalProfile.RetargetOpProfiles[RootProfileIndex]
            .OpToApplySettingsTo.ToString());
    Report.Root->SetObjectField(
        TEXT("baseline_profile_root_motion_settings"),
        RootSettingsToJson(*CanonicalRootSettings));
    Report.Root->SetObjectField(
        TEXT("baseline_profile_pelvis_motion_settings"),
        PelvisSettingsToJson(*CanonicalPelvisSettings));

    if (!CanonicalProfile.bApplySourceRetargetPose
        || !CanonicalProfile.bApplyTargetRetargetPose
        || CanonicalProfile.TargetRetargetPoseName != TargetRetargetPoseName
        || !CanonicalRootSettings->bEnabled
        || CanonicalRootSettings->SourceRoot.BoneName != SourceRigRoot
        || CanonicalRootSettings->TargetRoot.BoneName != TargetRigRoot
        || CanonicalRootSettings->TargetPelvis.BoneName != TargetRigPelvis
        || CanonicalRootSettings->RootMotionSource
            != ERootMotionSource::CopyFromSourceRoot
        || CanonicalRootSettings->RootHeightSource
            != ERootMotionHeightSource::CopyHeightFromSource
        || !CanonicalRootSettings->GlobalOffset.Equals(
            FTransform::Identity, UE_KINDA_SMALL_NUMBER))
    {
        return FinishFailure(
            TEXT("Canonical merged profile does not satisfy the enabled CopyFromSourceRoot pelvis-targeting hypothesis"));
    }

    auto ProfilesIdentical = [](const FRetargetProfile& A,
                                const FRetargetProfile& B)
    {
        if (A.bApplyTargetRetargetPose != B.bApplyTargetRetargetPose
            || A.TargetRetargetPoseName != B.TargetRetargetPoseName
            || A.bApplySourceRetargetPose != B.bApplySourceRetargetPose
            || A.SourceRetargetPoseName != B.SourceRetargetPoseName
            || A.bForceAllIKOff != B.bForceAllIKOff
            || A.RetargetOpProfiles.Num() != B.RetargetOpProfiles.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < A.RetargetOpProfiles.Num(); ++Index)
        {
            const FRetargetOpProfile& AOp = A.RetargetOpProfiles[Index];
            const FRetargetOpProfile& BOp = B.RetargetOpProfiles[Index];
            if (AOp.OpToApplySettingsTo != BOp.OpToApplySettingsTo
                || AOp.SettingsToApply != BOp.SettingsToApply)
            {
                return false;
            }
        }
        return true;
    };

    FRetargetProfile RootOffProfile = CanonicalProfile;
    TArray<FIKRetargetRootMotionOpSettings*> RootOffSettingsArray;
    RootOffProfile.GetOpSettingsByTypeInProfile(RootOffSettingsArray);
    if (RootOffSettingsArray.Num() != 1)
    {
        return FinishFailure(
            TEXT("Transient Root-off profile does not contain exactly one Root Motion settings copy"));
    }
    FIKRetargetRootMotionOpSettings* const RootOffSettings =
        RootOffSettingsArray[0];
    RootOffSettings->bEnabled = false;
    const bool bRootOffRequested = !RootOffSettings->bEnabled;
    RootOffSettings->bEnabled = CanonicalRootSettings->bEnabled;
    const bool bProfilesEqualWhenEnabledRestored = ProfilesIdentical(
        CanonicalProfile, RootOffProfile);
    RootOffSettings->bEnabled = false;
    const bool bOnlyRootEnabledChanged = bRootOffRequested
        && bProfilesEqualWhenEnabledRestored
        && RootOffProfile.RetargetOpProfiles[RootProfileIndex]
                .OpToApplySettingsTo
            == RootOpName;
    Report.Root->SetBoolField(
        TEXT("transient_profile_only_root_enabled_changed"),
        bOnlyRootEnabledChanged);
    Report.Root->SetObjectField(
        TEXT("counterfactual_profile_root_motion_settings"),
        RootSettingsToJson(*RootOffSettings));
    if (!bOnlyRootEnabledChanged)
    {
        return FinishFailure(
            TEXT("Could not prove Root Motion bEnabled is the transient profile's sole delta"));
    }

    UIKRetargeter* const BaselineRetargeter = DuplicateObject<UIKRetargeter>(
        Context.Retargeter,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UIKRetargeter::StaticClass(),
            FName(TEXT("Session8BRootMotionBaseline"))));
    UIKRetargeter* const RootOffRetargeter = DuplicateObject<UIKRetargeter>(
        Context.Retargeter,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UIKRetargeter::StaticClass(),
            FName(TEXT("Session8BRootMotionOff"))));
    if (!BaselineRetargeter || !RootOffRetargeter
        || BaselineRetargeter == Context.Retargeter
        || RootOffRetargeter == Context.Retargeter
        || BaselineRetargeter->GetOutermost() != GetTransientPackage()
        || RootOffRetargeter->GetOutermost() != GetTransientPackage())
    {
        return FinishFailure(
            TEXT("Could not isolate both processors on transient retargeter duplicates"));
    }
    BaselineRetargeter->ClearFlags(RF_Public | RF_Standalone);
    RootOffRetargeter->ClearFlags(RF_Public | RF_Standalone);
    BaselineRetargeter->SetFlags(RF_Transient);
    RootOffRetargeter->SetFlags(RF_Transient);
    Report.Root->SetBoolField(TEXT("transient_retargeter_duplicates_created"), true);

    FIKRetargetProcessor BaselineProcessor;
    FIKRetargetProcessor RootOffProcessor;
    auto InitializeProcessor = [&Context](
        FIKRetargetProcessor& Processor,
        UIKRetargeter* Retargeter,
        const FRetargetProfile& Profile)
    {
        FRetargetInitParameters InitParameters;
        InitParameters.SourceSkeletalMesh = Context.SourceMesh;
        InitParameters.TargetSkeletalMesh = Context.TargetMesh;
        InitParameters.RetargeterAsset = Retargeter;
        InitParameters.CustomProfile = &Profile;
        InitParameters.bSuppressWarnings = false;
        Processor.Initialize(InitParameters);
        if (Profile.bApplySourceRetargetPose)
        {
            Processor.UpdateRetargetPoseAtRuntime(
                Profile.SourceRetargetPoseName,
                ERetargetSourceOrTarget::Source);
        }
        if (Profile.bApplyTargetRetargetPose)
        {
            Processor.UpdateRetargetPoseAtRuntime(
                Profile.TargetRetargetPoseName,
                ERetargetSourceOrTarget::Target);
        }
        return Processor.IsInitialized()
            && Processor.WasInitializedWithTheseAssets(
                Context.SourceMesh, Context.TargetMesh, Retargeter);
    };
    if (!InitializeProcessor(
            BaselineProcessor, BaselineRetargeter, CanonicalProfile)
        || !InitializeProcessor(
            RootOffProcessor, RootOffRetargeter, RootOffProfile))
    {
        return FinishFailure(
            TEXT("A transient root-motion counterfactual processor failed initialization"));
    }

    auto FindProcessorRootSettings = [](
        const FIKRetargetProcessor& Processor,
        int32& OutCount,
        FName& OutName)
    {
        OutCount = 0;
        OutName = NAME_None;
        const FIKRetargetRootMotionOpSettings* Result = nullptr;
        for (const FInstancedStruct& OpStruct : Processor.GetRetargetOps())
        {
            const FIKRetargetOpBase* Op =
                OpStruct.GetPtr<FIKRetargetOpBase>();
            if (Op && Op->GetType() == FIKRetargetRootMotionOp::StaticStruct())
            {
                ++OutCount;
                OutName = Op->GetName();
                Result = static_cast<const FIKRetargetRootMotionOpSettings*>(
                    Op->GetSettingsConst());
            }
        }
        return Result;
    };
    int32 BaselineProcessorRootCount = 0;
    int32 RootOffProcessorRootCount = 0;
    FName BaselineProcessorRootName;
    FName RootOffProcessorRootName;
    const FIKRetargetRootMotionOpSettings* BaselineProcessorRootSettings =
        FindProcessorRootSettings(
            BaselineProcessor,
            BaselineProcessorRootCount,
            BaselineProcessorRootName);
    const FIKRetargetRootMotionOpSettings* RootOffProcessorRootSettings =
        FindProcessorRootSettings(
            RootOffProcessor,
            RootOffProcessorRootCount,
            RootOffProcessorRootName);
    if (BaselineProcessorRootCount != 1
        || RootOffProcessorRootCount != 1
        || !BaselineProcessorRootSettings
        || !RootOffProcessorRootSettings
        || BaselineProcessorRootName != RootOpName
        || RootOffProcessorRootName != RootOpName
        || !BaselineProcessorRootSettings->bEnabled
        || RootOffProcessorRootSettings->bEnabled)
    {
        return FinishFailure(
            TEXT("Transient processors did not apply the exact baseline/root-off Root Motion identities"));
    }
    Report.Root->SetObjectField(
        TEXT("baseline_processor_root_motion_settings"),
        RootSettingsToJson(*BaselineProcessorRootSettings));
    Report.Root->SetObjectField(
        TEXT("counterfactual_processor_root_motion_settings"),
        RootSettingsToJson(*RootOffProcessorRootSettings));

    const TArray<FTransform> BaselineSourceRetargetPose =
        BaselineProcessor.GetSkeleton(
            ERetargetSourceOrTarget::Source)
            .RetargetPoses.GetGlobalRetargetPose();
    const TArray<FTransform> BaselineTargetRetargetPose =
        BaselineProcessor.GetSkeleton(
            ERetargetSourceOrTarget::Target)
            .RetargetPoses.GetGlobalRetargetPose();
    const TArray<FTransform> RootOffSourceRetargetPose =
        RootOffProcessor.GetSkeleton(
            ERetargetSourceOrTarget::Source)
            .RetargetPoses.GetGlobalRetargetPose();
    const TArray<FTransform> RootOffTargetRetargetPose =
        RootOffProcessor.GetSkeleton(
            ERetargetSourceOrTarget::Target)
            .RetargetPoses.GetGlobalRetargetPose();
    auto PosesEqual = [](const TArray<FTransform>& A,
                         const TArray<FTransform>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < A.Num(); ++Index)
        {
            if (!A[Index].Equals(B[Index], UE_KINDA_SMALL_NUMBER))
            {
                return false;
            }
        }
        return true;
    };
    if (!PosesEqual(BaselineSourceRetargetPose, RootOffSourceRetargetPose)
        || !PosesEqual(BaselineTargetRetargetPose, RootOffTargetRetargetPose))
    {
        return FinishFailure(
            TEXT("Baseline and Root-off processors did not resolve identical source/target retarget poses"));
    }

    TArray<FTransform> BaselineInputPose = BaselineSourceRetargetPose;
    TArray<FTransform> RootOffInputPose = RootOffSourceRetargetPose;
    BaselineProcessor.ApplySourceScaleToPose(BaselineInputPose);
    RootOffProcessor.ApplySourceScaleToPose(RootOffInputPose);
    if (!PosesEqual(BaselineInputPose, RootOffInputPose))
    {
        return FinishFailure(
            TEXT("Baseline and Root-off processors produced different scaled source input poses"));
    }

    FRetargetRunParameters BaselineRunParameters;
    BaselineRunParameters.SourceGlobalPose = &BaselineInputPose;
    BaselineRunParameters.Profile = &CanonicalProfile;
    BaselineRunParameters.DeltaTime = 0.0f;
    BaselineRunParameters.LOD = INDEX_NONE;
    const TArray<FTransform>& BaselineOutput =
        BaselineProcessor.RunRetargeter(BaselineRunParameters);

    FRetargetRunParameters RootOffRunParameters;
    RootOffRunParameters.SourceGlobalPose = &RootOffInputPose;
    RootOffRunParameters.Profile = &RootOffProfile;
    RootOffRunParameters.DeltaTime = 0.0f;
    RootOffRunParameters.LOD = INDEX_NONE;
    const TArray<FTransform>& RootOffOutput =
        RootOffProcessor.RunRetargeter(RootOffRunParameters);

    const FReferenceSkeleton& SourceReference = Context.SourceMesh->GetRefSkeleton();
    const FReferenceSkeleton& TargetReference = Context.TargetMesh->GetRefSkeleton();
    if (BaselineSourceRetargetPose.Num() != SourceReference.GetNum()
        || BaselineInputPose.Num() != SourceReference.GetNum()
        || BaselineTargetRetargetPose.Num() != TargetReference.GetNum()
        || BaselineOutput.Num() != TargetReference.GetNum()
        || RootOffOutput.Num() != TargetReference.GetNum())
    {
        return FinishFailure(
            TEXT("A source/target retarget pose or output has an unexpected bone count"));
    }
    Report.Root->SetNumberField(
        TEXT("source_retarget_pose_bone_count"),
        BaselineSourceRetargetPose.Num());
    Report.Root->SetNumberField(
        TEXT("target_retarget_pose_bone_count"),
        BaselineTargetRetargetPose.Num());
    Report.Root->SetNumberField(
        TEXT("baseline_output_bone_count"), BaselineOutput.Num());
    Report.Root->SetNumberField(
        TEXT("root_off_output_bone_count"), RootOffOutput.Num());
    for (const FTransform& Transform : BaselineOutput)
    {
        if (!IsFiniteTransform(Transform))
        {
            return FinishFailure(
                TEXT("Baseline retarget output contains a non-finite transform"));
        }
    }
    for (const FTransform& Transform : RootOffOutput)
    {
        if (!IsFiniteTransform(Transform))
        {
            return FinishFailure(
                TEXT("Root-off retarget output contains a non-finite transform"));
        }
    }

    const FName AnchorBones[] = {
        FName(TEXT("root")),
        FName(TEXT("pelvis")),
        FName(TEXT("spine_01")),
        FName(TEXT("head")),
    };
    auto PoseAnchorsToJson = [&AnchorBones](
        const TArray<FTransform>& Pose,
        const FReferenceSkeleton& Reference,
        FString& OutError)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        for (const FName BoneName : AnchorBones)
        {
            const int32 BoneIndex = Reference.FindBoneIndex(BoneName);
            if (!Pose.IsValidIndex(BoneIndex)
                || !IsFiniteTransform(Pose[BoneIndex]))
            {
                OutError = FString::Printf(
                    TEXT("Pose is missing finite anchor bone %s"),
                    *BoneName.ToString());
                return TSharedPtr<FJsonObject>();
            }
            TSharedRef<FJsonObject> Anchor = ExactTransformToJson(Pose[BoneIndex]);
            Anchor->SetNumberField(TEXT("bone_index"), BoneIndex);
            Result->SetObjectField(BoneName.ToString(), Anchor);
        }
        return Result;
    };
    TSharedPtr<FJsonObject> SourceRetargetAnchors = PoseAnchorsToJson(
        BaselineSourceRetargetPose, SourceReference, Error);
    TSharedPtr<FJsonObject> ScaledSourceInputAnchors = PoseAnchorsToJson(
        BaselineInputPose, SourceReference, Error);
    TSharedPtr<FJsonObject> TargetRetargetAnchors = PoseAnchorsToJson(
        BaselineTargetRetargetPose, TargetReference, Error);
    TSharedPtr<FJsonObject> BaselineOutputAnchors = PoseAnchorsToJson(
        BaselineOutput, TargetReference, Error);
    TSharedPtr<FJsonObject> RootOffOutputAnchors = PoseAnchorsToJson(
        RootOffOutput, TargetReference, Error);
    if (!SourceRetargetAnchors || !ScaledSourceInputAnchors
        || !TargetRetargetAnchors || !BaselineOutputAnchors
        || !RootOffOutputAnchors)
    {
        return FinishFailure(Error);
    }
    Report.Root->SetObjectField(
        TEXT("source_retarget_pose_anchor_transforms"), SourceRetargetAnchors);
    Report.Root->SetObjectField(
        TEXT("scaled_source_input_anchor_transforms"), ScaledSourceInputAnchors);
    Report.Root->SetObjectField(
        TEXT("target_retarget_pose_anchor_transforms"), TargetRetargetAnchors);
    Report.Root->SetObjectField(
        TEXT("baseline_output_anchor_transforms"), BaselineOutputAnchors);
    Report.Root->SetObjectField(
        TEXT("root_off_output_anchor_transforms"), RootOffOutputAnchors);

    const int32 SourceRootIndex = SourceReference.FindBoneIndex(FName(TEXT("root")));
    const int32 SourcePelvisIndex =
        SourceReference.FindBoneIndex(FName(TEXT("pelvis")));
    const int32 TargetRootIndex = TargetReference.FindBoneIndex(FName(TEXT("root")));
    const int32 TargetPelvisIndex = TargetReference.FindBoneIndex(FName(TEXT("pelvis")));
    const int32 TargetSpineIndex = TargetReference.FindBoneIndex(FName(TEXT("spine_01")));
    const int32 TargetHeadIndex = TargetReference.FindBoneIndex(FName(TEXT("head")));
    if (!BaselineInputPose.IsValidIndex(SourceRootIndex)
        || !BaselineInputPose.IsValidIndex(SourcePelvisIndex)
        || !BaselineOutput.IsValidIndex(TargetRootIndex)
        || !BaselineOutput.IsValidIndex(TargetPelvisIndex)
        || !BaselineOutput.IsValidIndex(TargetSpineIndex)
        || !BaselineOutput.IsValidIndex(TargetHeadIndex))
    {
        return FinishFailure(
            TEXT("Required root/pelvis/spine/head indices are unavailable"));
    }

    auto RotationDeltaDegrees = [](const FTransform& A, const FTransform& B)
    {
        return FMath::RadiansToDegrees(
            A.GetRotation().AngularDistance(B.GetRotation()));
    };
    auto ScaleDeltaMaximum = [](const FTransform& A, const FTransform& B)
    {
        return (A.GetScale3D() - B.GetScale3D()).GetAbsMax();
    };
    const FVector SourceRootTranslation =
        BaselineInputPose[SourceRootIndex].GetTranslation();
    const FVector TargetRetargetPelvisTranslation =
        BaselineTargetRetargetPose[TargetPelvisIndex].GetTranslation();
    const FVector BaselinePelvisTranslation =
        BaselineOutput[TargetPelvisIndex].GetTranslation();
    const FVector RootOffPelvisTranslation =
        RootOffOutput[TargetPelvisIndex].GetTranslation();
    const FVector SourceRetargetPelvisTranslation =
        BaselineSourceRetargetPose[SourcePelvisIndex].GetTranslation();
    const FVector SourceInputPelvisTranslation =
        BaselineInputPose[SourcePelvisIndex].GetTranslation();
    const bool bNeutralPelvisMotionSettingsConfirmed =
        CanonicalPelvisSettings->BlendToAbsoluteOffset.Equals(
            FVector::ZeroVector, UE_KINDA_SMALL_NUMBER)
        && FMath::IsNearlyZero(
            CanonicalPelvisSettings->FloorConstraintWeight,
            UE_KINDA_SMALL_NUMBER)
        && FMath::IsNearlyZero(
            CanonicalPelvisSettings->BlendToSourceTranslation,
            UE_KINDA_SMALL_NUMBER)
        && FMath::IsNearlyEqual(
            CanonicalPelvisSettings->ScaleHorizontal,
            1.0,
            UE_KINDA_SMALL_NUMBER)
        && FMath::IsNearlyEqual(
            CanonicalPelvisSettings->ScaleVertical,
            1.0,
            UE_KINDA_SMALL_NUMBER)
        && CanonicalPelvisSettings->TranslationOffsetGlobal.Equals(
            FVector::ZeroVector, UE_KINDA_SMALL_NUMBER)
        && CanonicalPelvisSettings->TranslationOffsetLocal.Equals(
            FVector::ZeroVector, UE_KINDA_SMALL_NUMBER)
        && FMath::IsNearlyEqual(
            CanonicalPelvisSettings->TranslationAlpha,
            1.0,
            UE_KINDA_SMALL_NUMBER)
        && FMath::Abs(SourceRetargetPelvisTranslation.Z)
            > UE_KINDA_SMALL_NUMBER;
    const double PelvisHeightRatio = bNeutralPelvisMotionSettingsConfirmed
        ? TargetRetargetPelvisTranslation.Z
            / SourceRetargetPelvisTranslation.Z
        : 0.0;
    const FVector ExpectedNeutralPelvisMotionTranslation =
        SourceInputPelvisTranslation * PelvisHeightRatio;
    const double BaselinePelvisMagnitudeCm = BaselinePelvisTranslation.Size();
    const double BaselinePelvisFromSourceRootCm = FVector::Distance(
        BaselinePelvisTranslation, SourceRootTranslation);
    const double RootOffPelvisFromTargetPoseCm = FVector::Distance(
        RootOffPelvisTranslation, TargetRetargetPelvisTranslation);
    const double RootOffPelvisHeightFromTargetPoseCm = FMath::Abs(
        RootOffPelvisTranslation.Z - TargetRetargetPelvisTranslation.Z);
    const double RootOffPelvisLateralFromTargetPoseCm = FVector2D::Distance(
        FVector2D(RootOffPelvisTranslation.X, RootOffPelvisTranslation.Y),
        FVector2D(
            TargetRetargetPelvisTranslation.X,
            TargetRetargetPelvisTranslation.Y));
    const double RootOffPelvisFromExpectedPelvisMotionCm = FVector::Distance(
        RootOffPelvisTranslation, ExpectedNeutralPelvisMotionTranslation);
    const double PelvisRestorationCm = FVector::Distance(
        BaselinePelvisTranslation, RootOffPelvisTranslation);
    const double RootTranslationDeltaCm = FVector::Distance(
        BaselineOutput[TargetRootIndex].GetTranslation(),
        RootOffOutput[TargetRootIndex].GetTranslation());
    const double RootRotationDeltaDegrees = RotationDeltaDegrees(
        BaselineOutput[TargetRootIndex], RootOffOutput[TargetRootIndex]);
    const double RootScaleMaximumDelta = ScaleDeltaMaximum(
        BaselineOutput[TargetRootIndex], RootOffOutput[TargetRootIndex]);
    const double BaselineNamedRootMagnitudeCm =
        BaselineOutput[TargetRootIndex].GetTranslation().Size();
    const double RootOffNamedRootMagnitudeCm =
        RootOffOutput[TargetRootIndex].GetTranslation().Size();
    const double SpineTranslationDeltaCm = FVector::Distance(
        BaselineOutput[TargetSpineIndex].GetTranslation(),
        RootOffOutput[TargetSpineIndex].GetTranslation());
    const double SpineRotationDeltaDegrees = RotationDeltaDegrees(
        BaselineOutput[TargetSpineIndex], RootOffOutput[TargetSpineIndex]);
    const double SpineScaleMaximumDelta = ScaleDeltaMaximum(
        BaselineOutput[TargetSpineIndex], RootOffOutput[TargetSpineIndex]);
    const double HeadTranslationDeltaCm = FVector::Distance(
        BaselineOutput[TargetHeadIndex].GetTranslation(),
        RootOffOutput[TargetHeadIndex].GetTranslation());
    const double HeadRotationDeltaDegrees = RotationDeltaDegrees(
        BaselineOutput[TargetHeadIndex], RootOffOutput[TargetHeadIndex]);
    const double HeadScaleMaximumDelta = ScaleDeltaMaximum(
        BaselineOutput[TargetHeadIndex], RootOffOutput[TargetHeadIndex]);

    TSharedRef<FJsonObject> CounterfactualDeltas = MakeShared<FJsonObject>();
    CounterfactualDeltas->SetNumberField(
        TEXT("source_root_translation_z_cm"), SourceRootTranslation.Z);
    CounterfactualDeltas->SetNumberField(
        TEXT("target_retarget_pelvis_translation_z_cm"),
        TargetRetargetPelvisTranslation.Z);
    CounterfactualDeltas->SetNumberField(
        TEXT("baseline_pelvis_translation_z_cm"),
        BaselinePelvisTranslation.Z);
    CounterfactualDeltas->SetNumberField(
        TEXT("root_off_pelvis_translation_z_cm"),
        RootOffPelvisTranslation.Z);
    CounterfactualDeltas->SetNumberField(
        TEXT("baseline_pelvis_translation_magnitude_cm"),
        BaselinePelvisMagnitudeCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("baseline_pelvis_from_source_root_cm"),
        BaselinePelvisFromSourceRootCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("root_off_pelvis_from_target_retarget_pose_cm"),
        RootOffPelvisFromTargetPoseCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("root_off_pelvis_height_from_target_retarget_pose_cm"),
        RootOffPelvisHeightFromTargetPoseCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("root_off_pelvis_lateral_from_target_retarget_pose_cm"),
        RootOffPelvisLateralFromTargetPoseCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("root_off_pelvis_from_expected_pelvis_motion_cm"),
        RootOffPelvisFromExpectedPelvisMotionCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("pelvis_restoration_cm"), PelvisRestorationCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("named_root_translation_delta_cm"), RootTranslationDeltaCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("named_root_rotation_delta_degrees"),
        RootRotationDeltaDegrees);
    CounterfactualDeltas->SetNumberField(
        TEXT("named_root_scale_maximum_delta"), RootScaleMaximumDelta);
    CounterfactualDeltas->SetNumberField(
        TEXT("baseline_named_root_translation_magnitude_cm"),
        BaselineNamedRootMagnitudeCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("root_off_named_root_translation_magnitude_cm"),
        RootOffNamedRootMagnitudeCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("spine_01_translation_delta_cm"), SpineTranslationDeltaCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("spine_01_rotation_delta_degrees"),
        SpineRotationDeltaDegrees);
    CounterfactualDeltas->SetNumberField(
        TEXT("spine_01_scale_maximum_delta"), SpineScaleMaximumDelta);
    CounterfactualDeltas->SetNumberField(
        TEXT("head_translation_delta_cm"), HeadTranslationDeltaCm);
    CounterfactualDeltas->SetNumberField(
        TEXT("head_rotation_delta_degrees"), HeadRotationDeltaDegrees);
    CounterfactualDeltas->SetNumberField(
        TEXT("head_scale_maximum_delta"), HeadScaleMaximumDelta);
    Report.Root->SetObjectField(
        TEXT("counterfactual_transform_deltas"), CounterfactualDeltas);
    Report.Root->SetObjectField(
        TEXT("pelvis_motion_expected_neutral_output_translation_cm"),
        VectorToJson(ExpectedNeutralPelvisMotionTranslation));
    Report.Root->SetBoolField(
        TEXT("pelvis_motion_neutral_settings_confirmed"),
        bNeutralPelvisMotionSettingsConfirmed);
    Report.Root->SetStringField(
        TEXT("pelvis_motion_neutral_horizontal_policy"),
        TEXT("BLEND_TO_ABSOLUTE_OFFSET_ZERO_USES_HEIGHT_NORMALIZED_SOURCE_COMPONENT_POSITION"));
    Report.Root->SetBoolField(
        TEXT("target_retarget_pose_lateral_offset_is_not_neutral_output_invariant"),
        true);

    constexpr double BaselineZeroToleranceCm = 0.10;
    constexpr double TargetRetargetPelvisExpectedZCm = 93.164;
    constexpr double TargetRetargetPelvisZToleranceCm = 0.25;
    constexpr double RootOffTargetPoseHeightToleranceCm = 0.10;
    constexpr double RootOffPelvisMotionToleranceCm = 0.10;
    constexpr double StableTranslationToleranceCm = 0.10;
    constexpr double StableRotationToleranceDegrees = 0.10;
    constexpr double StableScaleMaximumTolerance = 0.001;
    constexpr double MinimumCausalPelvisRestorationCm = 90.0;
    Report.Root->SetNumberField(
        TEXT("baseline_zero_tolerance_cm"), BaselineZeroToleranceCm);
    Report.Root->SetNumberField(
        TEXT("target_retarget_pelvis_expected_z_cm"),
        TargetRetargetPelvisExpectedZCm);
    Report.Root->SetNumberField(
        TEXT("target_retarget_pelvis_z_tolerance_cm"),
        TargetRetargetPelvisZToleranceCm);
    Report.Root->SetNumberField(
        TEXT("root_off_target_pose_height_tolerance_cm"),
        RootOffTargetPoseHeightToleranceCm);
    Report.Root->SetNumberField(
        TEXT("root_off_pelvis_motion_tolerance_cm"),
        RootOffPelvisMotionToleranceCm);
    Report.Root->SetNumberField(
        TEXT("stable_translation_tolerance_cm"),
        StableTranslationToleranceCm);
    Report.Root->SetNumberField(
        TEXT("stable_rotation_tolerance_degrees"),
        StableRotationToleranceDegrees);
    Report.Root->SetNumberField(
        TEXT("stable_scale_maximum_tolerance"),
        StableScaleMaximumTolerance);

    const bool bSourceRootNearZero =
        SourceRootTranslation.Size() <= BaselineZeroToleranceCm;
    const bool bTargetRetargetPelvisExpected = FMath::Abs(
        TargetRetargetPelvisTranslation.Z
            - TargetRetargetPelvisExpectedZCm)
        <= TargetRetargetPelvisZToleranceCm;
    const bool bBaselinePelvisNearZero =
        BaselinePelvisMagnitudeCm <= BaselineZeroToleranceCm;
    const bool bBaselineMatchesSourceRoot =
        BaselinePelvisFromSourceRootCm <= BaselineZeroToleranceCm;
    const bool bRootOffRestoresTargetPoseHeight =
        RootOffPelvisHeightFromTargetPoseCm
            <= RootOffTargetPoseHeightToleranceCm;
    const bool bRootOffMatchesExpectedPelvisMotion =
        bNeutralPelvisMotionSettingsConfirmed
        && RootOffPelvisFromExpectedPelvisMotionCm
            <= RootOffPelvisMotionToleranceCm;
    const bool bPelvisCausalDeltaLarge =
        PelvisRestorationCm >= MinimumCausalPelvisRestorationCm;
    const bool bNamedRootStable =
        RootTranslationDeltaCm <= StableTranslationToleranceCm
        && RootRotationDeltaDegrees <= StableRotationToleranceDegrees
        && RootScaleMaximumDelta <= StableScaleMaximumTolerance;
    const bool bNamedRootNearZero =
        BaselineNamedRootMagnitudeCm <= BaselineZeroToleranceCm
        && RootOffNamedRootMagnitudeCm <= BaselineZeroToleranceCm;
    const bool bSpineStable =
        SpineTranslationDeltaCm <= StableTranslationToleranceCm
        && SpineRotationDeltaDegrees <= StableRotationToleranceDegrees
        && SpineScaleMaximumDelta <= StableScaleMaximumTolerance;
    const bool bHeadStable =
        HeadTranslationDeltaCm <= StableTranslationToleranceCm
        && HeadRotationDeltaDegrees <= StableRotationToleranceDegrees
        && HeadScaleMaximumDelta <= StableScaleMaximumTolerance;
    Report.Root->SetBoolField(
        TEXT("source_root_near_zero"), bSourceRootNearZero);
    Report.Root->SetBoolField(
        TEXT("target_retarget_pelvis_near_93_164_cm"),
        bTargetRetargetPelvisExpected);
    Report.Root->SetBoolField(
        TEXT("baseline_pelvis_near_zero"), bBaselinePelvisNearZero);
    Report.Root->SetBoolField(
        TEXT("baseline_pelvis_matches_source_root"),
        bBaselineMatchesSourceRoot);
    Report.Root->SetBoolField(
        TEXT("root_off_pelvis_restores_target_retarget_pose_height"),
        bRootOffRestoresTargetPoseHeight);
    Report.Root->SetBoolField(
        TEXT("root_off_pelvis_matches_expected_pelvis_motion"),
        bRootOffMatchesExpectedPelvisMotion);
    Report.Root->SetBoolField(
        TEXT("pelvis_counterfactual_delta_exceeds_90_cm"),
        bPelvisCausalDeltaLarge);
    Report.Root->SetBoolField(TEXT("named_root_stable"), bNamedRootStable);
    Report.Root->SetBoolField(
        TEXT("named_root_near_zero"), bNamedRootNearZero);
    Report.Root->SetBoolField(TEXT("spine_01_stable"), bSpineStable);
    Report.Root->SetBoolField(TEXT("head_stable"), bHeadStable);
    FRetargetProfile CanonicalProfileAfter;
    CanonicalProfileAfter.FillProfileWithAssetSettings(Context.Retargeter);
    const bool bCanonicalSettingsPreserved = ProfilesIdentical(
        CanonicalProfile, CanonicalProfileAfter)
        && Context.SourceIKRig->GetRoot() == SourceRigRoot
        && Context.SourceIKRig->GetPelvis() == SourceRigPelvis
        && Context.TargetIKRig->GetRoot() == TargetRigRoot
        && Context.TargetIKRig->GetPelvis() == TargetRigPelvis;
    FString DiskError;
    const bool bDiskPreserved = VerifyPackageSnapshotsUnchanged(
        CanonicalSnapshots, DiskError);
    const bool bDirtyPreserved = SameNameSet(
        DirtyPackagesBefore, CaptureDirtyPackages());
    const bool bSavableDirtyPreserved = SameNameSet(
        SavableDirtyPackagesBefore, CaptureSavableDirtyPackages());
    Report.Root->SetBoolField(
        TEXT("canonical_in_memory_settings_preserved"),
        bCanonicalSettingsPreserved);
    Report.Root->SetBoolField(
        TEXT("canonical_disk_bytes_preserved"), bDiskPreserved);
    Report.Root->SetBoolField(
        TEXT("dirty_package_baseline_preserved"), bDirtyPreserved);
    Report.Root->SetBoolField(
        TEXT("savable_dirty_package_baseline_preserved"),
        bSavableDirtyPreserved);
    if (!bCanonicalSettingsPreserved || !bDiskPreserved
        || !bDirtyPreserved || !bSavableDirtyPreserved)
    {
        return FinishFailure(DiskError.IsEmpty()
            ? TEXT("Canonical in-memory, disk, or dirty-package preservation failed")
            : DiskError);
    }

    if (!bSourceRootNearZero
        || !bTargetRetargetPelvisExpected
        || !bBaselinePelvisNearZero
        || !bBaselineMatchesSourceRoot
        || !bRootOffRestoresTargetPoseHeight
        || !bRootOffMatchesExpectedPelvisMotion
        || !bPelvisCausalDeltaLarge
        || !bNamedRootStable
        || !bNamedRootNearZero
        || !bSpineStable
        || !bHeadStable)
    {
        return FinishFailure(FString::Printf(
            TEXT("Root Motion counterfactual rejected after preservation proof: source root Z %.6f, target retarget pelvis Z %.6f, baseline pelvis Z %.6f, Root-off pelvis Z %.6f, Root-off expected Pelvis Motion delta %.6f cm, spine delta %.6f cm/%.6f deg"),
            SourceRootTranslation.Z,
            TargetRetargetPelvisTranslation.Z,
            BaselinePelvisTranslation.Z,
            RootOffPelvisTranslation.Z,
            RootOffPelvisFromExpectedPelvisMotionCm,
            SpineTranslationDeltaCm,
            SpineRotationDeltaDegrees));
    }

    Report.Root->SetBoolField(TEXT("diagnostic_completed"), true);
    Report.Root->SetBoolField(
        TEXT("root_motion_target_root_equals_pelvis_causal_counterfactual_confirmed"),
        true);
    Report.Pass(
        TEXT("Confirmed causally: enabled CopyFromSourceRoot writes the target pelvis to the zero-height source root, while the transient Root-off profile restores the 93.164 cm target pelvis height, matches neutral Pelvis Motion's height-normalized source position, and leaves root, spine_01, and head stable"),
        TEXT("NONE"));
    return Report.Serialize();
}

FString RunOutfitDiagnostic()
{
    FRunReport Report(TEXT("OUTFIT_DIAGNOSTIC"));
    Report.Root->SetStringField(
        TEXT("diagnostic_schema"),
        TEXT("DiscGolfTour.Session8BMetaHumanOutfitDiagnostic.v1"));
    Report.Root->SetStringField(
        TEXT("outfit_mesh"), ExpectedOutfitMeshPath);
    Report.Root->SetStringField(
        TEXT("rendered_lod_observation_scope"),
        TEXT("ASSET_RENDER_DATA_ONLY_NO_LIVE_RENDER_COMPONENT"));
    Report.Root->SetNumberField(TEXT("actual_rendered_lod"), INDEX_NONE);
    Report.Root->SetBoolField(TEXT("asset_save_attempted"), false);

    const TSet<FName> DirtyPackagesBefore = CaptureDirtyPackages();
    auto SameDirtySet = [](const TSet<FName>& A, const TSet<FName>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }
        for (const FName Name : A)
        {
            if (!B.Contains(Name))
            {
                return false;
            }
        }
        return true;
    };
    auto FinishFailure = [&Report, &DirtyPackagesBefore, &SameDirtySet](
        const FString& Error)
    {
        const bool bDirtyBaselinePreserved = SameDirtySet(
            DirtyPackagesBefore, CaptureDirtyPackages());
        Report.Root->SetBoolField(
            TEXT("dirty_package_baseline_preserved"),
            bDirtyBaselinePreserved);
        Report.Root->SetBoolField(TEXT("diagnostic_completed"), false);
        Report.Fail(bDirtyBaselinePreserved
            ? Error
            : Error + TEXT("; diagnostic changed the dirty-package baseline"));
        return Report.Serialize();
    };

    FString Error;
    UBlueprint* const Generated = LoadExact<UBlueprint>(
        GeneratedBlueprintPath, Error);
    USkeletalMesh* const ExpectedOutfit = LoadExact<USkeletalMesh>(
        ExpectedOutfitMeshPath, Error);
    if (!Generated || !Generated->GeneratedClass || !Generated->IsUpToDate()
        || !ExpectedOutfit)
    {
        return FinishFailure(Error.IsEmpty()
            ? TEXT("The exact generated MetaHuman or Outfit mesh is unavailable")
            : Error);
    }

    FNodeReference BodyReference;
    if (!FindUniqueSCSNode(
            Generated, BodyVariableName, BodyReference, Error))
    {
        return FinishFailure(Error);
    }
    USkeletalMeshComponent* const BodyComponent =
        Cast<USkeletalMeshComponent>(
            GetActualTemplate(Generated, BodyReference));

    TArray<FNodeReference> Nodes;
    GatherSCSNodes(Generated, Nodes);
    USkeletalMeshComponent* OutfitComponent = nullptr;
    int32 OutfitComponentCount = 0;
    for (const FNodeReference& Reference : Nodes)
    {
        USkeletalMeshComponent* const Candidate =
            Cast<USkeletalMeshComponent>(
                GetActualTemplate(Generated, Reference));
        if (Candidate
            && Candidate->GetSkeletalMeshAsset() == ExpectedOutfit
            && Candidate->GetSkeletalMeshAsset()->GetPathName()
                == ExpectedOutfitMeshPath)
        {
            OutfitComponent = Candidate;
            ++OutfitComponentCount;
        }
    }

    USkeletalMesh* const BodyMesh = BodyComponent
        ? BodyComponent->GetSkeletalMeshAsset() : nullptr;
    if (!BodyComponent || !BodyMesh || !OutfitComponent
        || OutfitComponentCount != 1
        || BodyComponent == OutfitComponent
        || OutfitComponent->GetSkeletalMeshAsset() != ExpectedOutfit)
    {
        return FinishFailure(FString::Printf(
            TEXT("The exact generated Blueprint exposes an invalid Body/Outfit tuple: body=%s outfit_count=%d outfit=%s"),
            BodyComponent ? *BodyComponent->GetPathName() : TEXT("<null>"),
            OutfitComponentCount,
            OutfitComponent ? *OutfitComponent->GetPathName()
                : TEXT("<null>")));
    }
    if (!BodyComponent->GetRelativeTransform().Equals(
            FTransform::Identity, 0.0001)
        || !OutfitComponent->GetRelativeTransform().Equals(
            FTransform::Identity, 0.0001))
    {
        return FinishFailure(
            TEXT("Generated Body and Outfit component templates are not identity-relative"));
    }

    Report.Root->SetStringField(
        TEXT("body_component_template"), BodyComponent->GetPathName());
    Report.Root->SetStringField(
        TEXT("outfit_component_template"), OutfitComponent->GetPathName());
    Report.Root->SetStringField(TEXT("body_mesh"), BodyMesh->GetPathName());
    Report.Root->SetBoolField(
        TEXT("body_component_identity_relative"), true);
    Report.Root->SetBoolField(
        TEXT("outfit_component_identity_relative"), true);
    Report.Root->SetNumberField(
        TEXT("outfit_component_count"), OutfitComponentCount);

    const FReferenceSkeleton& BodySkeleton = BodyMesh->GetRefSkeleton();
    const FReferenceSkeleton& OutfitSkeleton =
        ExpectedOutfit->GetRefSkeleton();
    const int32 BodyBoneCount = BodySkeleton.GetRawBoneNum();
    const int32 OutfitBoneCount = OutfitSkeleton.GetRawBoneNum();
    const TArray<FTransform>& BodyLocalPose =
        BodySkeleton.GetRawRefBonePose();
    const TArray<FTransform>& OutfitLocalPose =
        OutfitSkeleton.GetRawRefBonePose();
    const TArray<FMatrix44f>& BodyInverseBind =
        BodyMesh->GetRefBasesInvMatrix();
    const TArray<FMatrix44f>& OutfitInverseBind =
        ExpectedOutfit->GetRefBasesInvMatrix();
    if (BodyBoneCount <= 0 || OutfitBoneCount <= 0
        || BodyLocalPose.Num() != BodyBoneCount
        || OutfitLocalPose.Num() != OutfitBoneCount
        || BodyInverseBind.Num() != BodyBoneCount
        || OutfitInverseBind.Num() != OutfitBoneCount)
    {
        return FinishFailure(FString::Printf(
            TEXT("Reference skeleton/inverse-bind arrays are incomplete: body=%d/%d/%d outfit=%d/%d/%d"),
            BodyBoneCount,
            BodyLocalPose.Num(),
            BodyInverseBind.Num(),
            OutfitBoneCount,
            OutfitLocalPose.Num(),
            OutfitInverseBind.Num()));
    }

    auto BuildComponentPose = [](
        const FReferenceSkeleton& Skeleton,
        const TArray<FTransform>& LocalPose,
        TArray<FTransform>& OutComponentPose) -> bool
    {
        const int32 BoneCount = Skeleton.GetRawBoneNum();
        if (LocalPose.Num() != BoneCount)
        {
            return false;
        }
        OutComponentPose.SetNumUninitialized(BoneCount);
        for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            const int32 ParentIndex =
                Skeleton.GetRawParentIndex(BoneIndex);
            if ((BoneIndex == 0 && ParentIndex != INDEX_NONE)
                || (BoneIndex > 0
                    && (ParentIndex < 0 || ParentIndex >= BoneIndex)))
            {
                return false;
            }
            OutComponentPose[BoneIndex] = LocalPose[BoneIndex];
            if (ParentIndex != INDEX_NONE)
            {
                OutComponentPose[BoneIndex] =
                    OutComponentPose[BoneIndex]
                    * OutComponentPose[ParentIndex];
            }
            if (!IsFiniteTransform(OutComponentPose[BoneIndex]))
            {
                return false;
            }
        }
        return true;
    };

    TArray<FTransform> BodyComponentPose;
    TArray<FTransform> OutfitComponentPose;
    if (!BuildComponentPose(
            BodySkeleton, BodyLocalPose, BodyComponentPose)
        || !BuildComponentPose(
            OutfitSkeleton, OutfitLocalPose, OutfitComponentPose))
    {
        return FinishFailure(
            TEXT("Could not build finite component-space reference poses"));
    }

    struct FTransformDelta
    {
        double TranslationCm = 0.0;
        double RotationDegrees = 0.0;
        double ScaleMaximum = 0.0;
    };
    auto TransformDelta = [](const FTransform& A, const FTransform& B)
    {
        FTransformDelta Delta;
        Delta.TranslationCm = FVector::Distance(
            A.GetTranslation(), B.GetTranslation());
        const FQuat ARotation = A.GetRotation().GetNormalized();
        const FQuat BRotation = B.GetRotation().GetNormalized();
        Delta.RotationDegrees = FMath::RadiansToDegrees(
            ARotation.AngularDistance(BRotation));
        Delta.ScaleMaximum = (
            A.GetScale3D() - B.GetScale3D()).GetAbsMax();
        return Delta;
    };
    auto AddDelta = [](TSharedRef<FJsonObject> Json,
        const TCHAR* Prefix, const FTransformDelta& Delta)
    {
        Json->SetNumberField(
            FString::Printf(TEXT("%s_translation_cm"), Prefix),
            Delta.TranslationCm);
        Json->SetNumberField(
            FString::Printf(TEXT("%s_rotation_degrees"), Prefix),
            Delta.RotationDegrees);
        Json->SetNumberField(
            FString::Printf(TEXT("%s_scale_maximum"), Prefix),
            Delta.ScaleMaximum);
    };
    auto MatrixToJson = [](const FMatrix44f& Matrix)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        Rows.Reserve(4);
        for (int32 RowIndex = 0; RowIndex < 4; ++RowIndex)
        {
            TArray<TSharedPtr<FJsonValue>> Columns;
            Columns.Reserve(4);
            for (int32 ColumnIndex = 0; ColumnIndex < 4; ++ColumnIndex)
            {
                const double Value =
                    Matrix.M[RowIndex][ColumnIndex];
                Columns.Add(MakeShared<FJsonValueNumber>(
                    FMath::IsFinite(Value) ? Value : 0.0));
            }
            Rows.Add(MakeShared<FJsonValueArray>(Columns));
        }
        return Rows;
    };
    auto MatrixMaximumAbsoluteDelta = [](
        const FMatrix44f& A, const FMatrix44f& B,
        bool& bOutFinite)
    {
        double Maximum = 0.0;
        bOutFinite = true;
        for (int32 RowIndex = 0; RowIndex < 4; ++RowIndex)
        {
            for (int32 ColumnIndex = 0; ColumnIndex < 4; ++ColumnIndex)
            {
                const double AValue = A.M[RowIndex][ColumnIndex];
                const double BValue = B.M[RowIndex][ColumnIndex];
                const bool bValuesFinite = FMath::IsFinite(AValue)
                    && FMath::IsFinite(BValue);
                bOutFinite = bOutFinite && bValuesFinite;
                if (bValuesFinite)
                {
                    Maximum = FMath::Max(
                        Maximum, FMath::Abs(AValue - BValue));
                }
            }
        }
        return Maximum;
    };
    auto IsFiniteMatrix = [](const FMatrix44f& Matrix)
    {
        for (int32 RowIndex = 0; RowIndex < 4; ++RowIndex)
        {
            for (int32 ColumnIndex = 0; ColumnIndex < 4; ++ColumnIndex)
            {
                if (!FMath::IsFinite(
                        Matrix.M[RowIndex][ColumnIndex]))
                {
                    return false;
                }
            }
        }
        return true;
    };

    constexpr double TranslationToleranceCm = 0.01;
    constexpr double RotationToleranceDegrees = 0.01;
    constexpr double ScaleTolerance = 0.0001;
    constexpr double InverseBindTolerance = 0.0001;
    const TSet<FName> AnchorNames = {
        FName(TEXT("root")),
        FName(TEXT("pelvis")),
        FName(TEXT("spine_01")),
        FName(TEXT("hand_l")),
        FName(TEXT("hand_r")),
        FName(TEXT("calf_l")),
        FName(TEXT("calf_r")),
        FName(TEXT("foot_l")),
        FName(TEXT("foot_r")),
    };
    TArray<TSharedPtr<FJsonValue>> BoneValues;
    TSharedRef<FJsonObject> AnchorValues = MakeShared<FJsonObject>();
    TArray<int32> OutfitToBodyBone;
    OutfitToBodyBone.Init(INDEX_NONE, OutfitBoneCount);
    int32 MissingNameCount = 0;
    int32 OrderMismatchCount = 0;
    int32 ParentMismatchCount = 0;
    int32 LocalRefMismatchCount = 0;
    int32 ComponentRefMismatchCount = 0;
    int32 InverseBindMismatchCount = 0;
    int32 NonFiniteInverseBindCount = 0;
    double MaximumLocalTranslationCm = 0.0;
    double MaximumLocalRotationDegrees = 0.0;
    double MaximumLocalScale = 0.0;
    double MaximumComponentTranslationCm = 0.0;
    double MaximumComponentRotationDegrees = 0.0;
    double MaximumComponentScale = 0.0;
    double MaximumInverseBindAbsoluteDelta = 0.0;

    for (int32 OutfitBoneIndex = 0;
         OutfitBoneIndex < OutfitBoneCount;
         ++OutfitBoneIndex)
    {
        const FMeshBoneInfo& OutfitBoneInfo =
            OutfitSkeleton.GetRawRefBoneInfo()[OutfitBoneIndex];
        const FName BoneName = OutfitBoneInfo.Name;
        const int32 BodyBoneIndex =
            BodySkeleton.FindRawBoneIndex(BoneName);
        OutfitToBodyBone[OutfitBoneIndex] = BodyBoneIndex;
        const int32 OutfitParentIndex =
            OutfitSkeleton.GetRawParentIndex(OutfitBoneIndex);
        const FName OutfitParentName = OutfitParentIndex != INDEX_NONE
            ? OutfitSkeleton.GetRawRefBoneInfo()[OutfitParentIndex].Name
            : NAME_None;
        const int32 BodyParentIndex = BodyBoneIndex != INDEX_NONE
            ? BodySkeleton.GetRawParentIndex(BodyBoneIndex)
            : INDEX_NONE;
        const FName BodyParentName = BodyParentIndex != INDEX_NONE
            ? BodySkeleton.GetRawRefBoneInfo()[BodyParentIndex].Name
            : NAME_None;
        const bool bNameMapped = BodyBoneIndex != INDEX_NONE;
        const bool bSameOrder = bNameMapped
            && BodyBoneIndex == OutfitBoneIndex;
        const bool bSameParent = bNameMapped
            && BodyParentName == OutfitParentName;

        TSharedRef<FJsonObject> Bone = MakeShared<FJsonObject>();
        Bone->SetStringField(TEXT("bone_name"), BoneName.ToString());
        Bone->SetNumberField(TEXT("outfit_bone_index"), OutfitBoneIndex);
        Bone->SetNumberField(TEXT("body_bone_index"), BodyBoneIndex);
        Bone->SetStringField(
            TEXT("outfit_parent_name"), OutfitParentName.ToString());
        Bone->SetStringField(
            TEXT("body_parent_name"), BodyParentName.ToString());
        Bone->SetBoolField(TEXT("name_mapped"), bNameMapped);
        Bone->SetBoolField(TEXT("same_order"), bSameOrder);
        Bone->SetBoolField(TEXT("same_parent"), bSameParent);
        Bone->SetObjectField(
            TEXT("outfit_local_ref_transform"),
            TransformToJson(OutfitLocalPose[OutfitBoneIndex]));
        Bone->SetObjectField(
            TEXT("outfit_component_ref_transform"),
            TransformToJson(OutfitComponentPose[OutfitBoneIndex]));
        Bone->SetArrayField(
            TEXT("outfit_inverse_bind_matrix"),
            MatrixToJson(OutfitInverseBind[OutfitBoneIndex]));
        const bool bOutfitInverseBindFinite = IsFiniteMatrix(
            OutfitInverseBind[OutfitBoneIndex]);
        Bone->SetBoolField(
            TEXT("outfit_inverse_bind_finite"),
            bOutfitInverseBindFinite);
        if (!bOutfitInverseBindFinite)
        {
            ++NonFiniteInverseBindCount;
        }

        if (!bNameMapped)
        {
            ++MissingNameCount;
        }
        else
        {
            Bone->SetObjectField(
                TEXT("body_local_ref_transform"),
                TransformToJson(BodyLocalPose[BodyBoneIndex]));
            Bone->SetObjectField(
                TEXT("body_component_ref_transform"),
                TransformToJson(BodyComponentPose[BodyBoneIndex]));
            Bone->SetArrayField(
                TEXT("body_inverse_bind_matrix"),
                MatrixToJson(BodyInverseBind[BodyBoneIndex]));
            const FTransformDelta LocalDelta = TransformDelta(
                OutfitLocalPose[OutfitBoneIndex],
                BodyLocalPose[BodyBoneIndex]);
            const FTransformDelta ComponentDelta = TransformDelta(
                OutfitComponentPose[OutfitBoneIndex],
                BodyComponentPose[BodyBoneIndex]);
            AddDelta(Bone, TEXT("local_ref_delta"), LocalDelta);
            AddDelta(Bone, TEXT("component_ref_delta"), ComponentDelta);
            MaximumLocalTranslationCm = FMath::Max(
                MaximumLocalTranslationCm, LocalDelta.TranslationCm);
            MaximumLocalRotationDegrees = FMath::Max(
                MaximumLocalRotationDegrees, LocalDelta.RotationDegrees);
            MaximumLocalScale = FMath::Max(
                MaximumLocalScale, LocalDelta.ScaleMaximum);
            MaximumComponentTranslationCm = FMath::Max(
                MaximumComponentTranslationCm,
                ComponentDelta.TranslationCm);
            MaximumComponentRotationDegrees = FMath::Max(
                MaximumComponentRotationDegrees,
                ComponentDelta.RotationDegrees);
            MaximumComponentScale = FMath::Max(
                MaximumComponentScale,
                ComponentDelta.ScaleMaximum);
            const bool bLocalCompatible =
                LocalDelta.TranslationCm <= TranslationToleranceCm
                && LocalDelta.RotationDegrees
                    <= RotationToleranceDegrees
                && LocalDelta.ScaleMaximum <= ScaleTolerance;
            const bool bComponentCompatible =
                ComponentDelta.TranslationCm <= TranslationToleranceCm
                && ComponentDelta.RotationDegrees
                    <= RotationToleranceDegrees
                && ComponentDelta.ScaleMaximum <= ScaleTolerance;
            Bone->SetBoolField(
                TEXT("local_ref_compatible"), bLocalCompatible);
            Bone->SetBoolField(
                TEXT("component_ref_compatible"), bComponentCompatible);
            bool bInverseBindFinite = false;
            const double InverseBindDelta =
                MatrixMaximumAbsoluteDelta(
                    OutfitInverseBind[OutfitBoneIndex],
                    BodyInverseBind[BodyBoneIndex],
                    bInverseBindFinite);
            const bool bInverseBindCompatible = bInverseBindFinite
                && InverseBindDelta <= InverseBindTolerance;
            Bone->SetBoolField(
                TEXT("inverse_bind_finite"), bInverseBindFinite);
            Bone->SetNumberField(
                TEXT("inverse_bind_matrix_maximum_absolute_delta"),
                InverseBindDelta);
            Bone->SetBoolField(
                TEXT("inverse_bind_compatible"),
                bInverseBindCompatible);
            MaximumInverseBindAbsoluteDelta = FMath::Max(
                MaximumInverseBindAbsoluteDelta, InverseBindDelta);
            if (!bLocalCompatible)
            {
                ++LocalRefMismatchCount;
            }
            if (!bComponentCompatible)
            {
                ++ComponentRefMismatchCount;
            }
            if (!bInverseBindFinite && bOutfitInverseBindFinite)
            {
                ++NonFiniteInverseBindCount;
            }
            if (!bInverseBindCompatible)
            {
                ++InverseBindMismatchCount;
            }
        }
        if (!bSameOrder)
        {
            ++OrderMismatchCount;
        }
        if (!bSameParent)
        {
            ++ParentMismatchCount;
        }

        BoneValues.Add(MakeShared<FJsonValueObject>(Bone));
        if (AnchorNames.Contains(BoneName))
        {
            AnchorValues->SetObjectField(BoneName.ToString(), Bone);
        }
    }

    const bool bAllAnchorsPresent =
        AnchorValues->Values.Num() == AnchorNames.Num();
    const bool bBindCompatible = MissingNameCount == 0
        && ParentMismatchCount == 0
        && ComponentRefMismatchCount == 0
        && InverseBindMismatchCount == 0;
    Report.Root->SetNumberField(TEXT("body_raw_bone_count"), BodyBoneCount);
    Report.Root->SetNumberField(TEXT("outfit_raw_bone_count"), OutfitBoneCount);
    Report.Root->SetNumberField(
        TEXT("all_bone_comparison_count"), BoneValues.Num());
    Report.Root->SetNumberField(
        TEXT("missing_body_bone_name_count"), MissingNameCount);
    Report.Root->SetNumberField(
        TEXT("bone_order_mismatch_count"), OrderMismatchCount);
    Report.Root->SetNumberField(
        TEXT("bone_parent_mismatch_count"), ParentMismatchCount);
    Report.Root->SetNumberField(
        TEXT("local_ref_transform_mismatch_count"),
        LocalRefMismatchCount);
    Report.Root->SetNumberField(
        TEXT("component_ref_transform_mismatch_count"),
        ComponentRefMismatchCount);
    Report.Root->SetNumberField(
        TEXT("inverse_bind_mismatch_count"),
        InverseBindMismatchCount);
    Report.Root->SetNumberField(
        TEXT("non_finite_inverse_bind_count"),
        NonFiniteInverseBindCount);
    Report.Root->SetNumberField(
        TEXT("maximum_local_ref_translation_delta_cm"),
        MaximumLocalTranslationCm);
    Report.Root->SetNumberField(
        TEXT("maximum_local_ref_rotation_delta_degrees"),
        MaximumLocalRotationDegrees);
    Report.Root->SetNumberField(
        TEXT("maximum_local_ref_scale_delta"), MaximumLocalScale);
    Report.Root->SetNumberField(
        TEXT("maximum_component_ref_translation_delta_cm"),
        MaximumComponentTranslationCm);
    Report.Root->SetNumberField(
        TEXT("maximum_component_ref_rotation_delta_degrees"),
        MaximumComponentRotationDegrees);
    Report.Root->SetNumberField(
        TEXT("maximum_component_ref_scale_delta"),
        MaximumComponentScale);
    Report.Root->SetNumberField(
        TEXT("maximum_inverse_bind_matrix_absolute_delta"),
        MaximumInverseBindAbsoluteDelta);
    Report.Root->SetNumberField(
        TEXT("translation_tolerance_cm"), TranslationToleranceCm);
    Report.Root->SetNumberField(
        TEXT("rotation_tolerance_degrees"),
        RotationToleranceDegrees);
    Report.Root->SetNumberField(
        TEXT("scale_tolerance"), ScaleTolerance);
    Report.Root->SetNumberField(
        TEXT("inverse_bind_tolerance"), InverseBindTolerance);
    Report.Root->SetBoolField(
        TEXT("all_anchor_comparisons_present"), bAllAnchorsPresent);
    Report.Root->SetBoolField(
        TEXT("leader_pose_reference_bind_compatible"), bBindCompatible);
    Report.Root->SetStringField(
        TEXT("reference_bind_diagnosis"),
        MissingNameCount > 0
            ? TEXT("OUTFIT_BONES_MISSING_FROM_BODY")
            : ParentMismatchCount > 0
                ? TEXT("BODY_OUTFIT_PARENT_HIERARCHY_MISMATCH")
                : ComponentRefMismatchCount > 0
                    ? TEXT("BODY_OUTFIT_REFERENCE_BIND_MISMATCH")
                    : InverseBindMismatchCount > 0
                        ? TEXT("BODY_OUTFIT_INVERSE_BIND_MISMATCH")
                    : TEXT("BODY_OUTFIT_REFERENCE_BIND_COMPATIBLE"));
    Report.Root->SetArrayField(
        TEXT("all_bone_comparisons"), BoneValues);
    Report.Root->SetObjectField(
        TEXT("required_anchor_comparisons"), AnchorValues);
    Report.Root->SetBoolField(TEXT("all_bone_probe_completed"), true);
    if (!bAllAnchorsPresent)
    {
        return FinishFailure(
            TEXT("One or more required Body/Outfit anchor bones are absent"));
    }
    if (NonFiniteInverseBindCount > 0)
    {
        return FinishFailure(
            TEXT("Body or Outfit inverse-bind data contains non-finite values"));
    }

    const int32 CalfL = BodySkeleton.FindRawBoneIndex(
        FName(TEXT("calf_l")));
    const int32 CalfR = BodySkeleton.FindRawBoneIndex(
        FName(TEXT("calf_r")));
    const int32 FootL = BodySkeleton.FindRawBoneIndex(
        FName(TEXT("foot_l")));
    const int32 FootR = BodySkeleton.FindRawBoneIndex(
        FName(TEXT("foot_r")));
    if (CalfL == INDEX_NONE || CalfR == INDEX_NONE
        || FootL == INDEX_NONE || FootR == INDEX_NONE)
    {
        return FinishFailure(
            TEXT("Body reference pose lacks calf/foot silhouette planes"));
    }
    const double KneePlaneZ = 0.5 * (
        BodyComponentPose[CalfL].GetTranslation().Z
        + BodyComponentPose[CalfR].GetTranslation().Z);
    const double FootPlaneZ = 0.5 * (
        BodyComponentPose[FootL].GetTranslation().Z
        + BodyComponentPose[FootR].GetTranslation().Z);
    Report.Root->SetNumberField(
        TEXT("body_reference_knee_plane_z_cm"), KneePlaneZ);
    Report.Root->SetNumberField(
        TEXT("body_reference_foot_plane_z_cm"), FootPlaneZ);

    FSkeletalMeshRenderData* const RenderData =
        ExpectedOutfit->GetResourceForRendering();
    if (!RenderData || RenderData->LODRenderData.IsEmpty())
    {
        return FinishFailure(
            TEXT("Outfit has no skeletal render data"));
    }
    Report.Root->SetNumberField(
        TEXT("asset_min_lod"), ExpectedOutfit->GetMinLodIdx());
    Report.Root->SetNumberField(
        TEXT("component_compute_min_lod"),
        OutfitComponent->ComputeMinLOD());
    Report.Root->SetNumberField(
        TEXT("component_predicted_lod_template_value"),
        OutfitComponent->GetPredictedLODLevel());
    Report.Root->SetNumberField(
        TEXT("component_forced_lod_legacy_one_based"),
        OutfitComponent->GetForcedLOD());
    Report.Root->SetNumberField(
        TEXT("render_data_current_first_lod"),
        RenderData->CurrentFirstLODIdx);
    Report.Root->SetNumberField(
        TEXT("render_data_pending_first_lod"),
        RenderData->PendingFirstLODIdx);
    Report.Root->SetBoolField(
        TEXT("render_data_ready_for_streaming"),
        RenderData->bReadyForStreaming);
    Report.Root->SetBoolField(
        TEXT("render_data_initialized"),
        RenderData->IsInitialized());
    Report.Root->SetBoolField(
        TEXT("render_data_all_lods_ready"),
        RenderData->IsLODDataReady());
    Report.Root->SetNumberField(
        TEXT("render_data_inlined_lod_count"),
        RenderData->NumInlinedLODs);
    Report.Root->SetNumberField(
        TEXT("render_data_non_optional_lod_count"),
        RenderData->NumNonOptionalLODs);
    Report.Root->SetNumberField(
        TEXT("render_data_lod_bias_modifier"),
        RenderData->LODBiasModifier);
    Report.Root->SetNumberField(
        TEXT("outfit_lod_count"), RenderData->LODRenderData.Num());
    const IConsoleVariable* const FreeBuffers =
        IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.FreeSkeletalMeshBuffers"));
    Report.Root->SetNumberField(
        TEXT("r_free_skeletal_mesh_buffers"),
        FreeBuffers ? FreeBuffers->GetInt() : INDEX_NONE);

    TArray<FMatrix44f> OutfitSelfRefToLocal;
    TArray<FMatrix44f> BodyDrivenRefToLocal;
    OutfitSelfRefToLocal.SetNumUninitialized(OutfitBoneCount);
    BodyDrivenRefToLocal.SetNumUninitialized(OutfitBoneCount);
    bool bAllSkinBonesMapped = true;
    bool bAllSyntheticRefToLocalMatricesFinite = true;
    for (int32 OutfitBoneIndex = 0;
         OutfitBoneIndex < OutfitBoneCount;
         ++OutfitBoneIndex)
    {
        const int32 BodyBoneIndex = OutfitToBodyBone[OutfitBoneIndex];
        OutfitSelfRefToLocal[OutfitBoneIndex] =
            OutfitInverseBind[OutfitBoneIndex]
            * FMatrix44f(
                OutfitComponentPose[OutfitBoneIndex].ToMatrixWithScale());
        if (BodyBoneIndex == INDEX_NONE)
        {
            BodyDrivenRefToLocal[OutfitBoneIndex] = FMatrix44f::Identity;
            bAllSkinBonesMapped = false;
        }
        else
        {
            BodyDrivenRefToLocal[OutfitBoneIndex] =
                OutfitInverseBind[OutfitBoneIndex]
                * FMatrix44f(
                    BodyComponentPose[BodyBoneIndex].ToMatrixWithScale());
        }
        bAllSyntheticRefToLocalMatricesFinite =
            bAllSyntheticRefToLocalMatricesFinite
            && IsFiniteMatrix(
                OutfitSelfRefToLocal[OutfitBoneIndex])
            && IsFiniteMatrix(
                BodyDrivenRefToLocal[OutfitBoneIndex]);
    }

    auto IsFinitePosition = [](const FVector3f& Position)
    {
        return FMath::IsFinite(Position.X)
            && FMath::IsFinite(Position.Y)
            && FMath::IsFinite(Position.Z);
    };
    auto AddPositionToBounds = [](const FVector3f& Position,
        bool& bHasBounds, FVector3f& Minimum, FVector3f& Maximum)
    {
        if (!bHasBounds)
        {
            Minimum = Position;
            Maximum = Position;
            bHasBounds = true;
            return;
        }
        Minimum.X = FMath::Min(Minimum.X, Position.X);
        Minimum.Y = FMath::Min(Minimum.Y, Position.Y);
        Minimum.Z = FMath::Min(Minimum.Z, Position.Z);
        Maximum.X = FMath::Max(Maximum.X, Position.X);
        Maximum.Y = FMath::Max(Maximum.Y, Position.Y);
        Maximum.Z = FMath::Max(Maximum.Z, Position.Z);
    };
    auto PositionToJson = [](const FVector3f& Position)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Position.X);
        Json->SetNumberField(TEXT("y"), Position.Y);
        Json->SetNumberField(TEXT("z"), Position.Z);
        return Json;
    };
    auto BoundsToJson = [&PositionToJson](bool bHasBounds,
        const FVector3f& Minimum, const FVector3f& Maximum)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetBoolField(TEXT("valid"), bHasBounds);
        Json->SetObjectField(TEXT("minimum_cm"),
            PositionToJson(bHasBounds ? Minimum : FVector3f::ZeroVector));
        Json->SetObjectField(TEXT("maximum_cm"),
            PositionToJson(bHasBounds ? Maximum : FVector3f::ZeroVector));
        Json->SetObjectField(TEXT("extent_cm"),
            PositionToJson(bHasBounds
                ? 0.5f * (Maximum - Minimum)
                : FVector3f::ZeroVector));
        return Json;
    };

    struct FSectionSkinAudit
    {
        bool bVertexRangeValid = false;
        bool bBoneMapValid = false;
        int32 CheckedVertexCount = 0;
        int64 CheckedInfluenceSlotCount = 0;
        int32 InvalidBoneMapEntryCount = 0;
        int32 InvalidInfluenceBoneMapIndexCount = 0;
        int32 InvalidInfluenceMeshBoneIndexCount = 0;
        int32 InvalidInfluenceBodyMappingCount = 0;
        int32 InvalidInfluenceCountVertexCount = 0;
        int32 InvalidInfluenceSpanVertexCount = 0;
        int32 NonNormalizedWeightVertexCount = 0;
        double MinimumNormalizedWeightSum = 0.0;
        double MaximumNormalizedWeightSum = 0.0;
    };
    constexpr double MaximumRawSkinWeight = 65535.0;
    constexpr double NormalizedWeightTolerance =
        1.0 / MaximumRawSkinWeight;

    TArray<TSharedPtr<FJsonValue>> LODValues;
    bool bAllUsedSkinBonesMapped = true;
    bool bAllLODsComplete = bAllSyntheticRefToLocalMatricesFinite;
    int32 TotalEnabledSectionCount = 0;
    int32 TotalBelowKneeVertexCount = 0;
    int32 TotalBelowFootVertexCount = 0;
    double MaximumBodyDrivenDisplacementCm = 0.0;
    for (int32 LODIndex = 0;
         LODIndex < RenderData->LODRenderData.Num();
         ++LODIndex)
    {
        const FSkeletalMeshLODRenderData& LOD =
            RenderData->LODRenderData[LODIndex];
        FSkinWeightVertexBuffer* const SkinWeights =
            OutfitComponent->GetSkinWeightBuffer(LODIndex);
        const uint32 VertexCount = LOD.GetNumVertices();
        const bool bPositionCPUAccessRequested =
            LOD.StaticVertexBuffers.PositionVertexBuffer
                .GetAllowCPUAccess();
        const bool bPositionDataAvailable =
            LOD.StaticVertexBuffers.PositionVertexBuffer
                .GetVertexData() != nullptr;
        const bool bPositionCPUAccessible =
            bPositionCPUAccessRequested && bPositionDataAvailable;
        const FSkinWeightDataVertexBuffer* const SkinWeightData =
            SkinWeights ? SkinWeights->GetDataVertexBuffer() : nullptr;
        const FSkinWeightLookupVertexBuffer* const SkinWeightLookup =
            SkinWeights ? SkinWeights->GetLookupVertexBuffer() : nullptr;
        const bool bSkinWeightCPUAccessRequested = SkinWeights
            && SkinWeights->GetNeedsCPUAccess();
        const bool bSkinWeightDataAvailable = SkinWeightData
            && SkinWeightData->GetWeightData() != nullptr;
        const bool bSkinWeightLookupAvailable = SkinWeights
            && (!SkinWeights->GetVariableBonesPerVertex()
                || (SkinWeightLookup
                    && SkinWeightLookup->GetNeedsCPUAccess()
                    && SkinWeightLookup->GetLookupData() != nullptr
                    && SkinWeightLookup->GetNumVertices()
                        == VertexCount));
        const bool bSkinWeightCPUAccessible =
            bSkinWeightCPUAccessRequested
            && bSkinWeightDataAvailable
            && bSkinWeightLookupAvailable;
        const bool bSkinWeightVertexCountMatches = SkinWeights
            && SkinWeights->GetNumVertices() == VertexCount;
        const uint32 MaximumInfluences = SkinWeights
            ? SkinWeights->GetMaxBoneInfluences() : 0;
        const bool bMaximumInfluenceCountValid =
            MaximumInfluences > 0
            && MaximumInfluences <= MAX_TOTAL_INFLUENCES;
        const USkinnedMeshComponent* const LeaderComponent =
            OutfitComponent->LeaderPoseComponent.Get();
        const bool bLeaderStateSafe = !LeaderComponent
            || OutfitComponent->GetLeaderBoneMap().Num()
                == OutfitSkeleton.GetNum();
        const bool bVertexCountFitsArray =
            VertexCount <= static_cast<uint32>(MAX_int32);
        TArray<uint8> VertexCoverage;
        if (bVertexCountFitsArray)
        {
            VertexCoverage.Init(0, static_cast<int32>(VertexCount));
        }
        TArray<FSectionSkinAudit> SectionAudits;
        SectionAudits.SetNum(LOD.RenderSections.Num());
        bool bSectionBoneMapsValid = !LOD.RenderSections.IsEmpty();
        bool bInfluenceMappingsValid = bSkinWeightCPUAccessible
            && bSkinWeightVertexCountMatches
            && bMaximumInfluenceCountValid
            && bLeaderStateSafe;
        bool bSkinWeightsNormalized = bInfluenceMappingsValid;
        int32 OverlappingVertexCount = 0;
        int32 TotalInvalidInfluenceBoneMapIndexCount = 0;
        int32 TotalInvalidInfluenceMeshBoneIndexCount = 0;
        int32 TotalInvalidInfluenceBodyMappingCount = 0;
        int32 TotalInvalidInfluenceCountVertexCount = 0;
        int32 TotalInvalidInfluenceSpanVertexCount = 0;
        int32 TotalNonNormalizedWeightVertexCount = 0;
        for (int32 SectionIndex = 0;
             SectionIndex < LOD.RenderSections.Num();
             ++SectionIndex)
        {
            const FSkelMeshRenderSection& Section =
                LOD.RenderSections[SectionIndex];
            FSectionSkinAudit& Audit = SectionAudits[SectionIndex];
            Audit.bVertexRangeValid = Section.BaseVertexIndex <= VertexCount
                && Section.NumVertices
                    <= VertexCount - Section.BaseVertexIndex;
            Audit.bBoneMapValid = !Section.BoneMap.IsEmpty();
            for (const FBoneIndexType OutfitBoneIndex : Section.BoneMap)
            {
                if (!OutfitToBodyBone.IsValidIndex(OutfitBoneIndex)
                    || OutfitToBodyBone[OutfitBoneIndex] == INDEX_NONE)
                {
                    Audit.bBoneMapValid = false;
                    bAllUsedSkinBonesMapped = false;
                    ++Audit.InvalidBoneMapEntryCount;
                }
            }
            bSectionBoneMapsValid = bSectionBoneMapsValid
                && Audit.bVertexRangeValid
                && Audit.bBoneMapValid;
            if (!Audit.bVertexRangeValid
                || !bVertexCountFitsArray
                || !bSkinWeightCPUAccessible
                || !bSkinWeightVertexCountMatches
                || !bMaximumInfluenceCountValid
                || !bLeaderStateSafe)
            {
                bInfluenceMappingsValid = false;
                bSkinWeightsNormalized = false;
                continue;
            }

            bool bHasWeightSum = false;
            for (uint32 VertexOffset = 0;
                 VertexOffset < Section.NumVertices;
                 ++VertexOffset)
            {
                const uint32 VertexIndex =
                    Section.BaseVertexIndex + VertexOffset;
                if (VertexCoverage[VertexIndex] != 0)
                {
                    ++OverlappingVertexCount;
                }
                ++VertexCoverage[VertexIndex];
                ++Audit.CheckedVertexCount;

                uint32 VertexWeightOffset = 0;
                uint32 VertexInfluenceCount = 0;
                SkinWeights->GetVertexInfluenceOffsetCount(
                    VertexIndex,
                    VertexWeightOffset,
                    VertexInfluenceCount);
                if (VertexInfluenceCount == 0
                    || VertexInfluenceCount > MaximumInfluences
                    || MaximumInfluences == 0)
                {
                    ++Audit.InvalidInfluenceCountVertexCount;
                }
                const uint64 WeightDataSize =
                    SkinWeightData->GetVertexDataSize();
                const uint64 VertexWeightSpan =
                    static_cast<uint64>(VertexInfluenceCount)
                    * SkinWeights->GetBoneIndexAndWeightByteSize();
                const bool bInfluenceSpanValid =
                    static_cast<uint64>(VertexWeightOffset)
                        <= WeightDataSize
                    && VertexWeightSpan <= WeightDataSize
                        - static_cast<uint64>(VertexWeightOffset);
                if (!bInfluenceSpanValid)
                {
                    ++Audit.InvalidInfluenceSpanVertexCount;
                    ++Audit.NonNormalizedWeightVertexCount;
                    continue;
                }

                uint64 RawWeightSum = 0;
                for (uint32 InfluenceIndex = 0;
                     InfluenceIndex < MaximumInfluences;
                     ++InfluenceIndex)
                {
                    ++Audit.CheckedInfluenceSlotCount;
                    const uint32 BoneMapIndex =
                        SkinWeights->GetBoneIndex(
                            VertexIndex, InfluenceIndex);
                    const uint16 RawWeight =
                        SkinWeights->GetBoneWeight(
                            VertexIndex, InfluenceIndex);
                    RawWeightSum += RawWeight;
                    if (!Section.BoneMap.IsValidIndex(BoneMapIndex))
                    {
                        ++Audit.InvalidInfluenceBoneMapIndexCount;
                        continue;
                    }
                    const int32 OutfitBoneIndex =
                        Section.BoneMap[BoneMapIndex];
                    if (!OutfitToBodyBone.IsValidIndex(OutfitBoneIndex))
                    {
                        ++Audit.InvalidInfluenceMeshBoneIndexCount;
                        continue;
                    }
                    if (OutfitToBodyBone[OutfitBoneIndex] == INDEX_NONE)
                    {
                        ++Audit.InvalidInfluenceBodyMappingCount;
                    }
                }
                const double NormalizedWeightSum =
                    static_cast<double>(RawWeightSum)
                    / MaximumRawSkinWeight;
                if (!bHasWeightSum)
                {
                    Audit.MinimumNormalizedWeightSum =
                        NormalizedWeightSum;
                    Audit.MaximumNormalizedWeightSum =
                        NormalizedWeightSum;
                    bHasWeightSum = true;
                }
                else
                {
                    Audit.MinimumNormalizedWeightSum = FMath::Min(
                        Audit.MinimumNormalizedWeightSum,
                        NormalizedWeightSum);
                    Audit.MaximumNormalizedWeightSum = FMath::Max(
                        Audit.MaximumNormalizedWeightSum,
                        NormalizedWeightSum);
                }
                if (!FMath::IsFinite(NormalizedWeightSum)
                    || NormalizedWeightSum < 0.0
                    || FMath::Abs(NormalizedWeightSum - 1.0)
                        > NormalizedWeightTolerance)
                {
                    ++Audit.NonNormalizedWeightVertexCount;
                }
            }
            TotalInvalidInfluenceBoneMapIndexCount +=
                Audit.InvalidInfluenceBoneMapIndexCount;
            TotalInvalidInfluenceMeshBoneIndexCount +=
                Audit.InvalidInfluenceMeshBoneIndexCount;
            TotalInvalidInfluenceBodyMappingCount +=
                Audit.InvalidInfluenceBodyMappingCount;
            TotalInvalidInfluenceCountVertexCount +=
                Audit.InvalidInfluenceCountVertexCount;
            TotalInvalidInfluenceSpanVertexCount +=
                Audit.InvalidInfluenceSpanVertexCount;
            TotalNonNormalizedWeightVertexCount +=
                Audit.NonNormalizedWeightVertexCount;
        }
        int32 UncoveredVertexCount = 0;
        if (bVertexCountFitsArray)
        {
            for (const uint8 Coverage : VertexCoverage)
            {
                if (Coverage == 0)
                {
                    ++UncoveredVertexCount;
                }
            }
        }
        const bool bEveryVertexCoveredExactlyOnce =
            bVertexCountFitsArray
            && UncoveredVertexCount == 0
            && OverlappingVertexCount == 0;
        bInfluenceMappingsValid = bInfluenceMappingsValid
            && bEveryVertexCoveredExactlyOnce
            && TotalInvalidInfluenceBoneMapIndexCount == 0
            && TotalInvalidInfluenceMeshBoneIndexCount == 0
            && TotalInvalidInfluenceBodyMappingCount == 0
            && TotalInvalidInfluenceCountVertexCount == 0
            && TotalInvalidInfluenceSpanVertexCount == 0;
        bSkinWeightsNormalized = bSkinWeightsNormalized
            && TotalNonNormalizedWeightVertexCount == 0;
        const bool bLODProbeReady = LOD.IsDataReady()
            && VertexCount > 0
            && bAllSyntheticRefToLocalMatricesFinite
            && bPositionCPUAccessible
            && bSkinWeightCPUAccessible
            && bSkinWeightVertexCountMatches
            && bSectionBoneMapsValid
            && bInfluenceMappingsValid
            && bSkinWeightsNormalized;

        TSharedRef<FJsonObject> LODJson = MakeShared<FJsonObject>();
        LODJson->SetNumberField(TEXT("lod_index"), LODIndex);
        LODJson->SetNumberField(TEXT("vertex_count"), VertexCount);
        LODJson->SetNumberField(
            TEXT("section_count"), LOD.RenderSections.Num());
        LODJson->SetNumberField(
            TEXT("active_bone_count"), LOD.ActiveBoneIndices.Num());
        LODJson->SetNumberField(
            TEXT("required_bone_count"), LOD.RequiredBones.Num());
        LODJson->SetBoolField(TEXT("data_ready"), LOD.IsDataReady());
        LODJson->SetBoolField(
            TEXT("resident_by_current_first_lod"),
            LODIndex >= RenderData->CurrentFirstLODIdx);
        LODJson->SetBoolField(
            TEXT("resident_by_pending_first_lod"),
            LODIndex >= RenderData->PendingFirstLODIdx);
        LODJson->SetBoolField(
            TEXT("streamed_data_inlined"),
            LOD.bStreamedDataInlined != 0);
        LODJson->SetBoolField(
            TEXT("lod_optional"), LOD.bIsLODOptional != 0);
        LODJson->SetNumberField(
            TEXT("buffers_size_bytes"), LOD.BuffersSize);
        LODJson->SetBoolField(
            TEXT("position_cpu_access_requested"),
            bPositionCPUAccessRequested);
        LODJson->SetBoolField(
            TEXT("position_data_available"),
            bPositionDataAvailable);
        LODJson->SetBoolField(
            TEXT("position_cpu_accessible"),
            bPositionCPUAccessible);
        LODJson->SetBoolField(
            TEXT("skin_weight_cpu_access_requested"),
            bSkinWeightCPUAccessRequested);
        LODJson->SetBoolField(
            TEXT("skin_weight_data_available"),
            bSkinWeightDataAvailable);
        LODJson->SetBoolField(
            TEXT("skin_weight_lookup_available"),
            bSkinWeightLookupAvailable);
        LODJson->SetBoolField(
            TEXT("skin_weight_cpu_accessible"),
            bSkinWeightCPUAccessible);
        LODJson->SetBoolField(
            TEXT("skin_weight_vertex_count_matches"),
            bSkinWeightVertexCountMatches);
        LODJson->SetNumberField(
            TEXT("maximum_bone_influences"),
            MaximumInfluences);
        LODJson->SetBoolField(
            TEXT("maximum_bone_influence_count_valid"),
            bMaximumInfluenceCountValid);
        LODJson->SetStringField(
            TEXT("leader_component"),
            LeaderComponent ? LeaderComponent->GetPathName() : FString());
        LODJson->SetNumberField(
            TEXT("leader_bone_map_count"),
            OutfitComponent->GetLeaderBoneMap().Num());
        LODJson->SetBoolField(
            TEXT("leader_state_safe_for_cpu_skinning"),
            bLeaderStateSafe);
        LODJson->SetBoolField(
            TEXT("section_bone_maps_valid"),
            bSectionBoneMapsValid);
        LODJson->SetBoolField(
            TEXT("every_vertex_covered_exactly_once"),
            bEveryVertexCoveredExactlyOnce);
        LODJson->SetNumberField(
            TEXT("uncovered_vertex_count"),
            UncoveredVertexCount);
        LODJson->SetNumberField(
            TEXT("overlapping_vertex_count"),
            OverlappingVertexCount);
        LODJson->SetBoolField(
            TEXT("all_influence_mappings_valid"),
            bInfluenceMappingsValid);
        LODJson->SetBoolField(
            TEXT("all_skin_weight_sums_normalized"),
            bSkinWeightsNormalized);
        LODJson->SetNumberField(
            TEXT("invalid_influence_bone_map_index_count"),
            TotalInvalidInfluenceBoneMapIndexCount);
        LODJson->SetNumberField(
            TEXT("invalid_influence_mesh_bone_index_count"),
            TotalInvalidInfluenceMeshBoneIndexCount);
        LODJson->SetNumberField(
            TEXT("invalid_influence_body_mapping_count"),
            TotalInvalidInfluenceBodyMappingCount);
        LODJson->SetNumberField(
            TEXT("invalid_influence_count_vertex_count"),
            TotalInvalidInfluenceCountVertexCount);
        LODJson->SetNumberField(
            TEXT("invalid_influence_span_vertex_count"),
            TotalInvalidInfluenceSpanVertexCount);
        LODJson->SetNumberField(
            TEXT("non_normalized_weight_vertex_count"),
            TotalNonNormalizedWeightVertexCount);
        LODJson->SetNumberField(
            TEXT("normalized_weight_tolerance"),
            NormalizedWeightTolerance);
        LODJson->SetBoolField(TEXT("probe_ready"), bLODProbeReady);

        TArray<FVector3f> SelfSkinnedPositions;
        TArray<FVector3f> BodyDrivenPositions;
        if (bLODProbeReady)
        {
            USkinnedMeshComponent::ComputeSkinnedPositions(
                OutfitComponent,
                SelfSkinnedPositions,
                OutfitSelfRefToLocal,
                LOD,
                *SkinWeights);
            USkinnedMeshComponent::ComputeSkinnedPositions(
                OutfitComponent,
                BodyDrivenPositions,
                BodyDrivenRefToLocal,
                LOD,
                *SkinWeights);
        }
        const bool bPositionCountsMatch = bLODProbeReady
            && SelfSkinnedPositions.Num() == static_cast<int32>(VertexCount)
            && BodyDrivenPositions.Num() == static_cast<int32>(VertexCount);
        LODJson->SetBoolField(
            TEXT("skinned_position_counts_match"),
            bPositionCountsMatch);
        bAllLODsComplete = bAllLODsComplete && bPositionCountsMatch;

        int32 NonFiniteSelfCount = 0;
        int32 NonFiniteBodyDrivenCount = 0;
        int32 SelfBelowKneeCount = 0;
        int32 SelfBelowFootCount = 0;
        int32 BelowKneeCount = 0;
        int32 BelowFootCount = 0;
        double MaximumSelfRestDeltaCm = 0.0;
        double MaximumLODDisplacementCm = 0.0;
        bool bSelfBoundsValid = false;
        bool bBodyBoundsValid = false;
        FVector3f SelfMinimum = FVector3f::ZeroVector;
        FVector3f SelfMaximum = FVector3f::ZeroVector;
        FVector3f BodyMinimum = FVector3f::ZeroVector;
        FVector3f BodyMaximum = FVector3f::ZeroVector;
        if (bPositionCountsMatch)
        {
            for (uint32 VertexIndex = 0;
                 VertexIndex < VertexCount;
                 ++VertexIndex)
            {
                const FVector3f SelfPosition =
                    SelfSkinnedPositions[VertexIndex];
                const FVector3f BodyPosition =
                    BodyDrivenPositions[VertexIndex];
                const FVector3f RestPosition =
                    LOD.StaticVertexBuffers.PositionVertexBuffer
                        .VertexPosition(VertexIndex);
                if (!IsFinitePosition(SelfPosition))
                {
                    ++NonFiniteSelfCount;
                    continue;
                }
                AddPositionToBounds(SelfPosition,
                    bSelfBoundsValid, SelfMinimum, SelfMaximum);
                MaximumSelfRestDeltaCm = FMath::Max(
                    MaximumSelfRestDeltaCm,
                    static_cast<double>((SelfPosition - RestPosition).Size()));
                if (SelfPosition.Z < KneePlaneZ - 1.0)
                {
                    ++SelfBelowKneeCount;
                }
                if (SelfPosition.Z < FootPlaneZ - 1.0)
                {
                    ++SelfBelowFootCount;
                }
                if (!IsFinitePosition(BodyPosition))
                {
                    ++NonFiniteBodyDrivenCount;
                    continue;
                }
                AddPositionToBounds(BodyPosition,
                    bBodyBoundsValid, BodyMinimum, BodyMaximum);
                const double DisplacementCm =
                    (BodyPosition - SelfPosition).Size();
                MaximumLODDisplacementCm = FMath::Max(
                    MaximumLODDisplacementCm, DisplacementCm);
                if (BodyPosition.Z < KneePlaneZ - 1.0)
                {
                    ++BelowKneeCount;
                }
                if (BodyPosition.Z < FootPlaneZ - 1.0)
                {
                    ++BelowFootCount;
                }
            }
        }
        bAllLODsComplete = bAllLODsComplete
            && NonFiniteSelfCount == 0
            && NonFiniteBodyDrivenCount == 0
            && bSelfBoundsValid
            && bBodyBoundsValid;
        MaximumBodyDrivenDisplacementCm = FMath::Max(
            MaximumBodyDrivenDisplacementCm,
            MaximumLODDisplacementCm);
        TotalBelowKneeVertexCount += BelowKneeCount;
        TotalBelowFootVertexCount += BelowFootCount;
        LODJson->SetNumberField(
            TEXT("non_finite_self_skinned_vertex_count"),
            NonFiniteSelfCount);
        LODJson->SetNumberField(
            TEXT("non_finite_body_driven_vertex_count"),
            NonFiniteBodyDrivenCount);
        LODJson->SetNumberField(
            TEXT("maximum_self_skin_to_rest_delta_cm"),
            MaximumSelfRestDeltaCm);
        LODJson->SetNumberField(
            TEXT("maximum_body_driven_displacement_cm"),
            MaximumLODDisplacementCm);
        LODJson->SetNumberField(
            TEXT("self_skinned_below_knee_vertex_count"),
            SelfBelowKneeCount);
        LODJson->SetNumberField(
            TEXT("self_skinned_below_foot_vertex_count"),
            SelfBelowFootCount);
        LODJson->SetNumberField(
            TEXT("body_driven_below_knee_vertex_count"),
            BelowKneeCount);
        LODJson->SetNumberField(
            TEXT("body_driven_below_foot_vertex_count"),
            BelowFootCount);
        LODJson->SetObjectField(
            TEXT("self_skinned_bounds"),
            BoundsToJson(
                bSelfBoundsValid, SelfMinimum, SelfMaximum));
        LODJson->SetObjectField(
            TEXT("body_driven_bounds"),
            BoundsToJson(
                bBodyBoundsValid, BodyMinimum, BodyMaximum));

        TArray<TSharedPtr<FJsonValue>> SectionValues;
        for (int32 SectionIndex = 0;
             SectionIndex < LOD.RenderSections.Num();
             ++SectionIndex)
        {
            const FSkelMeshRenderSection& Section =
                LOD.RenderSections[SectionIndex];
            int32 MaterialIndex = Section.MaterialIndex;
            const FSkeletalMeshLODInfo* const LODInfo =
                ExpectedOutfit->GetLODInfo(LODIndex);
            if (LODInfo
                && LODInfo->LODMaterialMap.IsValidIndex(SectionIndex)
                && LODInfo->LODMaterialMap[SectionIndex] != INDEX_NONE)
            {
                MaterialIndex =
                    LODInfo->LODMaterialMap[SectionIndex];
            }
            const FString MaterialSlot =
                ExpectedOutfit->GetMaterials().IsValidIndex(MaterialIndex)
                ? ExpectedOutfit->GetMaterials()[MaterialIndex]
                    .MaterialSlotName.ToString()
                : FString();
            const UMaterialInterface* const Material =
                ExpectedOutfit->GetMaterials().IsValidIndex(MaterialIndex)
                ? ExpectedOutfit->GetMaterials()[MaterialIndex]
                    .MaterialInterface.Get()
                : nullptr;
            const FString MaterialPath = Material
                ? Material->GetPathName() : FString();
            const bool bEnabled = !Section.bDisabled
                && Section.NumTriangles > 0;
            if (bEnabled)
            {
                ++TotalEnabledSectionCount;
            }
            bool bSelfSectionBoundsValid = false;
            bool bBodySectionBoundsValid = false;
            FVector3f SelfSectionMinimum = FVector3f::ZeroVector;
            FVector3f SelfSectionMaximum = FVector3f::ZeroVector;
            FVector3f BodySectionMinimum = FVector3f::ZeroVector;
            FVector3f BodySectionMaximum = FVector3f::ZeroVector;
            int32 SelfSectionBelowKneeCount = 0;
            int32 SelfSectionBelowFootCount = 0;
            int32 BodySectionBelowKneeCount = 0;
            int32 BodySectionBelowFootCount = 0;
            double SectionMaximumDisplacementCm = 0.0;
            if (bPositionCountsMatch
                && Section.BaseVertexIndex <= VertexCount
                && Section.NumVertices
                    <= VertexCount - Section.BaseVertexIndex)
            {
                for (uint32 VertexOffset = 0;
                     VertexOffset < Section.NumVertices;
                     ++VertexOffset)
                {
                    const uint32 VertexIndex =
                        Section.BaseVertexIndex + VertexOffset;
                    const FVector3f SelfPosition =
                        SelfSkinnedPositions[VertexIndex];
                    const FVector3f BodyPosition =
                        BodyDrivenPositions[VertexIndex];
                    if (!IsFinitePosition(SelfPosition)
                        || !IsFinitePosition(BodyPosition))
                    {
                        continue;
                    }
                    AddPositionToBounds(SelfPosition,
                        bSelfSectionBoundsValid,
                        SelfSectionMinimum,
                        SelfSectionMaximum);
                    AddPositionToBounds(BodyPosition,
                        bBodySectionBoundsValid,
                        BodySectionMinimum,
                        BodySectionMaximum);
                    SectionMaximumDisplacementCm = FMath::Max(
                        SectionMaximumDisplacementCm,
                        static_cast<double>((BodyPosition
                            - SelfPosition).Size()));
                    if (SelfPosition.Z < KneePlaneZ - 1.0)
                    {
                        ++SelfSectionBelowKneeCount;
                    }
                    if (SelfPosition.Z < FootPlaneZ - 1.0)
                    {
                        ++SelfSectionBelowFootCount;
                    }
                    if (BodyPosition.Z < KneePlaneZ - 1.0)
                    {
                        ++BodySectionBelowKneeCount;
                    }
                    if (BodyPosition.Z < FootPlaneZ - 1.0)
                    {
                        ++BodySectionBelowFootCount;
                    }
                }
            }

            TSharedRef<FJsonObject> SectionJson =
                MakeShared<FJsonObject>();
            SectionJson->SetNumberField(
                TEXT("section_index"), SectionIndex);
            SectionJson->SetBoolField(TEXT("enabled"), bEnabled);
            SectionJson->SetNumberField(
                TEXT("authored_material_index"),
                Section.MaterialIndex);
            SectionJson->SetNumberField(
                TEXT("resolved_material_index"), MaterialIndex);
            SectionJson->SetStringField(
                TEXT("material_slot"), MaterialSlot);
            SectionJson->SetStringField(
                TEXT("material_path"), MaterialPath);
            SectionJson->SetNumberField(
                TEXT("base_vertex_index"),
                Section.BaseVertexIndex);
            SectionJson->SetNumberField(
                TEXT("vertex_count"), Section.NumVertices);
            SectionJson->SetNumberField(
                TEXT("triangle_count"), Section.NumTriangles);
            SectionJson->SetNumberField(
                TEXT("bone_map_count"), Section.BoneMap.Num());
            const FSectionSkinAudit& SkinAudit =
                SectionAudits[SectionIndex];
            SectionJson->SetBoolField(
                TEXT("vertex_range_valid"),
                SkinAudit.bVertexRangeValid);
            SectionJson->SetBoolField(
                TEXT("bone_map_valid"),
                SkinAudit.bBoneMapValid);
            SectionJson->SetNumberField(
                TEXT("checked_skin_weight_vertex_count"),
                SkinAudit.CheckedVertexCount);
            SectionJson->SetNumberField(
                TEXT("checked_influence_slot_count"),
                SkinAudit.CheckedInfluenceSlotCount);
            SectionJson->SetNumberField(
                TEXT("invalid_bone_map_entry_count"),
                SkinAudit.InvalidBoneMapEntryCount);
            SectionJson->SetNumberField(
                TEXT("invalid_influence_bone_map_index_count"),
                SkinAudit.InvalidInfluenceBoneMapIndexCount);
            SectionJson->SetNumberField(
                TEXT("invalid_influence_mesh_bone_index_count"),
                SkinAudit.InvalidInfluenceMeshBoneIndexCount);
            SectionJson->SetNumberField(
                TEXT("invalid_influence_body_mapping_count"),
                SkinAudit.InvalidInfluenceBodyMappingCount);
            SectionJson->SetNumberField(
                TEXT("invalid_influence_count_vertex_count"),
                SkinAudit.InvalidInfluenceCountVertexCount);
            SectionJson->SetNumberField(
                TEXT("invalid_influence_span_vertex_count"),
                SkinAudit.InvalidInfluenceSpanVertexCount);
            SectionJson->SetNumberField(
                TEXT("non_normalized_weight_vertex_count"),
                SkinAudit.NonNormalizedWeightVertexCount);
            SectionJson->SetNumberField(
                TEXT("minimum_normalized_weight_sum"),
                SkinAudit.MinimumNormalizedWeightSum);
            SectionJson->SetNumberField(
                TEXT("maximum_normalized_weight_sum"),
                SkinAudit.MaximumNormalizedWeightSum);
            SectionJson->SetNumberField(
                TEXT("maximum_body_driven_displacement_cm"),
                SectionMaximumDisplacementCm);
            SectionJson->SetNumberField(
                TEXT("self_skinned_below_knee_vertex_count"),
                SelfSectionBelowKneeCount);
            SectionJson->SetNumberField(
                TEXT("self_skinned_below_foot_vertex_count"),
                SelfSectionBelowFootCount);
            SectionJson->SetNumberField(
                TEXT("body_driven_below_knee_vertex_count"),
                BodySectionBelowKneeCount);
            SectionJson->SetNumberField(
                TEXT("body_driven_below_foot_vertex_count"),
                BodySectionBelowFootCount);
            SectionJson->SetNumberField(
                TEXT("body_driven_below_knee_fraction"),
                Section.NumVertices > 0
                    ? static_cast<double>(BodySectionBelowKneeCount)
                        / Section.NumVertices
                    : 0.0);
            SectionJson->SetObjectField(
                TEXT("self_skinned_bounds"),
                BoundsToJson(
                    bSelfSectionBoundsValid,
                    SelfSectionMinimum,
                    SelfSectionMaximum));
            SectionJson->SetObjectField(
                TEXT("body_driven_bounds"),
                BoundsToJson(
                    bBodySectionBoundsValid,
                    BodySectionMinimum,
                    BodySectionMaximum));

            TArray<TSharedPtr<FJsonValue>> BoneMapValues;
            for (const FBoneIndexType OutfitBoneIndex : Section.BoneMap)
            {
                TSharedRef<FJsonObject> BoneMapEntry =
                    MakeShared<FJsonObject>();
                BoneMapEntry->SetNumberField(
                    TEXT("outfit_bone_index"), OutfitBoneIndex);
                BoneMapEntry->SetStringField(
                    TEXT("bone_name"),
                    OutfitSkeleton.GetRawRefBoneInfo()
                        .IsValidIndex(OutfitBoneIndex)
                        ? OutfitSkeleton.GetRawRefBoneInfo()[OutfitBoneIndex]
                            .Name.ToString()
                        : FString());
                BoneMapEntry->SetNumberField(
                    TEXT("body_bone_index"),
                    OutfitToBodyBone.IsValidIndex(OutfitBoneIndex)
                        ? OutfitToBodyBone[OutfitBoneIndex]
                        : INDEX_NONE);
                BoneMapValues.Add(
                    MakeShared<FJsonValueObject>(BoneMapEntry));
            }
            SectionJson->SetArrayField(
                TEXT("bone_map"), BoneMapValues);
            SectionValues.Add(
                MakeShared<FJsonValueObject>(SectionJson));
        }
        LODJson->SetArrayField(TEXT("sections"), SectionValues);
        LODValues.Add(MakeShared<FJsonValueObject>(LODJson));
    }

    Report.Root->SetArrayField(TEXT("outfit_render_lods"), LODValues);
    Report.Root->SetBoolField(
        TEXT("all_outfit_raw_bones_mapped_to_body"),
        bAllSkinBonesMapped);
    Report.Root->SetBoolField(
        TEXT("all_skin_bones_mapped_to_body"),
        bAllUsedSkinBonesMapped);
    Report.Root->SetBoolField(
        TEXT("all_synthetic_ref_to_local_matrices_finite"),
        bAllSyntheticRefToLocalMatricesFinite);
    Report.Root->SetBoolField(
        TEXT("all_lods_probe_completed"), bAllLODsComplete);
    Report.Root->SetNumberField(
        TEXT("total_enabled_section_count"),
        TotalEnabledSectionCount);
    Report.Root->SetNumberField(
        TEXT("total_body_driven_below_knee_vertex_count"),
        TotalBelowKneeVertexCount);
    Report.Root->SetNumberField(
        TEXT("total_body_driven_below_foot_vertex_count"),
        TotalBelowFootVertexCount);
    Report.Root->SetNumberField(
        TEXT("maximum_body_driven_vertex_displacement_cm"),
        MaximumBodyDrivenDisplacementCm);
    if (!bAllLODsComplete)
    {
        return FinishFailure(
            TEXT("One or more Outfit render LODs lacked complete CPU skinning evidence"));
    }

    const bool bDirtyBaselinePreserved = SameDirtySet(
        DirtyPackagesBefore, CaptureDirtyPackages());
    Report.Root->SetBoolField(
        TEXT("dirty_package_baseline_preserved"),
        bDirtyBaselinePreserved);
    Report.Root->SetBoolField(TEXT("diagnostic_completed"), true);
    if (!bDirtyBaselinePreserved)
    {
        return FinishFailure(
            TEXT("Outfit diagnostic changed the dirty-package baseline"));
    }
    Report.Pass(
        TEXT("Exact generated Body/Outfit reference skeletons and every Outfit render LOD were diagnosed without asset writes"),
        TEXT("NONE"));
    return Report.Serialize();
}

FString RunValidation()
{
    FRunReport Report(TEXT("VALIDATE"));
    TArray<FPackageSnapshot> Snapshots;
    FString Error;
    const TSet<FName> DirtyPackagesBefore = CaptureDirtyPackages();
    const TSet<FName> NoAllowedDirtyPackages;
    int32 PresentCount = 0;
    if (!SnapshotCanonicalPackages(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    if (HasOrphanCanonicalSidecars(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    if (!AreAllCanonicalFilesPresent(PresentCount))
    {
        Report.Fail(FString::Printf(
            TEXT("Canonical asset state is incomplete: %d of 3 package files exist"),
            PresentCount));
        return Report.Serialize();
    }

    FAssetContext Context;
    if (!LoadDependencies(Context, Error)
        || !LoadCanonicalAssets(Context, Error)
        || !ReloadCanonicalAssetsFromDisk(Context, Report, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    ScanCanonicalFilesFromDisk();
    if (!ValidateAllCanonicalAssets(
            Context, Report, true, false, false, Error)
        || !VerifyPackageSnapshotsUnchanged(Snapshots, Error)
        || !ValidateCanonicalPackagesClean(Error)
        || !ValidateNoUnexpectedNewDirtyPackages(
            DirtyPackagesBefore, NoAllowedDirtyPackages, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }

    Report.Pass(
        TEXT("Exact canonical Session 8B assets passed fresh disk reload and strict runtime-contract validation"),
        TEXT("NONE"));
    return Report.Serialize();
}

FString RunAuthor()
{
    FRunReport Report(TEXT("AUTHOR"));
    TArray<FPackageSnapshot> Snapshots;
    FString Error;
    if (!SnapshotCanonicalPackages(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }
    if (HasOrphanCanonicalSidecars(Snapshots, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }

    int32 PresentCount = 0;
    const bool bAllPresent = AreAllCanonicalFilesPresent(PresentCount);
    if (PresentCount != 0 && !bAllPresent)
    {
        Report.Fail(FString::Printf(
            TEXT("Refusing mixed canonical state without mutation: %d of 3 package files exist"),
            PresentCount));
        return Report.Serialize();
    }

    const TSet<FName> DirtyPackagesBefore = CaptureDirtyPackages();
    const TSet<FName> AllowedDirtyPackages = {
        FName(FPackageName::ObjectPathToPackageName(FString(WrapperBlueprintPath))),
        FName(FPackageName::ObjectPathToPackageName(FString(RetargeterPath))),
        FName(FPackageName::ObjectPathToPackageName(FString(BackendProfilePath))),
    };
    FAssetContext Context;
    if (!LoadDependencies(Context, Error))
    {
        Report.Fail(Error);
        return Report.Serialize();
    }

    if (bAllPresent)
    {
        if (!LoadCanonicalAssets(Context, Error)
            || !ReloadCanonicalAssetsFromDisk(Context, Report, Error))
        {
            Report.Fail(Error);
            return Report.Serialize();
        }
        ScanCanonicalFilesFromDisk();
        if (!ValidateAllCanonicalAssets(
                Context, Report, true, false, false, Error)
            || !VerifyPackageSnapshotsUnchanged(Snapshots, Error)
            || !ValidateCanonicalPackagesClean(Error)
            || !ValidateNoUnexpectedNewDirtyPackages(
                DirtyPackagesBefore, AllowedDirtyPackages, Error))
        {
            Report.Fail(Error);
            return Report.Serialize();
        }
        Report.Root->SetBoolField(TEXT("rollback_verified"), true);
        Report.Root->SetStringField(
            TEXT("rollback_scope"), TEXT("ALL_FILES_WITH_EXACT_CANONICAL_PACKAGE_BASENAMES"));
        Report.Pass(
            TEXT("Canonical Session 8B assets already exist and passed strict fresh-disk validation; no package was written"),
            TEXT("NONE_ALREADY_CURRENT"));
        return Report.Serialize();
    }

    for (const TCHAR* ObjectPath : CanonicalObjectPaths)
    {
        if (LoadAny(ObjectPath))
        {
            return FailAuthorTransaction(
                Context,
                Snapshots,
                Report,
                FString::Printf(
                    TEXT("Unsaved canonical object already exists in memory: %s"),
                    ObjectPath));
        }
    }

    if (!AuthorWrapper(Context, Error)
        || !AuthorRetargeter(Context, Error)
        || !AuthorProfile(Context, Error)
        || !ValidateAllCanonicalAssets(
            Context, Report, false, false, false, Error)
        || !ValidateNoUnexpectedNewDirtyPackages(
            DirtyPackagesBefore, AllowedDirtyPackages, Error))
    {
        return FailAuthorTransaction(Context, Snapshots, Report, Error);
    }

    // The exact durable write order is part of the frozen authoring contract.
    if (!SaveCanonicalAsset(
            Context.WrapperBlueprint, WrapperBlueprintPath, Report, Error)
        || !SaveCanonicalAsset(
            Context.Retargeter, RetargeterPath, Report, Error)
        || !SaveCanonicalAsset(
            Context.BackendProfile, BackendProfilePath, Report, Error)
        || !ValidateNoUnexpectedNewDirtyPackages(
            DirtyPackagesBefore, AllowedDirtyPackages, Error))
    {
        return FailAuthorTransaction(Context, Snapshots, Report, Error);
    }

    ScanCanonicalFilesFromDisk();
    if (!ReloadCanonicalAssetsFromDisk(Context, Report, Error)
        || !ValidateAllCanonicalAssets(
            Context, Report, true, false, false, Error)
        || !ValidateCanonicalPackagesClean(Error)
        || !ValidateNoUnexpectedNewDirtyPackages(
            DirtyPackagesBefore, AllowedDirtyPackages, Error))
    {
        return FailAuthorTransaction(Context, Snapshots, Report, Error);
    }

    Report.Writes = {
        WrapperBlueprintPath,
        RetargeterPath,
        BackendProfilePath,
    };
    Report.Root->SetBoolField(TEXT("rollback_verified"), true);
    Report.Root->SetStringField(
        TEXT("rollback_scope"), TEXT("ALL_FILES_WITH_EXACT_CANONICAL_PACKAGE_BASENAMES"));
    Report.Root->SetStringField(
        TEXT("rollback_verification"),
        TEXT("PREWRITE_ALL_SIDECAR_SNAPSHOTS_AND_POSTWRITE_SCOPE_GATES_ARMED"));
    Report.Pass(
        TEXT("Authored and fresh-disk validated the exact wrapper, retargeter, and backend profile transaction"),
        TEXT("EXACT_THREE_ASSET_WRITES"));
    return Report.Serialize();
}
} // namespace DiscGolfSession8BMetaHuman

FString UDiscGolfSession8BMetaHumanUtility::AuthorSession8BMetaHumanAssets()
{
    return DiscGolfSession8BMetaHuman::RunAuthor();
}

FString UDiscGolfSession8BMetaHumanUtility::ValidateSession8BMetaHumanAssets()
{
    return DiscGolfSession8BMetaHuman::RunValidation();
}

FString UDiscGolfSession8BMetaHumanUtility::
PrepareSession8BMetaHumanRunIKCorrection()
{
    return DiscGolfSession8BMetaHuman::RunPrepareRunIKCorrection();
}

FString UDiscGolfSession8BMetaHumanUtility::
PrepareSession8BMetaHumanRootMotionCorrection()
{
    return DiscGolfSession8BMetaHuman::RunPrepareRootMotionCorrection();
}

FString UDiscGolfSession8BMetaHumanUtility::
PrepareSession8BMetaHumanWrapperOrientationCorrection()
{
    return DiscGolfSession8BMetaHuman::
        RunPrepareWrapperOrientationCorrection();
}

FString UDiscGolfSession8BMetaHumanUtility::
DiagnoseSession8BMetaHumanOutfit()
{
    return DiscGolfSession8BMetaHuman::RunOutfitDiagnostic();
}

FString UDiscGolfSession8BMetaHumanUtility::
DiagnoseSession8BMetaHumanRootMotionCounterfactual()
{
    return DiscGolfSession8BMetaHuman::
        RunRootMotionCounterfactualDiagnostic();
}
