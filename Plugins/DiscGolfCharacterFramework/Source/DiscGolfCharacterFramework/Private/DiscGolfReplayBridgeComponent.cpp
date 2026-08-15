#include "DiscGolfReplayBridgeComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDiscGolfReplayBridgeComponent::UDiscGolfReplayBridgeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UDiscGolfReplayBridgeComponent::StartEngineReplayRecording(
    FString ReplayName,
    FString FriendlyName,
    const TArray<FString>& AdditionalOptions)
{
    if (!GetWorld() || !GetWorld()->GetGameInstance())
    {
        return false;
    }

    UGameInstance* GameInstance = GetWorld()->GetGameInstance();
    GameInstance->StartRecordingReplay(
        ReplayName,
        FriendlyName,
        AdditionalOptions,
        nullptr
    );

    OnReplayStatusChanged.Broadcast(EDGReplayMode::EngineReplay, true);
    return true;
}

bool UDiscGolfReplayBridgeComponent::StopEngineReplayRecording()
{
    if (!GetWorld() || !GetWorld()->GetGameInstance())
    {
        return false;
    }

    GetWorld()->GetGameInstance()->StopRecordingReplay();
    OnReplayStatusChanged.Broadcast(EDGReplayMode::EngineReplay, false);
    return true;
}

bool UDiscGolfReplayBridgeComponent::PlayEngineReplay(
    FString ReplayName,
    const TArray<FString>& AdditionalOptions)
{
    if (!GetWorld() || !GetWorld()->GetGameInstance())
    {
        return false;
    }

    const bool bStarted = GetWorld()->GetGameInstance()->PlayReplay(
        ReplayName,
        GetWorld(),
        AdditionalOptions
    );

    OnReplayStatusChanged.Broadcast(EDGReplayMode::EngineReplay, bStarted);
    return bStarted;
}
