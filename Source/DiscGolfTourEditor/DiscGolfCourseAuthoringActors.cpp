#include "DiscGolfCourseAuthoringActors.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    bool IsFiniteVector(const FVector& Value)
    {
        return !Value.ContainsNaN()
            && FMath::IsFinite(Value.X)
            && FMath::IsFinite(Value.Y)
            && FMath::IsFinite(Value.Z);
    }

    bool IsFiniteTransform(const FTransform& Value)
    {
        return IsFiniteVector(Value.GetLocation())
            && IsFiniteVector(Value.GetScale3D())
            && !Value.GetRotation().ContainsNaN();
    }

    bool IsSupportedZoneType(EDGCourseZoneType ZoneType)
    {
        switch (ZoneType)
        {
        case EDGCourseZoneType::TeeSafety:
        case EDGCourseZoneType::FairwayPrimary:
        case EDGCourseZoneType::FairwaySecondary:
        case EDGCourseZoneType::Rough:
        case EDGCourseZoneType::DeepRough:
        case EDGCourseZoneType::Green:
        case EDGCourseZoneType::OutOfBounds:
        case EDGCourseZoneType::WaterHazard:
        case EDGCourseZoneType::Spectator:
        case EDGCourseZoneType::NoSpawn:
            return true;
        default:
            return false;
        }
    }

    template <typename RecordType>
    void SortByStableId(TArray<RecordType>& Records)
    {
        Records.Sort([](const RecordType& A, const RecordType& B)
        {
            return A.StableId.ToString() < B.StableId.ToString();
        });
    }

    template <typename RecordType>
    bool ValidateUniqueIds(const TArray<RecordType>& Records, const TCHAR* TypeName, FString& OutError)
    {
        TSet<FName> Seen;
        for (const RecordType& Record : Records)
        {
            if (Record.StableId.IsNone())
            {
                OutError = FString::Printf(TEXT("%s has no stable ID"), TypeName);
                return false;
            }
            if (Seen.Contains(Record.StableId))
            {
                OutError = FString::Printf(TEXT("Duplicate %s stable ID: %s"), TypeName, *Record.StableId.ToString());
                return false;
            }
            Seen.Add(Record.StableId);
        }
        return true;
    }

    void ConfigureAuthoringActor(AActor& Actor)
    {
        Actor.PrimaryActorTick.bCanEverTick = false;
        Actor.PrimaryActorTick.bStartWithTickEnabled = false;
        Actor.SetActorTickEnabled(false);
        Actor.bIsEditorOnlyActor = true;
    }

    UTexture2D* LoadEditorMarkerTexture()
    {
        static ConstructorHelpers::FObjectFinderOptional<UTexture2D> Texture(
            TEXT("/Engine/EditorResources/S_TargetPoint.S_TargetPoint"));
        return Texture.Get();
    }

    void ConfigureBillboard(UBillboardComponent& Billboard)
    {
        Billboard.SetSprite(LoadEditorMarkerTexture());
        Billboard.SetRelativeScale3D(FVector(0.75f));
        Billboard.SetHiddenInGame(true);
        Billboard.SpriteInfo.Category = TEXT("DiscGolfAuthoring");
        Billboard.SpriteInfo.DisplayName = FText::FromString(TEXT("Disc Golf Authoring"));
        Billboard.SpriteInfo.Description = FText::FromString(TEXT("Editor-only disc golf course marker"));
    }
}

FTransform DiscGolfCourseAuthoring::ToCourseRelativeTransform(
    const FTransform& WorldTransform,
    const FTransform& CourseOriginWorldTransform)
{
    return WorldTransform.GetRelativeTransform(CourseOriginWorldTransform);
}

FDGCourseZoneDefinition DiscGolfCourseAuthoring::ToCourseZoneDefinition(
    const FDiscGolfAuthoringZoneRecord& Record,
    const FTransform& CourseOriginWorldTransform)
{
    FDGCourseZoneDefinition Definition;
    Definition.ZoneId = Record.StableId;
    Definition.ZoneType = Record.ZoneType;
    Definition.PenaltyStrokes = Record.PenaltyStrokes;
    Definition.bAffectsVegetation = Record.bAffectsVegetation;
    Definition.PolygonPointsCm.Reserve(Record.PolygonPointsWorldCm.Num());
    for (const FVector& WorldPoint : Record.PolygonPointsWorldCm)
    {
        Definition.PolygonPointsCm.Add(CourseOriginWorldTransform.InverseTransformPosition(WorldPoint));
    }
    return Definition;
}

FDGMandoDefinition DiscGolfCourseAuthoring::ToMandoDefinition(
    const FDiscGolfAuthoringMandoRecord& Record,
    const FTransform& CourseOriginWorldTransform,
    int32 DropZoneIndex)
{
    FDGMandoDefinition Definition;
    Definition.MandoId = Record.StableId;
    Definition.GatePointACm = CourseOriginWorldTransform.InverseTransformPosition(Record.GatePointAWorldCm);
    Definition.GatePointBCm = CourseOriginWorldTransform.InverseTransformPosition(Record.GatePointBWorldCm);
    Definition.RequiredPassDirection = CourseOriginWorldTransform
        .InverseTransformVectorNoScale(Record.RequiredPassDirectionWorld).GetSafeNormal();
    Definition.MissPenaltyStrokes = Record.MissPenaltyStrokes;
    Definition.DropZoneIndex = DropZoneIndex;
    return Definition;
}

bool DiscGolfCourseAuthoring::ToHoleDefinition(
    const FDiscGolfHoleAuthoringSnapshot& Snapshot,
    FDGHoleDefinition& OutDefinition,
    FString& OutError)
{
    OutError.Reset();
    if (Snapshot.HoleId.IsNone() || Snapshot.HoleNumber < 1 || Snapshot.Par < 1)
    {
        OutError = TEXT("Hole metadata is incomplete");
        return false;
    }
    if (Snapshot.Tee.StableId.IsNone() || Snapshot.Basket.StableId.IsNone())
    {
        OutError = TEXT("Tee and basket require explicit stable IDs");
        return false;
    }
    if (!IsFiniteTransform(Snapshot.CourseOriginWorldTransform)
        || !IsFiniteTransform(Snapshot.Tee.WorldTransform)
        || !IsFiniteTransform(Snapshot.Basket.WorldTransform))
    {
        OutError = TEXT("Course origin, tee, or basket transform is not finite");
        return false;
    }
    if (!FMath::IsFinite(Snapshot.PublishedDistanceM)
        || Snapshot.PublishedDistanceM < 0.0f)
    {
        OutError = TEXT("Published distance must be finite and non-negative");
        return false;
    }
    if (!FMath::IsFinite(Snapshot.ElevationChangeM))
    {
        OutError = TEXT("Elevation change must be finite");
        return false;
    }

    TArray<FDiscGolfAuthoringMarkerRecord> SortedDropZones = Snapshot.DropZones;
    TArray<FDiscGolfAuthoringMandoRecord> SortedMandos = Snapshot.Mandos;
    TArray<FDiscGolfAuthoringZoneRecord> SortedZones = Snapshot.Zones;
    if (!ValidateUniqueIds(SortedDropZones, TEXT("drop zone"), OutError)
        || !ValidateUniqueIds(SortedMandos, TEXT("mando"), OutError)
        || !ValidateUniqueIds(SortedZones, TEXT("zone"), OutError))
    {
        return false;
    }
    SortByStableId(SortedDropZones);
    SortByStableId(SortedMandos);
    SortByStableId(SortedZones);

    FDGHoleDefinition Candidate;
    Candidate.HoleId = Snapshot.HoleId;
    Candidate.HoleNumber = Snapshot.HoleNumber;
    Candidate.Par = Snapshot.Par;
    Candidate.PublishedDistanceM = Snapshot.PublishedDistanceM;
    Candidate.ElevationChangeM = Snapshot.ElevationChangeM;
    Candidate.TeeTransform = ToCourseRelativeTransform(
        Snapshot.Tee.WorldTransform, Snapshot.CourseOriginWorldTransform);
    Candidate.BasketTransform = ToCourseRelativeTransform(
        Snapshot.Basket.WorldTransform, Snapshot.CourseOriginWorldTransform);

    TMap<FName, int32> DropZoneIndices;
    for (const FDiscGolfAuthoringMarkerRecord& DropZone : SortedDropZones)
    {
        if (!IsFiniteTransform(DropZone.WorldTransform))
        {
            OutError = FString::Printf(TEXT("Drop zone %s transform is not finite"), *DropZone.StableId.ToString());
            return false;
        }
        const int32 Index = Candidate.DropZoneTransforms.Add(ToCourseRelativeTransform(
            DropZone.WorldTransform, Snapshot.CourseOriginWorldTransform));
        DropZoneIndices.Add(DropZone.StableId, Index);
    }

    for (const FDiscGolfAuthoringMandoRecord& Mando : SortedMandos)
    {
        if (!IsFiniteVector(Mando.GatePointAWorldCm)
            || !IsFiniteVector(Mando.GatePointBWorldCm)
            || !IsFiniteVector(Mando.RequiredPassDirectionWorld)
            || Mando.GatePointAWorldCm.Equals(Mando.GatePointBWorldCm)
            || Mando.RequiredPassDirectionWorld.IsNearlyZero())
        {
            OutError = FString::Printf(TEXT("Mando %s has invalid gate geometry"), *Mando.StableId.ToString());
            return false;
        }
        if (Mando.MissPenaltyStrokes < 0)
        {
            OutError = FString::Printf(TEXT("Mando %s has a negative miss penalty"),
                *Mando.StableId.ToString());
            return false;
        }
        int32 DropZoneIndex = INDEX_NONE;
        if (!Mando.DropZoneId.IsNone())
        {
            const int32* FoundIndex = DropZoneIndices.Find(Mando.DropZoneId);
            if (!FoundIndex)
            {
                OutError = FString::Printf(TEXT("Mando %s references unknown drop zone %s"),
                    *Mando.StableId.ToString(), *Mando.DropZoneId.ToString());
                return false;
            }
            DropZoneIndex = *FoundIndex;
        }
        Candidate.Mandos.Add(ToMandoDefinition(
            Mando, Snapshot.CourseOriginWorldTransform, DropZoneIndex));
    }

    for (const FDiscGolfAuthoringZoneRecord& Zone : SortedZones)
    {
        if (!Zone.bIsClosedLoop)
        {
            OutError = FString::Printf(TEXT("Zone %s must be a closed polygon"),
                *Zone.StableId.ToString());
            return false;
        }
        if (!IsSupportedZoneType(Zone.ZoneType))
        {
            OutError = FString::Printf(TEXT("Zone %s has an unsupported zone type"),
                *Zone.StableId.ToString());
            return false;
        }
        if (Zone.PenaltyStrokes < 0)
        {
            OutError = FString::Printf(TEXT("Zone %s has a negative penalty"),
                *Zone.StableId.ToString());
            return false;
        }
        if (Zone.PolygonPointsWorldCm.Num() < 3
            || Zone.PolygonPointsWorldCm.ContainsByPredicate([](const FVector& Point)
            {
                return !IsFiniteVector(Point);
            }))
        {
            OutError = FString::Printf(TEXT("Zone %s requires at least three finite points"), *Zone.StableId.ToString());
            return false;
        }
        Candidate.Zones.Add(ToCourseZoneDefinition(Zone, Snapshot.CourseOriginWorldTransform));
    }

    OutDefinition = MoveTemp(Candidate);
    return true;
}

FBox DiscGolfCourseAuthoring::CalculateCourseRelativeBounds(const FDGHoleDefinition& Definition)
{
    FBox Bounds(ForceInit);
    Bounds += Definition.TeeTransform.GetLocation();
    Bounds += Definition.BasketTransform.GetLocation();
    for (const FTransform& DropZone : Definition.DropZoneTransforms)
    {
        Bounds += DropZone.GetLocation();
    }
    for (const FDGMandoDefinition& Mando : Definition.Mandos)
    {
        Bounds += Mando.GatePointACm;
        Bounds += Mando.GatePointBCm;
    }
    for (const FDGCourseZoneDefinition& Zone : Definition.Zones)
    {
        for (const FVector& Point : Zone.PolygonPointsCm)
        {
            Bounds += Point;
        }
    }
    return Bounds;
}

ADiscGolfAuthoringMarkerActor::ADiscGolfAuthoringMarkerActor()
{
    ConfigureAuthoringActor(*this);
    AuthoringRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AuthoringRoot"));
    SetRootComponent(AuthoringRoot);
    MarkerBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("MarkerBillboard"));
    MarkerBillboard->SetupAttachment(AuthoringRoot);
    ConfigureBillboard(*MarkerBillboard);
    ForwardArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardArrow"));
    ForwardArrow->SetupAttachment(AuthoringRoot);
    ForwardArrow->SetHiddenInGame(true);
    ForwardArrow->ArrowSize = 1.5f;
}

FDiscGolfAuthoringMarkerRecord ADiscGolfAuthoringMarkerActor::MakeMarkerRecord() const
{
    return {StableId, GetActorTransform()};
}

ADiscGolfTeeAuthoringActor::ADiscGolfTeeAuthoringActor()
{
    StableId = TEXT("Tee");
    ForwardArrow->ArrowColor = FColor::Cyan;
}

ADiscGolfBasketAuthoringActor::ADiscGolfBasketAuthoringActor()
{
    StableId = TEXT("Basket");
    ForwardArrow->ArrowColor = FColor::Yellow;
}

ADiscGolfDropZoneAuthoringActor::ADiscGolfDropZoneAuthoringActor()
{
    StableId = TEXT("DropZone_01");
    ForwardArrow->ArrowColor = FColor::Orange;
}

ADiscGolfMandoGateAuthoringActor::ADiscGolfMandoGateAuthoringActor()
{
    ConfigureAuthoringActor(*this);
    AuthoringRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AuthoringRoot"));
    SetRootComponent(AuthoringRoot);
    GateSpline = CreateDefaultSubobject<USplineComponent>(TEXT("GateSpline"));
    GateSpline->SetupAttachment(AuthoringRoot);
    GateSpline->SetHiddenInGame(true);
    GateSpline->SetDrawDebug(true);
    RequiredDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("RequiredDirectionArrow"));
    RequiredDirectionArrow->SetupAttachment(AuthoringRoot);
    RequiredDirectionArrow->ArrowColor = FColor::Orange;
    RequiredDirectionArrow->ArrowSize = 2.0f;
    RequiredDirectionArrow->SetHiddenInGame(true);
}

void ADiscGolfMandoGateAuthoringActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GateSpline->ClearSplinePoints(false);
    GateSpline->AddSplinePoint(GatePointALocalCm, ESplineCoordinateSpace::Local, false);
    GateSpline->AddSplinePoint(GatePointBLocalCm, ESplineCoordinateSpace::Local, false);
    GateSpline->SetClosedLoop(false, false);
    GateSpline->UpdateSpline();
    RequiredDirectionArrow->SetRelativeRotation(RequiredPassDirectionLocal.GetSafeNormal().Rotation());
}

FDiscGolfAuthoringMandoRecord ADiscGolfMandoGateAuthoringActor::MakeMandoRecord() const
{
    FDiscGolfAuthoringMandoRecord Record;
    Record.StableId = StableId;
    Record.GatePointAWorldCm = GetActorTransform().TransformPosition(GatePointALocalCm);
    Record.GatePointBWorldCm = GetActorTransform().TransformPosition(GatePointBLocalCm);
    Record.RequiredPassDirectionWorld = GetActorTransform().TransformVectorNoScale(RequiredPassDirectionLocal);
    Record.MissPenaltyStrokes = MissPenaltyStrokes;
    Record.DropZoneId = IsValid(MissDropZone) ? MissDropZone->StableId : NAME_None;
    return Record;
}

ADiscGolfGameplayZoneAuthoringActor::ADiscGolfGameplayZoneAuthoringActor()
{
    ConfigureAuthoringActor(*this);
    AuthoringRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AuthoringRoot"));
    SetRootComponent(AuthoringRoot);
    ZoneSpline = CreateDefaultSubobject<USplineComponent>(TEXT("ZoneSpline"));
    ZoneSpline->SetupAttachment(AuthoringRoot);
    ZoneSpline->SetHiddenInGame(true);
    ZoneSpline->SetDrawDebug(true);
    ZoneSpline->ClearSplinePoints(false);
    ZoneSpline->AddSplinePoint(FVector(-500.0f, -500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
    ZoneSpline->AddSplinePoint(FVector(500.0f, -500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
    ZoneSpline->AddSplinePoint(FVector(500.0f, 500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
    ZoneSpline->AddSplinePoint(FVector(-500.0f, 500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
    ZoneSpline->SetClosedLoop(true, false);
    ZoneSpline->UpdateSpline();
}

void ADiscGolfGameplayZoneAuthoringActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ZoneSpline->SetClosedLoop(bClosedLoop, true);
}

FDiscGolfAuthoringZoneRecord ADiscGolfGameplayZoneAuthoringActor::MakeZoneRecord() const
{
    FDiscGolfAuthoringZoneRecord Record;
    Record.StableId = StableId;
    Record.ZoneType = ZoneType;
    Record.PenaltyStrokes = PenaltyStrokes;
    Record.bAffectsVegetation = bAffectsVegetation;
    Record.bIsClosedLoop = bClosedLoop && ZoneSpline->IsClosedLoop();
    Record.PolygonPointsWorldCm.Reserve(ZoneSpline->GetNumberOfSplinePoints());
    for (int32 Index = 0; Index < ZoneSpline->GetNumberOfSplinePoints(); ++Index)
    {
        Record.PolygonPointsWorldCm.Add(ZoneSpline->GetLocationAtSplinePoint(
            Index, ESplineCoordinateSpace::World));
    }
    return Record;
}

ADiscGolfHoleAuthoringActor::ADiscGolfHoleAuthoringActor()
{
    ConfigureAuthoringActor(*this);
    CourseOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("CourseOrigin"));
    SetRootComponent(CourseOrigin);
    HoleBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("HoleBillboard"));
    HoleBillboard->SetupAttachment(CourseOrigin);
    ConfigureBillboard(*HoleBillboard);
}

FDiscGolfHoleAuthoringSnapshot ADiscGolfHoleAuthoringActor::MakeSnapshot(FString& OutError) const
{
    FDiscGolfHoleAuthoringSnapshot Snapshot;
    Snapshot.HoleId = HoleId;
    Snapshot.HoleNumber = HoleNumber;
    Snapshot.Par = Par;
    Snapshot.PublishedDistanceM = PublishedDistanceM;
    Snapshot.ElevationChangeM = ElevationChangeM;
    Snapshot.CourseOriginWorldTransform = GetActorTransform();
    if (!IsValid(Tee) || !IsValid(Basket))
    {
        OutError = TEXT("Hole root requires assigned tee and basket actors");
        return Snapshot;
    }
    Snapshot.Tee = Tee->MakeMarkerRecord();
    Snapshot.Basket = Basket->MakeMarkerRecord();
    for (const ADiscGolfDropZoneAuthoringActor* DropZone : DropZones)
    {
        if (!IsValid(DropZone))
        {
            OutError = TEXT("Hole root contains an invalid drop zone reference");
            return Snapshot;
        }
        Snapshot.DropZones.Add(DropZone->MakeMarkerRecord());
    }
    for (const ADiscGolfMandoGateAuthoringActor* Mando : Mandos)
    {
        if (!IsValid(Mando))
        {
            OutError = TEXT("Hole root contains an invalid mando reference");
            return Snapshot;
        }
        Snapshot.Mandos.Add(Mando->MakeMandoRecord());
    }
    for (const ADiscGolfGameplayZoneAuthoringActor* Zone : Zones)
    {
        if (!IsValid(Zone))
        {
            OutError = TEXT("Hole root contains an invalid zone reference");
            return Snapshot;
        }
        Snapshot.Zones.Add(Zone->MakeZoneRecord());
    }
    OutError.Reset();
    return Snapshot;
}

bool ADiscGolfHoleAuthoringActor::BuildHoleDefinition(
    FDGHoleDefinition& OutDefinition,
    FString& OutError) const
{
    const FDiscGolfHoleAuthoringSnapshot Snapshot = MakeSnapshot(OutError);
    return OutError.IsEmpty()
        && DiscGolfCourseAuthoring::ToHoleDefinition(Snapshot, OutDefinition, OutError);
}
