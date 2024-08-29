// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GRBAT_WaitChangeFOV.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChangeFOVDelegate);

/**
 * 异步任务: 用于动态调控视场角.
 */
UCLASS()
class GRBSHOOTER_API UGRBAT_WaitChangeFOV : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(BlueprintAssignable)
	FChangeFOVDelegate OnTargetFOVReached;

public:
	// Change the FOV to the specified value, using the float curve (range 0 - 1) or fallback to linear interpolation
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UGRBAT_WaitChangeFOV* WaitChangeFOV(UGameplayAbility* OwningAbility, FName TaskInstanceName, class UCameraComponent* CameraComponent, float TargetFOV, float Duration, UCurveFloat* OptionalInterpolationCurve);

	virtual void Activate() override;

	// Tick function for this task, if bTickingTask == true
	virtual void TickTask(float DeltaTime) override;

	virtual void OnDestroy(bool AbilityIsEnding) override;

	FChangeFOVDelegate& GetOnTargetFOVReached() { return OnTargetFOVReached; };

protected:
	bool bIsFinished;

	float StartFOV;

	float TargetFOV;

	float Duration;

	float TimeChangeStarted;

	float TimeChangeWillEnd;

	class UCameraComponent* CameraComponent;

	class UCurveFloat* LerpCurve;
};
