#include "DiscGolfCourseSurfaceActor.h"

#include "DiscGolfCourseRules.h"

ADiscGolfCourseSurfaceActor::ADiscGolfCourseSurfaceActor()
{
    PrimaryActorTick.bCanEverTick = false;
    GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
}

void ADiscGolfCourseSurfaceActor::SetCourseSurfaceType(ECourseSurfaceType InSurfaceType)
{
    SurfaceType = InSurfaceType;
    Tags.RemoveAll([](const FName& Tag)
    {
        return Tag.ToString().StartsWith(TEXT("Surface.")) || Tag.ToString().StartsWith(TEXT("Zone."));
    });

    FName Tag = DiscGolfCourseRules::TagSurfaceFairway;
    switch (SurfaceType)
    {
        case ECourseSurfaceType::TeePad: Tag = DiscGolfCourseRules::TagSurfaceTeePad; break;
        case ECourseSurfaceType::LightRough: Tag = DiscGolfCourseRules::TagSurfaceLightRough; break;
        case ECourseSurfaceType::DeepRough: Tag = DiscGolfCourseRules::TagSurfaceDeepRough; break;
        case ECourseSurfaceType::Dirt: Tag = DiscGolfCourseRules::TagSurfaceDirt; break;
        case ECourseSurfaceType::Rock: Tag = DiscGolfCourseRules::TagSurfaceRock; break;
        case ECourseSurfaceType::OutOfBounds: Tag = DiscGolfCourseRules::TagZoneOutOfBounds; break;
        case ECourseSurfaceType::Hazard: Tag = DiscGolfCourseRules::TagZoneHazard; break;
        case ECourseSurfaceType::Fairway:
        default: break;
    }
    Tags.AddUnique(Tag);
}
