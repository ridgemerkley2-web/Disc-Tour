#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ThrowFinished.generated.h"

UCLASS(meta=(DisplayName="Disc Golf - Throw Finished"))
class DISCGOLFCHARACTERFRAMEWORK_API UAnimNotify_ThrowFinished : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference
    ) override;

#if WITH_EDITOR
    virtual FString GetNotifyName_Implementation() const override { return TEXT("DG Throw Finished"); }
#endif
};
