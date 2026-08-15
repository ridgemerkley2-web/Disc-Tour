#include "DiscGolfEnvironmentAssetBinder.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "DiscGolfEnvironmentDataAssets.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    constexpr TCHAR DefaultCandidateReportRelativePath[] =
        TEXT("Developer/EnvironmentAssetBindingCandidates.json");
    constexpr TCHAR DefaultReadinessReportRelativePath[] =
        TEXT("Developer/EnvironmentAssetBindingReport.json");

    FString CategoryName(EDiscGolfEnvironmentAssetCategory Category)
    {
        if (const UEnum* Enum = StaticEnum<EDiscGolfEnvironmentAssetCategory>())
        {
            return Enum->GetNameStringByValue(static_cast<int64>(Category));
        }
        return TEXT("Unknown");
    }

    FString StatusName(EDiscGolfEnvironmentBindingStatus Status)
    {
        switch (Status)
        {
            case EDiscGolfEnvironmentBindingStatus::Ready: return TEXT("READY");
            case EDiscGolfEnvironmentBindingStatus::Missing: return TEXT("MISSING");
            case EDiscGolfEnvironmentBindingStatus::NeedsCollision: return TEXT("NEEDS_COLLISION");
            case EDiscGolfEnvironmentBindingStatus::NeedsLodReview: return TEXT("NEEDS_LOD_REVIEW");
            case EDiscGolfEnvironmentBindingStatus::NeedsWindBinding: return TEXT("NEEDS_WIND_BINDING");
            case EDiscGolfEnvironmentBindingStatus::ScaleWarning: return TEXT("SCALE_WARNING");
            case EDiscGolfEnvironmentBindingStatus::Ambiguous: return TEXT("AMBIGUOUS");
            default: return TEXT("UNKNOWN");
        }
    }

    bool IsTree(EDiscGolfEnvironmentAssetCategory Category)
    {
        return Category <= EDiscGolfEnvironmentAssetCategory::TreeDeciduousMedium;
    }

    bool NeedsSolidProxy(EDiscGolfEnvironmentAssetCategory Category)
    {
        return IsTree(Category)
            || Category == EDiscGolfEnvironmentAssetCategory::Log
            || Category == EDiscGolfEnvironmentAssetCategory::Stump
            || Category == EDiscGolfEnvironmentAssetCategory::RockSmall
            || Category == EDiscGolfEnvironmentAssetCategory::RockLarge;
    }

    bool NeedsWind(EDiscGolfEnvironmentAssetCategory Category)
    {
        return IsTree(Category)
            || Category == EDiscGolfEnvironmentAssetCategory::Sapling
            || Category == EDiscGolfEnvironmentAssetCategory::Shrub
            || Category == EDiscGolfEnvironmentAssetCategory::Fern
            || Category == EDiscGolfEnvironmentAssetCategory::Grass
            || Category == EDiscGolfEnvironmentAssetCategory::GroundCover;
    }

    bool MustNeverBlock(EDiscGolfEnvironmentAssetCategory Category)
    {
        return Category == EDiscGolfEnvironmentAssetCategory::Fern
            || Category == EDiscGolfEnvironmentAssetCategory::Grass
            || Category == EDiscGolfEnvironmentAssetCategory::GroundCover
            || Category == EDiscGolfEnvironmentAssetCategory::ForestDebris
            || Category == EDiscGolfEnvironmentAssetCategory::LeafLitter;
    }

    TArray<FString> Keywords(EDiscGolfEnvironmentAssetCategory Category)
    {
        switch (Category)
        {
            case EDiscGolfEnvironmentAssetCategory::TreeConiferLarge: return { TEXT("spruce"), TEXT("pine"), TEXT("fir"), TEXT("conifer"), TEXT("cedar"), TEXT("hemlock"), TEXT("large"), TEXT("mature"), TEXT("tall") };
            case EDiscGolfEnvironmentAssetCategory::TreeConiferMedium: return { TEXT("spruce"), TEXT("pine"), TEXT("fir"), TEXT("conifer"), TEXT("cedar"), TEXT("hemlock"), TEXT("medium") };
            case EDiscGolfEnvironmentAssetCategory::TreeConiferYoung: return { TEXT("spruce"), TEXT("pine"), TEXT("fir"), TEXT("conifer"), TEXT("cedar"), TEXT("hemlock"), TEXT("young"), TEXT("small"), TEXT("sapling") };
            case EDiscGolfEnvironmentAssetCategory::TreeDeciduousLarge: return { TEXT("beech"), TEXT("deciduous"), TEXT("aspen"), TEXT("birch"), TEXT("maple"), TEXT("oak"), TEXT("poplar"), TEXT("large"), TEXT("mature"), TEXT("tall") };
            case EDiscGolfEnvironmentAssetCategory::TreeDeciduousMedium: return { TEXT("beech"), TEXT("deciduous"), TEXT("aspen"), TEXT("birch"), TEXT("maple"), TEXT("oak"), TEXT("poplar"), TEXT("medium"), TEXT("small"), TEXT("young") };
            case EDiscGolfEnvironmentAssetCategory::Sapling: return { TEXT("sapling"), TEXT("seedling"), TEXT("young") };
            case EDiscGolfEnvironmentAssetCategory::Shrub: return { TEXT("shrub"), TEXT("bush") };
            case EDiscGolfEnvironmentAssetCategory::Fern: return { TEXT("fern") };
            case EDiscGolfEnvironmentAssetCategory::Grass: return { TEXT("grass"), TEXT("meadow") };
            case EDiscGolfEnvironmentAssetCategory::GroundCover: return { TEXT("groundcover"), TEXT("ground_cover"), TEXT("weed"), TEXT("flower"), TEXT("plant") };
            case EDiscGolfEnvironmentAssetCategory::Log: return { TEXT("log"), TEXT("fallen"), TEXT("fallenlog"), TEXT("deadwood") };
            case EDiscGolfEnvironmentAssetCategory::Stump: return { TEXT("stump") };
            case EDiscGolfEnvironmentAssetCategory::RockSmall: return { TEXT("rock"), TEXT("stone"), TEXT("small") };
            case EDiscGolfEnvironmentAssetCategory::RockLarge: return { TEXT("rock"), TEXT("boulder"), TEXT("large") };
            case EDiscGolfEnvironmentAssetCategory::ForestDebris: return { TEXT("debris"), TEXT("forestdebris"), TEXT("twig"), TEXT("cone"), TEXT("bark") };
            case EDiscGolfEnvironmentAssetCategory::LeafLitter: return { TEXT("litter"), TEXT("leaflitter"), TEXT("leaf"), TEXT("needle") };
            default: return {};
        }
    }

    TArray<FString> SemanticAnchors(EDiscGolfEnvironmentAssetCategory Category)
    {
        switch (Category)
        {
            case EDiscGolfEnvironmentAssetCategory::TreeConiferLarge:
            case EDiscGolfEnvironmentAssetCategory::TreeConiferMedium:
            case EDiscGolfEnvironmentAssetCategory::TreeConiferYoung:
                return { TEXT("spruce"), TEXT("pine"), TEXT("fir"), TEXT("conifer"), TEXT("cedar"), TEXT("hemlock") };
            case EDiscGolfEnvironmentAssetCategory::TreeDeciduousLarge:
            case EDiscGolfEnvironmentAssetCategory::TreeDeciduousMedium:
                return { TEXT("beech"), TEXT("deciduous"), TEXT("aspen"), TEXT("birch"), TEXT("maple"), TEXT("oak"), TEXT("poplar") };
            case EDiscGolfEnvironmentAssetCategory::RockSmall:
            case EDiscGolfEnvironmentAssetCategory::RockLarge:
                return { TEXT("rock"), TEXT("stone"), TEXT("boulder") };
            default:
                return {};
        }
    }

    bool ContainsDelimitedToken(const FString& LowerPath, const FString& LowerToken)
    {
        int32 SearchFrom = 0;
        while (SearchFrom < LowerPath.Len())
        {
            const int32 MatchIndex = LowerPath.Find(
                LowerToken, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
            if (MatchIndex == INDEX_NONE) return false;

            const int32 MatchEnd = MatchIndex + LowerToken.Len();
            const bool bLeftBoundary = MatchIndex == 0
                || !FChar::IsAlnum(LowerPath[MatchIndex - 1]);
            const bool bRightBoundary = MatchEnd == LowerPath.Len()
                || !FChar::IsAlnum(LowerPath[MatchEnd]);
            if (bLeftBoundary && bRightBoundary) return true;
            SearchFrom = MatchIndex + 1;
        }
        return false;
    }

    bool HasRequiredSemanticAnchor(
        const FString& ClassificationPath,
        EDiscGolfEnvironmentAssetCategory Category)
    {
        const TArray<FString> Anchors = SemanticAnchors(Category);
        if (Anchors.IsEmpty()) return true;

        const FString Lower = ClassificationPath.ToLower();
        return Anchors.ContainsByPredicate([&Lower](const FString& Anchor)
        {
            return ContainsDelimitedToken(Lower, Anchor);
        });
    }

    bool IsConiferTreeCategory(EDiscGolfEnvironmentAssetCategory Category)
    {
        return Category == EDiscGolfEnvironmentAssetCategory::TreeConiferLarge
            || Category == EDiscGolfEnvironmentAssetCategory::TreeConiferMedium
            || Category == EDiscGolfEnvironmentAssetCategory::TreeConiferYoung;
    }

    bool IsMatureConiferCategory(EDiscGolfEnvironmentAssetCategory Category)
    {
        return Category == EDiscGolfEnvironmentAssetCategory::TreeConiferLarge
            || Category == EDiscGolfEnvironmentAssetCategory::TreeConiferMedium;
    }

    float TrapezoidFitness(
        float Value,
        float Minimum,
        float FullFitnessMinimum,
        float FullFitnessMaximum,
        float Maximum)
    {
        if (Value <= Minimum || Value >= Maximum) return 0.0f;
        if (Value < FullFitnessMinimum)
        {
            return (Value - Minimum) / (FullFitnessMinimum - Minimum);
        }
        if (Value <= FullFitnessMaximum) return 1.0f;
        return (Maximum - Value) / (Maximum - FullFitnessMaximum);
    }

    float KeywordClassificationScore(
        const FString& ClassificationPath,
        EDiscGolfEnvironmentAssetCategory Category)
    {
        const FString Lower = ClassificationPath.ToLower();
        float Score = 0.0f;
        for (const FString& Keyword : Keywords(Category))
        {
            if (ContainsDelimitedToken(Lower, Keyword)) Score += 1.0f;
        }
        if (Lower.Contains(TEXT("collision")) || Lower.Contains(TEXT("proxy"))) Score -= 0.35f;
        return Score;
    }

    FString RelativeVendorAssetPath(
        const FString& AssetPath,
        const TArray<FString>& VendorContentRoots)
    {
        FString BestRoot;
        for (FString Root : VendorContentRoots)
        {
            Root.RemoveFromEnd(TEXT("/"));
            const FString RootPrefix = Root + TEXT("/");
            if (AssetPath.StartsWith(RootPrefix, ESearchCase::IgnoreCase)
                && Root.Len() > BestRoot.Len())
            {
                BestRoot = MoveTemp(Root);
            }
        }

        return BestRoot.IsEmpty()
            ? AssetPath
            : AssetPath.Mid(BestRoot.Len() + 1);
    }

    bool IsExcludedVendorSubtree(const FString& RelativeAssetPath)
    {
        FString Normalized = RelativeAssetPath.ToLower();
        Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
        Normalized = TEXT("/") + Normalized;

        static const TCHAR* ExcludedSegments[] =
        {
            TEXT("/demo/"),
            TEXT("/demos/"),
            TEXT("/example/"),
            TEXT("/examplecontent/"),
            TEXT("/map/"),
            TEXT("/maps/"),
            TEXT("/mannequin/"),
            TEXT("/showcase/"),
            TEXT("/ue4_mannequin/")
        };
        for (const TCHAR* Segment : ExcludedSegments)
        {
            if (Normalized.Contains(Segment)) return true;
        }
        return false;
    }

    bool HasWindParameters(const UStaticMesh* Mesh)
    {
        if (!Mesh) return false;
        for (const FStaticMaterial& StaticMaterial : Mesh->GetStaticMaterials())
        {
            const UMaterialInterface* Material = StaticMaterial.MaterialInterface;
            if (!Material) continue;
            TArray<FMaterialParameterInfo> ScalarInfo;
            TArray<FGuid> ScalarIds;
            Material->GetAllScalarParameterInfo(ScalarInfo, ScalarIds);
            TArray<FMaterialParameterInfo> VectorInfo;
            TArray<FGuid> VectorIds;
            Material->GetAllVectorParameterInfo(VectorInfo, VectorIds);
            const auto IsWindName = [](const FMaterialParameterInfo& Info)
            {
                const FString Name = Info.Name.ToString();
                return Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase)
                    || Name.Contains(TEXT("Gust"), ESearchCase::IgnoreCase);
            };
            if (ScalarInfo.ContainsByPredicate(IsWindName)
                || VectorInfo.ContainsByPredicate(IsWindName)) return true;
        }
        return false;
    }

    int32 SimpleCollisionCount(const UStaticMesh* Mesh)
    {
        const UBodySetup* BodySetup = Mesh ? Mesh->GetBodySetup() : nullptr;
        return BodySetup ? BodySetup->AggGeom.GetElementCount() : 0;
    }

    FDiscGolfEnvironmentBindingCandidate Inspect(
        const UStaticMesh* Mesh,
        EDiscGolfEnvironmentAssetCategory Category,
        float Score)
    {
        FDiscGolfEnvironmentBindingCandidate Candidate;
        Candidate.AssetPath = FSoftObjectPath(Mesh);
        Candidate.ClassificationScore = Score;
        Candidate.LodCount = Mesh ? Mesh->GetNumLODs() : 0;
        Candidate.MaterialCount = Mesh ? Mesh->GetStaticMaterials().Num() : 0;
        Candidate.SimpleCollisionShapeCount = SimpleCollisionCount(Mesh);
        Candidate.bNaniteEnabled = Mesh && Mesh->GetNaniteSettings().bEnabled;
        Candidate.bHasWindParameters = HasWindParameters(Mesh);
        Candidate.BoundsSizeCm = Mesh ? Mesh->GetBounds().BoxExtent * 2.0f : FVector::ZeroVector;
        Candidate.Statuses = UDiscGolfEnvironmentAssetBinder::ValidateMeshForCategory(
            Category, Mesh, Candidate.Notes);
        if (Mesh && NeedsSolidProxy(Category))
        {
            const bool bWasOtherwiseReady = Candidate.Statuses.Num() == 1
                && Candidate.Statuses[0] == EDiscGolfEnvironmentBindingStatus::Ready;
            Candidate.Statuses.Remove(EDiscGolfEnvironmentBindingStatus::Ready);
            Candidate.Statuses.AddUnique(EDiscGolfEnvironmentBindingStatus::NeedsCollision);
            if (bWasOtherwiseReady) Candidate.Notes.Reset();
            Candidate.Notes += TEXT(
                " Candidate scanning cannot approve a dedicated proxy; validate the assigned slot variant. ");
        }
        Candidate.bFoliageSuitable = Mesh
            && Candidate.MaterialCount <= 4
            && !Candidate.Statuses.Contains(EDiscGolfEnvironmentBindingStatus::ScaleWarning)
            && !Candidate.Statuses.Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision);
        return Candidate;
    }

    TSharedRef<FJsonObject> CandidateJson(const FDiscGolfEnvironmentBindingCandidate& Candidate)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("asset_path"), Candidate.AssetPath.ToString());
        Object->SetStringField(TEXT("collision_proxy_path"), Candidate.CollisionProxyPath.ToString());
        Object->SetStringField(TEXT("interaction_proxy_path"), Candidate.InteractionProxyPath.ToString());
        Object->SetNumberField(TEXT("classification_score"), Candidate.ClassificationScore);
        Object->SetNumberField(TEXT("lod_count"), Candidate.LodCount);
        Object->SetNumberField(TEXT("material_count"), Candidate.MaterialCount);
        Object->SetNumberField(TEXT("simple_collision_shapes"), Candidate.SimpleCollisionShapeCount);
        Object->SetBoolField(TEXT("nanite_enabled"), Candidate.bNaniteEnabled);
        Object->SetBoolField(TEXT("wind_parameters_detected"), Candidate.bHasWindParameters);
        Object->SetBoolField(TEXT("foliage_hism_suitable"), Candidate.bFoliageSuitable);
        TSharedRef<FJsonObject> Bounds = MakeShared<FJsonObject>();
        Bounds->SetNumberField(TEXT("x"), Candidate.BoundsSizeCm.X);
        Bounds->SetNumberField(TEXT("y"), Candidate.BoundsSizeCm.Y);
        Bounds->SetNumberField(TEXT("z"), Candidate.BoundsSizeCm.Z);
        Object->SetObjectField(TEXT("bounds_size_cm"), Bounds);
        TArray<TSharedPtr<FJsonValue>> Statuses;
        for (EDiscGolfEnvironmentBindingStatus Status : Candidate.Statuses)
        {
            Statuses.Add(MakeShared<FJsonValueString>(StatusName(Status)));
        }
        Object->SetArrayField(TEXT("statuses"), Statuses);
        Object->SetStringField(TEXT("notes"), Candidate.Notes);
        return Object;
    }

    TArray<EDiscGolfEnvironmentBindingStatus> ValidateMeshCharacteristics(
        EDiscGolfEnvironmentAssetCategory Category,
        const UStaticMesh* Mesh,
        bool bTreatMeshAsSolidGameplayGeometry,
        FString& OutNotes)
    {
        TArray<EDiscGolfEnvironmentBindingStatus> Statuses;
        if (!Mesh)
        {
            Statuses.Add(EDiscGolfEnvironmentBindingStatus::Missing);
            OutNotes = TEXT("No valid StaticMesh is assigned.");
            return Statuses;
        }

        const int32 CollisionShapes = SimpleCollisionCount(Mesh);
        const bool bComplexAsSimple = Mesh->GetBodySetup()
            && Mesh->GetBodySetup()->GetCollisionTraceFlag() == CTF_UseComplexAsSimple;
        if (MustNeverBlock(Category) && (CollisionShapes > 0 || bComplexAsSimple))
        {
            Statuses.Add(EDiscGolfEnvironmentBindingStatus::NeedsCollision);
            OutNotes += TEXT("Visual foliage carries collision; disable it before approval. ");
        }
        if (bTreatMeshAsSolidGameplayGeometry && NeedsSolidProxy(Category) && CollisionShapes == 0)
        {
            Statuses.Add(EDiscGolfEnvironmentBindingStatus::NeedsCollision);
            OutNotes += TEXT("Dedicated simple gameplay proxy is required. ");
        }
        if (bTreatMeshAsSolidGameplayGeometry && NeedsSolidProxy(Category) && bComplexAsSimple)
        {
            Statuses.AddUnique(EDiscGolfEnvironmentBindingStatus::NeedsCollision);
            OutNotes += TEXT("Complex-as-simple is not accepted for dense forest gameplay proxies. ");
        }
        if (Mesh->GetNumLODs() < 2 && !Mesh->GetNaniteSettings().bEnabled)
        {
            Statuses.Add(EDiscGolfEnvironmentBindingStatus::NeedsLodReview);
            OutNotes += TEXT("Mesh has one LOD and Nanite is disabled. ");
        }
        if (NeedsWind(Category) && !HasWindParameters(Mesh))
        {
            Statuses.Add(EDiscGolfEnvironmentBindingStatus::NeedsWindBinding);
            OutNotes += TEXT("No wind/gust parameter was detected in assigned materials. ");
        }
        const FVector Size = Mesh->GetBounds().BoxExtent * 2.0f;
        const float Height = Size.Z;
        const bool bScaleWarning = (IsTree(Category) && (Height < 300.0f || Height > 6000.0f))
            || (Category == EDiscGolfEnvironmentAssetCategory::Grass && Height > 250.0f)
            || (Category == EDiscGolfEnvironmentAssetCategory::Fern && Height > 350.0f)
            || (Category == EDiscGolfEnvironmentAssetCategory::RockSmall && Size.GetMax() > 500.0f)
            || (Category == EDiscGolfEnvironmentAssetCategory::RockLarge && Size.GetMax() < 100.0f);
        if (bScaleWarning)
        {
            Statuses.Add(EDiscGolfEnvironmentBindingStatus::ScaleWarning);
            OutNotes += TEXT("Bounds are outside the category review range. ");
        }
        if (Mesh->GetStaticMaterials().Num() > 4)
        {
            Statuses.AddUnique(EDiscGolfEnvironmentBindingStatus::NeedsLodReview);
            OutNotes += TEXT("More than four material slots requires draw-call review. ");
        }
        if (Statuses.IsEmpty())
        {
            Statuses.Add(EDiscGolfEnvironmentBindingStatus::Ready);
            OutNotes = TEXT("Static inspection passed; visual approval is still required.");
        }
        return Statuses;
    }

    void AddStatus(
        TArray<EDiscGolfEnvironmentBindingStatus>& Statuses,
        EDiscGolfEnvironmentBindingStatus Status,
        FString& Notes,
        const TCHAR* Note)
    {
        Statuses.Remove(EDiscGolfEnvironmentBindingStatus::Ready);
        Statuses.AddUnique(Status);
        Notes += Note;
    }

    FDiscGolfEnvironmentBindingCandidate InspectAssignedVariant(
        const FDiscGolfEnvironmentAssetSlot& Slot,
        const FDiscGolfEnvironmentMeshVariant& Variant)
    {
        FDiscGolfEnvironmentBindingCandidate Candidate = Inspect(
            Variant.VisualMesh.LoadSynchronous(), Slot.Category, 100.0f);
        Candidate.CollisionProxyPath = Variant.CollisionProxyMesh.ToSoftObjectPath();
        Candidate.InteractionProxyPath = Variant.InteractionProxyMesh.ToSoftObjectPath();
        Candidate.Statuses = UDiscGolfEnvironmentAssetBinder::ValidateVariantForSlot(
            Slot, Variant, Candidate.Notes);
        Candidate.bFoliageSuitable = Candidate.AssetPath.IsValid()
            && Candidate.MaterialCount <= 4
            && !Candidate.Statuses.Contains(EDiscGolfEnvironmentBindingStatus::ScaleWarning)
            && !Candidate.Statuses.Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision);
        return Candidate;
    }

    bool IsStructurallyComplete(const FDiscGolfEnvironmentBindingScan& Scan)
    {
        if (Scan.Proposals.Num() != 16) return false;

        TSet<EDiscGolfEnvironmentAssetCategory> Categories;
        for (const FDiscGolfEnvironmentBindingProposal& Proposal : Scan.Proposals)
        {
            Categories.Add(Proposal.Category);
        }
        return Categories.Num() == 16;
    }

    bool IsProductionReady(const FDiscGolfEnvironmentBindingScan& Scan)
    {
        if (!IsStructurallyComplete(Scan)) return false;

        for (const FDiscGolfEnvironmentBindingProposal& Proposal : Scan.Proposals)
        {
            if (Proposal.Candidates.IsEmpty()) return false;
            for (const FDiscGolfEnvironmentBindingCandidate& Candidate : Proposal.Candidates)
            {
                if (Candidate.Statuses.Num() != 1
                    || Candidate.Statuses[0] != EDiscGolfEnvironmentBindingStatus::Ready)
                {
                    return false;
                }
            }
        }
        return true;
    }
}

TArray<EDiscGolfEnvironmentBindingStatus>
UDiscGolfEnvironmentAssetBinder::ValidateMeshForCategory(
    EDiscGolfEnvironmentAssetCategory Category,
    const UStaticMesh* Mesh,
    FString& OutNotes)
{
    OutNotes.Reset();
    return ValidateMeshCharacteristics(Category, Mesh, true, OutNotes);
}

float UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
    const FString& AssetPath,
    const TArray<FString>& VendorContentRoots,
    EDiscGolfEnvironmentAssetCategory Category)
{
    const FString RelativePath = RelativeVendorAssetPath(AssetPath, VendorContentRoots);
    // Check both forms: otherwise selecting ExampleContent itself as a root strips the
    // very segment that is supposed to keep demonstration assets out of proposals.
    if (IsExcludedVendorSubtree(AssetPath) || IsExcludedVendorSubtree(RelativePath)) return 0.0f;
    if (!HasRequiredSemanticAnchor(RelativePath, Category)) return 0.0f;
    return KeywordClassificationScore(RelativePath, Category);
}

float UDiscGolfEnvironmentAssetBinder::ScoreTreeBoundsForCategory(
    EDiscGolfEnvironmentAssetCategory Category,
    const FVector& BoundsSizeCm)
{
    const float HeightCm = BoundsSizeCm.Z;
    switch (Category)
    {
        // Soft overlaps prevent a threshold-edge tree from being silently forced into one
        // ecological stratum. Sub-three-metre regeneration belongs in the Sapling review.
        case EDiscGolfEnvironmentAssetCategory::TreeConiferYoung:
            return TrapezoidFitness(HeightCm, 300.0f, 400.0f, 800.0f, 900.0f);
        case EDiscGolfEnvironmentAssetCategory::TreeConiferMedium:
            return TrapezoidFitness(HeightCm, 800.0f, 950.0f, 1350.0f, 1500.0f);
        case EDiscGolfEnvironmentAssetCategory::TreeConiferLarge:
            if (HeightCm <= 1300.0f || HeightCm > 6000.0f) return 0.0f;
            return FMath::Clamp((HeightCm - 1300.0f) / 200.0f, 0.0f, 1.0f);
        default:
            return 0.0f;
    }
}

TArray<EDiscGolfEnvironmentBindingStatus>
UDiscGolfEnvironmentAssetBinder::ValidateVariantForSlot(
    const FDiscGolfEnvironmentAssetSlot& Slot,
    const FDiscGolfEnvironmentMeshVariant& Variant,
    FString& OutNotes)
{
    OutNotes.Reset();
    const UStaticMesh* VisualMesh = Variant.VisualMesh.LoadSynchronous();
    TArray<EDiscGolfEnvironmentBindingStatus> Statuses = ValidateMeshCharacteristics(
        Slot.Category, VisualMesh, false, OutNotes);

    const bool bSolidPolicy = Slot.CollisionMode
            == EDiscGolfEnvironmentCollisionMode::TrunkOrBranchBlocking
        || Slot.CollisionMode == EDiscGolfEnvironmentCollisionMode::SolidBlocking;
    const bool bOverlapPolicy = Slot.CollisionMode
            == EDiscGolfEnvironmentCollisionMode::CanopyOverlap
        || Slot.CollisionMode == EDiscGolfEnvironmentCollisionMode::ShrubOverlap;

    if (bSolidPolicy)
    {
        const UStaticMesh* Proxy = Variant.CollisionProxyMesh.LoadSynchronous();
        if (!Proxy)
        {
            AddStatus(Statuses, EDiscGolfEnvironmentBindingStatus::NeedsCollision,
                OutNotes, TEXT("Slot policy requires a dedicated CollisionProxyMesh. "));
        }
        else
        {
            const bool bComplexAsSimple = Proxy->GetBodySetup()
                && Proxy->GetBodySetup()->GetCollisionTraceFlag() == CTF_UseComplexAsSimple;
            if (SimpleCollisionCount(Proxy) == 0 || bComplexAsSimple)
            {
                AddStatus(Statuses, EDiscGolfEnvironmentBindingStatus::NeedsCollision,
                    OutNotes,
                    TEXT("CollisionProxyMesh must provide simple collision and cannot use complex-as-simple. "));
            }
            if (Proxy == VisualMesh)
            {
                AddStatus(Statuses, EDiscGolfEnvironmentBindingStatus::NeedsCollision,
                    OutNotes, TEXT("CollisionProxyMesh must be separate from VisualMesh. "));
            }
        }
    }
    else if (!Variant.CollisionProxyMesh.IsNull())
    {
        AddStatus(Statuses, EDiscGolfEnvironmentBindingStatus::NeedsCollision,
            OutNotes, TEXT("CollisionProxyMesh is assigned to a nonblocking slot policy. "));
    }

    if (bOverlapPolicy)
    {
        const UStaticMesh* Proxy = Variant.InteractionProxyMesh.LoadSynchronous();
        if (!Proxy)
        {
            AddStatus(Statuses, EDiscGolfEnvironmentBindingStatus::NeedsCollision,
                OutNotes, TEXT("Overlap slot policy requires an InteractionProxyMesh. "));
        }
        if (VisualMesh)
        {
            const bool bVisualComplexAsSimple = VisualMesh->GetBodySetup()
                && VisualMesh->GetBodySetup()->GetCollisionTraceFlag() == CTF_UseComplexAsSimple;
            if (SimpleCollisionCount(VisualMesh) > 0 || bVisualComplexAsSimple)
            {
                AddStatus(Statuses, EDiscGolfEnvironmentBindingStatus::NeedsCollision,
                    OutNotes, TEXT("Overlap foliage VisualMesh must not carry blocking collision. "));
            }
        }
    }

    if (Statuses.IsEmpty())
    {
        Statuses.Add(EDiscGolfEnvironmentBindingStatus::Ready);
    }
    return Statuses;
}

FDiscGolfEnvironmentBindingScan UDiscGolfEnvironmentAssetBinder::ScanEnvironmentAssets(
    const TArray<FString>& VendorContentRoots,
    UDiscGolfEnvironmentAssetSet* ExistingAssetSet)
{
    return ProposeBindings(VendorContentRoots, ExistingAssetSet);
}

FDiscGolfEnvironmentBindingScan UDiscGolfEnvironmentAssetBinder::ProposeBindings(
    const TArray<FString>& VendorContentRoots,
    UDiscGolfEnvironmentAssetSet* ExistingAssetSet)
{
    return ProposeBindingsToReport(
        VendorContentRoots, ExistingAssetSet, DefaultCandidateReportRelativePath);
}

FDiscGolfEnvironmentBindingScan UDiscGolfEnvironmentAssetBinder::ProposeBindingsToReport(
    const TArray<FString>& VendorContentRoots,
    UDiscGolfEnvironmentAssetSet* ExistingAssetSet,
    const FString& SavedRelativeReportPath)
{
    FDiscGolfEnvironmentBindingScan Scan;
    for (FString Root : VendorContentRoots)
    {
        Root.TrimStartAndEndInline();
        Root.ReplaceInline(TEXT("\\"), TEXT("/"));
        while (Root.RemoveFromEnd(TEXT("/")))
        {
        }
        const bool bValidProjectRoot = Root.StartsWith(TEXT("/Game/"))
            && !Root.Contains(TEXT(".."));
        const bool bExcludedRoot = IsExcludedVendorSubtree(Root + TEXT("/"));
        const bool bAlreadyPresent = Scan.VendorContentRoots.ContainsByPredicate(
            [&Root](const FString& ExistingRoot)
            {
                return ExistingRoot.Equals(Root, ESearchCase::IgnoreCase);
            });
        if (bValidProjectRoot && !bExcludedRoot && !bAlreadyPresent)
        {
            Scan.VendorContentRoots.Add(MoveTemp(Root));
        }
    }

    TArray<FAssetData> MeshAssets;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
        TEXT("AssetRegistry")).Get();
    for (const FString& Root : Scan.VendorContentRoots)
    {
        TArray<FAssetData> RootAssets;
        Registry.GetAssetsByPath(FName(*Root), RootAssets, true, true);
        for (const FAssetData& Asset : RootAssets)
        {
            if (Asset.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
            {
                MeshAssets.AddUnique(Asset);
            }
        }
    }

    for (int32 CategoryIndex = 0; CategoryIndex < 16; ++CategoryIndex)
    {
        const EDiscGolfEnvironmentAssetCategory Category =
            static_cast<EDiscGolfEnvironmentAssetCategory>(CategoryIndex);
        FDiscGolfEnvironmentBindingProposal Proposal;
        Proposal.Category = Category;
        Proposal.bRetainedExistingBinding = ExistingAssetSet
            && ExistingAssetSet->HasPopulatedSlot(Category);

        for (const FAssetData& AssetData : MeshAssets)
        {
            float Score = ScoreAssetPathForCategory(
                AssetData.GetObjectPathString(), Scan.VendorContentRoots, Category);
            if (Score <= 0.0f) continue;
            if (const UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
            {
                if (IsConiferTreeCategory(Category))
                {
                    const float BoundsFitness = ScoreTreeBoundsForCategory(
                        Category, Mesh->GetBounds().BoxExtent * 2.0f);
                    if (BoundsFitness <= 0.0f) continue;
                    Score += BoundsFitness;
                }

                FDiscGolfEnvironmentBindingCandidate Candidate = Inspect(Mesh, Category, Score);
                if (IsMatureConiferCategory(Category))
                {
                    Candidate.Statuses.AddUnique(
                        EDiscGolfEnvironmentBindingStatus::Ambiguous);
                    Candidate.Notes += TEXT(
                        " Mature-tree bounds ranking is advisory; visual approval is required. ");
                }
                Proposal.Candidates.Add(MoveTemp(Candidate));
            }
        }
        Proposal.Candidates.Sort([](const auto& A, const auto& B)
        {
            if (!FMath::IsNearlyEqual(A.ClassificationScore, B.ClassificationScore))
            {
                return A.ClassificationScore > B.ClassificationScore;
            }
            return A.AssetPath.ToString() < B.AssetPath.ToString();
        });
        // Quality/form families such as high/low and full/half remain separately visible.
        // Collapsing them safely requires an explicit quality-tier policy and is deferred.
        if (Proposal.Candidates.Num() > 12) Proposal.Candidates.SetNum(12);
        if (Proposal.Candidates.Num() > 1
            && FMath::IsNearlyEqual(Proposal.Candidates[0].ClassificationScore,
                Proposal.Candidates[1].ClassificationScore))
        {
            Proposal.Candidates[0].Statuses.AddUnique(
                EDiscGolfEnvironmentBindingStatus::Ambiguous);
            Proposal.Candidates[0].Notes += TEXT(" Multiple top candidates tie; existing slot retained. ");
        }
        Scan.Proposals.Add(MoveTemp(Proposal));
    }
    WriteReport(Scan, Scan.ReportPath, SavedRelativeReportPath, false, false, false);
    return Scan;
}

bool UDiscGolfEnvironmentAssetBinder::ValidateEnvironmentAssets(
    UDiscGolfEnvironmentAssetSet* AssetSet,
    FString& OutReportPath)
{
    const FDiscGolfEnvironmentValidationResult Result =
        ValidateEnvironmentAssetReadiness(AssetSet);
    OutReportPath = Result.ReportPath;
    // Preserve the legacy contract: this bool reports that validation completed and the
    // expected category set exists. Call ValidateEnvironmentAssetReadiness when the caller
    // needs the stricter production-ready result.
    return Result.bReportGenerated && Result.bStructurallyComplete;
}

FDiscGolfEnvironmentValidationResult
UDiscGolfEnvironmentAssetBinder::ValidateEnvironmentAssetReadiness(
    UDiscGolfEnvironmentAssetSet* AssetSet)
{
    return ValidateEnvironmentAssetReadinessToReport(
        AssetSet, DefaultReadinessReportRelativePath);
}

FDiscGolfEnvironmentValidationResult
UDiscGolfEnvironmentAssetBinder::ValidateEnvironmentAssetReadinessToReport(
    UDiscGolfEnvironmentAssetSet* AssetSet,
    const FString& SavedRelativeReportPath)
{
    FDiscGolfEnvironmentValidationResult Result;
    FDiscGolfEnvironmentBindingScan Scan;
    Scan.VendorContentRoots.Add(TEXT("ASSIGNED_DATA_ASSET_ONLY"));
    if (AssetSet)
    {
        for (const FDiscGolfEnvironmentAssetSlot& Slot : AssetSet->Slots)
        {
            FDiscGolfEnvironmentBindingProposal Proposal;
            Proposal.Category = Slot.Category;
            Proposal.bRetainedExistingBinding = true;
            for (const FDiscGolfEnvironmentMeshVariant& Variant : Slot.Variants)
            {
                Proposal.Candidates.Add(InspectAssignedVariant(Slot, Variant));
            }
            if (Proposal.Candidates.IsEmpty())
            {
                Proposal.Candidates.Add(Inspect(nullptr, Slot.Category, 0.0f));
            }
            Scan.Proposals.Add(MoveTemp(Proposal));
        }
    }

    Result.bStructurallyComplete = IsStructurallyComplete(Scan);
    Result.bProductionReady = IsProductionReady(Scan);
    Result.bReportGenerated = WriteReport(
        Scan,
        Result.ReportPath,
        SavedRelativeReportPath,
        true,
        Result.bStructurallyComplete,
        Result.bProductionReady);
    return Result;
}

bool UDiscGolfEnvironmentAssetBinder::ApplyApprovedBindings(
    UDiscGolfEnvironmentAssetSet* AssetSet,
    EDiscGolfEnvironmentAssetCategory Category,
    const TArray<FSoftObjectPath>& ApprovedVisualMeshes,
    FString& OutError)
{
    if (!AssetSet || ApprovedVisualMeshes.IsEmpty())
    {
        OutError = TEXT("Asset set and at least one explicitly approved mesh are required.");
        return false;
    }
    FDiscGolfEnvironmentAssetSlot* Slot = AssetSet->Slots.FindByPredicate(
        [Category](const FDiscGolfEnvironmentAssetSlot& Entry)
        {
            return Entry.Category == Category;
        });
    if (!Slot)
    {
        OutError = TEXT("The requested category does not exist in this asset set.");
        return false;
    }

    TArray<FDiscGolfEnvironmentMeshVariant> Approved;
    for (const FSoftObjectPath& Path : ApprovedVisualMeshes)
    {
        const UStaticMesh* Mesh = Cast<UStaticMesh>(Path.TryLoad());
        if (!Mesh)
        {
            OutError = FString::Printf(TEXT("Approved path is not a StaticMesh: %s"), *Path.ToString());
            return false;
        }
        const FDiscGolfEnvironmentMeshVariant* Existing = Slot->Variants.FindByPredicate(
            [&Path](const FDiscGolfEnvironmentMeshVariant& Variant)
            {
                return Variant.VisualMesh.ToSoftObjectPath() == Path;
            });
        FDiscGolfEnvironmentMeshVariant Variant;
        if (Existing)
        {
            Variant = *Existing;
        }
        else
        {
            Variant.VisualMesh = const_cast<UStaticMesh*>(Mesh);
            Variant.Weight = 1.0f;
            Variant.bNaniteSuitable = Mesh->GetNaniteSettings().bEnabled;
        }
        if (!Approved.ContainsByPredicate(
            [&Path](const FDiscGolfEnvironmentMeshVariant& Entry)
            {
                return Entry.VisualMesh.ToSoftObjectPath() == Path;
            }))
        {
            Approved.Add(Variant);
        }
    }
    const TArray<FDiscGolfEnvironmentMeshVariant> PreviousVariants = Slot->Variants;
    UPackage* Package = AssetSet->GetOutermost();
    const bool bPackageWasDirty = Package && Package->IsDirty();
    AssetSet->Modify();
    Slot->Variants = MoveTemp(Approved);
    AssetSet->PostEditChange();
    AssetSet->MarkPackageDirty();
    const TArray<UPackage*> PackagesToSave = { Package };
    const bool bPersistentPackage = Package
        && Package != GetTransientPackage()
        && !Package->HasAnyFlags(RF_Transient)
        && !Package->HasAnyPackageFlags(PKG_InMemoryOnly | PKG_PlayInEditor | PKG_CompiledIn);
    if (bPersistentPackage
        && !UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
    {
        Slot->Variants = PreviousVariants;
        AssetSet->PostEditChange();
        Package->SetDirtyFlag(bPackageWasDirty);
        OutError = TEXT("Approved bindings could not be saved; the in-memory slot was restored.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UDiscGolfEnvironmentAssetBinder::WriteReport(
    const FDiscGolfEnvironmentBindingScan& Scan,
    FString& OutReportPath,
    const FString& SavedRelativeReportPath,
    bool bReadinessEvaluated,
    bool bStructurallyComplete,
    bool bProductionReady)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_environment_asset_binding_report"));
    Root->SetNumberField(TEXT("schema_version"), 2);
    Root->SetStringField(TEXT("generated_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetBoolField(TEXT("automatic_apply_performed"), false);
    Root->SetBoolField(TEXT("readiness_evaluated"), bReadinessEvaluated);
    Root->SetBoolField(TEXT("structurally_complete"), bStructurallyComplete);
    Root->SetBoolField(TEXT("production_ready"), bProductionReady);
    Root->SetStringField(TEXT("report_kind"),
        bReadinessEvaluated ? TEXT("assigned_readiness") : TEXT("candidate_proposals"));
    Root->SetStringField(TEXT("approval_policy"),
        TEXT("Candidates are advisory. Existing bindings remain until ApplyApprovedBindings receives explicit paths."));
    TArray<TSharedPtr<FJsonValue>> Roots;
    for (const FString& VendorRoot : Scan.VendorContentRoots)
    {
        Roots.Add(MakeShared<FJsonValueString>(VendorRoot));
    }
    Root->SetArrayField(TEXT("vendor_content_roots"), Roots);

    TArray<TSharedPtr<FJsonValue>> Slots;
    for (const FDiscGolfEnvironmentBindingProposal& Proposal : Scan.Proposals)
    {
        TSharedRef<FJsonObject> Slot = MakeShared<FJsonObject>();
        Slot->SetStringField(TEXT("slot"), CategoryName(Proposal.Category));
        Slot->SetBoolField(TEXT("retained_existing_binding"), Proposal.bRetainedExistingBinding);
        TSet<EDiscGolfEnvironmentBindingStatus> SlotStatuses;
        if (Proposal.Candidates.IsEmpty())
        {
            SlotStatuses.Add(EDiscGolfEnvironmentBindingStatus::Missing);
        }
        for (const FDiscGolfEnvironmentBindingCandidate& Candidate : Proposal.Candidates)
        {
            for (const EDiscGolfEnvironmentBindingStatus Status : Candidate.Statuses)
            {
                SlotStatuses.Add(Status);
            }
        }
        TArray<TSharedPtr<FJsonValue>> StatusValues;
        for (const EDiscGolfEnvironmentBindingStatus Status : SlotStatuses)
        {
            StatusValues.Add(MakeShared<FJsonValueString>(StatusName(Status)));
        }
        Slot->SetArrayField(TEXT("slot_statuses"), StatusValues);
        TArray<TSharedPtr<FJsonValue>> Candidates;
        for (const FDiscGolfEnvironmentBindingCandidate& Candidate : Proposal.Candidates)
        {
            Candidates.Add(MakeShared<FJsonValueObject>(CandidateJson(Candidate)));
        }
        Slot->SetArrayField(TEXT("candidates"), Candidates);
        Slots.Add(MakeShared<FJsonValueObject>(Slot));
    }
    Root->SetArrayField(TEXT("slots"), Slots);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);

    FString RelativeReportPath = SavedRelativeReportPath;
    FPaths::NormalizeFilename(RelativeReportPath);
    if (RelativeReportPath.IsEmpty()
        || !FPaths::IsRelative(RelativeReportPath)
        || RelativeReportPath.Contains(TEXT(".."))
        || !FPaths::GetExtension(RelativeReportPath).Equals(TEXT("json"), ESearchCase::IgnoreCase))
    {
        RelativeReportPath = bReadinessEvaluated
            ? DefaultReadinessReportRelativePath
            : DefaultCandidateReportRelativePath;
    }
    OutReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), RelativeReportPath);
    const FString Directory = FPaths::GetPath(OutReportPath);
    IFileManager::Get().MakeDirectory(*Directory, true);
    return FFileHelper::SaveStringToFile(Json, *OutReportPath);
}
