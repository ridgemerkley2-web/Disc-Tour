#include "DiscGolfPresentationAudioRouterComponent.h"

#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
float Sanitize(float Value, float Minimum, float Maximum, float Fallback = 0.0f)
{
    return FMath::Clamp(FMath::IsFinite(Value) ? Value : Fallback, Minimum, Maximum);
}

bool IsKnownAudioEventType(EDGAudioEventType Type)
{
    return Type >= EDGAudioEventType::ThrowRelease
        && Type <= EDGAudioEventType::EnvironmentAmbience;
}

bool IsKnownImpactSurface(EDGImpactSurface Surface)
{
    return Surface >= EDGImpactSurface::Unknown && Surface <= EDGImpactSurface::OutOfBounds;
}

FName CategoryId(EDiscGolfPresentationAudioCategory Category)
{
    switch (Category)
    {
        case EDiscGolfPresentationAudioCategory::ThrowRelease: return TEXT("ThrowRelease");
        case EDiscGolfPresentationAudioCategory::AirborneFlight: return TEXT("AirborneFlight");
        case EDiscGolfPresentationAudioCategory::GroundContact: return TEXT("GroundContact");
        case EDiscGolfPresentationAudioCategory::GroundState: return TEXT("GroundState");
        case EDiscGolfPresentationAudioCategory::BasketOutcome: return TEXT("BasketOutcome");
        case EDiscGolfPresentationAudioCategory::Penalty: return TEXT("Penalty");
        case EDiscGolfPresentationAudioCategory::HoleStart: return TEXT("HoleStart");
        case EDiscGolfPresentationAudioCategory::HoleCompletion: return TEXT("HoleCompletion");
        case EDiscGolfPresentationAudioCategory::HoleTransition: return TEXT("HoleTransition");
        case EDiscGolfPresentationAudioCategory::RoundCompletion: return TEXT("RoundCompletion");
        case EDiscGolfPresentationAudioCategory::Replay: return TEXT("Replay");
        case EDiscGolfPresentationAudioCategory::Flyover: return TEXT("Flyover");
        case EDiscGolfPresentationAudioCategory::Invalid:
        default: return NAME_None;
    }
}

EDGAudioEventType ResolveProviderType(const FDiscGolfPresentationAudioEvent& Event)
{
    switch (Event.Category)
    {
        case EDiscGolfPresentationAudioCategory::ThrowRelease:
            return EDGAudioEventType::ThrowRelease;
        case EDiscGolfPresentationAudioCategory::AirborneFlight:
            return EDGAudioEventType::DiscFlightLoop;
        case EDiscGolfPresentationAudioCategory::GroundContact:
        case EDiscGolfPresentationAudioCategory::GroundState:
            return EDGAudioEventType::DiscImpact;
        case EDiscGolfPresentationAudioCategory::BasketOutcome:
            return Event.Context.BasketResult == EBasketContactResult::Caught
                || Event.Context.BasketResult == EBasketContactResult::ChainDeflection
                ? EDGAudioEventType::BasketChains
                : EDGAudioEventType::BasketCage;
        case EDiscGolfPresentationAudioCategory::Penalty:
        case EDiscGolfPresentationAudioCategory::HoleCompletion:
        case EDiscGolfPresentationAudioCategory::RoundCompletion:
            return EDGAudioEventType::CrowdReaction;
        case EDiscGolfPresentationAudioCategory::HoleStart:
        case EDiscGolfPresentationAudioCategory::HoleTransition:
        case EDiscGolfPresentationAudioCategory::Replay:
        case EDiscGolfPresentationAudioCategory::Flyover:
        case EDiscGolfPresentationAudioCategory::Invalid:
        default:
            return EDGAudioEventType::EnvironmentAmbience;
    }
}

EDGImpactSurface ResolveProviderSurface(const FDiscGolfPresentationAudioEvent& Event)
{
    if (Event.Category == EDiscGolfPresentationAudioCategory::BasketOutcome)
    {
        return Event.Context.BasketResult == EBasketContactResult::Caught
            || Event.Context.BasketResult == EBasketContactResult::ChainDeflection
            ? EDGImpactSurface::Chain
            : EDGImpactSurface::Basket;
    }
    if (Event.EventId.ToString().Contains(TEXT("HazardWater")))
    {
        return EDGImpactSurface::Water;
    }
    if (!Event.bHasContext)
    {
        return EDGImpactSurface::Unknown;
    }
    switch (Event.Context.CourseSurface)
    {
        case ECourseSurfaceType::Fairway:
        case ECourseSurfaceType::TeePad:
        case ECourseSurfaceType::LightRough:
        case ECourseSurfaceType::DeepRough:
            return EDGImpactSurface::Grass;
        case ECourseSurfaceType::Dirt:
        case ECourseSurfaceType::Hazard:
            return EDGImpactSurface::Dirt;
        case ECourseSurfaceType::Rock:
            return EDGImpactSurface::Rock;
        case ECourseSurfaceType::OutOfBounds:
            return EDGImpactSurface::OutOfBounds;
        default:
            return EDGImpactSurface::Unknown;
    }
}
}

bool FDiscGolfPresentationAudioRoute::IsValid() const
{
    return !EventId.IsNone()
        && IsKnownAudioEventType(Payload.EventType)
        && IsKnownImpactSurface(Payload.Surface)
        && FMath::IsFinite(Payload.WorldLocationCm.X)
        && FMath::IsFinite(Payload.WorldLocationCm.Y)
        && FMath::IsFinite(Payload.WorldLocationCm.Z)
        && FMath::IsFinite(Payload.SpeedMps) && Payload.SpeedMps >= 0.0f
        && Payload.SpeedMps <= UDiscGolfPresentationAudioRouterComponent::MaximumSpeedMps
        && FMath::IsFinite(Payload.SpinRpm) && Payload.SpinRpm >= 0.0f
        && Payload.SpinRpm <= UDiscGolfPresentationAudioRouterComponent::MaximumSpinRpm
        && FMath::IsFinite(Payload.WobbleDegrees) && Payload.WobbleDegrees >= 0.0f
        && Payload.WobbleDegrees <= UDiscGolfPresentationAudioRouterComponent::MaximumWobbleDegrees
        && FMath::IsFinite(Payload.Wetness01) && Payload.Wetness01 >= 0.0f
        && Payload.Wetness01 <= 1.0f
        && FMath::IsFinite(Payload.Intensity01) && Payload.Intensity01 >= 0.0f
        && Payload.Intensity01 <= 1.0f
        && FMath::IsFinite(PitchMultiplier)
        && PitchMultiplier >= DiscGolfPresentationAudio::MinimumPitchMultiplier
        && PitchMultiplier <= DiscGolfPresentationAudio::MaximumPitchMultiplier
        && FMath::IsFinite(ListenerDistanceCm) && ListenerDistanceCm >= 0.0f
        && ListenerDistanceCm <= UDiscGolfPresentationAudioRouterComponent::MaximumListenerDistanceCm
        && FMath::IsFinite(OutputGain01) && OutputGain01 >= 0.0f && OutputGain01 <= 1.0f;
}

UDiscGolfPresentationAudioRouterComponent::UDiscGolfPresentationAudioRouterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    const auto AddGeneratedCandidate = [this](const TCHAR* Category, const TCHAR* ObjectPath)
    {
        FDiscGolfPresentationAudioFallbackBinding& Binding =
            GeneratedCandidateFallbackSounds.AddDefaulted_GetRef();
        Binding.CategoryId = FName(Category);
        Binding.Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(ObjectPath));
    };
    AddGeneratedCandidate(TEXT("ThrowRelease"),
        TEXT("/Game/Presentation/Audio/Generated/SW_ThrowRelease.SW_ThrowRelease"));
    AddGeneratedCandidate(TEXT("AirborneFlight"),
        TEXT("/Game/Presentation/Audio/Generated/SW_AirborneFlight.SW_AirborneFlight"));
    AddGeneratedCandidate(TEXT("GroundContact"),
        TEXT("/Game/Presentation/Audio/Generated/SW_GroundContact.SW_GroundContact"));
    AddGeneratedCandidate(TEXT("GroundState"),
        TEXT("/Game/Presentation/Audio/Generated/SW_GroundState.SW_GroundState"));
    AddGeneratedCandidate(TEXT("BasketOutcome"),
        TEXT("/Game/Presentation/Audio/Generated/SW_BasketOutcome.SW_BasketOutcome"));
    AddGeneratedCandidate(TEXT("Penalty"),
        TEXT("/Game/Presentation/Audio/Generated/SW_Penalty.SW_Penalty"));
    AddGeneratedCandidate(TEXT("HoleStart"),
        TEXT("/Game/Presentation/Audio/Generated/SW_HoleStart.SW_HoleStart"));
    AddGeneratedCandidate(TEXT("HoleCompletion"),
        TEXT("/Game/Presentation/Audio/Generated/SW_HoleCompletion.SW_HoleCompletion"));
    AddGeneratedCandidate(TEXT("HoleTransition"),
        TEXT("/Game/Presentation/Audio/Generated/SW_HoleTransition.SW_HoleTransition"));
    AddGeneratedCandidate(TEXT("RoundCompletion"),
        TEXT("/Game/Presentation/Audio/Generated/SW_RoundCompletion.SW_RoundCompletion"));
    AddGeneratedCandidate(TEXT("Replay"),
        TEXT("/Game/Presentation/Audio/Generated/SW_Replay.SW_Replay"));
    AddGeneratedCandidate(TEXT("Flyover"),
        TEXT("/Game/Presentation/Audio/Generated/SW_Flyover.SW_Flyover"));
}

void UDiscGolfPresentationAudioRouterComponent::BeginPlay()
{
    Super::BeginPlay();
    PrimeFallbackAssets();
}

void UDiscGolfPresentationAudioRouterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FallbackPreloadHandle.Reset();
    Super::EndPlay(EndPlayReason);
}

bool UDiscGolfPresentationAudioRouterComponent::BuildRoute(
    const FDiscGolfPresentationAudioEvent& Event,
    FDiscGolfPresentationAudioRoute& OutRoute,
    float SpeedMps,
    float SpinRpm,
    float WobbleDegrees,
    float Wetness01,
    float ListenerDistanceCm,
    float OutputGain01)
{
    if (!Event.IsValid())
    {
        return false;
    }

    FDiscGolfPresentationAudioRoute Candidate;
    Candidate.EventId = Event.EventId;
    Candidate.Payload.EventType = ResolveProviderType(Event);
    Candidate.Payload.WorldLocationCm = Event.bHasContext
        ? Event.Context.WorldLocationCm : FVector::ZeroVector;
    Candidate.Payload.SpeedMps = Sanitize(SpeedMps, 0.0f, MaximumSpeedMps);
    Candidate.Payload.SpinRpm = Sanitize(FMath::Abs(SpinRpm), 0.0f, MaximumSpinRpm);
    Candidate.Payload.WobbleDegrees = Sanitize(FMath::Abs(WobbleDegrees), 0.0f, MaximumWobbleDegrees);
    Candidate.Payload.Surface = ResolveProviderSurface(Event);
    Candidate.Payload.Wetness01 = Sanitize(Wetness01, 0.0f, 1.0f);
    Candidate.Payload.Intensity01 = Sanitize(Event.Intensity01, 0.0f, 1.0f);
    Candidate.PitchMultiplier = Sanitize(
        Event.PitchMultiplier,
        DiscGolfPresentationAudio::MinimumPitchMultiplier,
        DiscGolfPresentationAudio::MaximumPitchMultiplier,
        1.0f);
    Candidate.ListenerDistanceCm = Sanitize(
        ListenerDistanceCm, 0.0f, MaximumListenerDistanceCm);
    Candidate.OutputGain01 = Sanitize(OutputGain01, 0.0f, 1.0f, 1.0f);
    Candidate.bReplayPresentation = Event.bHasContext && Event.Context.bReplayPresentation;
    if (!Candidate.IsValid())
    {
        return false;
    }
    OutRoute = Candidate;
    return true;
}

bool UDiscGolfPresentationAudioRouterComponent::ConsumeSemanticEvent(
    const FDiscGolfPresentationAudioEvent& Event,
    float SpeedMps,
    float SpinRpm,
    float WobbleDegrees,
    float Wetness01,
    float ListenerDistanceCm,
    float OutputGain01)
{
    FDiscGolfPresentationAudioRoute Route;
    if (!BuildRoute(Event, Route, SpeedMps, SpinRpm, WobbleDegrees,
        Wetness01, ListenerDistanceCm, OutputGain01))
    {
        RouterStatusText = TEXT("AUDIO ROUTER REJECTED INVALID SEMANTIC EVENT");
        return false;
    }

    const FString DedupeKey = Event.DedupeKey();
    if (!DedupeKey.IsEmpty() && DedupeKey == LastSemanticDedupeKey)
    {
        RouterStatusText = TEXT("AUDIO ROUTER SUPPRESSED DUPLICATE EVENT");
        return false;
    }
    LastSemanticDedupeKey = DedupeKey;

    const FDiscGolfPresentationAudioFallbackBinding* Binding = FindFallback(Event);
    USoundBase* ReadySound = Binding ? Binding->Sound.Get() : nullptr;
    Route.bFallbackSoundReady = ReadySound != nullptr;
    if (bEnableFallbackPlayback && ReadySound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            ReadySound,
            Route.Payload.WorldLocationCm,
            Route.Payload.Intensity01 * Route.OutputGain01,
            Route.PitchMultiplier);
        RouterStatusText = FString::Printf(
            TEXT("AUDIO ROUTED | %s | FALLBACK READY"), *Route.EventId.ToString());
    }
    else
    {
        RouterStatusText = FString::Printf(
            TEXT("AUDIO ROUTED | %s | SILENT ASSET FALLBACK"), *Route.EventId.ToString());
    }

    RouteTrace.Add(Route);
    if (RouteTrace.Num() > MaximumRouteTrace)
    {
        RouteTrace.RemoveAt(0, RouteTrace.Num() - MaximumRouteTrace, EAllowShrinking::No);
    }
    RoutePresentationAudioEvent(Route);
    return true;
}

void UDiscGolfPresentationAudioRouterComponent::PrimeFallbackAssets()
{
    TArray<FSoftObjectPath> Paths;
    const auto AppendPaths = [&Paths](
        const TArray<FDiscGolfPresentationAudioFallbackBinding>& Bindings)
    {
        for (const FDiscGolfPresentationAudioFallbackBinding& Binding : Bindings)
        {
            const FSoftObjectPath Path = Binding.Sound.ToSoftObjectPath();
            if ((!Binding.EventId.IsNone() || !Binding.CategoryId.IsNone()) && Path.IsValid())
            {
                Paths.AddUnique(Path);
            }
        }
    };
    AppendPaths(FallbackSounds);
    AppendPaths(GeneratedCandidateFallbackSounds);
    if (!Paths.IsEmpty())
    {
        FallbackPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
    }
}

void UDiscGolfPresentationAudioRouterComponent::ResetRouteTrace()
{
    RouteTrace.Reset();
    LastSemanticDedupeKey.Reset();
    RouterStatusText = TEXT("AUDIO ROUTER READY - SILENT ASSET FALLBACK");
}

bool UDiscGolfPresentationAudioRouterComponent::HasGeneratedCandidateBinding(
    EDiscGolfPresentationAudioCategory Category) const
{
    const FName RequiredCategory = CategoryId(Category);
    return GeneratedCandidateFallbackSounds.ContainsByPredicate(
        [RequiredCategory](const FDiscGolfPresentationAudioFallbackBinding& Candidate)
        {
            return !RequiredCategory.IsNone() && Candidate.CategoryId == RequiredCategory
                && Candidate.Sound.ToSoftObjectPath().IsValid();
        });
}

const FDiscGolfPresentationAudioFallbackBinding*
UDiscGolfPresentationAudioRouterComponent::FindFallback(
    const FDiscGolfPresentationAudioEvent& Event) const
{
    const FDiscGolfPresentationAudioFallbackBinding* Exact = FallbackSounds.FindByPredicate(
        [&Event](const FDiscGolfPresentationAudioFallbackBinding& Candidate)
        {
            return !Event.EventId.IsNone() && Candidate.EventId == Event.EventId;
        });
    if (Exact)
    {
        return Exact;
    }
    const FName RequiredCategory = CategoryId(Event.Category);
    return GeneratedCandidateFallbackSounds.FindByPredicate(
        [RequiredCategory](const FDiscGolfPresentationAudioFallbackBinding& Candidate)
        {
            return !RequiredCategory.IsNone() && Candidate.CategoryId == RequiredCategory;
        });
}
