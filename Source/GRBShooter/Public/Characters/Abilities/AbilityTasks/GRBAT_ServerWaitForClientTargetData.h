// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "GRBAT_ServerWaitForClientTargetData.generated.h"

/**
 * 特殊的AbilityTask; 负责以下业务
 * 服务器：ServerWaitForClientTargetData这个AbilityTask：等待从客户端发送过来的TargetData，如果接收到，就会BroadCast一个ValidData触发HandleTargetData。
 * 客户端：仅仅激活ServerWaitForClientTargetData，什么都不做处理，然后就执行蓝图后面的FireRifleBullet功能块。在单机状态下，客户端就是Server，ServerWaitForClientTargetData等于没用
 * 
 */
UCLASS()
class GRBSHOOTER_API UGRBAT_ServerWaitForClientTargetData : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

public:
	// WaitTargetData异步节点里定义的1个委托; 负责等待即将传入的targetdata数据payload句柄
	// 负责在蓝图里绑定异步回调
	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate ValidDataDelegate;

public:
	///--@brief 构建异步节点--/
	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms = "Instigator"), Category = "Ability|Tasks")
	static UGRBAT_ServerWaitForClientTargetData* ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool TriggerOnce);

	///--@brief 激活异步节点--/
	virtual void Activate() override;

	///--@brief 销毁异步节点--/
	virtual void OnDestroy(bool AbilityEnded) override;

	///--@brief 当委托:ASC在服务端已预备好技能目标数据, 会执行回调业务--/
	UFUNCTION()
	void OnASCTargetDataHasSetCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);

protected:
	// 限制仅触发一此
	bool bTriggerOnce;
};
