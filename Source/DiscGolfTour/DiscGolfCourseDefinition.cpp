#include "DiscGolfCourseDefinition.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    struct FJsonScope
    {
        bool bIsObject = false;
        TSet<FString> Keys;
    };

    bool RejectDuplicateJsonKeys(const FString& Json, FString& OutError)
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        TArray<FJsonScope> Scopes;
        EJsonNotation Notation;
        while (Reader->ReadNext(Notation))
        {
            if (Notation == EJsonNotation::Error)
            {
                OutError = TEXT("invalid JSON");
                return false;
            }

            if (Notation == EJsonNotation::ObjectEnd || Notation == EJsonNotation::ArrayEnd)
            {
                if (!Scopes.IsEmpty()) Scopes.Pop();
                continue;
            }

            const FString& Identifier = Reader->GetIdentifier();
            if (!Scopes.IsEmpty() && Scopes.Last().bIsObject)
            {
                if (Identifier.IsEmpty() || Scopes.Last().Keys.Contains(Identifier))
                {
                    OutError = Identifier.IsEmpty()
                        ? TEXT("invalid unnamed JSON object field")
                        : FString::Printf(TEXT("duplicate JSON field '%s'"), *Identifier);
                    return false;
                }
                Scopes.Last().Keys.Add(Identifier);
            }

            if (Notation == EJsonNotation::ObjectStart || Notation == EJsonNotation::ArrayStart)
            {
                FJsonScope& Scope = Scopes.AddDefaulted_GetRef();
                Scope.bIsObject = Notation == EJsonNotation::ObjectStart;
            }
        }
        return true;
    }

    bool ValidateAllowedFields(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Context,
        std::initializer_list<const TCHAR*> AllowedFields,
        FString& OutError)
    {
        if (!Object.IsValid())
        {
            OutError = FString::Printf(TEXT("%s must be an object"), Context);
            return false;
        }

        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            bool bAllowed = false;
            for (const TCHAR* Allowed : AllowedFields)
            {
                if (Pair.Key == Allowed)
                {
                    bAllowed = true;
                    break;
                }
            }
            if (!bAllowed)
            {
                OutError = FString::Printf(TEXT("unknown %s field '%s'"), Context, *Pair.Key);
                return false;
            }
        }
        return true;
    }

    bool RequireString(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const TCHAR* Context,
        FString& OutValue,
        FString& OutError)
    {
        if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue))
        {
            OutError = FString::Printf(TEXT("%s.%s must be a string"), Context, Field);
            return false;
        }
        return true;
    }

    bool RequireFiniteNumber(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const TCHAR* Context,
        double& OutValue,
        FString& OutError)
    {
        const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(Field) : nullptr;
        if (!Value || !Value->IsValid() || (*Value)->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("%s.%s must be a number"), Context, Field);
            return false;
        }
        OutValue = (*Value)->AsNumber();
        if (!FMath::IsFinite(OutValue))
        {
            OutError = FString::Printf(TEXT("%s.%s must be finite"), Context, Field);
            return false;
        }
        return true;
    }

    bool RequireInteger(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const TCHAR* Context,
        FString& OutError)
    {
        double Number = 0.0;
        if (!RequireFiniteNumber(Object, Field, Context, Number, OutError)) return false;
        if (FMath::TruncToDouble(Number) != Number
            || Number < static_cast<double>(MIN_int32)
            || Number > static_cast<double>(MAX_int32))
        {
            OutError = FString::Printf(TEXT("%s.%s must be an integer"), Context, Field);
            return false;
        }
        return true;
    }

    bool RequireArray(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const TCHAR* Context,
        const TArray<TSharedPtr<FJsonValue>>*& OutValues,
        FString& OutError)
    {
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, OutValues) || !OutValues)
        {
            OutError = FString::Printf(TEXT("%s.%s must be an array"), Context, Field);
            return false;
        }
        return true;
    }

    bool RequireObject(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const TCHAR* Context,
        TSharedPtr<FJsonObject>& OutValue,
        FString& OutError)
    {
        const TSharedPtr<FJsonObject>* Value = nullptr;
        if (!Object.IsValid() || !Object->TryGetObjectField(Field, Value)
            || !Value || !Value->IsValid())
        {
            OutError = FString::Printf(TEXT("%s.%s must be an object"), Context, Field);
            return false;
        }
        OutValue = *Value;
        return true;
    }

    bool ValidateVectorJson(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Context,
        FString& OutError)
    {
        if (!ValidateAllowedFields(Object, Context, { TEXT("x"), TEXT("y"), TEXT("z") }, OutError))
            return false;
        double Ignored = 0.0;
        return RequireFiniteNumber(Object, TEXT("x"), Context, Ignored, OutError)
            && RequireFiniteNumber(Object, TEXT("y"), Context, Ignored, OutError)
            && RequireFiniteNumber(Object, TEXT("z"), Context, Ignored, OutError);
    }

    bool ValidateVectorField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const TCHAR* Context,
        FString& OutError)
    {
        TSharedPtr<FJsonObject> Vector;
        if (!RequireObject(Object, Field, Context, Vector, OutError)) return false;
        const FString VectorContext = FString::Printf(TEXT("%s.%s"), Context, Field);
        return ValidateVectorJson(Vector, *VectorContext, OutError);
    }

    bool ValidateRotationField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Context,
        FString& OutError)
    {
        TSharedPtr<FJsonObject> Rotation;
        if (!RequireObject(Object, TEXT("rotationDeg"), Context, Rotation, OutError)) return false;
        const FString RotationContext = FString::Printf(TEXT("%s.rotationDeg"), Context);
        if (!ValidateAllowedFields(Rotation, *RotationContext,
            { TEXT("pitch"), TEXT("yaw"), TEXT("roll") }, OutError)) return false;
        double Ignored = 0.0;
        return RequireFiniteNumber(Rotation, TEXT("pitch"), *RotationContext, Ignored, OutError)
            && RequireFiniteNumber(Rotation, TEXT("yaw"), *RotationContext, Ignored, OutError)
            && RequireFiniteNumber(Rotation, TEXT("roll"), *RotationContext, Ignored, OutError);
    }

    bool RequireKnownToken(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const TCHAR* Context,
        std::initializer_list<const TCHAR*> AllowedTokens,
        FString& OutError)
    {
        FString Token;
        if (!RequireString(Object, Field, Context, Token, OutError)) return false;
        for (const TCHAR* Allowed : AllowedTokens)
        {
            if (Token.Equals(Allowed, ESearchCase::IgnoreCase)) return true;
        }
        OutError = FString::Printf(TEXT("unknown %s.%s token '%s'"), Context, Field, *Token);
        return false;
    }

    bool RequireObjectArrayElement(
        const TSharedPtr<FJsonValue>& Value,
        const TCHAR* ArrayName,
        int32 Index,
        TSharedPtr<FJsonObject>& OutObject,
        FString& OutError)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object || !Value->AsObject().IsValid())
        {
            OutError = FString::Printf(TEXT("%s[%d] must be an object"), ArrayName, Index);
            return false;
        }
        OutObject = Value->AsObject();
        return true;
    }

    bool ValidateManifestJsonShape(const TSharedPtr<FJsonObject>& Root, FString& OutError)
    {
        if (!ValidateAllowedFields(Root, TEXT("manifest"),
            { TEXT("schema"), TEXT("schemaVersion"), TEXT("courseId"), TEXT("layoutId"),
              TEXT("displayName"), TEXT("holes") }, OutError)) return false;
        FString IgnoredString;
        if (!RequireString(Root, TEXT("schema"), TEXT("manifest"), IgnoredString, OutError)
            || !RequireInteger(Root, TEXT("schemaVersion"), TEXT("manifest"), OutError)
            || !RequireString(Root, TEXT("courseId"), TEXT("manifest"), IgnoredString, OutError)
            || !RequireString(Root, TEXT("layoutId"), TEXT("manifest"), IgnoredString, OutError)
            || !RequireString(Root, TEXT("displayName"), TEXT("manifest"), IgnoredString, OutError)) return false;
        const TArray<TSharedPtr<FJsonValue>>* Holes = nullptr;
        if (!RequireArray(Root, TEXT("holes"), TEXT("manifest"), Holes, OutError)) return false;
        for (int32 Index = 0; Index < Holes->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Hole;
            if (!RequireObjectArrayElement((*Holes)[Index], TEXT("manifest.holes"), Index, Hole, OutError)) return false;
            const FString Context = FString::Printf(TEXT("manifest.holes[%d]"), Index);
            if (!ValidateAllowedFields(Hole, *Context,
                { TEXT("holeNumber"), TEXT("definitionFile"), TEXT("worldOriginCm"), TEXT("worldYawDeg") }, OutError)
                || !RequireInteger(Hole, TEXT("holeNumber"), *Context, OutError)
                || !RequireString(Hole, TEXT("definitionFile"), *Context, IgnoredString, OutError)
                || !ValidateVectorField(Hole, TEXT("worldOriginCm"), *Context, OutError)) return false;
            double IgnoredNumber = 0.0;
            if (!RequireFiniteNumber(Hole, TEXT("worldYawDeg"), *Context, IgnoredNumber, OutError)) return false;
        }
        return true;
    }

    bool ValidateHoleJsonShape(const TSharedPtr<FJsonObject>& Root, FString& OutError)
    {
        if (!ValidateAllowedFields(Root, TEXT("hole"),
            { TEXT("schema"), TEXT("schemaVersion"), TEXT("courseId"), TEXT("layoutId"),
              TEXT("holeNumber"), TEXT("holeName"), TEXT("par"), TEXT("teeLocationCm"),
              TEXT("basketLocationCm"), TEXT("surfaces"), TEXT("trees"), TEXT("collisionFixtures"),
              TEXT("landingZones"), TEXT("shotRoutes"), TEXT("cameraAnchors"),
              TEXT("spectatorBoundaries"), TEXT("windZones"), TEXT("flyoverPointsCm") }, OutError)) return false;

        FString IgnoredString;
        if (!RequireString(Root, TEXT("schema"), TEXT("hole"), IgnoredString, OutError)
            || !RequireInteger(Root, TEXT("schemaVersion"), TEXT("hole"), OutError)
            || !RequireString(Root, TEXT("courseId"), TEXT("hole"), IgnoredString, OutError)
            || !RequireString(Root, TEXT("layoutId"), TEXT("hole"), IgnoredString, OutError)
            || !RequireInteger(Root, TEXT("holeNumber"), TEXT("hole"), OutError)
            || !RequireString(Root, TEXT("holeName"), TEXT("hole"), IgnoredString, OutError)
            || !RequireInteger(Root, TEXT("par"), TEXT("hole"), OutError)
            || !ValidateVectorField(Root, TEXT("teeLocationCm"), TEXT("hole"), OutError)
            || !ValidateVectorField(Root, TEXT("basketLocationCm"), TEXT("hole"), OutError)) return false;

        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!RequireArray(Root, TEXT("surfaces"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("surfaces"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("surfaces[%d]"), Index);
            if (!ValidateAllowedFields(O, *C, { TEXT("id"), TEXT("surface"), TEXT("shape"), TEXT("locationCm"), TEXT("scale"), TEXT("rotationDeg") }, OutError)
                || !RequireString(O, TEXT("id"), *C, IgnoredString, OutError)
                || !RequireKnownToken(O, TEXT("surface"), *C,
                    { TEXT("Fairway"), TEXT("TeePad"), TEXT("LightRough"), TEXT("DeepRough"), TEXT("Dirt"), TEXT("Rock"), TEXT("OutOfBounds"), TEXT("Hazard") }, OutError)
                || !RequireKnownToken(O, TEXT("shape"), *C, { TEXT("Box"), TEXT("Cylinder"), TEXT("Sphere") }, OutError)
                || !ValidateVectorField(O, TEXT("locationCm"), *C, OutError)
                || !ValidateVectorField(O, TEXT("scale"), *C, OutError)
                || !ValidateRotationField(O, *C, OutError)) return false;
        }

        if (!RequireArray(Root, TEXT("trees"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("trees"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("trees[%d]"), Index);
            double IgnoredNumber = 0.0;
            if (!ValidateAllowedFields(O, *C, { TEXT("locationCm"), TEXT("heightScale") }, OutError)
                || !ValidateVectorField(O, TEXT("locationCm"), *C, OutError)
                || !RequireFiniteNumber(O, TEXT("heightScale"), *C, IgnoredNumber, OutError)) return false;
        }

        if (!RequireArray(Root, TEXT("collisionFixtures"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("collisionFixtures"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("collisionFixtures[%d]"), Index);
            if (!ValidateAllowedFields(O, *C, { TEXT("id"), TEXT("type"), TEXT("shape"), TEXT("locationCm"), TEXT("scale"), TEXT("rotationDeg") }, OutError)
                || !RequireString(O, TEXT("id"), *C, IgnoredString, OutError)
                || !RequireKnownToken(O, TEXT("type"), *C,
                    { TEXT("Tree"), TEXT("DenseGrass"), TEXT("Grass"), TEXT("Brush"), TEXT("Rock"), TEXT("Sign") }, OutError)
                || !RequireKnownToken(O, TEXT("shape"), *C, { TEXT("Box"), TEXT("Cylinder"), TEXT("Sphere") }, OutError)
                || !ValidateVectorField(O, TEXT("locationCm"), *C, OutError)
                || !ValidateVectorField(O, TEXT("scale"), *C, OutError)
                || !ValidateRotationField(O, *C, OutError)) return false;
        }

        if (!RequireArray(Root, TEXT("landingZones"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("landingZones"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("landingZones[%d]"), Index);
            if (!ValidateAllowedFields(O, *C, { TEXT("id"), TEXT("label"), TEXT("locationCm"), TEXT("extentCm"), TEXT("rotationDeg") }, OutError)
                || !RequireString(O, TEXT("id"), *C, IgnoredString, OutError)
                || !RequireString(O, TEXT("label"), *C, IgnoredString, OutError)
                || !ValidateVectorField(O, TEXT("locationCm"), *C, OutError)
                || !ValidateVectorField(O, TEXT("extentCm"), *C, OutError)
                || !ValidateRotationField(O, *C, OutError)) return false;
        }

        if (!RequireArray(Root, TEXT("shotRoutes"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("shotRoutes"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("shotRoutes[%d]"), Index);
            if (!ValidateAllowedFields(O, *C,
                { TEXT("id"), TEXT("label"), TEXT("type"), TEXT("shotIntent"), TEXT("landingZoneId"),
                  TEXT("targetStrokes"), TEXT("riskRating"), TEXT("rewardRating"), TEXT("corridorWidthCm"), TEXT("waypointsCm") }, OutError)
                || !RequireString(O, TEXT("id"), *C, IgnoredString, OutError)
                || !RequireString(O, TEXT("label"), *C, IgnoredString, OutError)
                || !RequireKnownToken(O, TEXT("type"), *C, { TEXT("Primary"), TEXT("RiskReward"), TEXT("Bailout") }, OutError)
                || !RequireString(O, TEXT("shotIntent"), *C, IgnoredString, OutError)
                || !RequireString(O, TEXT("landingZoneId"), *C, IgnoredString, OutError)
                || !RequireInteger(O, TEXT("targetStrokes"), *C, OutError)
                || !RequireInteger(O, TEXT("riskRating"), *C, OutError)
                || !RequireInteger(O, TEXT("rewardRating"), *C, OutError)) return false;
            double IgnoredNumber = 0.0;
            if (!RequireFiniteNumber(O, TEXT("corridorWidthCm"), *C, IgnoredNumber, OutError)) return false;
            const TArray<TSharedPtr<FJsonValue>>* Waypoints = nullptr;
            if (!RequireArray(O, TEXT("waypointsCm"), *C, Waypoints, OutError)) return false;
            for (int32 PointIndex = 0; PointIndex < Waypoints->Num(); ++PointIndex)
            {
                TSharedPtr<FJsonObject> Point;
                if (!RequireObjectArrayElement((*Waypoints)[PointIndex], TEXT("shotRoutes.waypointsCm"), PointIndex, Point, OutError)) return false;
                const FString PointContext = FString::Printf(TEXT("%s.waypointsCm[%d]"), *C, PointIndex);
                if (!ValidateVectorJson(Point, *PointContext, OutError)) return false;
            }
        }

        if (!RequireArray(Root, TEXT("cameraAnchors"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("cameraAnchors"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("cameraAnchors[%d]"), Index);
            double IgnoredNumber = 0.0;
            if (!ValidateAllowedFields(O, *C, { TEXT("id"), TEXT("mode"), TEXT("locationCm"), TEXT("fieldOfViewDeg") }, OutError)
                || !RequireString(O, TEXT("id"), *C, IgnoredString, OutError)
                || !RequireKnownToken(O, TEXT("mode"), *C, { TEXT("Launch"), TEXT("Fairway"), TEXT("Finish") }, OutError)
                || !ValidateVectorField(O, TEXT("locationCm"), *C, OutError)
                || !RequireFiniteNumber(O, TEXT("fieldOfViewDeg"), *C, IgnoredNumber, OutError)) return false;
        }

        if (!RequireArray(Root, TEXT("spectatorBoundaries"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("spectatorBoundaries"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("spectatorBoundaries[%d]"), Index);
            if (!ValidateAllowedFields(O, *C, { TEXT("id"), TEXT("locationCm"), TEXT("extentCm"), TEXT("rotationDeg") }, OutError)
                || !RequireString(O, TEXT("id"), *C, IgnoredString, OutError)
                || !ValidateVectorField(O, TEXT("locationCm"), *C, OutError)
                || !ValidateVectorField(O, TEXT("extentCm"), *C, OutError)
                || !ValidateRotationField(O, *C, OutError)) return false;
        }

        if (!RequireArray(Root, TEXT("windZones"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> O;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("windZones"), Index, O, OutError)) return false;
            const FString C = FString::Printf(TEXT("windZones[%d]"), Index);
            double IgnoredNumber = 0.0;
            if (!ValidateAllowedFields(O, *C, { TEXT("id"), TEXT("locationCm"), TEXT("extentCm"), TEXT("baseWindScale"), TEXT("additiveWindMps") }, OutError)
                || !RequireString(O, TEXT("id"), *C, IgnoredString, OutError)
                || !ValidateVectorField(O, TEXT("locationCm"), *C, OutError)
                || !ValidateVectorField(O, TEXT("extentCm"), *C, OutError)
                || !RequireFiniteNumber(O, TEXT("baseWindScale"), *C, IgnoredNumber, OutError)
                || !ValidateVectorField(O, TEXT("additiveWindMps"), *C, OutError)) return false;
        }

        if (!RequireArray(Root, TEXT("flyoverPointsCm"), TEXT("hole"), Values, OutError)) return false;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Point;
            if (!RequireObjectArrayElement((*Values)[Index], TEXT("flyoverPointsCm"), Index, Point, OutError)) return false;
            const FString C = FString::Printf(TEXT("flyoverPointsCm[%d]"), Index);
            if (!ValidateVectorJson(Point, *C, OutError)) return false;
        }
        return true;
    }

    FString DescribeAuthoredDataFailure(
        const TCHAR* DataLabel,
        const FString& Path,
        EDiscGolfAuthoredCourseDataState DataState,
        const FString& Detail)
    {
        const TCHAR* StateLabel = DataState == EDiscGolfAuthoredCourseDataState::Missing
            ? TEXT("missing")
            : TEXT("invalid");
        const FString Cause = Detail.IsEmpty()
            ? FString::Printf(TEXT("authored data is %s"), StateLabel)
            : Detail;
        return FString::Printf(
            TEXT("Shipping build requires valid authored Pine Ridge %s JSON at '%s'; %s"),
            DataLabel,
            *Path,
            *Cause);
    }

    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    FVector VectorFromJson(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FVector& Default)
    {
        const TSharedPtr<FJsonObject>* VectorObject = nullptr;
        if (!Object.IsValid() || !Object->TryGetObjectField(Field, VectorObject) || !VectorObject || !VectorObject->IsValid())
        {
            return Default;
        }
        return FVector(
            (*VectorObject)->GetNumberField(TEXT("x")),
            (*VectorObject)->GetNumberField(TEXT("y")),
            (*VectorObject)->GetNumberField(TEXT("z")));
    }

    FRotator RotationFromJson(const TSharedPtr<FJsonObject>& Object)
    {
        const TSharedPtr<FJsonObject>* RotationObject = nullptr;
        if (!Object.IsValid() || !Object->TryGetObjectField(TEXT("rotationDeg"), RotationObject)
            || !RotationObject || !RotationObject->IsValid())
        {
            return FRotator::ZeroRotator;
        }
        return FRotator(
            (*RotationObject)->GetNumberField(TEXT("pitch")),
            (*RotationObject)->GetNumberField(TEXT("yaw")),
            (*RotationObject)->GetNumberField(TEXT("roll")));
    }

    ECourseSurfaceType ParseSurface(const FString& Value)
    {
        if (Value.Equals(TEXT("TeePad"), ESearchCase::IgnoreCase)) return ECourseSurfaceType::TeePad;
        if (Value.Equals(TEXT("LightRough"), ESearchCase::IgnoreCase)) return ECourseSurfaceType::LightRough;
        if (Value.Equals(TEXT("DeepRough"), ESearchCase::IgnoreCase)) return ECourseSurfaceType::DeepRough;
        if (Value.Equals(TEXT("Dirt"), ESearchCase::IgnoreCase)) return ECourseSurfaceType::Dirt;
        if (Value.Equals(TEXT("Rock"), ESearchCase::IgnoreCase)) return ECourseSurfaceType::Rock;
        if (Value.Equals(TEXT("OutOfBounds"), ESearchCase::IgnoreCase)) return ECourseSurfaceType::OutOfBounds;
        if (Value.Equals(TEXT("Hazard"), ESearchCase::IgnoreCase)) return ECourseSurfaceType::Hazard;
        return ECourseSurfaceType::Fairway;
    }

    EDiscGolfPrimitiveShape ParseShape(const FString& Value)
    {
        if (Value.Equals(TEXT("Cylinder"), ESearchCase::IgnoreCase)) return EDiscGolfPrimitiveShape::Cylinder;
        if (Value.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase)) return EDiscGolfPrimitiveShape::Sphere;
        return EDiscGolfPrimitiveShape::Box;
    }

    EDiscGolfFixtureType ParseFixtureType(const FString& Value)
    {
        if (Value.Equals(TEXT("Tree"), ESearchCase::IgnoreCase)) return EDiscGolfFixtureType::Tree;
        if (Value.Equals(TEXT("DenseGrass"), ESearchCase::IgnoreCase)
            || Value.Equals(TEXT("Grass"), ESearchCase::IgnoreCase)
            || Value.Equals(TEXT("Brush"), ESearchCase::IgnoreCase))
        {
            return EDiscGolfFixtureType::DenseGrass;
        }
        if (Value.Equals(TEXT("Rock"), ESearchCase::IgnoreCase)) return EDiscGolfFixtureType::Rock;
        if (Value.Equals(TEXT("Sign"), ESearchCase::IgnoreCase)) return EDiscGolfFixtureType::Sign;
        return EDiscGolfFixtureType::Unknown;
    }

    EDiscGolfCameraAnchorMode ParseCameraMode(const FString& Value)
    {
        if (Value.Equals(TEXT("Fairway"), ESearchCase::IgnoreCase)) return EDiscGolfCameraAnchorMode::Fairway;
        if (Value.Equals(TEXT("Finish"), ESearchCase::IgnoreCase)) return EDiscGolfCameraAnchorMode::Finish;
        return EDiscGolfCameraAnchorMode::Launch;
    }

    EDiscGolfShotRouteType ParseShotRouteType(const FString& Value)
    {
        if (Value.Equals(TEXT("RiskReward"), ESearchCase::IgnoreCase))
            return EDiscGolfShotRouteType::RiskReward;
        if (Value.Equals(TEXT("Bailout"), ESearchCase::IgnoreCase))
            return EDiscGolfShotRouteType::Bailout;
        return EDiscGolfShotRouteType::Primary;
    }

    template<typename T>
    bool HasDuplicateIds(const TArray<T>& Values, TFunctionRef<FName(const T&)> GetId)
    {
        TSet<FName> Seen;
        for (const T& Value : Values)
        {
            const FName Id = GetId(Value);
            if (Id.IsNone() || Seen.Contains(Id)) return true;
            Seen.Add(Id);
        }
        return false;
    }
}

EDiscGolfAuthoredCourseLoadAction DiscGolfCourseDefinition::ResolveAuthoredCourseLoadAction(
    EDiscGolfAuthoredCourseDataState DataState,
    bool bIsShippingBuild)
{
    if (DataState == EDiscGolfAuthoredCourseDataState::Valid)
    {
        return EDiscGolfAuthoredCourseLoadAction::UseAuthoredData;
    }
    return bIsShippingBuild
        ? EDiscGolfAuthoredCourseLoadAction::FailClosed
        : EDiscGolfAuthoredCourseLoadAction::UseSourceFallback;
}

FDiscGolfCourseManifestDefinition DiscGolfCourseDefinition::PineRidgeCourseFallback()
{
    FDiscGolfCourseManifestDefinition Manifest;
    Manifest.SchemaVersion = 1;
    Manifest.CourseId = TEXT("PineRidgeChampionship");
    Manifest.LayoutId = TEXT("Championship");
    Manifest.DisplayName = FText::FromString(TEXT("Pine Ridge Championship"));
    const FVector WorldOrigins[] =
    {
        FVector(0.0f, 0.0f, 0.0f),
        FVector(14500.0f, 4000.0f, 100.0f),
        FVector(36000.0f, 10500.0f, -250.0f)
    };
    const float WorldYaws[] = { 0.0f, 8.0f, -12.0f };
    for (int32 HoleNumber = 1; HoleNumber <= 3; ++HoleNumber)
    {
        FDiscGolfCourseManifestHoleEntry Entry;
        Entry.HoleNumber = HoleNumber;
        Entry.DefinitionFile = FString::Printf(TEXT("Data/PineRidgeHole%d.json"), HoleNumber);
        Entry.WorldOriginCm = WorldOrigins[HoleNumber - 1];
        Entry.WorldYawDeg = WorldYaws[HoleNumber - 1];
        Manifest.Holes.Add(Entry);
    }
    return Manifest;
}

FDiscGolfHoleBlockoutDefinition DiscGolfCourseDefinition::PineRidgeHole1Fallback()
{
    FDiscGolfHoleBlockoutDefinition D;
    D.SchemaVersion = 1;
    D.CourseId = TEXT("PineRidgeChampionship");
    D.LayoutId = TEXT("Championship");
    D.HoleNumber = 1;
    D.HoleName = FText::FromString(TEXT("Pine Ridge Opening"));
    D.Par = 3;
    D.TeeLocationCm = FVector::ZeroVector;
    D.BasketLocationCm = FVector(11000.0f, 800.0f, 80.0f);

    auto Surface = [&D](const TCHAR* Id, ECourseSurfaceType Type, EDiscGolfPrimitiveShape Shape,
        FVector Location, FVector Scale, float Yaw = 0.0f)
    {
        FDiscGolfBlockoutSurfaceDefinition S;
        S.SurfaceId = Id; S.SurfaceType = Type; S.Shape = Shape; S.LocationCm = Location; S.Scale = Scale;
        S.Rotation = FRotator(0.0f, Yaw, 0.0f); D.Surfaces.Add(S);
    };
    Surface(TEXT("DeepRoughBase"), ECourseSurfaceType::DeepRough, EDiscGolfPrimitiveShape::Box,
        FVector(5600, 300, -55), FVector(125, 72, 1));
    Surface(TEXT("OpeningFairway"), ECourseSurfaceType::Fairway, EDiscGolfPrimitiveShape::Box,
        FVector(2600, -80, -1), FVector(53, 14, 0.08f), 4.0f);
    Surface(TEXT("LandingFairway"), ECourseSurfaceType::Fairway, EDiscGolfPrimitiveShape::Box,
        FVector(7000, 260, 4), FVector(48, 16, 0.08f), 2.0f);
    Surface(TEXT("Green"), ECourseSurfaceType::Fairway, EDiscGolfPrimitiveShape::Cylinder,
        FVector(11000, 800, 55), FVector(24, 24, 0.50f));
    Surface(TEXT("Tee"), ECourseSurfaceType::TeePad, EDiscGolfPrimitiveShape::Box,
        FVector(0, 0, 4), FVector(4.2f, 1.8f, 0.08f));
    Surface(TEXT("LeftApron"), ECourseSurfaceType::LightRough, EDiscGolfPrimitiveShape::Box,
        FVector(7100, -1550, 7), FVector(40, 12, 0.07f), 2.0f);
    Surface(TEXT("RightApron"), ECourseSurfaceType::LightRough, EDiscGolfPrimitiveShape::Box,
        FVector(7200, 2050, 7), FVector(40, 11, 0.07f), 2.0f);
    Surface(TEXT("SkipShelf"), ECourseSurfaceType::Dirt, EDiscGolfPrimitiveShape::Box,
        FVector(9100, -450, 12), FVector(15, 5, 0.10f), -8.0f);
    Surface(TEXT("GuardRock"), ECourseSurfaceType::Rock, EDiscGolfPrimitiveShape::Box,
        FVector(10100, 1850, 18), FVector(9, 4, 0.12f), 14.0f);
    Surface(TEXT("TournamentRopeOB"), ECourseSurfaceType::OutOfBounds, EDiscGolfPrimitiveShape::Box,
        FVector(6800, -4300, 8), FVector(75, 18, 0.08f), 2.0f);
    Surface(TEXT("PineNeedleHazard"), ECourseSurfaceType::Hazard, EDiscGolfPrimitiveShape::Box,
        FVector(9600, 3150, 9), FVector(26, 10, 0.09f), 8.0f);

    const FVector Trees[] = {
        FVector(3300,-2050,0), FVector(4300,1900,0), FVector(5600,-2300,0), FVector(6400,2350,0),
        FVector(7900,-2050,0), FVector(8350,2350,0), FVector(9300,-1700,0), FVector(9700,2200,0),
        FVector(10300,-1150,0), FVector(10500,2600,0), FVector(11400,-650,0), FVector(11800,1700,0)
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Trees); ++Index)
    {
        FDiscGolfTreeDefinition Tree; Tree.LocationCm = Trees[Index]; Tree.HeightScale = 0.92f + 0.07f * (Index % 4);
        D.Trees.Add(Tree);
    }

    auto Fixture = [&D](const TCHAR* Id, EDiscGolfFixtureType Type, EDiscGolfPrimitiveShape Shape,
        FVector Location, FVector Scale, float Yaw = 0.0f)
    {
        FDiscGolfCollisionFixtureDefinition F;
        F.FixtureId = Id; F.FixtureType = Type; F.Shape = Shape; F.LocationCm = Location; F.Scale = Scale;
        F.Rotation = FRotator(0.0f, Yaw, 0.0f); D.CollisionFixtures.Add(F);
    };
    Fixture(TEXT("LandingBrush"), EDiscGolfFixtureType::DenseGrass, EDiscGolfPrimitiveShape::Box,
        FVector(7000,-1550,90), FVector(5.5f,2.8f,1.8f), 8.0f);
    Fixture(TEXT("GreenBoulder"), EDiscGolfFixtureType::Rock, EDiscGolfPrimitiveShape::Sphere,
        FVector(10150,2550,52), FVector(1.05f,0.85f,0.80f));
    Fixture(TEXT("TeeSponsorSign"), EDiscGolfFixtureType::Sign, EDiscGolfPrimitiveShape::Box,
        FVector(900,-1300,135), FVector(0.12f,2.0f,1.35f), 12.0f);

    auto Landing = [&D](const TCHAR* Id, const TCHAR* Label, FVector Location, FVector Extent, float Yaw)
    {
        FDiscGolfLandingZoneDefinition Z; Z.ZoneId = Id; Z.Label = FText::FromString(Label);
        Z.LocationCm = Location; Z.ExtentCm = Extent; Z.Rotation = FRotator(0, Yaw, 0); D.LandingZones.Add(Z);
    };
    Landing(TEXT("PrimaryLanding"), TEXT("PRIMARY LANDING"), FVector(6900, 250, 35), FVector(1450, 720, 28), 2.0f);
    Landing(TEXT("BailoutLanding"), TEXT("SAFE BAILOUT"), FVector(6100, 1600, 35), FVector(950, 560, 28), 5.0f);

    auto Route = [&D](const TCHAR* Id, const TCHAR* Label, EDiscGolfShotRouteType Type,
        const TCHAR* Intent, const TCHAR* LandingZone, int32 TargetStrokes, int32 Risk, int32 Reward,
        float WidthCm, std::initializer_list<FVector> Waypoints)
    {
        FDiscGolfShotRouteDefinition R;
        R.RouteId = Id; R.Label = FText::FromString(Label); R.RouteType = Type;
        R.ShotIntent = FText::FromString(Intent); R.LandingZoneId = LandingZone;
        R.TargetStrokes = TargetStrokes; R.RiskRating = Risk; R.RewardRating = Reward;
        R.CorridorWidthCm = WidthCm; R.WaypointsCm.Append(Waypoints); D.ShotRoutes.Add(R);
    };
    Route(TEXT("CenterPlacement"), TEXT("CENTER PLACEMENT"), EDiscGolfShotRouteType::Primary,
        TEXT("Shape a controlled fairway drive through the opening and leave a clean green approach."),
        TEXT("PrimaryLanding"), 2, 3, 4, 1500.0f,
        { FVector(0,0,20), FVector(6900,250,45), FVector(11000,800,100) });
    Route(TEXT("SkipShelfAttack"), TEXT("SKIP-SHELF ATTACK"), EDiscGolfShotRouteType::RiskReward,
        TEXT("Challenge the left-center shelf with a low stable line for a direct birdie or ace look."),
        TEXT("PrimaryLanding"), 2, 5, 5, 900.0f,
        { FVector(0,0,20), FVector(9000,-450,45), FVector(11000,800,100) });
    Route(TEXT("RightBailout"), TEXT("TURNOVER / FOREHAND"), EDiscGolfShotRouteType::Bailout,
        TEXT("Use the wider right window with a committed turnover or forehand and shape back toward the framed green."),
        TEXT("BailoutLanding"), 3, 1, 2, 1400.0f,
        { FVector(0,0,20), FVector(6100,1600,45), FVector(9000,1800,65), FVector(11000,800,100) });

    auto Camera = [&D](const TCHAR* Id, EDiscGolfCameraAnchorMode Mode, FVector Location, float Fov)
    {
        FDiscGolfCameraAnchorDefinition A; A.AnchorId = Id; A.Mode = Mode; A.LocationCm = Location;
        A.FieldOfViewDeg = Fov; D.CameraAnchors.Add(A);
    };
    Camera(TEXT("TeeBroadcast"), EDiscGolfCameraAnchorMode::Launch, FVector(-450, 780, 300), 66.0f);
    Camera(TEXT("FairwayBroadcast"), EDiscGolfCameraAnchorMode::Fairway, FVector(5500, 3600, 1450), 38.0f);
    Camera(TEXT("GreenBroadcast"), EDiscGolfCameraAnchorMode::Finish, FVector(11750, 2200, 720), 46.0f);

    auto Boundary = [&D](const TCHAR* Id, FVector Location, FVector Extent, float Yaw)
    {
        FDiscGolfSpectatorBoundaryDefinition B; B.BoundaryId = Id; B.LocationCm = Location; B.ExtentCm = Extent;
        B.Rotation = FRotator(0, Yaw, 0); D.SpectatorBoundaries.Add(B);
    };
    Boundary(TEXT("GalleryLeft"), FVector(7600,-2850,90), FVector(3900,35,90), 2.0f);
    Boundary(TEXT("GalleryRight"), FVector(7600,2850,90), FVector(3900,35,90), 2.0f);
    Boundary(TEXT("GreenGallery"), FVector(11900,800,90), FVector(35,1350,90), 0.0f);

    FDiscGolfWindZoneDefinition CorridorWind;
    CorridorWind.ZoneId = TEXT("OpenCorridor"); CorridorWind.LocationCm = FVector(4300,0,700);
    CorridorWind.ExtentCm = FVector(3300,1800,1200); CorridorWind.BaseWindScale = 1.15f;
    CorridorWind.AdditiveWindMps = FVector(0.0f,0.35f,0.0f); D.WindZones.Add(CorridorWind);
    FDiscGolfWindZoneDefinition GreenShelter;
    GreenShelter.ZoneId = TEXT("GuardedGreen"); GreenShelter.LocationCm = FVector(10300,850,650);
    GreenShelter.ExtentCm = FVector(1900,1700,1000); GreenShelter.BaseWindScale = 0.72f;
    GreenShelter.AdditiveWindMps = FVector(0.0f,-0.20f,0.0f); D.WindZones.Add(GreenShelter);

    D.FlyoverPointsCm = {
        FVector(-900,-1100,500), FVector(1200,-600,720), FVector(4100,500,1050),
        FVector(7100,900,1250), FVector(9300,1700,950), FVector(11300,1200,560)
    };
    return D;
}

FDiscGolfHoleBlockoutDefinition DiscGolfCourseDefinition::PineRidgeHole2Fallback()
{
    FDiscGolfHoleBlockoutDefinition D = PineRidgeHole1Fallback();
    D.HoleNumber = 2;
    D.HoleName = FText::FromString(TEXT("Needle Gate"));
    D.Par = 4;
    D.BasketLocationCm = FVector(19450.0f, 1500.0f, 120.0f);
    D.Surfaces[0].LocationCm = FVector(9800,700,-55); D.Surfaces[0].Scale = FVector(218,76,1);
    D.Surfaces[1].SurfaceId = TEXT("GateFairway"); D.Surfaces[1].LocationCm = FVector(3100,0,-1); D.Surfaces[1].Scale = FVector(61,9,0.08f);
    D.Surfaces[2].SurfaceId = TEXT("LandingFairway"); D.Surfaces[2].LocationCm = FVector(13800,920,18); D.Surfaces[2].Scale = FVector(67,17,0.12f);
    D.Surfaces[3].LocationCm = FVector(19450,1500,95); D.Surfaces[3].Scale = FVector(25,25,0.5f);
    D.Surfaces[5].LocationCm = FVector(9800,-1150,7); D.Surfaces[5].Scale = FVector(70,10,0.08f);
    D.Surfaces[6].LocationCm = FVector(10900,2500,8); D.Surfaces[6].Scale = FVector(68,11,0.08f);
    D.Surfaces[7].LocationCm = FVector(11900,-650,14); D.Surfaces[8].LocationCm = FVector(15400,2600,24);
    D.Surfaces[9].LocationCm = FVector(11500,-4300,8); D.Surfaces[9].Scale = FVector(125,18,0.08f);
    D.Surfaces[10].LocationCm = FVector(15800,4100,9); D.Surfaces[10].Scale = FVector(58,14,0.09f);
    FDiscGolfBlockoutSurfaceDefinition Corridor = D.Surfaces[1];
    Corridor.SurfaceId = TEXT("NeedleCorridor"); Corridor.LocationCm = FVector(8200,330,3);
    Corridor.Scale = FVector(49,8,0.08f); Corridor.Rotation = FRotator(0,4,0); D.Surfaces.Add(Corridor);
    const FVector Trees[] = {
        FVector(3900,-560,0), FVector(3900,560,0), FVector(5200,-1050,0), FVector(5350,1100,0),
        FVector(6800,-950,0), FVector(7000,1300,0), FVector(8600,-900,0), FVector(8800,1450,0),
        FVector(10600,-1250,0), FVector(11100,2200,0), FVector(13100,-1450,0), FVector(13700,2800,0),
        FVector(15300,-1100,0), FVector(16000,3200,0), FVector(17600,-700,0), FVector(18100,3000,0),
        FVector(19900,-150,0), FVector(20300,3050,0)
    };
    D.Trees.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Trees); ++Index)
    {
        FDiscGolfTreeDefinition Tree; Tree.LocationCm = Trees[Index]; Tree.HeightScale = 0.98f + 0.05f * (Index % 4); D.Trees.Add(Tree);
    }
    D.CollisionFixtures.Reset();
    auto Fixture = [&D](const TCHAR* Id, EDiscGolfFixtureType Type, EDiscGolfPrimitiveShape Shape,
        FVector Location, FVector Scale, float Yaw = 0.0f)
    {
        FDiscGolfCollisionFixtureDefinition F;
        F.FixtureId = Id; F.FixtureType = Type; F.Shape = Shape; F.LocationCm = Location; F.Scale = Scale;
        F.Rotation = FRotator(0.0f, Yaw, 0.0f); D.CollisionFixtures.Add(F);
    };
    Fixture(TEXT("GateBrush"), EDiscGolfFixtureType::DenseGrass, EDiscGolfPrimitiveShape::Box,
        FVector(7800,2000,100), FVector(5.0f,2.8f,2.0f), 6.0f);
    Fixture(TEXT("GateBoulder"), EDiscGolfFixtureType::Rock, EDiscGolfPrimitiveShape::Sphere,
        FVector(15400,2850,76), FVector(1.15f,0.95f,0.90f));
    Fixture(TEXT("DirectionSign"), EDiscGolfFixtureType::Sign, EDiscGolfPrimitiveShape::Box,
        FVector(1200,-1450,135), FVector(0.12f,2.0f,1.35f), 10.0f);
    D.LandingZones[0].ZoneId = TEXT("GateLanding"); D.LandingZones[0].Label = FText::FromString(TEXT("THREAD THE GATE"));
    D.LandingZones[0].LocationCm = FVector(12600,650,40); D.LandingZones[0].ExtentCm = FVector(1750,620,28);
    D.LandingZones[1].ZoneId = TEXT("PitchOutLanding"); D.LandingZones[1].Label = FText::FromString(TEXT("SAFE PITCH OUT"));
    D.LandingZones[1].LocationCm = FVector(10700,-900,38); D.LandingZones[1].ExtentCm = FVector(1250,520,28);
    D.ShotRoutes.Reset();
    auto Route = [&D](const TCHAR* Id, const TCHAR* Label, EDiscGolfShotRouteType Type,
        const TCHAR* Intent, const TCHAR* LandingZone, int32 TargetStrokes, int32 Risk, int32 Reward,
        float WidthCm, std::initializer_list<FVector> Waypoints)
    {
        FDiscGolfShotRouteDefinition R;
        R.RouteId = Id; R.Label = FText::FromString(Label); R.RouteType = Type;
        R.ShotIntent = FText::FromString(Intent); R.LandingZoneId = LandingZone;
        R.TargetStrokes = TargetStrokes; R.RiskRating = Risk; R.RewardRating = Reward;
        R.CorridorWidthCm = WidthCm; R.WaypointsCm.Append(Waypoints); D.ShotRoutes.Add(R);
    };
    Route(TEXT("NeedlePlacement"), TEXT("NEEDLE-GATE PLACEMENT"), EDiscGolfShotRouteType::Primary,
        TEXT("Hit the first gate at controlled speed, then attack from the center landing window."),
        TEXT("GateLanding"), 3, 3, 4, 1000.0f,
        { FVector(0,0,20), FVector(7600,300,35), FVector(12600,650,60), FVector(19450,1500,140) });
    Route(TEXT("LateCrosswindAttack"), TEXT("LATE-CROSSWIND ATTACK"), EDiscGolfShotRouteType::RiskReward,
        TEXT("Push deeper through the narrow corridor to shorten the exposed second shot."),
        TEXT("GateLanding"), 3, 5, 5, 750.0f,
        { FVector(0,0,20), FVector(8500,500,35), FVector(14800,1200,70), FVector(19450,1500,140) });
    Route(TEXT("LeftPitchOut"), TEXT("LEFT PITCH-OUT PAR"), EDiscGolfShotRouteType::Bailout,
        TEXT("Use the wider left pocket, concede distance, and preserve a clean three-shot par plan."),
        TEXT("PitchOutLanding"), 4, 1, 2, 1250.0f,
        { FVector(0,0,20), FVector(7000,-700,35), FVector(10700,-900,55),
          FVector(15700,-250,85), FVector(19450,1500,140) });
    D.CameraAnchors[0].LocationCm = FVector(-500,850,340);
    D.CameraAnchors[1].LocationCm = FVector(9000,3400,1650);
    D.CameraAnchors[2].LocationCm = FVector(20350,3150,820);
    D.SpectatorBoundaries[0].LocationCm = FVector(13900,-3000,90); D.SpectatorBoundaries[0].ExtentCm = FVector(5400,35,90);
    D.SpectatorBoundaries[1].LocationCm = FVector(14500,3600,90); D.SpectatorBoundaries[1].ExtentCm = FVector(5100,35,90);
    D.SpectatorBoundaries[2].LocationCm = FVector(20900,1500,90); D.SpectatorBoundaries[2].ExtentCm = FVector(35,1450,90);
    D.WindZones[0].ZoneId = TEXT("NeedleTunnel"); D.WindZones[0].LocationCm = FVector(7600,300,750);
    D.WindZones[0].ExtentCm = FVector(5000,1200,1250); D.WindZones[0].BaseWindScale = 0.58f;
    D.WindZones[1].ZoneId = TEXT("LandingCrosswind"); D.WindZones[1].LocationCm = FVector(15500,1100,800);
    D.WindZones[1].ExtentCm = FVector(3800,2300,1300); D.WindZones[1].BaseWindScale = 1.28f;
    D.FlyoverPointsCm = {
        FVector(-900,-1100,520), FVector(1900,-500,780), FVector(4600,100,980),
        FVector(8500,650,1350), FVector(13200,1200,1450), FVector(17700,1900,1050), FVector(19800,1900,650)
    };
    return D;
}

FDiscGolfHoleBlockoutDefinition DiscGolfCourseDefinition::PineRidgeHole3Fallback()
{
    FDiscGolfHoleBlockoutDefinition D = PineRidgeHole1Fallback();
    D.HoleNumber = 3;
    D.HoleName = FText::FromString(TEXT("Gallery Lake"));
    D.Par = 4;
    D.TeeLocationCm = FVector(0,0,450);
    D.BasketLocationCm = FVector(21890,2900,-350);
    D.Surfaces[0].LocationCm = FVector(11000,1200,-455); D.Surfaces[0].Scale = FVector(245,90,1);
    D.Surfaces[1].SurfaceId = TEXT("OverlookFairway"); D.Surfaces[1].LocationCm = FVector(3400,250,300); D.Surfaces[1].Scale = FVector(67,18,0.1f);
    D.Surfaces[2].SurfaceId = TEXT("GalleryLandingFairway"); D.Surfaces[2].LocationCm = FVector(16900,2600,-285); D.Surfaces[2].Scale = FVector(78,22,0.12f);
    D.Surfaces[3].LocationCm = FVector(21890,2900,-375); D.Surfaces[3].Scale = FVector(27,27,0.5f);
    D.Surfaces[4].LocationCm = FVector(0,0,453); D.Surfaces[4].Scale = FVector(2.4f,1.1f,0.06f);
    D.Surfaces[5].LocationCm = FVector(4700,-1800,185); D.Surfaces[5].Scale = FVector(65,16,0.1f);
    D.Surfaces[6].LocationCm = FVector(18000,5400,-330); D.Surfaces[6].Scale = FVector(70,15,0.1f);
    D.Surfaces[7].SurfaceId = TEXT("LakeBeach"); D.Surfaces[7].LocationCm = FVector(14500,1650,-235); D.Surfaces[7].Scale = FVector(36,8,0.1f);
    D.Surfaces[8].SurfaceId = TEXT("LakeShoreRock"); D.Surfaces[8].LocationCm = FVector(13900,-1300,-210); D.Surfaces[8].Scale = FVector(28,7,0.14f);
    D.Surfaces[9].SurfaceId = TEXT("FinalGalleryOB"); D.Surfaces[9].LocationCm = FVector(20400,7600,-395); D.Surfaces[9].Scale = FVector(68,22,0.08f);
    D.Surfaces[10].SurfaceId = TEXT("GalleryLakeWater"); D.Surfaces[10].Shape = EDiscGolfPrimitiveShape::Cylinder;
    D.Surfaces[10].LocationCm = FVector(11200,700,-155); D.Surfaces[10].Scale = FVector(58,42,0.18f);
    FDiscGolfBlockoutSurfaceDefinition RightRoute = D.Surfaces[1];
    RightRoute.SurfaceId = TEXT("LakeRightRoute"); RightRoute.LocationCm = FVector(10200,3350,-40);
    RightRoute.Scale = FVector(72,14,0.12f); RightRoute.Rotation = FRotator(0,18,0); D.Surfaces.Add(RightRoute);
    const FVector Trees[] = {
        FVector(2500,-2200,290), FVector(3300,2350,260), FVector(5100,-2700,130), FVector(5900,2900,95),
        FVector(7600,-3400,-20), FVector(8200,5200,-60), FVector(11100,-4200,-170), FVector(12600,5600,-190),
        FVector(14600,-3000,-245), FVector(15300,5900,-270), FVector(17700,-1700,-315), FVector(18400,6200,-325),
        FVector(20700,600,-350), FVector(22400,5200,-350)
    };
    D.Trees.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Trees); ++Index)
    {
        FDiscGolfTreeDefinition Tree; Tree.LocationCm = Trees[Index]; Tree.HeightScale = 0.98f + 0.05f * (Index % 4); D.Trees.Add(Tree);
    }
    D.CollisionFixtures.Reset();
    auto Fixture = [&D](const TCHAR* Id, EDiscGolfFixtureType Type, EDiscGolfPrimitiveShape Shape,
        FVector Location, FVector Scale, float Yaw = 0.0f)
    {
        FDiscGolfCollisionFixtureDefinition F;
        F.FixtureId = Id; F.FixtureType = Type; F.Shape = Shape; F.LocationCm = Location; F.Scale = Scale;
        F.Rotation = FRotator(0.0f, Yaw, 0.0f); D.CollisionFixtures.Add(F);
    };
    Fixture(TEXT("ShoreReeds"), EDiscGolfFixtureType::DenseGrass, EDiscGolfPrimitiveShape::Box,
        FVector(12500,4300,-80), FVector(6.0f,2.8f,1.6f), -8.0f);
    Fixture(TEXT("ShoreBoulder"), EDiscGolfFixtureType::Rock, EDiscGolfPrimitiveShape::Sphere,
        FVector(14500,-1550,-158), FVector(1.25f,1.0f,0.9f));
    Fixture(TEXT("GallerySign"), EDiscGolfFixtureType::Sign, EDiscGolfPrimitiveShape::Box,
        FVector(1800,-1800,430), FVector(0.12f,2.2f,1.4f), 8.0f);
    D.LandingZones[0].ZoneId = TEXT("LakeCarryLanding"); D.LandingZones[0].Label = FText::FromString(TEXT("CLEAR THE WATER"));
    D.LandingZones[0].LocationCm = FVector(15600,2300,-245); D.LandingZones[0].ExtentCm = FVector(1900,850,28);
    D.LandingZones[1].ZoneId = TEXT("RightRouteLanding"); D.LandingZones[1].Label = FText::FromString(TEXT("SAFE RIGHT ROUTE"));
    D.LandingZones[1].LocationCm = FVector(12600,4100,-150); D.LandingZones[1].ExtentCm = FVector(1550,650,28);
    D.ShotRoutes.Reset();
    auto Route = [&D](const TCHAR* Id, const TCHAR* Label, EDiscGolfShotRouteType Type,
        const TCHAR* Intent, const TCHAR* LandingZone, int32 TargetStrokes, int32 Risk, int32 Reward,
        float WidthCm, std::initializer_list<FVector> Waypoints)
    {
        FDiscGolfShotRouteDefinition R;
        R.RouteId = Id; R.Label = FText::FromString(Label); R.RouteType = Type;
        R.ShotIntent = FText::FromString(Intent); R.LandingZoneId = LandingZone;
        R.TargetStrokes = TargetStrokes; R.RiskRating = Risk; R.RewardRating = Reward;
        R.CorridorWidthCm = WidthCm; R.WaypointsCm.Append(Waypoints); D.ShotRoutes.Add(R);
    };
    Route(TEXT("LakeCarry"), TEXT("LAKE-CARRY ATTACK"), EDiscGolfShotRouteType::Primary,
        TEXT("Commit over open water and land beyond the beach for the shortest birdie approach."),
        TEXT("LakeCarryLanding"), 3, 4, 5, 1400.0f,
        { FVector(0,0,470), FVector(6500,300,180), FVector(11200,900,-80),
          FVector(15600,2300,-220), FVector(21890,2900,-330) });
    Route(TEXT("DirectWaterAttack"), TEXT("DIRECT WATER ATTACK"), EDiscGolfShotRouteType::RiskReward,
        TEXT("Take the exposed center line over the widest water to maximize distance and eagle chances."),
        TEXT("LakeCarryLanding"), 3, 5, 5, 950.0f,
        { FVector(0,0,470), FVector(7600,500,110), FVector(12500,1200,-120),
          FVector(17200,2500,-270), FVector(21890,2900,-330) });
    Route(TEXT("RightShore"), TEXT("RIGHT-SHORE PLACEMENT"), EDiscGolfShotRouteType::Bailout,
        TEXT("Follow the dry right shelf, trade distance for safety, and approach from the gallery side."),
        TEXT("RightRouteLanding"), 4, 2, 3, 1500.0f,
        { FVector(0,0,470), FVector(6500,2400,130), FVector(12600,4100,-130),
          FVector(18000,4300,-280), FVector(21890,2900,-330) });
    D.CameraAnchors[0].LocationCm = FVector(-600,1050,860); D.CameraAnchors[0].FieldOfViewDeg = 68;
    D.CameraAnchors[1].LocationCm = FVector(10300,5700,1850);
    D.CameraAnchors[2].LocationCm = FVector(22900,5000,520);
    D.SpectatorBoundaries[0].LocationCm = FVector(16900,-2200,-210); D.SpectatorBoundaries[0].ExtentCm = FVector(5700,35,90);
    D.SpectatorBoundaries[1].LocationCm = FVector(17400,6500,-210); D.SpectatorBoundaries[1].ExtentCm = FVector(5400,35,90);
    D.SpectatorBoundaries[2].LocationCm = FVector(23300,2900,-260); D.SpectatorBoundaries[2].ExtentCm = FVector(35,1850,110);
    D.WindZones[0].ZoneId = TEXT("LakeExposure"); D.WindZones[0].LocationCm = FVector(10500,900,850);
    D.WindZones[0].ExtentCm = FVector(5700,3900,1700); D.WindZones[0].BaseWindScale = 1.42f;
    D.WindZones[1].ZoneId = TEXT("GalleryShelter"); D.WindZones[1].LocationCm = FVector(19800,2900,550);
    D.WindZones[1].ExtentCm = FVector(3200,2700,1300); D.WindZones[1].BaseWindScale = 0.68f;
    D.FlyoverPointsCm = {
        FVector(-1000,-1300,980), FVector(2400,-500,1250), FVector(6500,300,1500),
        FVector(10600,1300,1800), FVector(15000,2600,1250), FVector(19000,3600,850), FVector(22200,3500,260)
    };
    return D;
}

bool DiscGolfCourseDefinition::ParseManifestJson(
    const FString& Json,
    FDiscGolfCourseManifestDefinition& OutManifest,
    FString& OutError)
{
    if (!RejectDuplicateJsonKeys(Json, OutError)) return false;
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("invalid manifest JSON");
        return false;
    }
    if (!ValidateManifestJsonShape(Root, OutError)) return false;
    FString Schema;
    if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("disc_golf_course_manifest"))
    {
        OutError = TEXT("invalid course manifest schema identity");
        return false;
    }

    FDiscGolfCourseManifestDefinition Manifest;
    Manifest.SchemaVersion = Root->GetIntegerField(TEXT("schemaVersion"));
    Manifest.CourseId = FName(*Root->GetStringField(TEXT("courseId")));
    Manifest.LayoutId = FName(*Root->GetStringField(TEXT("layoutId")));
    Manifest.DisplayName = FText::FromString(Root->GetStringField(TEXT("displayName")));
    const TArray<TSharedPtr<FJsonValue>>* HoleValues = nullptr;
    if (Root->TryGetArrayField(TEXT("holes"), HoleValues))
    {
        for (const TSharedPtr<FJsonValue>& Value : *HoleValues)
        {
            const TSharedPtr<FJsonObject> HoleObject = Value->AsObject();
            if (!HoleObject.IsValid()) continue;
            FDiscGolfCourseManifestHoleEntry Entry;
            Entry.HoleNumber = HoleObject->GetIntegerField(TEXT("holeNumber"));
            Entry.DefinitionFile = HoleObject->GetStringField(TEXT("definitionFile"));
            Entry.WorldOriginCm = VectorFromJson(HoleObject, TEXT("worldOriginCm"), FVector::ZeroVector);
            Entry.WorldYawDeg = HoleObject->GetNumberField(TEXT("worldYawDeg"));
            Manifest.Holes.Add(Entry);
        }
    }
    if (!ValidateManifest(Manifest, OutError)) return false;
    OutManifest = MoveTemp(Manifest);
    return true;
}

bool DiscGolfCourseDefinition::ValidateManifest(
    const FDiscGolfCourseManifestDefinition& Manifest,
    FString& OutError)
{
    if (Manifest.SchemaVersion != 1) { OutError = TEXT("unsupported manifest schema version"); return false; }
    if (Manifest.CourseId.IsNone() || Manifest.LayoutId.IsNone() || Manifest.DisplayName.IsEmpty() || Manifest.Holes.IsEmpty())
    { OutError = TEXT("course manifest identity or holes are invalid"); return false; }
    TSet<int32> SeenHoles;
    for (int32 Index = 0; Index < Manifest.Holes.Num(); ++Index)
    {
        const FDiscGolfCourseManifestHoleEntry& Entry = Manifest.Holes[Index];
        const FString Normalized = Entry.DefinitionFile.Replace(TEXT("\\"), TEXT("/"));
        if (Entry.HoleNumber != Index + 1 || SeenHoles.Contains(Entry.HoleNumber)
            || !Normalized.StartsWith(TEXT("Data/")) || Normalized.Contains(TEXT(".."))
            || !Normalized.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase)
            || FPaths::IsRelative(Normalized) == false
            || !IsFiniteVector(Entry.WorldOriginCm) || !FMath::IsFinite(Entry.WorldYawDeg)
            || FMath::Abs(Entry.WorldYawDeg) > 180.0f)
        {
            OutError = TEXT("manifest holes must be unique, contiguous, spatially valid, and use safe Data JSON paths");
            return false;
        }
        for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
        {
            if (FVector::Dist2D(Entry.WorldOriginCm, Manifest.Holes[PreviousIndex].WorldOriginCm) < 5000.0f)
            {
                OutError = TEXT("persistent course hole origins must be spatially distinct");
                return false;
            }
        }
        SeenHoles.Add(Entry.HoleNumber);
    }
    OutError.Reset();
    return true;
}

bool DiscGolfCourseDefinition::ParseJson(
    const FString& Json,
    FDiscGolfHoleBlockoutDefinition& OutDefinition,
    FString& OutError)
{
    if (!RejectDuplicateJsonKeys(Json, OutError)) return false;
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("invalid JSON");
        return false;
    }
    if (!ValidateHoleJsonShape(Root, OutError)) return false;

    FString Schema;
    if (!Root->TryGetStringField(TEXT("schema"), Schema)
        || Schema != TEXT("disc_golf_hole_blockout"))
    {
        OutError = TEXT("invalid course schema identity");
        return false;
    }

    FDiscGolfHoleBlockoutDefinition D;
    D.SchemaVersion = Root->GetIntegerField(TEXT("schemaVersion"));
    D.CourseId = FName(*Root->GetStringField(TEXT("courseId")));
    D.LayoutId = FName(*Root->GetStringField(TEXT("layoutId")));
    D.HoleNumber = Root->GetIntegerField(TEXT("holeNumber"));
    D.HoleName = FText::FromString(Root->GetStringField(TEXT("holeName")));
    D.Par = Root->GetIntegerField(TEXT("par"));
    D.TeeLocationCm = VectorFromJson(Root, TEXT("teeLocationCm"), FVector::ZeroVector);
    D.BasketLocationCm = VectorFromJson(Root, TEXT("basketLocationCm"), FVector(11000,800,0));

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Root->TryGetArrayField(TEXT("surfaces"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfBlockoutSurfaceDefinition S; S.SurfaceId = FName(*O->GetStringField(TEXT("id")));
        S.SurfaceType = ParseSurface(O->GetStringField(TEXT("surface")));
        S.Shape = ParseShape(O->GetStringField(TEXT("shape")));
        S.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector);
        S.Scale = VectorFromJson(O, TEXT("scale"), FVector::OneVector); S.Rotation = RotationFromJson(O); D.Surfaces.Add(S);
    }
    if (Root->TryGetArrayField(TEXT("trees"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfTreeDefinition T; T.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector);
        T.HeightScale = O->GetNumberField(TEXT("heightScale")); D.Trees.Add(T);
    }
    if (Root->TryGetArrayField(TEXT("collisionFixtures"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfCollisionFixtureDefinition F;
        F.FixtureId = FName(*O->GetStringField(TEXT("id")));
        F.FixtureType = ParseFixtureType(O->GetStringField(TEXT("type")));
        F.Shape = ParseShape(O->GetStringField(TEXT("shape")));
        F.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector);
        F.Scale = VectorFromJson(O, TEXT("scale"), FVector::OneVector);
        F.Rotation = RotationFromJson(O);
        D.CollisionFixtures.Add(F);
    }
    if (Root->TryGetArrayField(TEXT("landingZones"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfLandingZoneDefinition Z; Z.ZoneId = FName(*O->GetStringField(TEXT("id")));
        Z.Label = FText::FromString(O->GetStringField(TEXT("label")));
        Z.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector);
        Z.ExtentCm = VectorFromJson(O, TEXT("extentCm"), FVector(1200,700,40)); Z.Rotation = RotationFromJson(O); D.LandingZones.Add(Z);
    }
    if (Root->TryGetArrayField(TEXT("shotRoutes"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfShotRouteDefinition R;
        R.RouteId = FName(*O->GetStringField(TEXT("id")));
        R.Label = FText::FromString(O->GetStringField(TEXT("label")));
        R.RouteType = ParseShotRouteType(O->GetStringField(TEXT("type")));
        R.ShotIntent = FText::FromString(O->GetStringField(TEXT("shotIntent")));
        R.LandingZoneId = FName(*O->GetStringField(TEXT("landingZoneId")));
        R.TargetStrokes = O->GetIntegerField(TEXT("targetStrokes"));
        R.RiskRating = O->GetIntegerField(TEXT("riskRating"));
        R.RewardRating = O->GetIntegerField(TEXT("rewardRating"));
        R.CorridorWidthCm = O->GetNumberField(TEXT("corridorWidthCm"));
        const TArray<TSharedPtr<FJsonValue>>* RoutePoints = nullptr;
        if (O->TryGetArrayField(TEXT("waypointsCm"), RoutePoints))
        {
            for (const TSharedPtr<FJsonValue>& PointValue : *RoutePoints)
            {
                const TSharedPtr<FJsonObject> Point = PointValue->AsObject();
                if (!Point.IsValid()) continue;
                R.WaypointsCm.Add(FVector(Point->GetNumberField(TEXT("x")),
                    Point->GetNumberField(TEXT("y")), Point->GetNumberField(TEXT("z"))));
            }
        }
        D.ShotRoutes.Add(R);
    }
    if (Root->TryGetArrayField(TEXT("cameraAnchors"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfCameraAnchorDefinition A; A.AnchorId = FName(*O->GetStringField(TEXT("id")));
        A.Mode = ParseCameraMode(O->GetStringField(TEXT("mode")));
        A.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector);
        A.FieldOfViewDeg = O->GetNumberField(TEXT("fieldOfViewDeg")); D.CameraAnchors.Add(A);
    }
    if (Root->TryGetArrayField(TEXT("spectatorBoundaries"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfSpectatorBoundaryDefinition B; B.BoundaryId = FName(*O->GetStringField(TEXT("id")));
        B.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector);
        B.ExtentCm = VectorFromJson(O, TEXT("extentCm"), FVector(1000,40,80)); B.Rotation = RotationFromJson(O); D.SpectatorBoundaries.Add(B);
    }
    if (Root->TryGetArrayField(TEXT("windZones"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        FDiscGolfWindZoneDefinition W; W.ZoneId = FName(*O->GetStringField(TEXT("id")));
        W.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector);
        W.ExtentCm = VectorFromJson(O, TEXT("extentCm"), FVector(1500)); W.BaseWindScale = O->GetNumberField(TEXT("baseWindScale"));
        W.AdditiveWindMps = VectorFromJson(O, TEXT("additiveWindMps"), FVector::ZeroVector); D.WindZones.Add(W);
    }
    if (Root->TryGetArrayField(TEXT("flyoverPointsCm"), Values)) for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> O = Value->AsObject(); if (!O.IsValid()) continue;
        D.FlyoverPointsCm.Add(FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))));
    }

    if (!Validate(D, OutError)) return false;
    OutDefinition = MoveTemp(D);
    return true;
}

bool DiscGolfCourseDefinition::Validate(const FDiscGolfHoleBlockoutDefinition& D, FString& OutError)
{
    if (D.SchemaVersion != 1) { OutError = TEXT("unsupported schema version"); return false; }
    if (D.CourseId.IsNone() || D.LayoutId.IsNone() || D.HoleNumber <= 0 || D.Par <= 0 || D.HoleName.IsEmpty())
    { OutError = TEXT("course identity, hole metadata, or par is invalid"); return false; }
    if (!IsFiniteVector(D.TeeLocationCm) || !IsFiniteVector(D.BasketLocationCm) || FVector::Dist2D(D.TeeLocationCm,D.BasketLocationCm) < 3000.0f)
    { OutError = TEXT("tee/basket geometry is invalid"); return false; }
    if (D.Surfaces.Num() < 8 || D.Trees.Num() < 8 || D.CollisionFixtures.Num() < 3
        || D.LandingZones.Num() < 2 || D.ShotRoutes.Num() < 2 || D.CameraAnchors.Num() < 3
        || D.SpectatorBoundaries.Num() < 3 || D.WindZones.Num() < 2 || D.FlyoverPointsCm.Num() < 4)
    { OutError = TEXT("required authored feature coverage is incomplete"); return false; }
    if (HasDuplicateIds<FDiscGolfBlockoutSurfaceDefinition>(D.Surfaces, [](const auto& V){return V.SurfaceId;})
        || HasDuplicateIds<FDiscGolfCollisionFixtureDefinition>(D.CollisionFixtures, [](const auto& V){return V.FixtureId;})
        || HasDuplicateIds<FDiscGolfLandingZoneDefinition>(D.LandingZones, [](const auto& V){return V.ZoneId;})
        || HasDuplicateIds<FDiscGolfShotRouteDefinition>(D.ShotRoutes, [](const auto& V){return V.RouteId;})
        || HasDuplicateIds<FDiscGolfCameraAnchorDefinition>(D.CameraAnchors, [](const auto& V){return V.AnchorId;})
        || HasDuplicateIds<FDiscGolfSpectatorBoundaryDefinition>(D.SpectatorBoundaries, [](const auto& V){return V.BoundaryId;})
        || HasDuplicateIds<FDiscGolfWindZoneDefinition>(D.WindZones, [](const auto& V){return V.ZoneId;}))
    { OutError = TEXT("empty or duplicate authored feature id"); return false; }

    TSet<ECourseSurfaceType> SurfaceTypes;
    for (const FDiscGolfBlockoutSurfaceDefinition& S : D.Surfaces)
    {
        if (!IsFiniteVector(S.LocationCm) || !IsFiniteVector(S.Scale) || S.Scale.GetMin() <= 0.0f)
        { OutError = TEXT("surface transform is invalid"); return false; }
        SurfaceTypes.Add(S.SurfaceType);
    }
    for (ECourseSurfaceType Required : { ECourseSurfaceType::Fairway, ECourseSurfaceType::TeePad,
        ECourseSurfaceType::LightRough, ECourseSurfaceType::DeepRough,
        ECourseSurfaceType::OutOfBounds, ECourseSurfaceType::Hazard })
    {
        if (!SurfaceTypes.Contains(Required)) { OutError = TEXT("required surface coverage is incomplete"); return false; }
    }
    for (const FDiscGolfTreeDefinition& Tree : D.Trees)
    {
        if (!IsFiniteVector(Tree.LocationCm) || !FMath::IsFinite(Tree.HeightScale)
            || Tree.HeightScale < 0.25f || Tree.HeightScale > 4.0f)
        { OutError = TEXT("tree definition is invalid"); return false; }
    }
    TSet<EDiscGolfFixtureType> FixtureTypes;
    for (const FDiscGolfCollisionFixtureDefinition& Fixture : D.CollisionFixtures)
    {
        if (Fixture.FixtureType == EDiscGolfFixtureType::Unknown
            || !IsFiniteVector(Fixture.LocationCm) || !IsFiniteVector(Fixture.Scale)
            || Fixture.Rotation.ContainsNaN()
            || Fixture.Scale.GetMin() <= 0.0f || Fixture.Scale.GetMax() > 20.0f)
        { OutError = TEXT("collision fixture definition is invalid"); return false; }
        FixtureTypes.Add(Fixture.FixtureType);
    }
    for (EDiscGolfFixtureType Required : { EDiscGolfFixtureType::DenseGrass,
        EDiscGolfFixtureType::Rock, EDiscGolfFixtureType::Sign })
    {
        if (!FixtureTypes.Contains(Required))
        { OutError = TEXT("dense-grass, rock, and sign fixture coverage is required"); return false; }
    }
    for (const FDiscGolfLandingZoneDefinition& Zone : D.LandingZones)
    {
        if (Zone.Label.IsEmpty() || !IsFiniteVector(Zone.LocationCm) || !IsFiniteVector(Zone.ExtentCm)
            || Zone.ExtentCm.GetMin() <= 0.0f)
        { OutError = TEXT("landing zone is invalid"); return false; }
    }
    TSet<FName> LandingZoneIds;
    for (const FDiscGolfLandingZoneDefinition& Zone : D.LandingZones) LandingZoneIds.Add(Zone.ZoneId);
    TSet<EDiscGolfShotRouteType> RouteTypes;
    for (const FDiscGolfShotRouteDefinition& Route : D.ShotRoutes)
    {
        if (Route.Label.IsEmpty() || Route.ShotIntent.IsEmpty()
            || !LandingZoneIds.Contains(Route.LandingZoneId)
            || Route.TargetStrokes < 1 || Route.TargetStrokes > D.Par + 1
            || Route.RiskRating < 1 || Route.RiskRating > 5
            || Route.RewardRating < 1 || Route.RewardRating > 5
            || !FMath::IsFinite(Route.CorridorWidthCm)
            || Route.CorridorWidthCm < 500.0f || Route.CorridorWidthCm > 4000.0f
            || Route.WaypointsCm.Num() < 3)
        { OutError = TEXT("shot route strategy metadata is invalid"); return false; }
        float RouteLengthCm = 0.0f;
        for (int32 PointIndex = 0; PointIndex < Route.WaypointsCm.Num(); ++PointIndex)
        {
            if (!IsFiniteVector(Route.WaypointsCm[PointIndex]))
            { OutError = TEXT("shot route waypoint is invalid"); return false; }
            if (PointIndex > 0) RouteLengthCm += FVector::Dist(
                Route.WaypointsCm[PointIndex - 1], Route.WaypointsCm[PointIndex]);
        }
        const float DirectLengthCm = FVector::Dist(D.TeeLocationCm, D.BasketLocationCm);
        if (FVector::Dist(Route.WaypointsCm[0], D.TeeLocationCm) > 500.0f
            || FVector::Dist(Route.WaypointsCm.Last(), D.BasketLocationCm) > 500.0f
            || RouteLengthCm < DirectLengthCm * 0.95f || RouteLengthCm > DirectLengthCm * 1.8f)
        { OutError = TEXT("shot route must connect tee to basket with a plausible corridor"); return false; }
        RouteTypes.Add(Route.RouteType);
    }
    if (!RouteTypes.Contains(EDiscGolfShotRouteType::Primary)
        || !RouteTypes.Contains(EDiscGolfShotRouteType::Bailout))
    { OutError = TEXT("primary and bailout shot-route coverage is required"); return false; }
    TSet<EDiscGolfCameraAnchorMode> Modes;
    for (const FDiscGolfCameraAnchorDefinition& A : D.CameraAnchors)
    {
        if (!IsFiniteVector(A.LocationCm) || A.FieldOfViewDeg < 20.0f || A.FieldOfViewDeg > 100.0f)
        { OutError = TEXT("camera anchor is invalid"); return false; }
        Modes.Add(A.Mode);
    }
    if (Modes.Num() != 3) { OutError = TEXT("launch/fairway/finish camera coverage is required"); return false; }
    for (const FDiscGolfSpectatorBoundaryDefinition& Boundary : D.SpectatorBoundaries)
    {
        if (!IsFiniteVector(Boundary.LocationCm) || !IsFiniteVector(Boundary.ExtentCm)
            || Boundary.ExtentCm.GetMin() <= 0.0f)
        { OutError = TEXT("spectator boundary is invalid"); return false; }
    }
    for (const FDiscGolfWindZoneDefinition& Zone : D.WindZones)
    {
        if (!IsFiniteVector(Zone.LocationCm) || !IsFiniteVector(Zone.ExtentCm)
            || !IsFiniteVector(Zone.AdditiveWindMps) || Zone.ExtentCm.GetMin() <= 0.0f
            || !FMath::IsFinite(Zone.BaseWindScale) || Zone.BaseWindScale < 0.0f || Zone.BaseWindScale > 3.0f)
        { OutError = TEXT("wind zone is invalid"); return false; }
    }
    for (const FVector& Point : D.FlyoverPointsCm) if (!IsFiniteVector(Point))
    { OutError = TEXT("flyover point is invalid"); return false; }
    if (FVector::Dist(D.FlyoverPointsCm[0], D.TeeLocationCm) > 2500.0f
        || FVector::Dist(D.FlyoverPointsCm.Last(), D.BasketLocationCm) > 2500.0f)
    { OutError = TEXT("flyover endpoints must cover tee and basket"); return false; }
    OutError.Reset(); return true;
}

bool DiscGolfCourseDefinition::ValidateCourseWindZoneIdentities(
    const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
    FString& OutError)
{
    TMap<FName, int32> OwningHoleByZoneId;
    for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
    {
        for (const FDiscGolfWindZoneDefinition& Zone : Definition.WindZones)
        {
            // Per-hole validation owns missing identities. This course-wide
            // pass only prevents two otherwise valid holes from publishing the
            // same actor identity into one persistent world.
            if (Zone.ZoneId.IsNone()) continue;

            if (const int32* OwningHole = OwningHoleByZoneId.Find(Zone.ZoneId))
            {
                if (*OwningHole != Definition.HoleNumber)
                {
                    OutError = FString::Printf(
                        TEXT("wind zone id %s is duplicated across holes %d and %d"),
                        *Zone.ZoneId.ToString(), *OwningHole, Definition.HoleNumber);
                    return false;
                }
            }
            else
            {
                OwningHoleByZoneId.Add(Zone.ZoneId, Definition.HoleNumber);
            }
        }
    }

    OutError.Reset();
    return true;
}

bool DiscGolfCourseDefinition::PlaceHoleInCourse(
    const FDiscGolfHoleBlockoutDefinition& LocalDefinition,
    const FDiscGolfCourseManifestHoleEntry& ManifestEntry,
    FDiscGolfHoleBlockoutDefinition& OutWorldDefinition,
    FString& OutError)
{
    if (LocalDefinition.HoleNumber != ManifestEntry.HoleNumber
        || !IsFiniteVector(ManifestEntry.WorldOriginCm)
        || !FMath::IsFinite(ManifestEntry.WorldYawDeg))
    {
        OutError = TEXT("hole placement identity or transform is invalid");
        return false;
    }
    FDiscGolfHoleBlockoutDefinition World = LocalDefinition;
    const FTransform Placement(FRotator(0.0f, ManifestEntry.WorldYawDeg, 0.0f),
        ManifestEntry.WorldOriginCm);
    const auto PlacePosition = [&Placement](FVector& Position)
    {
        Position = Placement.TransformPosition(Position);
    };
    const auto PlaceRotation = [&ManifestEntry](FRotator& Rotation)
    {
        Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + ManifestEntry.WorldYawDeg);
    };

    PlacePosition(World.TeeLocationCm);
    PlacePosition(World.BasketLocationCm);
    for (FDiscGolfBlockoutSurfaceDefinition& Surface : World.Surfaces)
    {
        PlacePosition(Surface.LocationCm);
        PlaceRotation(Surface.Rotation);
    }
    for (FDiscGolfTreeDefinition& Tree : World.Trees) PlacePosition(Tree.LocationCm);
    for (FDiscGolfCollisionFixtureDefinition& Fixture : World.CollisionFixtures)
    {
        PlacePosition(Fixture.LocationCm);
        PlaceRotation(Fixture.Rotation);
    }
    for (FDiscGolfLandingZoneDefinition& Zone : World.LandingZones)
    {
        PlacePosition(Zone.LocationCm);
        PlaceRotation(Zone.Rotation);
    }
    for (FDiscGolfShotRouteDefinition& Route : World.ShotRoutes)
    {
        for (FVector& Waypoint : Route.WaypointsCm) PlacePosition(Waypoint);
    }
    for (FDiscGolfCameraAnchorDefinition& Camera : World.CameraAnchors)
    {
        PlacePosition(Camera.LocationCm);
    }
    for (FDiscGolfSpectatorBoundaryDefinition& Boundary : World.SpectatorBoundaries)
    {
        PlacePosition(Boundary.LocationCm);
        PlaceRotation(Boundary.Rotation);
    }
    for (FDiscGolfWindZoneDefinition& Zone : World.WindZones) PlacePosition(Zone.LocationCm);
    for (FVector& Point : World.FlyoverPointsCm) PlacePosition(Point);

    if (!Validate(World, OutError)) return false;
    OutWorldDefinition = MoveTemp(World);
    OutError.Reset();
    return true;
}

namespace
{
bool ResolveManifestHoleForCourseIdentityValidation(
    const FDiscGolfCourseManifestDefinition& Manifest,
    const FDiscGolfCourseManifestHoleEntry& Entry,
    FDiscGolfHoleBlockoutDefinition& OutDefinition,
    FString& OutError)
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), Entry.DefinitionFile);
    FString Json;
    FString ParseError;
    FDiscGolfHoleBlockoutDefinition ParsedDefinition;
    const bool bFileLoaded = FFileHelper::LoadFileToString(Json, *Path);
    const bool bParsed = bFileLoaded
        && DiscGolfCourseDefinition::ParseJson(Json, ParsedDefinition, ParseError);
    const bool bAuthoredDataValid = bParsed
        && ParsedDefinition.CourseId == Manifest.CourseId
        && ParsedDefinition.LayoutId == Manifest.LayoutId
        && ParsedDefinition.HoleNumber == Entry.HoleNumber;
    const EDiscGolfAuthoredCourseDataState DataState = !bFileLoaded
        ? EDiscGolfAuthoredCourseDataState::Missing
        : (bAuthoredDataValid
            ? EDiscGolfAuthoredCourseDataState::Valid
            : EDiscGolfAuthoredCourseDataState::Invalid);
    const EDiscGolfAuthoredCourseLoadAction LoadAction =
        DiscGolfCourseDefinition::ResolveAuthoredCourseLoadAction(
            DataState, UE_BUILD_SHIPPING != 0);

    if (LoadAction == EDiscGolfAuthoredCourseLoadAction::UseAuthoredData)
    {
        OutDefinition = MoveTemp(ParsedDefinition);
        return true;
    }
    if (LoadAction == EDiscGolfAuthoredCourseLoadAction::FailClosed)
    {
        if (ParseError.IsEmpty() && bFileLoaded)
        {
            ParseError = FString::Printf(
                TEXT("authored hole identity does not match manifest hole %d"),
                Entry.HoleNumber);
        }
        OutError = DescribeAuthoredDataFailure(
            TEXT("hole"), Path, DataState, ParseError);
        return false;
    }

    switch (Entry.HoleNumber)
    {
        case 1: OutDefinition = DiscGolfCourseDefinition::PineRidgeHole1Fallback(); break;
        case 2: OutDefinition = DiscGolfCourseDefinition::PineRidgeHole2Fallback(); break;
        case 3: OutDefinition = DiscGolfCourseDefinition::PineRidgeHole3Fallback(); break;
        default:
            OutError = FString::Printf(
                TEXT("no source fallback exists for hole %d"), Entry.HoleNumber);
            return false;
    }
    return DiscGolfCourseDefinition::Validate(OutDefinition, OutError);
}

bool ValidateManifestCourseWindZoneIdentities(
    const FDiscGolfCourseManifestDefinition& Manifest,
    FString& OutError)
{
    TArray<FDiscGolfHoleBlockoutDefinition> Definitions;
    Definitions.Reserve(Manifest.Holes.Num());
    for (const FDiscGolfCourseManifestHoleEntry& Entry : Manifest.Holes)
    {
        FDiscGolfHoleBlockoutDefinition Definition;
        if (!ResolveManifestHoleForCourseIdentityValidation(
            Manifest, Entry, Definition, OutError))
        {
            return false;
        }
        Definitions.Add(MoveTemp(Definition));
    }
    return DiscGolfCourseDefinition::ValidateCourseWindZoneIdentities(
        Definitions, OutError);
}
}

bool DiscGolfCourseDefinition::LoadPineRidgeCourseManifest(
    FDiscGolfCourseManifestDefinition& OutManifest,
    FString& OutSource,
    FString& OutError)
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/PineRidgeCourse.json"));
    FString Json;
    FString ParseError;
    FDiscGolfCourseManifestDefinition ParsedManifest;
    const bool bFileLoaded = FFileHelper::LoadFileToString(Json, *Path);
    const bool bParsed = bFileLoaded && ParseManifestJson(Json, ParsedManifest, ParseError);
    const bool bAuthoredDataValid = bParsed
        && ParsedManifest.CourseId == TEXT("PineRidgeChampionship")
        && ParsedManifest.LayoutId == TEXT("Championship")
        && ParsedManifest.Holes.Num() == 3;
    const EDiscGolfAuthoredCourseDataState DataState = !bFileLoaded
        ? EDiscGolfAuthoredCourseDataState::Missing
        : (bAuthoredDataValid
            ? EDiscGolfAuthoredCourseDataState::Valid
            : EDiscGolfAuthoredCourseDataState::Invalid);
    const EDiscGolfAuthoredCourseLoadAction LoadAction = ResolveAuthoredCourseLoadAction(
        DataState,
        UE_BUILD_SHIPPING != 0);
    if (LoadAction == EDiscGolfAuthoredCourseLoadAction::UseAuthoredData)
    {
        OutManifest = MoveTemp(ParsedManifest);
        if (!ValidateManifestCourseWindZoneIdentities(OutManifest, OutError))
        {
            OutManifest = FDiscGolfCourseManifestDefinition();
            OutSource.Reset();
            return false;
        }
        OutSource = TEXT("AUTHORED JSON"); OutError.Reset(); return true;
    }
    if (LoadAction == EDiscGolfAuthoredCourseLoadAction::FailClosed)
    {
        if (ParseError.IsEmpty() && bFileLoaded)
        {
            ParseError = TEXT("authored manifest identity or hole coverage does not match Pine Ridge Championship");
        }
        OutManifest = FDiscGolfCourseManifestDefinition();
        OutSource.Reset();
        OutError = DescribeAuthoredDataFailure(TEXT("manifest"), Path, DataState, ParseError);
        return false;
    }
    OutManifest = PineRidgeCourseFallback();
    if (!ValidateManifest(OutManifest, OutError)) return false;
    if (!ValidateManifestCourseWindZoneIdentities(OutManifest, OutError))
    {
        OutManifest = FDiscGolfCourseManifestDefinition();
        OutSource.Reset();
        return false;
    }
    OutSource = TEXT("SOURCE FALLBACK");
    OutError = ParseError.IsEmpty()
        ? (bFileLoaded
            ? TEXT("authored manifest identity or hole coverage is invalid; using source fallback")
            : TEXT("authored manifest missing; using source fallback"))
        : ParseError;
    return true;
}

bool DiscGolfCourseDefinition::LoadPineRidgeHole(
    int32 HoleNumber,
    FDiscGolfHoleBlockoutDefinition& OutDefinition,
    FString& OutSource,
    FString& OutError)
{
    FDiscGolfCourseManifestDefinition Manifest;
    FString ManifestSource;
    FString ManifestError;
    if (!LoadPineRidgeCourseManifest(Manifest, ManifestSource, ManifestError))
    {
        OutDefinition = FDiscGolfHoleBlockoutDefinition();
        OutSource.Reset();
        OutError = ManifestError;
        return false;
    }
    const FDiscGolfCourseManifestHoleEntry* Entry = Manifest.Holes.FindByPredicate(
        [HoleNumber](const FDiscGolfCourseManifestHoleEntry& Candidate)
        {
            return Candidate.HoleNumber == HoleNumber;
        });
    if (!Entry)
    {
        OutError = FString::Printf(TEXT("hole %d is not present in the course manifest"), HoleNumber);
        return false;
    }

    const FString Path = FPaths::Combine(FPaths::ProjectDir(), Entry->DefinitionFile);
    FString Json;
    FString ParseError;
    const bool bFileLoaded = FFileHelper::LoadFileToString(Json, *Path);
    const bool bParsed = bFileLoaded && ParseJson(Json, OutDefinition, ParseError);
    const bool bAuthoredDataValid = bParsed
        && OutDefinition.CourseId == Manifest.CourseId
        && OutDefinition.LayoutId == Manifest.LayoutId
        && OutDefinition.HoleNumber == HoleNumber;
    const EDiscGolfAuthoredCourseDataState DataState = !bFileLoaded
        ? EDiscGolfAuthoredCourseDataState::Missing
        : (bAuthoredDataValid
            ? EDiscGolfAuthoredCourseDataState::Valid
            : EDiscGolfAuthoredCourseDataState::Invalid);
    const EDiscGolfAuthoredCourseLoadAction LoadAction = ResolveAuthoredCourseLoadAction(
        DataState,
        UE_BUILD_SHIPPING != 0);
    if (LoadAction == EDiscGolfAuthoredCourseLoadAction::UseAuthoredData)
    {
        OutSource = TEXT("AUTHORED JSON");
        OutError.Reset();
        return true;
    }
    if (LoadAction == EDiscGolfAuthoredCourseLoadAction::FailClosed)
    {
        if (ParseError.IsEmpty() && bFileLoaded)
        {
            ParseError = FString::Printf(
                TEXT("authored hole identity does not match Pine Ridge Championship hole %d"),
                HoleNumber);
        }
        OutDefinition = FDiscGolfHoleBlockoutDefinition();
        OutSource.Reset();
        OutError = DescribeAuthoredDataFailure(TEXT("hole"), Path, DataState, ParseError);
        return false;
    }

    switch (HoleNumber)
    {
        case 1: OutDefinition = PineRidgeHole1Fallback(); break;
        case 2: OutDefinition = PineRidgeHole2Fallback(); break;
        case 3: OutDefinition = PineRidgeHole3Fallback(); break;
        default: OutError = FString::Printf(TEXT("no source fallback exists for hole %d"), HoleNumber); return false;
    }
    if (!Validate(OutDefinition, OutError)) return false;
    OutSource = TEXT("SOURCE FALLBACK");
    OutError = ParseError.IsEmpty()
        ? (bFileLoaded
            ? TEXT("authored hole identity is invalid; using source fallback")
            : TEXT("authored hole JSON missing; using source fallback"))
        : ParseError;
    return true;
}

bool DiscGolfCourseDefinition::LoadPineRidgeHole1(
    FDiscGolfHoleBlockoutDefinition& OutDefinition,
    FString& OutSource,
    FString& OutError)
{
    return LoadPineRidgeHole(1, OutDefinition, OutSource, OutError);
}

float DiscGolfCourseDefinition::MeasuredDistanceFeet(const FDiscGolfHoleBlockoutDefinition& D)
{
    return FVector::Dist(D.TeeLocationCm, D.BasketLocationCm) / 30.48f;
}
