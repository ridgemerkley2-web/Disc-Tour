#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ThrowFinished.generated.h"

UCLASS()
class DISCGOLFRUNTIMEFOUNDATION_API UAnimNotify_ThrowFinished : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
    virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
};
