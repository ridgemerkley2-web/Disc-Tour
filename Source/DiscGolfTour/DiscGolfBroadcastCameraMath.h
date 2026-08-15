#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"

enum class EDiscBroadcastCameraMode : uint8
{
    Launch,
    Fairway,
    Finish
};

struct FDiscBroadcastCameraInput
{
    FVector ReleaseLocationCm = FVector::ZeroVector;
    FVector BasketLocationCm = FVector(10000.0f, 0.0f, 0.0f);
    FVector DiscLocationCm = FVector::ZeroVector;
    FVector VelocityMps = FVector::ZeroVector;
    EDiscShotContext ShotContext = EDiscShotContext::Drive;
    EDiscGroundState GroundState = EDiscGroundState::Airborne;
    float ElapsedSeconds = 0.0f;
};

struct FDiscBroadcastCameraPlan
{
    FVector WorldLocationCm = FVector::ZeroVector;
    FVector LookAtWorldCm = FVector::ZeroVector;
    float FieldOfViewDeg = 60.0f;
};

namespace DiscGolfBroadcastCameraMath
{
    inline const TCHAR* ModeLabel(EDiscBroadcastCameraMode Mode)
    {
        switch (Mode)
        {
            case EDiscBroadcastCameraMode::Launch: return TEXT("LAUNCH");
            case EDiscBroadcastCameraMode::Fairway: return TEXT("FAIRWAY TRACK");
            case EDiscBroadcastCameraMode::Finish: return TEXT("BASKET / LANDING");
            default: return TEXT("UNKNOWN");
        }
    }

    inline int32 ModeRank(EDiscBroadcastCameraMode Mode)
    {
        switch (Mode)
        {
            case EDiscBroadcastCameraMode::Launch: return 0;
            case EDiscBroadcastCameraMode::Fairway: return 1;
            case EDiscBroadcastCameraMode::Finish: return 2;
            default: return 0;
        }
    }

    inline FVector ShotForward(const FDiscBroadcastCameraInput& Input)
    {
        FVector Forward = Input.BasketLocationCm - Input.ReleaseLocationCm;
        Forward.Z = 0.0f;
        if (!Forward.Normalize())
        {
            Forward = Input.VelocityMps;
            Forward.Z = 0.0f;
            Forward = Forward.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
        }
        return Forward;
    }

    inline EDiscBroadcastCameraMode SelectMode(const FDiscBroadcastCameraInput& Input)
    {
        const float TravelCm = FVector::Dist2D(Input.ReleaseLocationCm, Input.DiscLocationCm);
        const float DistanceToBasketCm = FVector::Dist2D(Input.DiscLocationCm, Input.BasketLocationCm);
        const bool bPutting = Input.ShotContext != EDiscShotContext::Drive;

        if (Input.ElapsedSeconds < (bPutting ? 0.22f : 0.90f)
            && TravelCm < (bPutting ? 260.0f : 1400.0f))
        {
            return EDiscBroadcastCameraMode::Launch;
        }

        const bool bGroundPlay = Input.GroundState != EDiscGroundState::Airborne;
        const bool bNearBasket = DistanceToBasketCm <= (bPutting ? 2400.0f : 3300.0f);
        const bool bDescendingIntoLandingWindow = Input.ElapsedSeconds >= 1.8f
            && Input.VelocityMps.Z < -0.45f
            && Input.DiscLocationCm.Z <= Input.ReleaseLocationCm.Z + 550.0f;
        if (bPutting || bGroundPlay || bNearBasket || bDescendingIntoLandingWindow)
        {
            return EDiscBroadcastCameraMode::Finish;
        }

        return EDiscBroadcastCameraMode::Fairway;
    }

    inline FDiscBroadcastCameraPlan BuildPlan(
        const FDiscBroadcastCameraInput& Input,
        EDiscBroadcastCameraMode Mode)
    {
        const FVector Up = FVector::UpVector;
        const FVector Forward = ShotForward(Input);
        const FVector Side = FVector::CrossProduct(Up, Forward).GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
        const FVector VelocityLeadCm = Input.VelocityMps * 100.0f;
        const bool bPutting = Input.ShotContext != EDiscShotContext::Drive;

        FDiscBroadcastCameraPlan Plan;
        switch (Mode)
        {
            case EDiscBroadcastCameraMode::Launch:
                Plan.WorldLocationCm = Input.ReleaseLocationCm - Forward * 430.0f + Side * 175.0f + Up * 225.0f;
                Plan.LookAtWorldCm = Input.DiscLocationCm + VelocityLeadCm * 0.18f + Up * 25.0f;
                Plan.FieldOfViewDeg = bPutting ? 62.0f : 68.0f;
                break;

            case EDiscBroadcastCameraMode::Fairway:
            {
                const float HoleDistanceCm = FVector::Dist2D(Input.ReleaseLocationCm, Input.BasketLocationCm);
                const float AlongHoleCm = FMath::Min(HoleDistanceCm * 0.46f, 5200.0f);
                Plan.WorldLocationCm = Input.ReleaseLocationCm + Forward * AlongHoleCm
                    + Side * 3000.0f + Up * 1200.0f;
                Plan.LookAtWorldCm = Input.DiscLocationCm + VelocityLeadCm * 0.22f + Up * 35.0f;
                Plan.FieldOfViewDeg = 38.0f;
                break;
            }

            case EDiscBroadcastCameraMode::Finish:
            default:
            {
                const float DistanceToBasketCm = FVector::Dist2D(Input.DiscLocationCm, Input.BasketLocationCm);
                FVector LandingFocus = Input.BasketLocationCm;
                if (!bPutting && DistanceToBasketCm > 4500.0f)
                {
                    FVector HorizontalVelocity = VelocityLeadCm;
                    HorizontalVelocity.Z = 0.0f;
                    LandingFocus = Input.DiscLocationCm + HorizontalVelocity * 0.65f;
                    LandingFocus.Z = Input.ReleaseLocationCm.Z;
                }

                const float ForwardOffset = bPutting ? 280.0f : 720.0f;
                const float SideOffset = bPutting ? 480.0f : 1000.0f;
                const float HeightOffset = bPutting ? 260.0f : 620.0f;
                Plan.WorldLocationCm = LandingFocus + Forward * ForwardOffset + Side * SideOffset + Up * HeightOffset;
                const FVector DiscFocus = Input.DiscLocationCm
                    + VelocityLeadCm * (bPutting ? 0.05f : 0.10f) + Up * 35.0f;
                const FVector LandingFocusRaised = LandingFocus + Up * (bPutting ? 150.0f : 75.0f);
                Plan.LookAtWorldCm = FMath::Lerp(DiscFocus, LandingFocusRaised, bPutting ? 0.45f : 0.20f);
                Plan.FieldOfViewDeg = bPutting ? 58.0f : 46.0f;
                break;
            }
        }
        return Plan;
    }
}
