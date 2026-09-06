#include "DiscGolfCameraViewContract.h"

namespace
{
uint32 NextGeneration(uint32 Current)
{
    return Current == MAX_uint32 ? 1u : Current + 1u;
}
}

EDiscGolfCameraViewOwner DiscGolfCameraViewContract::RequiredOwner(
    EDiscGolfCameraViewMode Mode)
{
    switch (Mode)
    {
        case EDiscGolfCameraViewMode::Aim:
        case EDiscGolfCameraViewMode::Tee:
            return EDiscGolfCameraViewOwner::PlayerSetup;
        case EDiscGolfCameraViewMode::FollowDisc:
        case EDiscGolfCameraViewMode::Landing:
        case EDiscGolfCameraViewMode::Basket:
            return EDiscGolfCameraViewOwner::LiveShot;
        case EDiscGolfCameraViewMode::ShotReplay:
            return EDiscGolfCameraViewOwner::Replay;
        case EDiscGolfCameraViewMode::CharacterCreator:
            return EDiscGolfCameraViewOwner::CharacterCreator;
        case EDiscGolfCameraViewMode::FreeCamera:
            return EDiscGolfCameraViewOwner::FreeCamera;
        case EDiscGolfCameraViewMode::CourseFlyover:
            return EDiscGolfCameraViewOwner::CourseFlyover;
        default:
            return EDiscGolfCameraViewOwner::None;
    }
}

bool DiscGolfCameraViewContract::TryAcquire(
    FDiscGolfCameraViewState& InOutState,
    EDiscGolfCameraViewOwner RequestingOwner,
    EDiscGolfCameraViewMode RequestedMode,
    FDiscGolfCameraViewToken& OutToken,
    FString& OutError)
{
    const EDiscGolfCameraViewOwner ExpectedOwner = RequiredOwner(RequestedMode);
    if (RequestingOwner == EDiscGolfCameraViewOwner::None
        || ExpectedOwner != RequestingOwner)
    {
        OutError = TEXT("Camera view request owner does not match the requested mode");
        return false;
    }
    if (InOutState.bActive && InOutState.Owner != RequestingOwner)
    {
        OutError = TEXT("Camera view is already owned by another presentation lifecycle");
        return false;
    }

    InOutState.bActive = true;
    InOutState.Owner = RequestingOwner;
    InOutState.Mode = RequestedMode;
    InOutState.Generation = NextGeneration(InOutState.Generation);
    FDiscGolfCameraViewToken AcquiredToken;
    AcquiredToken.Owner = InOutState.Owner;
    AcquiredToken.Mode = InOutState.Mode;
    AcquiredToken.Generation = InOutState.Generation;
    OutToken = AcquiredToken;
    OutError.Reset();
    return true;
}

bool DiscGolfCameraViewContract::TryRelease(
    FDiscGolfCameraViewState& InOutState,
    const FDiscGolfCameraViewToken& Token,
    FString& OutError)
{
    if (!IsCurrent(InOutState, Token))
    {
        OutError = TEXT("Camera view release token is stale or does not own the active view");
        return false;
    }

    InOutState.bActive = false;
    InOutState.Owner = EDiscGolfCameraViewOwner::None;
    InOutState.Mode = EDiscGolfCameraViewMode::Aim;
    OutError.Reset();
    return true;
}

bool DiscGolfCameraViewContract::IsCurrent(
    const FDiscGolfCameraViewState& State,
    const FDiscGolfCameraViewToken& Token)
{
    return State.bActive
        && Token.IsValid()
        && State.Owner == Token.Owner
        && State.Mode == Token.Mode
        && State.Generation == Token.Generation;
}
