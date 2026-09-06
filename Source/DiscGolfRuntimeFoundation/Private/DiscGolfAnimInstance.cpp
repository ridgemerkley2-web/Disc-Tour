#include "DiscGolfAnimInstance.h"

#include "DiscGolfCharacterProfile.h"
#include "DiscGolfThrowComponent.h"
#include "GameFramework/Actor.h"

void UDiscGolfAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    AActor* Owner = GetOwningActor();
    const UDiscGolfThrowComponent* ThrowComponent = Owner
        ? Owner->FindComponentByClass<UDiscGolfThrowComponent>()
        : nullptr;
    if (!ThrowComponent)
    {
        ThrowIntent = FDGThrowIntent();
        ThrowPhase = EDGThrowPhase::Idle;
        bThrowActive = false;
        return;
    }

    ThrowIntent = ThrowComponent->CurrentIntent;
    ThrowPhase = ThrowComponent->CurrentPhase;
    bThrowActive = ThrowComponent->bThrowActive;

    if (ThrowComponent->CharacterProfile)
    {
        BodyProfile = ThrowComponent->CharacterProfile->Body;
        ThrowStyle = ThrowComponent->CharacterProfile->ThrowStyle;
        Handedness = ThrowComponent->CharacterProfile->Handedness;
    }
}
