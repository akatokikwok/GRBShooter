// Copyright 2024 GRB.


#include "Characters/Abilities/AbilityTasks/GRBAT_WaitDelayOneFrame.h"
#include "TimerManager.h"

UGRBAT_WaitDelayOneFrame::UGRBAT_WaitDelayOneFrame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

///--@brief 负责自动射击延时的AbilityTask; 仿自引擎的UAbilityTask_WaitDelay; 但是仅延时一帧--/
UGRBAT_WaitDelayOneFrame* UGRBAT_WaitDelayOneFrame::WaitDelayOneFrame(UGameplayAbility* OwningAbility)
{
	UGRBAT_WaitDelayOneFrame* MyObj = NewAbilityTask<UGRBAT_WaitDelayOneFrame>(OwningAbility);
	return MyObj;
}

///--@brief 激活节点--/
void UGRBAT_WaitDelayOneFrame::Activate()
{
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGRBAT_WaitDelayOneFrame::OnDelayFinishCallback);
}

///--@brief 给下一帧的tick绑定业务回调--/
void UGRBAT_WaitDelayOneFrame::OnDelayFinishCallback()
{
	// 负责下一帧的业务
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinishDelegate.Broadcast();
	}
	// 同时把上一帧的AT终止掉
	EndTask();
}
