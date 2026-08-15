#include "DiscGolfUIFlowSubsystem.h"

void UDiscGolfUIFlowSubsystem::PushRoute(FName Route)
{
    if (Route.IsNone())
    {
        return;
    }

    RouteStack.Add(Route);
    OnRouteChanged.Broadcast(GetActiveRoute(), RouteStack.Num());
}

bool UDiscGolfUIFlowSubsystem::PopRoute()
{
    if (RouteStack.Num() <= 0)
    {
        return false;
    }

    RouteStack.Pop();
    OnRouteChanged.Broadcast(GetActiveRoute(), RouteStack.Num());
    return true;
}

void UDiscGolfUIFlowSubsystem::ResetToRoute(FName Route)
{
    RouteStack.Reset();
    if (!Route.IsNone())
    {
        RouteStack.Add(Route);
    }

    OnRouteChanged.Broadcast(GetActiveRoute(), RouteStack.Num());
}

FName UDiscGolfUIFlowSubsystem::GetActiveRoute() const
{
    return RouteStack.Num() > 0 ? RouteStack.Last() : NAME_None;
}
