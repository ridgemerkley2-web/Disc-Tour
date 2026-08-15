#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Elements/PCGPointOperationElementBase.h"
#include "DiscGolfEnvironmentTypes.h"
#include "PCGDiscGolfZoneDensity.generated.h"

/** PCG point node that applies the highest-priority disc-golf vegetation zone at each point. */
UCLASS(BlueprintType, ClassGroup=(Procedural))
class DISCGOLFTOUR_API UPCGDiscGolfZoneDensitySettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Disc Golf Environment", meta=(PCG_Overridable))
    EDiscGolfEnvironmentAssetCategory AssetCategory = EDiscGolfEnvironmentAssetCategory::TreeConiferLarge;

    /** Writes a mesh reference selected from the preset asset set for a by-attribute PCG spawner. */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Disc Golf Environment", meta=(PCG_Overridable))
    bool bWriteMeshAttribute = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Disc Golf Environment", meta=(EditCondition="bWriteMeshAttribute", PCG_Overridable))
    FName MeshAttributeName = TEXT("DiscGolfMesh");

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("DiscGolfZoneDensity"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("DiscGolfEnvironment", "ZoneDensityTitle", "Disc Golf Zone Density"); }
    virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("DiscGolfEnvironment", "ZoneDensityTooltip", "Applies tee, fairway, rough, green, OB, and hard-exclusion density rules without hard-coded mesh paths."); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Density; }
#endif

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};

class FPCGDiscGolfZoneDensityElement : public FPCGPointOperationElementBase
{
public:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGPointNativeProperties GetPropertiesToAllocate(FPCGContext* Context) const override;
    virtual bool ShouldCopyPoints() const override { return true; }
};
