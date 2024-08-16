// Copyright 2024 Dan GRB.


#include "Characters/Abilities/AbilityTasks/GRBAT_WaitTargetDataUsingActor.h"
#include "AbilitySystemComponent.h"
#include "Characters/Abilities/GRBGATA_Trace.h"

UGRBAT_WaitTargetDataUsingActor::UGRBAT_WaitTargetDataUsingActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

///--@brief 构建静态节点.--/
UGRBAT_WaitTargetDataUsingActor* UGRBAT_WaitTargetDataUsingActor::WaitTargetDataWithReusableActor(UGameplayAbility* OwningAbility, FName TaskInstanceName, TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType, AGameplayAbilityTargetActor* InTargetActor, bool bCreateKeyIfNotValidForMorePredicting)
{
	UGRBAT_WaitTargetDataUsingActor* MyObj = NewAbilityTask<UGRBAT_WaitTargetDataUsingActor>(OwningAbility, TaskInstanceName); //Register for task list here, providing a given FName as a key
	MyObj->m_GameplayTargetActor = InTargetActor;
	MyObj->ConfirmationType = ConfirmationType;
	MyObj->bCreateKeyIfNotValidForMorePredicting = bCreateKeyIfNotValidForMorePredicting;
	return MyObj;
}


#pragma region ~ Override ~
///--@brief AT激活时候的逻辑入口.--/
void UGRBAT_WaitTargetDataUsingActor::Activate()
{
	if (!IsValid(this))
	{
		return;
	}

	if (Ability && m_GameplayTargetActor)
	{
		InitializeTargetActor(); // 针对管理的target actor, 为target actor的 目标数据选中/目标数据取消委托绑定回调
		RegisterTargetDataCallbacks(); // 当且仅当为非本地客户端的ASC绑定目标数据的回调并执行任务流等待远端的输入
		FinalizeTargetActor(); // 最终完成场景探查器的操作, 等待输入层面的调度来激发探查器运作(Instant/UserConfirmed)
	}
	else
	{
		EndTask();
	}
}

///--@brief 进行探查器的清理工作. --/
void UGRBAT_WaitTargetDataUsingActor::OnDestroy(bool AbilityEnded)
{
	if (m_GameplayTargetActor)
	{
		AGRBGATA_Trace* GRBTracer = Cast<AGRBGATA_Trace>(m_GameplayTargetActor);
		if (GRBTracer)
		{
			GRBTracer->StopTargeting();
		}
		else
		{
			// TargetActor doesn't have a StopTargeting function
			m_GameplayTargetActor->SetActorTickEnabled(false);

			// Clear added callbacks
			m_GameplayTargetActor->TargetDataReadyDelegate.RemoveAll(this);
			m_GameplayTargetActor->CanceledDelegate.RemoveAll(this);
			AbilitySystemComponent->GenericLocalConfirmCallbacks.RemoveDynamic(m_GameplayTargetActor, &AGameplayAbilityTargetActor::ConfirmTargeting);
			AbilitySystemComponent->GenericLocalCancelCallbacks.RemoveDynamic(m_GameplayTargetActor, &AGameplayAbilityTargetActor::CancelTargeting);
			m_GameplayTargetActor->GenericDelegateBoundASC = nullptr;
		}
	}

	Super::OnDestroy(AbilityEnded);
}

///--@brief UGameplayTask::ExternalConfirm 这是个异步节点,当接收到有外部用户输入的时候会调用 --/
void UGRBAT_WaitTargetDataUsingActor::ExternalConfirm(bool bEndTask)
{
	check(AbilitySystemComponent.IsValid());
	if (m_GameplayTargetActor)
	{
		if (m_GameplayTargetActor->ShouldProduceTargetData())
		{
			/*
			 * 执行探查器的核心业务; 三步走
			 * 执行探查器PerformTrace-- TArray<FHitResult> HitResults = PerformTrace(SourceActor);
			 * 为一组命中hit制作 目标数据句柄 并存储它们-- FGameplayAbilityTargetDataHandle Handle = MakeTargetData(HitResults);
			 * 为探查器的 "ready"事件广播; 并传入组好的payload 目标数据句柄-- AGameplayAbilityTargetActor::TargetDataReadyDelegate.Broadcast(Handle);
			 */
			m_GameplayTargetActor->ConfirmTargetingAndContinue();
		}
	}
	Super::ExternalConfirm(bEndTask);
}

///--@brief UGameplayTask::ExternalCancel 这是个异步节点,当接收到有取消外部用户输入的时候会调用 --/
void UGRBAT_WaitTargetDataUsingActor::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		// 广播"取消输入"事件
		CancelledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
	}
	Super::ExternalCancel();
}
#pragma endregion


#pragma region ~ Virtual ~
///--@brief 条件函数; 当位于客户端的时候是否开启往服务器制作探查器数据--/
bool UGRBAT_WaitTargetDataUsingActor::ShouldReplicateDataToServer() const
{
	if (!Ability || !m_GameplayTargetActor)
	{
		return false;
	}

	// Send TargetData to the server IFF we are the client and this isn't a GameplayTargetActor that can produce data on the server	
	const FGameplayAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
	if (!Info->IsNetAuthority() && !m_GameplayTargetActor->ShouldProduceTargetDataOnServer)
	{
		return true;
	}

	return false;
}

///--@brief 回调; 负责处理探查器目标数据准备好了之后的业务. --/
void UGRBAT_WaitTargetDataUsingActor::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& Data)
{
	check(AbilitySystemComponent.IsValid());
	if (!Ability)
	{
		return;
	}

	/*
	 * 户端会预测角色的移动，同时将这些预测结果发送给服务器。服务器会根据收到的数据重新计算并可能校正客户端的状态。
	 * 为了在代码中方便地处理这些预测时间窗口，FScopedPredictionWindow 提供了一种RAII风格（Resource Acquisition Is Initialization）的机制，
	 * 保证在进入和离开某个代码块时自动处理预测窗口的开启和关闭
	 * 类似于区域锁.
	 */
	// 按已有判断条件和字段组织1个客户端预测窗口
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get(),
	                                         ShouldReplicateDataToServer() && (bCreateKeyIfNotValidForMorePredicting && !AbilitySystemComponent->ScopedPredictionKey.IsValidForMorePrediction()));

	// 处理在客户端预测的一些预测和与服务器交互
	const FGameplayAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
	if (UAbilityTask::IsPredictingClient())
	{
		if (!m_GameplayTargetActor->ShouldProduceTargetDataOnServer)
		{
			FGameplayTag ApplicationTag; // Fixme: where would this be useful?
			// 把探查器目标数据发送到服务器;以达成网络同步
			AbilitySystemComponent->CallServerSetReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey(), Data, ApplicationTag, AbilitySystemComponent->ScopedPredictionKey);
		}
		else if (ConfirmationType == EGameplayTargetingConfirmation::UserConfirmed)
		{
			// RPC到服务器 处理技能事件,设置为通用确认
			// We aren't going to send the target data, but we will send a generic confirmed message.
			AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericConfirm, GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
		}
	}

	if (UAbilityTask::ShouldBroadcastAbilityTaskDelegates())
	{
		ValidDataDelegate.Broadcast(Data);
	}

	// EGameplayTargetingConfirmation 定义了目标确认的不同方式。在能力系统中，有时需要玩家手动确认目标，例如点击确认目标、自动确认目标等
	// 必须是多阶段目标确认模式才不会终止abilityTask
	// 示例情景：一个复杂的技能，需要玩家先确认第一个位置，然后确认第二个位置。
	if (ConfirmationType != EGameplayTargetingConfirmation::CustomMulti)
	{
		EndTask();
	}
}

///--@brief 回调; 负责处理探查器目标数据被取消选中了之后的业务. --/
void UGRBAT_WaitTargetDataUsingActor::OnTargetDataCancelledCallback(const FGameplayAbilityTargetDataHandle& Data)
{
	check(AbilitySystemComponent.IsValid());

	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get(), IsPredictingClient());

	if (IsPredictingClient())
	{
		if (!m_GameplayTargetActor->ShouldProduceTargetDataOnServer)
		{
			// RPC到服务器取消探查器的目标数据取消选中
			AbilitySystemComponent->ServerSetReplicatedTargetDataCancelled(GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
		}
		else
		{
			// RPC到服务器 处理技能事件,设置为通用取消
			// We aren't going to send the target data, but we will send a generic confirmed message.
			AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
		}
	}
	// AT节点内, 广播"取消选中"委托
	CancelledDelegate.Broadcast(Data);
	EndTask();
}

///--@brief 回调：处理事件当ASC上技能目标数据已准备--/
void UGRBAT_WaitTargetDataUsingActor::OnASCTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	check(AbilitySystemComponent.IsValid());

	FGameplayAbilityTargetDataHandle MutableDataHandle = Data;

	/*
	 * ConsumeClientReplicatedTargetData 函数，在服务端执行一个技能（能力）时，消耗目标数据。这些数据是通过网络从服务器复制过来的，并且包含了执行技能时所需的目标信息，例如目标位置、目标角色等
	 */
	if (Ability->GetCurrentActorInfo()->IsNetAuthority())
	{
		AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	/** 依据条件校验 OnReplicatedTargetDataReceived 校验探查器数据; 再执行 "选中"或者"取消选中"委托
	 *  Call into the TargetActor to sanitize/verify the data. If this returns false, we are rejecting the replicated target data and will treat this as a cancel.
	 *	This can also be used for bandwidth optimizations. OnReplicatedTargetDataReceived could do an actual
	 *	trace/check/whatever server side and use that data. So rather than having the client send that data
	 *	explicitly, the client is basically just sending a 'confirm' and the server is now going to do the work in OnReplicatedTargetDataReceived.
	 */
	if (m_GameplayTargetActor && !m_GameplayTargetActor->OnReplicatedTargetDataReceived(MutableDataHandle))
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			CancelledDelegate.Broadcast(MutableDataHandle);
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			ValidDataDelegate.Broadcast(MutableDataHandle);
		}
	}

	// EGameplayTargetingConfirmation 定义了目标确认的不同方式。在能力系统中，有时需要玩家手动确认目标，例如点击确认目标、自动确认目标等
	// 必须是多阶段目标确认模式才不会终止abilityTask
	// 示例情景：一个复杂的技能，需要玩家先确认第一个位置，然后确认第二个位置。
	if (ConfirmationType != EGameplayTargetingConfirmation::CustomMulti)
	{
		EndTask();
	}
}

///--@brief 回调：处理事件当ASC上技能目标数据已取消--/
void UGRBAT_WaitTargetDataUsingActor::OnASCTargetDataReplicatedCancelledCallback()
{
	check(AbilitySystemComponent.IsValid());
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		CancelledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
	}
	EndTask();
}

///--@brief 针对管理的target actor, 为target actor的 目标数据选中/目标数据取消委托绑定回调.--/
void UGRBAT_WaitTargetDataUsingActor::InitializeTargetActor() const
{
	check(m_GameplayTargetActor);
	check(Ability);

	// 注册targetactor的控制器
	m_GameplayTargetActor->PrimaryPC = Ability->GetCurrentActorInfo()->PlayerController.Get();
	// 预先为target actor的"数据准备好" "取消" 2个事件绑定回调
	// If we spawned the target actor, always register the callbacks for when the data is ready.
	m_GameplayTargetActor->TargetDataReadyDelegate.AddUObject(const_cast<UGRBAT_WaitTargetDataUsingActor*>(this), &UGRBAT_WaitTargetDataUsingActor::OnTargetDataReadyCallback);
	m_GameplayTargetActor->CanceledDelegate.AddUObject(const_cast<UGRBAT_WaitTargetDataUsingActor*>(this), &UGRBAT_WaitTargetDataUsingActor::OnTargetDataCancelledCallback);
}

///--@brief 当且仅当为非本地客户端的ASC绑定目标数据的回调并执行任务流等待远端的输入--/
void UGRBAT_WaitTargetDataUsingActor::RegisterTargetDataCallbacks()
{
	// if (!IsValid(this))
	// {
	// 	return;
	// }
	check(Ability);

	const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled(); // 是否是本机客户端
	const bool bShouldProduceTargetDataOnServer = m_GameplayTargetActor->ShouldProduceTargetDataOnServer; // 是否场景探查器在服务端制造目标数据

	// If not locally controlled (server for remote client), see if TargetData was already sent else register callback for when it does get here.
	if (!bIsLocallyControlled)
	{
		// Register with the TargetData callbacks if we are expecting client to send them
		if (!bShouldProduceTargetDataOnServer)
		{
			FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle(); // 获取AT关联技能句柄
			FPredictionKey ActivationPredictionKey = GetActivationPredictionKey(); // 获取非本机客户端下的预测key

			// ASC上的两个事件"目标数据准备好" "目标数据取消" 绑定关联的回调函数
			//Since multifire is supported, we still need to hook up the callbacks
			AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UGRBAT_WaitTargetDataUsingActor::OnASCTargetDataReplicatedCallback);
			AbilitySystemComponent->AbilityTargetDataCancelledDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UGRBAT_WaitTargetDataUsingActor::OnASCTargetDataReplicatedCancelledCallback);
			// CallReplicatedTargetDataDelegatesIfSet() 是在接收到复制的目标数据时触发相应处理逻辑的重要方法
			AbilitySystemComponent->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey);
			// 该方法主要用于在等待远程玩家返回目标数据或其他必要的输入时，通过设置任务状态来管理任务的执行流
			UAbilityTask::SetWaitingOnRemotePlayerData();
		}
	}
}

///--@brief 最终完成场景探查器的操作, 等待输入层面的调度来激发探查器运作(Instant/UserConfirmed)--/
void UGRBAT_WaitTargetDataUsingActor::FinalizeTargetActor() const
{
	check(m_GameplayTargetActor);
	check(Ability);

	// 覆写; 开始探查器的探查准备工作并开启tick
	m_GameplayTargetActor->StartTargeting(Ability);

	if (m_GameplayTargetActor->ShouldProduceTargetData())
	{
		// If instant confirm, then stop targeting immediately.
		// Note this is kind of bad: we should be able to just call a static func on the CDO to do this. 
		// But then we wouldn't get to set ExposeOnSpawnParameters.
		if (ConfirmationType == EGameplayTargetingConfirmation::Instant)
		{
			// 这里进入之后会间接触发多态的 AGRBGATA_Trace::ConfirmTargetingAndContinue()
			/*
			 * 执行探查器的核心业务; 三步走
			 * 执行探查器PerformTrace-- TArray<FHitResult> HitResults = PerformTrace(SourceActor);
			 * 为一组命中hit制作 目标数据句柄 并存储它们-- FGameplayAbilityTargetDataHandle Handle = MakeTargetData(HitResults);
			 * 为探查器的 "ready"事件广播; 并传入组好的payload 目标数据句柄-- AGameplayAbilityTargetActor::TargetDataReadyDelegate.Broadcast(Handle);
			 */
			m_GameplayTargetActor->ConfirmTargeting();
		}
		else if (ConfirmationType == EGameplayTargetingConfirmation::UserConfirmed)
		{
			// 用于绑定确认和取消输入，允许玩家在选择目标时进行确认或取消操作; 非瞬发的,等待玩家第二步选择
			// Bind to the Cancel/Confirm Delegates (called from local confirm or from repped confirm)
			m_GameplayTargetActor->BindToConfirmCancelInputs();
		}
	}
}
#pragma endregion
