#include "PCGDiscGolfZoneDensity.h"

#include "DiscGolfEnvironmentDataAssets.h"
#include "DiscGolfEnvironmentController.h"
#include "DiscGolfEnvironmentZoneActor.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "EngineUtils.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "PCGGraphExecutionStateInterface.h"

FPCGElementPtr UPCGDiscGolfZoneDensitySettings::CreateElement() const
{
    return MakeShared<FPCGDiscGolfZoneDensityElement>();
}

EPCGPointNativeProperties FPCGDiscGolfZoneDensityElement::GetPropertiesToAllocate(
    FPCGContext* Context) const
{
    return EPCGPointNativeProperties::Density | EPCGPointNativeProperties::Transform
        | EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax
        | EPCGPointNativeProperties::MetadataEntry;
}

bool FPCGDiscGolfZoneDensityElement::ExecuteInternal(FPCGContext* InContext) const
{
    ContextType* Context = static_cast<ContextType*>(InContext);
    const UPCGDiscGolfZoneDensitySettings* Settings =
        Context->GetInputSettings<UPCGDiscGolfZoneDensitySettings>();
    if (!Settings || !Context->ExecutionSource.IsValid()) return true;

    UWorld* World = Context->ExecutionSource->GetExecutionState().GetWorld();
    ADiscGolfEnvironmentController* Controller = nullptr;
    if (World)
    {
        for (TActorIterator<ADiscGolfEnvironmentController> It(World); It; ++It)
        {
            Controller = *It;
            break;
        }
    }
    if (!Controller) return ExecutePointOperation(Context,
        [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
        {
            OutPoint = InPoint;
            OutPoint.Density = 0.0f;
            return true;
        });

    const EDiscGolfEnvironmentAssetCategory Category = Settings->AssetCategory;
    const UDiscGolfForestPreset* Preset = Controller->ForestPreset.LoadSynchronous();
    TArray<const ADiscGolfEnvironmentZoneActor*> Zones;
    if (World)
    {
        for (TActorIterator<ADiscGolfEnvironmentZoneActor> It(World); It; ++It)
        {
            Zones.Add(*It);
        }
    }
    const UDiscGolfEnvironmentAssetSet* AssetSet = Preset
        ? Preset->AssetSet.LoadSynchronous() : nullptr;
    const FDiscGolfEnvironmentAssetSlot* AssetSlot = AssetSet
        ? AssetSet->Slots.FindByPredicate(
            [Category](const FDiscGolfEnvironmentAssetSlot& Candidate)
            {
                return Candidate.Category == Category;
            })
        : nullptr;
    const bool bWriteMeshAttribute = Settings->bWriteMeshAttribute
        && !Settings->MeshAttributeName.IsNone() && AssetSlot;
    const FName MeshAttributeName = Settings->MeshAttributeName;

    if (bWriteMeshAttribute)
    {
        for (FPCGTaggedData& TaggedOutput : Context->OutputData.TaggedData)
        {
            if (UPCGBasePointData* OutputData = Cast<UPCGBasePointData>(
                const_cast<UPCGData*>(TaggedOutput.Data.Get())))
            {
                OutputData->MutableMetadata()->FindOrCreateAttribute<FSoftObjectPath>(
                    MeshAttributeName, FSoftObjectPath(), false, false, true);
            }
        }
    }

    return ExecutePointOperation(Context,
        [Controller, Category, Preset, Zones, AssetSlot, bWriteMeshAttribute, MeshAttributeName]
        (const UPCGBasePointData* InputData, UPCGBasePointData* OutputData,
            int32 StartIndex, int32 Count)
        {
            const TConstPCGValueRange<FTransform> InputTransforms =
                InputData->GetConstTransformValueRange();
            const TConstPCGValueRange<float> InputDensities =
                InputData->GetConstDensityValueRange();
            const TConstPCGValueRange<int32> InputSeeds =
                InputData->GetConstSeedValueRange();
            TPCGValueRange<float> OutputDensities = OutputData->GetDensityValueRange();
            TPCGValueRange<FTransform> OutputTransforms = OutputData->GetTransformValueRange();
            TPCGValueRange<FVector> OutputBoundsMin = OutputData->GetBoundsMinValueRange();
            TPCGValueRange<FVector> OutputBoundsMax = OutputData->GetBoundsMaxValueRange();
            TPCGValueRange<int64> OutputMetadataEntries =
                OutputData->GetMetadataEntryValueRange();
            FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = bWriteMeshAttribute
                ? OutputData->MutableMetadata()->GetMutableTypedAttribute<FSoftObjectPath>(MeshAttributeName)
                : nullptr;

            for (int32 Index = StartIndex; Index < StartIndex + Count; ++Index)
            {
                const float EffectiveDensity = FMath::Clamp(InputDensities[Index]
                    * Controller->EvaluateDensityFromZones(
                        InputTransforms[Index].GetLocation(), Category, Preset, Zones),
                    0.0f, 1.0f);
                FRandomStream DensityRandom(PCGHelpers::ComputeSeed(
                    InputSeeds[Index], static_cast<int32>(Category), 0x44454E53));
                OutputDensities[Index] = DensityRandom.FRand() <= EffectiveDensity ? 1.0f : 0.0f;

                const float HalfSpacing = AssetSlot
                    ? FMath::Max(1.0f, AssetSlot->MinimumSpacingCm * 0.5f) : 1.0f;
                OutputBoundsMin[Index] = FVector(-HalfSpacing, -HalfSpacing, -HalfSpacing);
                OutputBoundsMax[Index] = FVector(HalfSpacing, HalfSpacing, HalfSpacing);
                if (!MeshAttribute) continue;

                float TotalWeight = 0.0f;
                for (const FDiscGolfEnvironmentMeshVariant& Variant : AssetSlot->Variants)
                {
                    if (!Variant.VisualMesh.IsNull() && Variant.Weight > 0.0f)
                    {
                        TotalWeight += Variant.Weight;
                    }
                }
                if (TotalWeight <= 0.0f)
                {
                    OutputDensities[Index] = 0.0f;
                    continue;
                }

                FRandomStream Random(PCGHelpers::ComputeSeed(
                    InputSeeds[Index], static_cast<int32>(Category), 0x4D465354));
                float Pick = Random.FRandRange(0.0f, TotalWeight);
                const FDiscGolfEnvironmentMeshVariant* Selected = nullptr;
                for (const FDiscGolfEnvironmentMeshVariant& Variant : AssetSlot->Variants)
                {
                    if (Variant.VisualMesh.IsNull() || Variant.Weight <= 0.0f) continue;
                    Selected = &Variant;
                    Pick -= Variant.Weight;
                    if (Pick <= 0.0f) break;
                }
                if (!Selected)
                {
                    OutputDensities[Index] = 0.0f;
                    continue;
                }

                FTransform Transform = OutputTransforms[Index];
                const float UniformScale = Random.FRandRange(
                    Selected->UniformScaleRange.X, Selected->UniformScaleRange.Y);
                Transform.SetScale3D(Transform.GetScale3D() * UniformScale);
                FRotator Rotation = Transform.Rotator();
                Rotation.Yaw = Random.FRandRange(-180.0f, 180.0f);
                Transform.SetRotation(Rotation.Quaternion());
                OutputTransforms[Index] = Transform;

                OutputData->MutableMetadata()->InitializeOnSet(OutputMetadataEntries[Index]);
                MeshAttribute->SetValue(OutputMetadataEntries[Index],
                    Selected->VisualMesh.ToSoftObjectPath());
            }
            return true;
        });
}
