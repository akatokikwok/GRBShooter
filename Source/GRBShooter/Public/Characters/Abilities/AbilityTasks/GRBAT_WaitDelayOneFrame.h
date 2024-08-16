// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GRBAT_WaitDelayOneFrame.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitDelayOneFrameDelegate);

/**
 * 负责自动射击延时的AbilityTask
 * 仿自引擎的UAbilityTask_WaitDelay; 但是仅延时一帧
 * Like WaitDelay but only delays one frame (tick).
 */
UCLASS()
class GRBSHOOTER_API UGRBAT_WaitDelayOneFrame : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

private:
	// 节点内部异步委托; 负责下一帧的业务
	UPROPERTY(BlueprintAssignable)
	FWaitDelayOneFrameDelegate OnFinishDelegate;

public:
	///--@brief 负责自动射击延时的AbilityTask; 仿自引擎的UAbilityTask_WaitDelay; 但是仅延时一帧--/
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UGRBAT_WaitDelayOneFrame* WaitDelayOneFrame(UGameplayAbility* OwningAbility);

	///--@brief 激活节点--/
	virtual void Activate() override;

private:
	///--@brief 给下一帧的tick绑定业务回调--/
	void OnDelayFinishCallback();
};
