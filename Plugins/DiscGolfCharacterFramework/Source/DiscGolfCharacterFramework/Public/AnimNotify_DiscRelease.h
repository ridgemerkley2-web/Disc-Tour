#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_DiscRelease.generated.h"

UCLASS(meta=(DisplayName="Disc Golf - Release Disc"))
class DISCGOLFCHARACTERFRAMEWORK_API UAnimNotify_DiscRelease : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference
    ) override;

#if WITH_EDITOR
    virtual FString GetNotifyName_Implementation() const override { return TEXT("DG Release Disc"); }
#endif
};
