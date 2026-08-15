#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "DiscGolfTypes.h"
#include "DiscGolfWorldFixtureActor.generated.h"

class UStaticMesh;
class UBoxComponent;
struct FDiscGolfCollisionFixtureDefinition;

/** Runtime collision proxy for a discrete course obstacle. Visual quality never changes this actor. */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfWorldFixtureActor : public AStaticMeshActor
{
    GENERATED_BODY()

public:
    ADiscGolfWorldFixtureActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Course Fixture") TObjectPtr<UBoxComponent> OverlapVolume;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Fixture") FName FixtureId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Fixture") EDiscGolfFixtureType FixtureType = EDiscGolfFixtureType::Unknown;

    void Configure(const FDiscGolfCollisionFixtureDefinition& Definition, UStaticMesh* Mesh);
    void ConfigureTree(FName InFixtureId, const FVector& LocationCm, float HeightScale, UStaticMesh* Mesh);

    UFUNCTION(BlueprintPure, Category="Course Fixture") bool IsOverlapFixture() const;
    UFUNCTION(BlueprintPure, Category="Course Fixture") bool HasValidCollisionContract() const;
};
