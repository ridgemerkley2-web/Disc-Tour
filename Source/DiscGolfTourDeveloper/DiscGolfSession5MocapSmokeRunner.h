#pragma once

#include "CoreMinimal.h"
#include "DiscGolfSession3SmokeRunner.h"
#include "DiscGolfSession5MocapSmokeRunner.generated.h"

/**
 * Session 5 owns only pipeline preflight and reporting.  The accepted Session
 * 3 runner remains the source of the cancellation, exactly-once release,
 * authoritative flight, recovery, camera, and next-action assertions.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession5MocapSmokeRunner
    : public ADiscGolfSession3SmokeRunner
{
    GENERATED_BODY()

public:
    virtual void Start() override;

protected:
    virtual void Fail(const FString& Reason) override;
    virtual void Pass() override;

private:
    FString RequestedProfile;
};
