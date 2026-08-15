#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DiscGolfCharacterTypes.h"
#include "AnimNotify_ThrowPhase.generated.h"

UCLASS(meta=(DisplayName="Disc Golf - Throw Phase"))
class DISCGOLFCHARACTERFRAMEWORK_API UAnimNotify_ThrowPhase : public UAnimNotify
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf")
    EDGThrowPhase Phase = EDGThrowPhase::RunUp;

    virtual void Notify(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference
    ) override;

#if WITH_EDITOR
    virtual FString GetNotifyName_Implementation() const override;
#endif
};
