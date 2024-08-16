// Copyright 2024 GRB.


#include "Characters/Abilities/AbilityTasks/GRBAT_ServerWaitForClientTargetData.h"
#include "AbilitySystemComponent.h"

UGRBAT_ServerWaitForClientTargetData::UGRBAT_ServerWaitForClientTargetData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

///--@brief 构建异步节点--/
UGRBAT_ServerWaitForClientTargetData* UGRBAT_ServerWaitForClientTargetData::ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool TriggerOnce)
{
	UGRBAT_ServerWaitForClientTargetData* MyObj = NewAbilityTask<UGRBAT_ServerWaitForClientTargetData>(OwningAbility, TaskInstanceName);
	MyObj->bTriggerOnce = TriggerOnce;
	return MyObj;
}

///--@brief 激活异步节点--/
void UGRBAT_ServerWaitForClientTargetData::Activate()
{
	// 仅承认服务端
	if (!Ability || !Ability->GetCurrentActorInfo()->IsNetAuthority())
	{
		return;
	}

	// 当服务器上人的ASC的技能目标数据预备好的时机, 绑定回调
	FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
	FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
	AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UGRBAT_ServerWaitForClientTargetData::OnASCTargetDataHasSetCallback);
}

///--@brief 销毁异步节点--/
void UGRBAT_ServerWaitForClientTargetData::OnDestroy(bool AbilityEnded)
{
	// 清除人ASC上 负责处理当目标数据预备好时 的所有业务回调
	if (AbilitySystemComponent.IsValid())
	{
		FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
		FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
		AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).RemoveAll(this);
	}

	Super::OnDestroy(AbilityEnded);
}

///--@brief 当委托:ASC在服务端已预备好技能目标数据, 会执行回调业务--/
void UGRBAT_ServerWaitForClientTargetData::OnASCTargetDataHasSetCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	// 1.在服务器上消耗从客户端组织好发来的目标数据
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	// 2.在服务器已预备好技能目标数据的同一刻; 广播节点上挂好的蓝图功能块(如BP_GARiflePrimaryInstant::HandleTargetData)
	FGameplayAbilityTargetDataHandle MutableData = Data;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		ValidDataDelegate.Broadcast(MutableData);
	}
	// 3.复核若设置了触发单词,则终止异步节点
	if (bTriggerOnce)
	{
		EndTask();
	}
}
