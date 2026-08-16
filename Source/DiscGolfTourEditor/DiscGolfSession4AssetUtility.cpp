#include "DiscGolfSession4AssetUtility.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_ControlRig.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRig.h"
#include "DiscGolfCharacterRigUnits.h"
#include "DiscGolfCharacterTypes.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "JsonObjectConverter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVMVariableDescription.h"
#include "RigVMEditorAsset.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace DiscGolfSession4Assets
{
const TCHAR* ControlRigPath = TEXT("/Game/DiscGolf/Rigs/CR_DG_Master.CR_DG_Master");
const TCHAR* AnimBlueprintPath = TEXT("/Game/DiscGolf/Animation/ABP_DG_Player.ABP_DG_Player");
const TCHAR* SkeletonPath = TEXT("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master");
const TCHAR* CreatorWidgetPath = TEXT("/Game/DiscGolf/UI/WBP_DG_CharacterCreator.WBP_DG_CharacterCreator");
const TCHAR* CreatorWidgetParentPath = TEXT("/Script/DiscGolfTour.DiscGolfCharacterCreatorWidget");
const TCHAR* AnimInstanceParentPath = TEXT("/Script/DiscGolfCharacterFramework.DiscGolfAnimInstance");

const TCHAR* LocalRefPoseClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_LocalRefPose");
const TCHAR* SlotClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_Slot");
const TCHAR* ControlRigNodeClassPath = TEXT("/Script/ControlRigDeveloper.AnimGraphNode_ControlRig");
const TCHAR* RootClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_Root");

const FName ProfileUnitNodeName(TEXT("DGApplyCharacterProfile"));
const FName BeginNodeName(TEXT("BeginExecution"));
const FName PbikNodeName(TEXT("DGFullBodyIK"));
const FName DefaultSlot(TEXT("DefaultSlot"));

struct FVariableSpec
{
    FName Name;
    FGuid Guid;
    FString CPPType;
    UObject* CPPTypeObject = nullptr;
    FString DefaultValue;
    FVector2D Position;
};

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

bool SaveAsset(UObject* Asset, FString& Error)
{
    if (!Asset)
    {
        Error = TEXT("Attempted to save a null Session 4 asset");
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
        Error = FString::Printf(TEXT("Failed to save %s"), *Asset->GetPathName());
        return false;
    }
    return true;
}

template <typename T>
FString StructDefault(const T& Value)
{
    FString Result;
    T::StaticStruct()->ExportText(Result, &Value, nullptr, nullptr, PPF_None, nullptr);
    return Result;
}

TArray<FVariableSpec> RequiredVariables()
{
    FDGBodyProfile Body;
    FDGThrowStyle Style;
    EDGHandedness Handedness = EDGHandedness::Right;
    EDGThrowPhase Phase = EDGThrowPhase::Idle;
    bool bThrowActive = false;

    FRigVMExternalVariable BodyExternal = FRigVMExternalVariable::Make(
        FGuid(), TEXT("BodyProfile"), Body);
    FRigVMExternalVariable StyleExternal = FRigVMExternalVariable::Make(
        FGuid(), TEXT("ThrowStyle"), Style);
    FRigVMExternalVariable HandednessExternal = FRigVMExternalVariable::Make(
        FGuid(), TEXT("Handedness"), Handedness);
    FRigVMExternalVariable PhaseExternal = FRigVMExternalVariable::Make(
        FGuid(), TEXT("ThrowPhase"), Phase);
    FRigVMExternalVariable ActiveExternal = FRigVMExternalVariable::Make(
        FGuid(), TEXT("bThrowActive"), bThrowActive);

    return {
        {TEXT("BodyProfile"), FGuid(0xD6154101u, 0xA11E0004u, 0x50000001u, 0x00000004u), BodyExternal.GetExtendedCPPType().ToString(),
            BodyExternal.GetCPPTypeObject(), StructDefault(Body), FVector2D(-500.0, -500.0)},
        {TEXT("ThrowStyle"), FGuid(0xD6154102u, 0xA11E0004u, 0x50000002u, 0x00000004u), StyleExternal.GetExtendedCPPType().ToString(),
            StyleExternal.GetCPPTypeObject(), StructDefault(Style), FVector2D(-500.0, -350.0)},
        {TEXT("Handedness"), FGuid(0xD6154103u, 0xA11E0004u, 0x50000003u, 0x00000004u), HandednessExternal.GetExtendedCPPType().ToString(),
            HandednessExternal.GetCPPTypeObject(), TEXT("Right"), FVector2D(-500.0, -200.0)},
        {TEXT("ThrowPhase"), FGuid(0xD6154104u, 0xA11E0004u, 0x50000004u, 0x00000004u), PhaseExternal.GetExtendedCPPType().ToString(),
            PhaseExternal.GetCPPTypeObject(), TEXT("Idle"), FVector2D(-500.0, -50.0)},
        {TEXT("bThrowActive"), FGuid(0xD6154105u, 0xA11E0004u, 0x50000005u, 0x00000004u), ActiveExternal.GetExtendedCPPType().ToString(),
            ActiveExternal.GetCPPTypeObject(), TEXT("False"), FVector2D(-500.0, 100.0)},
    };
}

bool IsBlueprintCompileStatusHealthy(const UBlueprint* Blueprint)
{
    return Blueprint &&
        (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
}

bool HasCompletePublicAssetVariableSet(const UControlRigBlueprint* Rig)
{
    if (!Rig)
    {
        return false;
    }
    const TArray<FRigVMGraphVariableDescription> Variables = Rig->GetAssetVariables();
    for (const FVariableSpec& Spec : RequiredVariables())
    {
        int32 MatchCount = 0;
        bool bPublic = false;
        for (const FRigVMGraphVariableDescription& Variable : Variables)
        {
            if (Variable.Name == Spec.Name)
            {
                ++MatchCount;
                bPublic = Variable.bPublic;
            }
        }
        if (MatchCount != 1 || !bPublic)
        {
            return false;
        }
    }
    return true;
}

bool ValidateGeneratedRigVariableContract(UControlRigBlueprint* Rig, FString& Error)
{
    UClass* GeneratedClass = Rig ? Rig->GeneratedClass.Get() : nullptr;
    const UControlRig* DefaultRig = GeneratedClass
        ? Cast<UControlRig>(GeneratedClass->GetDefaultObject())
        : nullptr;
    if (!GeneratedClass || !DefaultRig)
    {
        Error = TEXT("CR_DG_Master has no generated Control Rig class/default object");
        return false;
    }

    for (const FVariableSpec& Spec : RequiredVariables())
    {
        const FRigVMExternalVariable PublicVariable = DefaultRig->GetPublicVariableByName(Spec.Name);
        if (!PublicVariable.IsValid() || !PublicVariable.GetProperty() ||
            PublicVariable.GetName() != Spec.Name || PublicVariable.GetGuid() != Spec.Guid ||
            !PublicVariable.IsPublic() || PublicVariable.IsReadOnly() ||
            PublicVariable.GetExtendedCPPType().ToString() != Spec.CPPType ||
            PublicVariable.GetCPPTypeObject() != Spec.CPPTypeObject)
        {
            Error = FString::Printf(
                TEXT("CR_DG_Master generated/public variable contract mismatch: %s"),
                *Spec.Name.ToString());
            return false;
        }
    }
    return true;
}

bool CompileRigBlueprintAndValidateVariables(UControlRigBlueprint* Rig, FString& Error)
{
    if (!Rig)
    {
        Error = TEXT("CR_DG_Master did not load");
        return false;
    }

    // UE 5.8's legacy AddHostMemberVariableFromExternal only regenerates the
    // skeleton class. A full synchronous compile is required before getter
    // nodes are authored because RigVM compilation reads the generated class.
    FKismetEditorUtilities::CompileBlueprint(Rig, EBlueprintCompileOptions::SkipGarbageCollection);
    if (!IsBlueprintCompileStatusHealthy(Rig))
    {
        Error = FString::Printf(
            TEXT("CR_DG_Master full Blueprint compile status is %d"),
            static_cast<int32>(Rig->Status));
        return false;
    }
    return ValidateGeneratedRigVariableContract(Rig, Error);
}

URigVMNode* FindRigNode(const URigVMGraph* Model, FName Name)
{
    if (!Model)
    {
        return nullptr;
    }
    for (URigVMNode* Node : Model->GetNodes())
    {
        if (Node && Node->GetFName() == Name)
        {
            return Node;
        }
    }
    return nullptr;
}

bool PinHasSourceNode(const URigVMPin* Pin, FName SourceNodeName)
{
    if (!Pin)
    {
        return false;
    }
    const TArray<URigVMLink*> Links = Pin->GetSourceLinks(false);
    return Links.Num() == 1 && Links[0] && Links[0]->GetSourceNode() &&
        Links[0]->GetSourceNode()->GetFName() == SourceNodeName;
}

FString CompactDefaultValue(FString Value)
{
    Value.ReplaceInline(TEXT(" "), TEXT(""));
    Value.ReplaceInline(TEXT("\t"), TEXT(""));
    Value.ReplaceInline(TEXT("\r"), TEXT(""));
    Value.ReplaceInline(TEXT("\n"), TEXT(""));
    return Value;
}

bool PinDefaultEquals(const URigVMNode* Node, const TCHAR* PinPath, const TCHAR* Expected)
{
    const URigVMPin* Pin = Node ? Node->FindPin(PinPath) : nullptr;
    return Pin && Pin->GetDefaultValue().Equals(Expected, ESearchCase::IgnoreCase);
}

bool PinDefaultNameEqualsNormalized(const URigVMNode* Node, const TCHAR* PinPath, const TCHAR* Expected)
{
    const URigVMPin* Pin = Node ? Node->FindPin(PinPath) : nullptr;
    if (!Pin)
    {
        return false;
    }
    FString Value = Pin->GetDefaultValue().TrimStartAndEnd();
    if (Value.Len() >= 2 &&
        ((Value[0] == TCHAR('"') && Value[Value.Len() - 1] == TCHAR('"')) ||
         (Value[0] == TCHAR('\'') && Value[Value.Len() - 1] == TCHAR('\''))))
    {
        Value = Value.Mid(1, Value.Len() - 2).TrimStartAndEnd();
    }
    return Value.Equals(Expected, ESearchCase::IgnoreCase);
}

bool PinDefaultNumberEquals(const URigVMNode* Node, const TCHAR* PinPath, double Expected)
{
    const URigVMPin* Pin = Node ? Node->FindPin(PinPath) : nullptr;
    return Pin && FMath::IsNearlyEqual(FCString::Atod(*Pin->GetDefaultValue()), Expected, 1.0e-6);
}

bool ValidateAcceptedPbik(UControlRigBlueprint* Rig, FString& Error, URigVMUnitNode** OutPbik = nullptr)
{
    URigVMGraph* Model = Rig ? Rig->GetDefaultModel() : nullptr;
    URigVMUnitNode* Pbik = Cast<URigVMUnitNode>(FindRigNode(Model, PbikNodeName));
    const URigVMPin* Effectors = Pbik ? Pbik->FindPin(TEXT("Effectors")) : nullptr;
    const URigVMPin* BoneSettings = Pbik ? Pbik->FindPin(TEXT("BoneSettings")) : nullptr;
    if (!Pbik || !Pbik->GetScriptStruct() ||
        Pbik->GetScriptStruct()->GetPathName() != TEXT("/Script/PBIK.RigUnit_PBIK") ||
        !PinDefaultEquals(Pbik, TEXT("Root"), TEXT("pelvis")) ||
        !PinDefaultEquals(Pbik, TEXT("Settings.RootBehavior"), TEXT("Free")) ||
        !PinDefaultNumberEquals(Pbik, TEXT("Settings.Iterations"), 20.0) ||
        !PinDefaultNumberEquals(Pbik, TEXT("Settings.SubIterations"), 10.0) ||
        !PinDefaultNumberEquals(Pbik, TEXT("Settings.GlobalPullChainAlpha"), 0.0) ||
        !PinDefaultEquals(Pbik, TEXT("Settings.bAllowStretch"), TEXT("False")) ||
        !Effectors || Effectors->GetArraySize() != 4 ||
        !BoneSettings || BoneSettings->GetArraySize() != 4)
    {
        Error = TEXT("CR_DG_Master no longer matches the accepted Session 2 PBIK type/settings; Session 4 refuses to rewrite it");
        return false;
    }

    const TArray<FName> EffectorBones = {
        TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r")};
    for (int32 Index = 0; Index < EffectorBones.Num(); ++Index)
    {
        const FString Prefix = FString::Printf(TEXT("Effectors.%d"), Index);
        const FName Bone = EffectorBones[Index];
        const FName TransformSource(*FString::Printf(TEXT("Get_%s_Control"), *Bone.ToString()));
        const bool bFoot = Bone.ToString().StartsWith(TEXT("foot_"));
        const bool bLeft = Bone.ToString().EndsWith(TEXT("_l"));
        const FString AlphaSourceString = bFoot
            ? FString::Printf(TEXT("Get_DG_FootPlant_%s"), bLeft ? TEXT("L") : TEXT("R"))
            : FString::Printf(TEXT("Get_dg_hand_ik_alpha_%s"), bLeft ? TEXT("l") : TEXT("r"));
        const FName AlphaSource(*AlphaSourceString);
        const URigVMUnitNode* TransformGetter = Cast<URigVMUnitNode>(FindRigNode(Model, TransformSource));
        const URigVMUnitNode* AlphaGetter = Cast<URigVMUnitNode>(FindRigNode(Model, AlphaSource));
        const FString ExpectedAlphaStruct = bFoot
            ? TEXT("/Script/ControlRig.RigUnit_GetCurveValue")
            : TEXT("/Script/ControlRig.RigUnit_GetControlFloat");
        const FString ExpectedTransformControl = FString::Printf(TEXT("ctrl_%s"), *Bone.ToString());
        const FString ExpectedAlphaTarget = bFoot
            ? FString::Printf(TEXT("DG_FootPlant_%s"), bLeft ? TEXT("L") : TEXT("R"))
            : FString::Printf(TEXT("dg_hand_ik_alpha_%s"), bLeft ? TEXT("l") : TEXT("r"));
        if (!PinDefaultEquals(Pbik, *FString::Printf(TEXT("%s.Bone"), *Prefix), *Bone.ToString()) ||
            !PinDefaultNumberEquals(Pbik, *FString::Printf(TEXT("%s.ChainDepth"), *Prefix), 2.0) ||
            !PinHasSourceNode(Pbik->FindPin(FString::Printf(TEXT("%s.Transform"), *Prefix)), TransformSource) ||
            !PinHasSourceNode(Pbik->FindPin(FString::Printf(TEXT("%s.PositionAlpha"), *Prefix)), AlphaSource) ||
            !PinHasSourceNode(Pbik->FindPin(FString::Printf(TEXT("%s.RotationAlpha"), *Prefix)), AlphaSource) ||
            !TransformGetter || !TransformGetter->GetScriptStruct() ||
            TransformGetter->GetScriptStruct()->GetPathName() != TEXT("/Script/ControlRig.RigUnit_GetTransform") ||
            !PinDefaultNameEqualsNormalized(TransformGetter, TEXT("Item.Name"), *ExpectedTransformControl) ||
            !AlphaGetter || !AlphaGetter->GetScriptStruct() ||
            AlphaGetter->GetScriptStruct()->GetPathName() != ExpectedAlphaStruct ||
            !PinDefaultNameEqualsNormalized(
                AlphaGetter, bFoot ? TEXT("Curve") : TEXT("Control"), *ExpectedAlphaTarget))
        {
            Error = FString::Printf(
                TEXT("Accepted PBIK effector/link contract drifted at %s; Session 4 refuses to rewrite it"),
                *Bone.ToString());
            return false;
        }
    }

    const TArray<FName> BendBones = {
        TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("calf_l"), TEXT("calf_r")};
    const FVector BendAngles[] = {
        FVector(0.0, 0.0, -45.0), FVector(0.0, 0.0, 45.0),
        FVector(45.0, 0.0, 0.0), FVector(45.0, 0.0, 0.0)};
    for (int32 Index = 0; Index < BendBones.Num(); ++Index)
    {
        const FString Prefix = FString::Printf(TEXT("BoneSettings.%d"), Index);
        const FName Bone = BendBones[Index];
        if (!PinDefaultEquals(Pbik, *FString::Printf(TEXT("%s.Bone"), *Prefix), *Bone.ToString()) ||
            !PinDefaultEquals(Pbik, *FString::Printf(TEXT("%s.bUsePreferredAngles"), *Prefix), TEXT("True")) ||
            !PinDefaultNumberEquals(Pbik, *FString::Printf(TEXT("%s.PreferredAngles.X"), *Prefix), BendAngles[Index].X) ||
            !PinDefaultNumberEquals(Pbik, *FString::Printf(TEXT("%s.PreferredAngles.Y"), *Prefix), BendAngles[Index].Y) ||
            !PinDefaultNumberEquals(Pbik, *FString::Printf(TEXT("%s.PreferredAngles.Z"), *Prefix), BendAngles[Index].Z) ||
            !PinDefaultNumberEquals(Pbik, *FString::Printf(TEXT("%s.PositionStiffness"), *Prefix), 0.0) ||
            !PinDefaultNumberEquals(Pbik, *FString::Printf(TEXT("%s.RotationStiffness"), *Prefix), 0.0) ||
            !PinDefaultEquals(Pbik, *FString::Printf(TEXT("%s.X"), *Prefix), TEXT("Free")) ||
            !PinDefaultEquals(Pbik, *FString::Printf(TEXT("%s.Y"), *Prefix), TEXT("Free")) ||
            !PinDefaultEquals(Pbik, *FString::Printf(TEXT("%s.Z"), *Prefix), TEXT("Free")))
        {
            Error = FString::Printf(
                TEXT("Accepted PBIK bend contract drifted at %s; Session 4 refuses to rewrite it"),
                *Bone.ToString());
            return false;
        }
    }
    if (OutPbik)
    {
        *OutPbik = Pbik;
    }
    return true;
}

bool EnsureRigVariable(UControlRigBlueprint* Rig, const FVariableSpec& Spec, bool& bChanged, FString& Error)
{
    IRigVMEditorAssetInterface* RigEditorAsset = static_cast<IRigVMEditorAssetInterface*>(Rig);
    for (const FRigVMGraphVariableDescription& Existing : Rig->GetAssetVariables())
    {
        if (Existing.Name != Spec.Name)
        {
            continue;
        }
        if (Existing.CPPType != Spec.CPPType || Existing.CPPTypeObject != Spec.CPPTypeObject)
        {
            Error = FString::Printf(
                TEXT("Control Rig variable %s has an unexpected type; refusing to rewrite it"),
                *Spec.Name.ToString());
            return false;
        }
        if (!CompactDefaultValue(Existing.DefaultValue).Equals(
                CompactDefaultValue(Spec.DefaultValue), ESearchCase::IgnoreCase))
        {
            // UE 5.8's legacy ChangeMemberVariableType ignores its default-value
            // argument. Treat drift as immutable rather than saving a false repair.
            Error = FString::Printf(
                TEXT("Control Rig variable %s has an unexpected default; refusing to rewrite it"),
                *Spec.Name.ToString());
            return false;
        }
        if (!Existing.bPublic && !RigEditorAsset->SetVariablePublic(Spec.Name, true))
        {
            Error = FString::Printf(TEXT("Could not expose Control Rig variable %s"), *Spec.Name.ToString());
            return false;
        }
        bChanged |= !Existing.bPublic;
        return true;
    }

    const FRigVMExternalVariable External = FRigVMExternalVariable::Make(
        Spec.Guid, Spec.Name, Spec.CPPType, Spec.CPPTypeObject, true, false);
    const FName Added = RigEditorAsset->AddHostMemberVariableFromExternal(External, Spec.DefaultValue);
    if (Added.IsNone())
    {
        Error = FString::Printf(TEXT("Could not add Control Rig variable %s"), *Spec.Name.ToString());
        return false;
    }
    bChanged = true;
    return true;
}

bool PreflightRigVariableOwnership(UControlRigBlueprint* Rig, FString& Error)
{
    if (!Rig)
    {
        Error = TEXT("CR_DG_Master did not load");
        return false;
    }
    const TArray<FRigVMGraphVariableDescription> ExistingVariables = Rig->GetAssetVariables();
    for (const FVariableSpec& Spec : RequiredVariables())
    {
        const FRigVMGraphVariableDescription* Existing = ExistingVariables.FindByPredicate(
            [&Spec](const FRigVMGraphVariableDescription& Candidate) { return Candidate.Name == Spec.Name; });
        if (!Existing)
        {
            continue;
        }
        if (Existing->CPPType != Spec.CPPType || Existing->CPPTypeObject != Spec.CPPTypeObject ||
            !CompactDefaultValue(Existing->DefaultValue).Equals(
                CompactDefaultValue(Spec.DefaultValue), ESearchCase::IgnoreCase))
        {
            Error = FString::Printf(
                TEXT("CR_DG_Master variable %s is outside the owned Session 4 contract; refusing all asset writes"),
                *Spec.Name.ToString());
            return false;
        }
    }
    return true;
}

bool EnsureRigGraph(UControlRigBlueprint* Rig, bool& bChanged, FString& Error)
{
    URigVMGraph* Model = Rig->GetDefaultModel();
    URigVMController* Controller = Model ? Rig->GetControllerByName(Model->GetName()) : nullptr;
    if (!Model || !Controller)
    {
        Error = TEXT("CR_DG_Master has no editable default RigVM model");
        return false;
    }

    URigVMNode* Begin = FindRigNode(Model, BeginNodeName);
    URigVMNode* Pbik = FindRigNode(Model, PbikNodeName);
    if (!Begin || !Pbik)
    {
        Error = TEXT("CR_DG_Master is missing the accepted BeginExecution or DGFullBodyIK node");
        return false;
    }

    URigVMUnitNode* ProfileUnit = Cast<URigVMUnitNode>(FindRigNode(Model, ProfileUnitNodeName));
    if (ProfileUnit && ProfileUnit->GetScriptStruct() != FRigUnit_DGApplyCharacterProfile::StaticStruct())
    {
        Error = TEXT("DGApplyCharacterProfile exists with an unexpected unit type");
        return false;
    }
    if (!ProfileUnit)
    {
        const FRigUnit_DGApplyCharacterProfile Defaults;
        ProfileUnit = Controller->AddUnitNodeWithDefaults(
            FRigUnit_DGApplyCharacterProfile::StaticStruct(), FRigStructScope(Defaults), FRigVMStruct::ExecuteName,
            FVector2D(40.0, 0.0), ProfileUnitNodeName.ToString(), false, false);
        if (!ProfileUnit)
        {
            Error = TEXT("Could not add DGApplyCharacterProfile to CR_DG_Master");
            return false;
        }
        bChanged = true;
    }

    const TArray<FVariableSpec> Specs = RequiredVariables();
    for (const FVariableSpec& Spec : Specs)
    {
        URigVMVariableNode* Getter = Cast<URigVMVariableNode>(FindRigNode(
            Model, FName(*FString::Printf(TEXT("Get_%s"), *Spec.Name.ToString()))));
        if (Getter && (!Getter->IsGetter() || Getter->GetVariableName() != Spec.Name ||
            Getter->GetCPPType() != Spec.CPPType || Getter->GetCPPTypeObject() != Spec.CPPTypeObject))
        {
            Error = FString::Printf(TEXT("Control Rig getter for %s has an unexpected contract"), *Spec.Name.ToString());
            return false;
        }
        if (!Getter)
        {
            Getter = Controller->AddVariableNode(Spec.Name, Spec.CPPType, Spec.CPPTypeObject, true,
                Spec.DefaultValue, Spec.Position,
                FString::Printf(TEXT("Get_%s"), *Spec.Name.ToString()), false, false);
            if (!Getter)
            {
                Error = FString::Printf(TEXT("Could not add getter for %s"), *Spec.Name.ToString());
                return false;
            }
            bChanged = true;
        }
        URigVMPin* ValuePin = Getter->GetValuePin();
        URigVMPin* InputPin = ProfileUnit->FindPin(Spec.Name.ToString());
        if (!ValuePin || !InputPin)
        {
            Error = FString::Printf(TEXT("Missing profile pin for %s"), *Spec.Name.ToString());
            return false;
        }
        if (!PinHasSourceNode(InputPin, Getter->GetFName()))
        {
            for (URigVMLink* Link : InputPin->GetSourceLinks(false))
            {
                if (Link)
                {
                    Controller->BreakLink(Link->GetSourcePinPath(), Link->GetTargetPinPath(), false, false);
                }
            }
            if (!Controller->AddLink(ValuePin->GetPinPath(), InputPin->GetPinPath(), false, false))
            {
                Error = FString::Printf(TEXT("Could not wire %s into the profile unit"), *Spec.Name.ToString());
                return false;
            }
            bChanged = true;
        }
    }

    URigVMPin* BeginExec = Begin->FindPin(TEXT("ExecuteContext"));
    URigVMPin* UnitExec = ProfileUnit->FindPin(TEXT("ExecuteContext"));
    URigVMPin* PbikExec = Pbik->FindPin(TEXT("ExecuteContext"));
    if (!BeginExec || !UnitExec || !PbikExec)
    {
        Error = TEXT("Control Rig execution pins are missing");
        return false;
    }
    if (!PinHasSourceNode(UnitExec, BeginNodeName))
    {
        for (URigVMLink* Link : UnitExec->GetSourceLinks(false))
        {
            if (Link) Controller->BreakLink(Link->GetSourcePinPath(), Link->GetTargetPinPath(), false, false);
        }
        if (!Controller->AddLink(BeginExec->GetPinPath(), UnitExec->GetPinPath(), false, false))
        {
            Error = TEXT("Could not wire BeginExecution to DGApplyCharacterProfile");
            return false;
        }
        bChanged = true;
    }
    if (!PinHasSourceNode(PbikExec, ProfileUnitNodeName))
    {
        for (URigVMLink* Link : PbikExec->GetSourceLinks(false))
        {
            if (Link) Controller->BreakLink(Link->GetSourcePinPath(), Link->GetTargetPinPath(), false, false);
        }
        if (!Controller->AddLink(UnitExec->GetPinPath(), PbikExec->GetPinPath(), false, false))
        {
            Error = TEXT("Could not wire DGApplyCharacterProfile to DGFullBodyIK");
            return false;
        }
        bChanged = true;
    }
    return true;
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

UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    if (!Node) return nullptr;
    for (UEdGraphPin* Pin : Node->GetAllPins())
    {
        if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType))
        {
            return Pin;
        }
    }
    // The Session 3 helper intentionally used direction-only matching; retain
    // that safe fallback for engine presentation changes to the pose pin type.
    for (UEdGraphPin* Pin : Node->GetAllPins())
    {
        if (Pin && Pin->Direction == Direction) return Pin;
    }
    return nullptr;
}

bool PinsAreExclusivelyLinked(const UEdGraphPin* Output, const UEdGraphPin* Input)
{
    return Output && Input && Output->LinkedTo.Num() == 1 && Input->LinkedTo.Num() == 1 &&
        Output->LinkedTo[0] == Input && Input->LinkedTo[0] == Output;
}

FName ReadSlotName(const UEdGraphNode* SlotNode)
{
    const FStructProperty* NodeProperty = SlotNode
        ? FindFProperty<FStructProperty>(SlotNode->GetClass(), TEXT("Node"))
        : nullptr;
    const FNameProperty* SlotProperty = NodeProperty
        ? FindFProperty<FNameProperty>(NodeProperty->Struct, TEXT("SlotName"))
        : nullptr;
    if (!NodeProperty || !SlotProperty)
    {
        return NAME_None;
    }
    const void* NodeAddress = NodeProperty->ContainerPtrToValuePtr<void>(SlotNode);
    return SlotProperty->GetPropertyValue_InContainer(NodeAddress);
}

template <typename TProperty>
bool SetAnimNodeProperty(FAnimNode_ControlRig& Node, const TCHAR* Name,
    typename TProperty::TCppType Value, FString& Error)
{
    TProperty* Property = FindFProperty<TProperty>(FAnimNode_ControlRig::StaticStruct(), Name);
    if (!Property)
    {
        Error = FString::Printf(TEXT("FAnimNode_ControlRig property is missing: %s"), Name);
        return false;
    }
    Property->SetPropertyValue_InContainer(&Node, Value);
    return true;
}

bool SetAnimNodeAlphaInputType(FAnimNode_ControlRig& Node, EAnimAlphaInputType Value, FString& Error)
{
    FEnumProperty* Property = FindFProperty<FEnumProperty>(
        FAnimNode_ControlRig::StaticStruct(), TEXT("AlphaInputType"));
    if (!Property || !Property->GetUnderlyingProperty())
    {
        Error = TEXT("FAnimNode_ControlRig AlphaInputType property is missing");
        return false;
    }
    void* Address = Property->ContainerPtrToValuePtr<void>(&Node);
    Property->GetUnderlyingProperty()->SetIntPropertyValue(Address, static_cast<int64>(Value));
    return true;
}

EAnimAlphaInputType ReadAnimNodeAlphaInputType(const FAnimNode_ControlRig& Node)
{
    const FEnumProperty* Property = FindFProperty<FEnumProperty>(
        FAnimNode_ControlRig::StaticStruct(), TEXT("AlphaInputType"));
    if (!Property || !Property->GetUnderlyingProperty())
    {
        return EAnimAlphaInputType::Bool;
    }
    const void* Address = Property->ContainerPtrToValuePtr<void>(&Node);
    return static_cast<EAnimAlphaInputType>(
        Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(Address));
}

void EmptyNameArrayProperty(FAnimNode_ControlRig& Node, const TCHAR* Name)
{
    if (FArrayProperty* Property = FindFProperty<FArrayProperty>(FAnimNode_ControlRig::StaticStruct(), Name))
    {
        FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(&Node));
        Helper.EmptyValues();
    }
}

TArray<FName> ReadNameArrayProperty(const FAnimNode_ControlRig& Node, const TCHAR* Name)
{
    TArray<FName> Result;
    const FArrayProperty* Property = FindFProperty<FArrayProperty>(FAnimNode_ControlRig::StaticStruct(), Name);
    const FNameProperty* Inner = Property ? CastField<FNameProperty>(Property->Inner) : nullptr;
    if (!Property || !Inner)
    {
        return Result;
    }
    FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(&Node));
    Result.Reserve(Helper.Num());
    for (int32 Index = 0; Index < Helper.Num(); ++Index)
    {
        Result.Add(Inner->GetPropertyValue(Helper.GetRawPtr(Index)));
    }
    return Result;
}

template <typename TProperty>
typename TProperty::TCppType ReadAnimNodeProperty(const FAnimNode_ControlRig& Node, const TCHAR* Name,
    typename TProperty::TCppType Fallback)
{
    const TProperty* Property = FindFProperty<TProperty>(FAnimNode_ControlRig::StaticStruct(), Name);
    return Property ? Property->GetPropertyValue_InContainer(&Node) : Fallback;
}

bool ValidateAnimBlueprint(UAnimBlueprint* Blueprint, UControlRigBlueprint* Rig,
    TSharedPtr<FJsonObject>& Out, FString& Error);
bool ValidateRig(UControlRigBlueprint* Rig, TSharedPtr<FJsonObject>& Out, FString& Error);

bool PreflightAnimBlueprintOwnership(UAnimBlueprint* Blueprint, FString& Error)
{
    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
    UEdGraph* Graph = Blueprint ? FindAnimGraph(Blueprint) : nullptr;
    if (!Blueprint || !Skeleton || Blueprint->TargetSkeleton != Skeleton ||
        !Blueprint->ParentClass || Blueprint->ParentClass->GetPathName() != AnimInstanceParentPath || !Graph)
    {
        Error = TEXT("ABP_DG_Player has an unexpected skeleton, parent, or AnimGraph; refusing all asset writes");
        return false;
    }

    int32 RefCount = 0;
    int32 SlotCount = 0;
    int32 ControlRigCount = 0;
    int32 RootCount = 0;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        const FString ClassPath = Node ? Node->GetClass()->GetPathName() : FString();
        if (ClassPath == LocalRefPoseClassPath) ++RefCount;
        else if (ClassPath == SlotClassPath) ++SlotCount;
        else if (ClassPath == ControlRigNodeClassPath) ++ControlRigCount;
        else if (ClassPath == RootClassPath) ++RootCount;
        else
        {
            Error = FString::Printf(
                TEXT("ABP_DG_Player contains an unexpected node %s; refusing all asset writes"),
                *ClassPath);
            return false;
        }
    }
    if (RefCount != 1 || SlotCount != 1 || RootCount != 1 || ControlRigCount > 1)
    {
        Error = TEXT("ABP_DG_Player node ownership is ambiguous; refusing all asset writes");
        return false;
    }
    return true;
}

bool PreflightCreatorWidgetOwnership(FString& Error)
{
    UWidgetBlueprint* Widget = LoadObject<UWidgetBlueprint>(nullptr, CreatorWidgetPath);
    if (!Widget)
    {
        const FString PackageName = FPackageName::ObjectPathToPackageName(FString(CreatorWidgetPath));
        if (FPackageName::DoesPackageExist(PackageName))
        {
            Error = TEXT("WBP_DG_CharacterCreator package exists but is not a loadable Widget Blueprint; refusing all asset writes");
            return false;
        }
        return true;
    }
    UClass* ParentClass = LoadObject<UClass>(nullptr, CreatorWidgetParentPath);
    if (!ParentClass || Widget->ParentClass != ParentClass ||
        (Widget->WidgetTree && Widget->WidgetTree->RootWidget) ||
        !Widget->Bindings.IsEmpty() || !Widget->Animations.IsEmpty())
    {
        Error = TEXT("Existing WBP_DG_CharacterCreator is not the owned empty foundation; refusing all asset writes");
        return false;
    }
    return true;
}

bool ConfigureAnimGraph(UAnimBlueprint* Blueprint, UControlRigBlueprint* Rig,
    bool& bChanged, FString& Error)
{
    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
    if (!Blueprint || !Rig || !Skeleton || Blueprint->TargetSkeleton != Skeleton ||
        !Blueprint->ParentClass || Blueprint->ParentClass->GetPathName() != AnimInstanceParentPath)
    {
        Error = TEXT("Refusing to mutate ABP_DG_Player with an unexpected skeleton or parent class");
        return false;
    }
    TSharedPtr<FJsonObject> CurrentContract;
    FString CurrentError;
    if (ValidateAnimBlueprint(Blueprint, Rig, CurrentContract, CurrentError))
    {
        return true;
    }

    UEdGraph* Graph = FindAnimGraph(Blueprint);
    UClass* GeneratedRigClass = Rig->GeneratedClass.Get();
    if (!Graph || !GeneratedRigClass || !GeneratedRigClass->IsChildOf(UControlRig::StaticClass()))
    {
        Error = TEXT("ABP_DG_Player AnimGraph or CR_DG_Master generated class is missing");
        return false;
    }
    const TSubclassOf<UControlRig> ControlRigClass(GeneratedRigClass);

    UEdGraphNode* RefNode = nullptr;
    UEdGraphNode* SlotNode = nullptr;
    UAnimGraphNode_ControlRig* ControlNode = nullptr;
    UEdGraphNode* RootNode = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        const FString ClassPath = Node ? Node->GetClass()->GetPathName() : FString();
        if (ClassPath == LocalRefPoseClassPath) RefNode = Node;
        else if (ClassPath == SlotClassPath) SlotNode = Node;
        else if (ClassPath == ControlRigNodeClassPath) ControlNode = Cast<UAnimGraphNode_ControlRig>(Node);
        else if (ClassPath == RootClassPath) RootNode = Node;
        else
        {
            Error = FString::Printf(TEXT("Refusing to replace unexpected ABP node: %s"), *ClassPath);
            return false;
        }
    }
    if (!RefNode || !SlotNode || !RootNode)
    {
        Error = TEXT("ABP_DG_Player is missing the accepted RefPose, DefaultSlot, or Root node");
        return false;
    }
    Graph->Modify();
    Blueprint->Modify();
    if (!ControlNode)
    {
        ControlNode = NewObject<UAnimGraphNode_ControlRig>(Graph, NAME_None, RF_Transactional);
        Graph->AddNode(ControlNode, false, false);
        ControlNode->NodeGuid = FGuid(0xD6154001u, 0xA11E0004u, 0x50000001u, 0x00000004u);
        ControlNode->NodePosX = 330;
        ControlNode->NodePosY = 0;
        ControlNode->Node.SetControlRigClass(ControlRigClass);
        ControlNode->PostPlacedNewNode();
        ControlNode->AllocateDefaultPins();
        bChanged = true;
    }
    else
    {
        ControlNode->Modify();
    }

    ControlNode->Node.SetControlRigClass(ControlRigClass);
    if (!SetAnimNodeProperty<FBoolProperty>(ControlNode->Node, TEXT("bResetInputPoseToInitial"), false, Error) ||
        !SetAnimNodeProperty<FBoolProperty>(ControlNode->Node, TEXT("bTransferInputPose"), true, Error) ||
        !SetAnimNodeProperty<FBoolProperty>(ControlNode->Node, TEXT("bTransferInputCurves"), true, Error) ||
        !SetAnimNodeProperty<FFloatProperty>(ControlNode->Node, TEXT("Alpha"), 1.0f, Error) ||
        !SetAnimNodeAlphaInputType(ControlNode->Node, EAnimAlphaInputType::Float, Error))
    {
        return false;
    }

    ControlNode->ReconstructNode();
    EmptyNameArrayProperty(ControlNode->Node, TEXT("SourcePropertyNames"));
    EmptyNameArrayProperty(ControlNode->Node, TEXT("DestPropertyNames"));
    for (const FVariableSpec& Spec : RequiredVariables())
    {
        ControlNode->AddSourceTargetProperties(Spec.Name, Spec.Name);
    }

    UEdGraphPin* RefOut = FindPosePin(RefNode, EGPD_Output);
    UEdGraphPin* SlotIn = FindPosePin(SlotNode, EGPD_Input);
    UEdGraphPin* SlotOut = FindPosePin(SlotNode, EGPD_Output);
    UEdGraphPin* ControlIn = FindPosePin(ControlNode, EGPD_Input);
    UEdGraphPin* ControlOut = FindPosePin(ControlNode, EGPD_Output);
    UEdGraphPin* RootIn = FindPosePin(RootNode, EGPD_Input);
    if (!RefOut || !SlotIn || !SlotOut || !ControlIn || !ControlOut || !RootIn)
    {
        Error = TEXT("Could not resolve Session 4 ABP pose pins");
        return false;
    }
    RefOut->BreakAllPinLinks(false, false);
    SlotIn->BreakAllPinLinks(false, false);
    SlotOut->BreakAllPinLinks(false, false);
    ControlIn->BreakAllPinLinks(false, false);
    ControlOut->BreakAllPinLinks(false, false);
    RootIn->BreakAllPinLinks(false, false);
    RefOut->MakeLinkTo(SlotIn, false);
    SlotOut->MakeLinkTo(ControlIn, false);
    ControlOut->MakeLinkTo(RootIn, false);
    RootNode->NodePosX = 700;
    Graph->NotifyGraphChanged();

    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    if (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings)
    {
        Error = FString::Printf(TEXT("ABP_DG_Player compile status is %d"), static_cast<int32>(Blueprint->Status));
        return false;
    }
    bChanged = true;
    return true;
}

UWidgetBlueprint* EnsureCreatorWidget(bool& bChanged, FString& Error)
{
    UClass* ParentClass = LoadObject<UClass>(nullptr, CreatorWidgetParentPath);
    if (!ParentClass)
    {
        Error = FString::Printf(TEXT("Creator parent class did not load: %s"), CreatorWidgetParentPath);
        return nullptr;
    }
    UWidgetBlueprint* Widget = LoadObject<UWidgetBlueprint>(nullptr, CreatorWidgetPath);
    if (Widget)
    {
        if (Widget->ParentClass != ParentClass)
        {
            Error = TEXT("WBP_DG_CharacterCreator exists with the wrong parent; refusing to replace it");
            return nullptr;
        }
        if ((Widget->WidgetTree && Widget->WidgetTree->RootWidget) ||
            !Widget->Bindings.IsEmpty() || !Widget->Animations.IsEmpty())
        {
            Error = TEXT("WBP_DG_CharacterCreator is not the owned empty foundation; refusing to remove authored UI");
            return nullptr;
        }
        if (Widget->Status != BS_UpToDate && Widget->Status != BS_UpToDateWithWarnings)
        {
            Widget->Modify();
            FKismetEditorUtilities::CompileBlueprint(Widget, EBlueprintCompileOptions::SkipGarbageCollection);
            if (Widget->Status != BS_UpToDate && Widget->Status != BS_UpToDateWithWarnings)
            {
                Error = FString::Printf(TEXT("WBP_DG_CharacterCreator compile status is %d"),
                    static_cast<int32>(Widget->Status));
                return nullptr;
            }
            bChanged = true;
        }
        return Widget;
    }

    const FString PackageName = FPackageName::ObjectPathToPackageName(FString(CreatorWidgetPath));
    const FString AssetName = FPackageName::ObjectPathToObjectName(FString(CreatorWidgetPath));
    UPackage* Package = CreatePackage(*PackageName);
    UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
    Factory->BlueprintType = BPTYPE_Normal;
    Factory->ParentClass = ParentClass;
    Widget = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(
        UWidgetBlueprint::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!Widget)
    {
        Error = TEXT("WidgetBlueprintFactory failed to create WBP_DG_CharacterCreator");
        return nullptr;
    }
    if (Widget->WidgetTree)
    {
        Widget->WidgetTree->Modify();
        Widget->WidgetTree->RootWidget = nullptr;
    }
    Widget->Bindings.Reset();
    Widget->Animations.Reset();
    FAssetRegistryModule::AssetCreated(Widget);
    FKismetEditorUtilities::CompileBlueprint(Widget, EBlueprintCompileOptions::SkipGarbageCollection);
    if (Widget->Status != BS_UpToDate && Widget->Status != BS_UpToDateWithWarnings)
    {
        Error = FString::Printf(TEXT("WBP_DG_CharacterCreator compile status is %d"), static_cast<int32>(Widget->Status));
        return nullptr;
    }
    bChanged = true;
    return Widget;
}

bool ValidateRig(UControlRigBlueprint* Rig, TSharedPtr<FJsonObject>& Out, FString& Error)
{
    Out = MakeShared<FJsonObject>();
    if (!Rig)
    {
        Error = TEXT("CR_DG_Master did not load");
        return false;
    }
    if (!IsBlueprintCompileStatusHealthy(Rig))
    {
        Error = FString::Printf(
            TEXT("CR_DG_Master is not fully compiled (status %d)"),
            static_cast<int32>(Rig->Status));
        return false;
    }
    if (!ValidateGeneratedRigVariableContract(Rig, Error))
    {
        return false;
    }
    const TArray<FRigVMGraphVariableDescription> Variables = Rig->GetAssetVariables();
    TArray<TSharedPtr<FJsonValue>> VariableJson;
    for (const FVariableSpec& Spec : RequiredVariables())
    {
        const FRigVMGraphVariableDescription* Match = Variables.FindByPredicate(
            [&Spec](const FRigVMGraphVariableDescription& Candidate) { return Candidate.Name == Spec.Name; });
        if (!Match || Match->CPPType != Spec.CPPType || Match->CPPTypeObject != Spec.CPPTypeObject ||
            Match->Guid != Spec.Guid || !Match->bPublic ||
            !CompactDefaultValue(Match->DefaultValue).Equals(
                CompactDefaultValue(Spec.DefaultValue), ESearchCase::IgnoreCase))
        {
            Error = FString::Printf(TEXT("Control Rig public variable mismatch: %s"), *Spec.Name.ToString());
            return false;
        }
        VariableJson.Add(MakeShared<FJsonValueString>(Spec.Name.ToString()));
    }
    URigVMGraph* Model = Rig->GetDefaultModel();
    URigVMUnitNode* Unit = Cast<URigVMUnitNode>(FindRigNode(Model, ProfileUnitNodeName));
    URigVMUnitNode* Pbik = nullptr;
    FString PbikError;
    if (!ValidateAcceptedPbik(Rig, PbikError, &Pbik))
    {
        Error = PbikError;
        return false;
    }
    if (!Unit || Unit->GetScriptStruct() != FRigUnit_DGApplyCharacterProfile::StaticStruct() ||
        !PinHasSourceNode(Unit->FindPin(TEXT("ExecuteContext")), BeginNodeName) ||
        !PinHasSourceNode(Pbik->FindPin(TEXT("ExecuteContext")), ProfileUnitNodeName))
    {
        Error = TEXT("Control Rig must preserve the accepted PBIK type/settings and execute BeginExecution -> DGApplyCharacterProfile -> DGFullBodyIK");
        return false;
    }
    for (const FVariableSpec& Spec : RequiredVariables())
    {
        URigVMVariableNode* Getter = Cast<URigVMVariableNode>(FindRigNode(
            Model, FName(*FString::Printf(TEXT("Get_%s"), *Spec.Name.ToString()))));
        if (!Getter || !Getter->IsGetter() || Getter->GetVariableName() != Spec.Name ||
            !PinHasSourceNode(Unit->FindPin(Spec.Name.ToString()), Getter->GetFName()))
        {
            Error = FString::Printf(TEXT("Control Rig input wiring mismatch: %s"), *Spec.Name.ToString());
            return false;
        }
    }
    Out->SetStringField(TEXT("status"), TEXT("PASS"));
    Out->SetStringField(TEXT("path"), Rig->GetPathName());
    Out->SetStringField(TEXT("execution"), TEXT("BeginExecution -> DGApplyCharacterProfile -> DGFullBodyIK"));
    Out->SetStringField(TEXT("pbik_type"), TEXT("/Script/PBIK.RigUnit_PBIK"));
    Out->SetStringField(TEXT("pbik_settings"),
        TEXT("Root=pelvis Free Iterations=20 SubIterations=10 GlobalPull=0 Stretch=false Effectors=4 BoneSettings=4"));
    Out->SetStringField(TEXT("idle_hand_ik_reset_contract"),
        TEXT("IMPLEMENTATION_STATIC_CHECK_REQUIRES_BOTH_ALPHAS_ZEROED_BEFORE_ACTIVE_THROW_WEIGHT"));
    Out->SetArrayField(TEXT("public_variables"), VariableJson);
    return true;
}

bool ValidateAnimBlueprint(UAnimBlueprint* Blueprint, UControlRigBlueprint* Rig,
    TSharedPtr<FJsonObject>& Out, FString& Error)
{
    Out = MakeShared<FJsonObject>();
    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
    UEdGraph* Graph = Blueprint ? FindAnimGraph(Blueprint) : nullptr;
    if (!Blueprint || !Rig || !Rig->GeneratedClass || !Skeleton || Blueprint->TargetSkeleton != Skeleton ||
        !Blueprint->ParentClass || Blueprint->ParentClass->GetPathName() != AnimInstanceParentPath ||
        (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings) ||
        !Graph || Graph->Nodes.Num() != 4)
    {
        Error = TEXT("ABP_DG_Player must retain its master skeleton, anim-instance parent, and four Session 4 pose nodes");
        return false;
    }
    UEdGraphNode* Ref = nullptr;
    UEdGraphNode* Slot = nullptr;
    UAnimGraphNode_ControlRig* Control = nullptr;
    UEdGraphNode* Root = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        const FString Path = Node->GetClass()->GetPathName();
        if (Path == LocalRefPoseClassPath) Ref = Node;
        else if (Path == SlotClassPath) Slot = Node;
        else if (Path == ControlRigNodeClassPath) Control = Cast<UAnimGraphNode_ControlRig>(Node);
        else if (Path == RootClassPath) Root = Node;
        else
        {
            Error = FString::Printf(TEXT("Unexpected Session 4 ABP node: %s"), *Path);
            return false;
        }
    }
    UEdGraphPin* RefOut = FindPosePin(Ref, EGPD_Output);
    UEdGraphPin* SlotIn = FindPosePin(Slot, EGPD_Input);
    UEdGraphPin* SlotOut = FindPosePin(Slot, EGPD_Output);
    UEdGraphPin* ControlIn = FindPosePin(Control, EGPD_Input);
    UEdGraphPin* ControlOut = FindPosePin(Control, EGPD_Output);
    UEdGraphPin* RootIn = FindPosePin(Root, EGPD_Input);
    if (ReadSlotName(Slot) != DefaultSlot ||
        !PinsAreExclusivelyLinked(RefOut, SlotIn) ||
        !PinsAreExclusivelyLinked(SlotOut, ControlIn) ||
        !PinsAreExclusivelyLinked(ControlOut, RootIn) ||
        Control->Node.GetControlRigAssetReference().GetBlueprintClass().Get() != Rig->GeneratedClass.Get())
    {
        Error = TEXT("ABP pose flow or CR_DG_Master binding is invalid");
        return false;
    }
    TArray<FName> ExpectedMappings;
    for (const FVariableSpec& Spec : RequiredVariables()) ExpectedMappings.Add(Spec.Name);
    const TArray<FName> Sources = ReadNameArrayProperty(Control->Node, TEXT("SourcePropertyNames"));
    const TArray<FName> Destinations = ReadNameArrayProperty(Control->Node, TEXT("DestPropertyNames"));
    if (Sources != ExpectedMappings || Destinations != ExpectedMappings ||
        ReadAnimNodeProperty<FBoolProperty>(Control->Node, TEXT("bResetInputPoseToInitial"), true) ||
        !ReadAnimNodeProperty<FBoolProperty>(Control->Node, TEXT("bTransferInputPose"), false) ||
        !ReadAnimNodeProperty<FBoolProperty>(Control->Node, TEXT("bTransferInputCurves"), false) ||
        ReadAnimNodeAlphaInputType(Control->Node) != EAnimAlphaInputType::Float ||
        !FMath::IsNearlyEqual(ReadAnimNodeProperty<FFloatProperty>(Control->Node, TEXT("Alpha"), 0.0f), 1.0f))
    {
        Error = TEXT("ABP Control Rig input-transfer or direct source-target mappings are invalid");
        return false;
    }
    Out->SetStringField(TEXT("status"), TEXT("PASS"));
    Out->SetStringField(TEXT("path"), Blueprint->GetPathName());
    Out->SetStringField(TEXT("pose_flow"), TEXT("LocalRefPose -> DefaultSlot -> ControlRig -> Root"));
    Out->SetNumberField(TEXT("direct_property_mapping_count"), RequiredVariables().Num());
    return true;
}

bool ValidateWidget(UWidgetBlueprint* Widget, TSharedPtr<FJsonObject>& Out, FString& Error)
{
    Out = MakeShared<FJsonObject>();
    UClass* Parent = LoadObject<UClass>(nullptr, CreatorWidgetParentPath);
    if (!Widget || !Parent || Widget->ParentClass != Parent)
    {
        Error = TEXT("WBP_DG_CharacterCreator is missing or has the wrong project-owned parent");
        return false;
    }
    if ((Widget->WidgetTree && Widget->WidgetTree->RootWidget) ||
        !Widget->Bindings.IsEmpty() || !Widget->Animations.IsEmpty())
    {
        Error = TEXT("WBP_DG_CharacterCreator must remain an empty native-widget foundation");
        return false;
    }
    if (Widget->Status != BS_UpToDate && Widget->Status != BS_UpToDateWithWarnings)
    {
        Error = TEXT("WBP_DG_CharacterCreator is not compiled");
        return false;
    }
    Out->SetStringField(TEXT("status"), TEXT("PASS_EMPTY_FOUNDATION"));
    Out->SetStringField(TEXT("path"), Widget->GetPathName());
    Out->SetStringField(TEXT("parent_class"), Parent->GetPathName());
    Out->SetStringField(TEXT("scope"), TEXT("BODY_AND_THROW_STYLE_ONLY_NO_FAKE_FUTURE_TABS"));
    return true;
}

FString BuildValidation(FString* OutError = nullptr)
{
    FString Error;
    UControlRigBlueprint* Rig = LoadObject<UControlRigBlueprint>(nullptr, ControlRigPath);
    UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, AnimBlueprintPath);
    UWidgetBlueprint* Widget = LoadObject<UWidgetBlueprint>(nullptr, CreatorWidgetPath);
    TSharedPtr<FJsonObject> RigJson;
    TSharedPtr<FJsonObject> AnimJson;
    TSharedPtr<FJsonObject> WidgetJson;
    if (!ValidateRig(Rig, RigJson, Error) ||
        !ValidateAnimBlueprint(AnimBlueprint, Rig, AnimJson, Error) ||
        !ValidateWidget(Widget, WidgetJson, Error))
    {
        if (OutError) *OutError = Error;
        return FailureJson(Error);
    }
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), TEXT("PASS_STRICT_SESSION4_ASSET_CONTRACT"));
    Root->SetObjectField(TEXT("control_rig"), RigJson);
    Root->SetObjectField(TEXT("animation_blueprint"), AnimJson);
    Root->SetObjectField(TEXT("creator_widget"), WidgetJson);
    Root->SetStringField(TEXT("skeleton_policy"), TEXT("ONE_UNCHANGED_SK_DG_MASTER"));
    Root->SetStringField(TEXT("throw_style_authority"), TEXT("VISUAL_ONLY_FIXED_MONTAGE_TIMING"));
    if (OutError) OutError->Reset();
    return JsonString(Root);
}
} // namespace DiscGolfSession4Assets

FString UDiscGolfSession4AssetUtility::ValidateSession4Assets()
{
    return DiscGolfSession4Assets::BuildValidation();
}

FString UDiscGolfSession4AssetUtility::AuthorSession4Assets()
{
    using namespace DiscGolfSession4Assets;
    FString Error;
    UControlRigBlueprint* Rig = LoadObject<UControlRigBlueprint>(nullptr, ControlRigPath);
    UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, AnimBlueprintPath);
    if (!Rig || !AnimBlueprint)
    {
        return FailureJson(TEXT("Required CR_DG_Master or ABP_DG_Player asset did not load"));
    }

    FString AcceptedPbikError;
    if (!ValidateAcceptedPbik(Rig, AcceptedPbikError) ||
        !PreflightRigVariableOwnership(Rig, AcceptedPbikError) ||
        !PreflightAnimBlueprintOwnership(AnimBlueprint, AcceptedPbikError) ||
        !PreflightCreatorWidgetOwnership(AcceptedPbikError))
    {
        return FailureJson(AcceptedPbikError);
    }

    bool bRigChanged = false;

    // Recover a previously persisted partial authoring pass before structural
    // validation can short-circuit on nodes that reference a stale generated
    // class. Healthy assets skip the compile; a repaired compile contract is
    // counted as an asset write.
    if (HasCompletePublicAssetVariableSet(Rig))
    {
        FString ExistingGeneratedContractError;
        const bool bGeneratedContractWasHealthy =
            IsBlueprintCompileStatusHealthy(Rig) &&
            ValidateGeneratedRigVariableContract(Rig, ExistingGeneratedContractError);
        if (!bGeneratedContractWasHealthy &&
            !CompileRigBlueprintAndValidateVariables(Rig, Error))
        {
            return FailureJson(Error);
        }
        bRigChanged |= !bGeneratedContractWasHealthy;
    }

    TSharedPtr<FJsonObject> CurrentRigContract;
    FString CurrentRigError;
    if (!ValidateRig(Rig, CurrentRigContract, CurrentRigError))
    {
        bool bVariablesChanged = false;
        for (const FVariableSpec& Spec : RequiredVariables())
        {
            if (!EnsureRigVariable(Rig, Spec, bVariablesChanged, Error)) return FailureJson(Error);
        }
        bRigChanged |= bVariablesChanged;

        // AddHostMemberVariableFromExternal only regenerates the skeleton class
        // in UE 5.8. Synchronize the generated class exactly once after all five
        // variables exist and before authoring any RigVM variable getter nodes.
        if (bVariablesChanged && !CompileRigBlueprintAndValidateVariables(Rig, Error))
        {
            return FailureJson(Error);
        }

        bool bGraphChanged = false;
        if (!EnsureRigGraph(Rig, bGraphChanged, Error)) return FailureJson(Error);
        bRigChanged |= bGraphChanged;
        if (!bRigChanged)
        {
            return FailureJson(FString::Printf(
                TEXT("Session 4 rig validation failed but no owned repair was available: %s"),
                *CurrentRigError));
        }
        if (bGraphChanged)
        {
            Rig->RecompileVM();
        }
    }

    TSharedPtr<FJsonObject> RepairedRigContract;
    FString RepairedRigError;
    if (!ValidateRig(Rig, RepairedRigContract, RepairedRigError))
    {
        return FailureJson(FString::Printf(
            TEXT("Post-author Control Rig validation failed: %s"),
            *RepairedRigError));
    }

    bool bAnimChanged = false;
    if (!ConfigureAnimGraph(AnimBlueprint, Rig, bAnimChanged, Error)) return FailureJson(Error);

    bool bWidgetChanged = false;
    UWidgetBlueprint* Widget = EnsureCreatorWidget(bWidgetChanged, Error);
    if (!Widget) return FailureJson(Error);

    FString ValidationError;
    const FString Validation = BuildValidation(&ValidationError);
    if (!ValidationError.IsEmpty())
    {
        return FailureJson(FString::Printf(TEXT("Post-author validation failed: %s"), *ValidationError));
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Validation);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return FailureJson(TEXT("Could not parse strict Session 4 validation result"));
    }

    // All owned assets are first repaired and strictly validated in memory.
    // No package is written if a later immutable ownership check fails.
    if (bRigChanged && !SaveAsset(Rig, Error)) return FailureJson(Error);
    if (bAnimChanged && !SaveAsset(AnimBlueprint, Error)) return FailureJson(Error);
    if (bWidgetChanged && !SaveAsset(Widget, Error)) return FailureJson(Error);

    Root->SetStringField(TEXT("authoring_status"),
        (bRigChanged || bAnimChanged || bWidgetChanged)
            ? TEXT("PASS_AUTHORED_AND_VALIDATED")
            : TEXT("PASS_ALREADY_CURRENT_NO_ASSET_WRITES"));
    TArray<TSharedPtr<FJsonValue>> Writes;
    if (bRigChanged) Writes.Add(MakeShared<FJsonValueString>(Rig->GetPathName()));
    if (bAnimChanged) Writes.Add(MakeShared<FJsonValueString>(AnimBlueprint->GetPathName()));
    if (bWidgetChanged) Writes.Add(MakeShared<FJsonValueString>(Widget->GetPathName()));
    Root->SetArrayField(TEXT("asset_writes"), Writes);
    return JsonString(Root.ToSharedRef());
}
