// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "GameplayTagContainer.h"
#include "GRBAT_WaitTargetDataUsingActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitTargetDataUsingActorDelegate, const FGameplayAbilityTargetDataHandle&, Data);

/**
 * 仿自虚幻原生的 UAbilityTask_WaitTargetData; 负责生成1个场景信息探测器(TargetActor)
 * 
 * 一般流程为：使用WaitTargetData_AbilityTask生成TargetActor,之后通过TargetActor的内部函数或者射线获取场景信息, 最后通过委托传递携带这些信息构建的FGameplayAbilityTargetDataHandle
 * Waits for TargetData from an already spawned TargetActor and does *NOT* destroy it when it receives data.
 * The original WaitTargetData's comments expects us to subclass it heavily, but the majority of its functions are not virtual.
 * Therefore this is a total rewrite of it to add bCreateKeyIfNotValidForMorePredicting functionality.
 */
UCLASS()
class GRBSHOOTER_API UGRBAT_WaitTargetDataUsingActor : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

private:
#pragma region ~ 委托 ~
	// 蓝图委托: 当本异步节点接收到探查器数据选中
	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataUsingActorDelegate ValidDataDelegate;

	// 蓝图委托: 当本异步节点接收到探查器数据取消选中
	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataUsingActorDelegate CancelledDelegate;
#pragma endregion

public:
	/**
	* ///--@brief 构建静态节点.--/
	* Uses specified spawned TargetActor and waits for it to return valid data or to be canceled. The TargetActor is *NOT* destroyed.
	*
	* @param bCreateKeyIfNotValidForMorePredicting Will create a new scoped prediction key if the current scoped prediction key is not valid for more predicting.
	* If false, it will always create a new scoped prediction key. We would want to set this to true if we want to use a potentially existing valid scoped prediction key like the ability's activation key in a batched ability.
	*/
	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms = "Instigator"), Category = "Ability|Tasks")
	static UGRBAT_WaitTargetDataUsingActor* WaitTargetDataWithReusableActor(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType,
		AGameplayAbilityTargetActor* InTargetActor,
		bool bCreateKeyIfNotValidForMorePredicting = false
	);

	///--@brief AT激活时候的逻辑入口.--/
	virtual void Activate() override;

	///--@brief 进行探查器的清理工作. --/
	virtual void OnDestroy(bool AbilityEnded) override;

	///--@brief UGameplayTask::ExternalConfirm 这是个异步节点,当接收到有外部用户输入的时候会调用 --/
	// Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true.
	virtual void ExternalConfirm(bool bEndTask) override;

	///--@brief UGameplayTask::ExternalCancel 这是个异步节点,当接收到有取消外部用户输入的时候会调用 --/
	// Called when the ability is asked to cancel from an outside node. What this means depends on the individual task. By default, this does nothing other than ending the task.
	virtual void ExternalCancel() override;

protected:
	///--@brief 条件函数; 当位于客户端的时候是否开启往服务器制作探查器数据--/
	virtual bool ShouldReplicateDataToServer() const;

	///--@brief 回调; 负责处理探查器目标数据准备好了之后的业务. --/
	UFUNCTION()
	virtual void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& Data);

	///--@brief 回调; 负责处理探查器目标数据被取消选中了之后的业务. --/
	UFUNCTION()
	virtual void OnTargetDataCancelledCallback(const FGameplayAbilityTargetDataHandle& Data);

	///--@brief 回调：处理事件当ASC上技能目标数据已准备--/
	UFUNCTION()
	virtual void OnASCTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);

	///--@brief 回调：处理事件当ASC上技能目标数据已取消--/
	UFUNCTION()
	virtual void OnASCTargetDataReplicatedCancelledCallback();

	///--@brief 针对管理的target actor, 为target actor的 目标数据选中/目标数据取消委托绑定回调.--/
	virtual void InitializeTargetActor() const;

	///--@brief 当且仅当为非本地客户端的ASC绑定目标数据的回调并执行任务流等待远端的输入--/
	virtual void RegisterTargetDataCallbacks();

	///--@brief 最终完成场景探查器的操作, 等待输入层面的调度来激发探查器运作(Instant/UserConfirmed)--/
	virtual void FinalizeTargetActor() const;

	// 管理的场景探查器TargetActor
	UPROPERTY()
	AGameplayAbilityTargetActor* m_GameplayTargetActor;

	// 是否创建更多的预测Key
	bool bCreateKeyIfNotValidForMorePredicting;

	// 确认类型
	TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType;

	FDelegateHandle OnTargetDataReplicatedCallbackDelegateHandle;
};
