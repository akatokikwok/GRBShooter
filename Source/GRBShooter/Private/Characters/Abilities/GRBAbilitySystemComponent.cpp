// Copyright 2024 GRB.


#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "Animation/AnimInstance.h"
#include "Characters/Abilities/GRBGameplayAbility.h"
#include "GameplayCueManager.h"
#include "GRBBlueprintFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/GRBWeapon.h"


static TAutoConsoleVariable<float> CVarReplayMontageErrorThreshold(
	TEXT("GRB.replay.MontageErrorThreshold"),
	0.5f,
	TEXT("Tolerance level for when montage playback position correction occurs in replays")
);


UGRBAbilitySystemComponent::UGRBAbilitySystemComponent()
{
}

void UGRBAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGRBAbilitySystemComponent, RepAnimMontageInfoForMeshes);
}

/** ~Start Implements UGameplayTasksComponent::GetShouldTick.*/
bool UGRBAbilitySystemComponent::GetShouldTick() const
{
	for (FGameplayAbilityRepAnimMontageForMesh RepMontageInfo : RepAnimMontageInfoForMeshes)
	{
		// 仅承认服务端且蒙太奇未被中断的业务情形
		const bool bHasReplicatedMontageInfoToUpdate = (IsOwnerActorAuthoritative() && RepMontageInfo.RepMontageInfo.IsStopped == false);
		if (bHasReplicatedMontageInfoToUpdate)
		{
			return true;
		}
	}
	return Super::GetShouldTick();
}

/** TickComponent. */
void UGRBAbilitySystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// 每帧在服务端 把本地蒙太奇骨架数据拷贝至 RepMontage数据
	if (IsOwnerActorAuthoritative())
	{
		for (FGameplayAbilityLocalAnimMontageForMesh& MontageInfo : LocalAnimMontageInfoForMeshes)
		{
			AnimMontage_UpdateReplicatedDataForMesh(MontageInfo.Mesh); // 在服务端拷贝本地蒙太奇骨架动画信息集到Rep
		}
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

/** 用于初始化与特定角色和控制器相关联的能力信息。该方法通常在角色初始化完成、或者需要手动初始化能力信息时调用，以确保能力系统正确地关联到游戏角色和控制器.*/
void UGRBAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	// 初始化所有蒙太奇数据结构为空, 并初始化一下蒙太奇客户端表现回调
	LocalAnimMontageInfoForMeshes = TArray<FGameplayAbilityLocalAnimMontageForMesh>();
	RepAnimMontageInfoForMeshes = TArray<FGameplayAbilityRepAnimMontageForMesh>();
	if (bPendingMontageRep)
	{
		OnRep_ReplicatedAnimMontageForMesh(); // 客户端收到RepMontage复制之后的客户端表现回调
	}
}

/** 在一个能力（Ability）结束时调用，用于通知系统该能力已经完成、取消或中止。通过这一通知，系统可以进行一些清理工作、触发回调.*/
void UGRBAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);

	// 清空传入的动画技能
	ClearAnimatingAbilityForAllMeshes(Ability);
}

/** 助手方法, 从ACTOR拿取其ASC.*/
UGRBAbilitySystemComponent* UGRBAbilitySystemComponent::GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent)
{
	return Cast<UGRBAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, LookForComponent));
}

// ~Start Implements UAbilitySystemComponent::AbilityLocalInputPressed
/** 0.主要功能是响应本地输入,
 * 1.并尝试激活对应的游戏能力; 检查输入绑定：根据 InputID 查找是否有与之关联的能力。
 * 2.尝试激活能力：如果找到绑定的能力，尝试激活它。这涉及检查能力是否满足激活条件，诸如冷却时间是否完成、资源是否充足等。
 * 3.调试信息：输出和记录输入相关的调试信息
 */
void UGRBAbilitySystemComponent::AbilityLocalInputPressed(int32 InputID)
{
	// 输入的意外情况处理: 如果此 InputID 已通过 GenericConfirm/Cancel 重载，并且已绑定 GenericConfim/Cancel 回调，则使用该输入
	if (IsGenericConfirmInputBound(InputID))
	{
		LocalInputConfirm();
		return;
	}
	if (IsGenericCancelInputBound(InputID))
	{
		LocalInputCancel();
		return;
	}

	// ---------------------------------------------------------

	// ABILITYLIST_SCOPE_LOCK 宏用于锁定某个范围内对 TArray 的访问，确保在该范围内的操作是线程安全的
	// 当添加、删除或检查能力或效果是否存在时，可能会有并发访问从而导致数据竞争
	ABILITYLIST_SCOPE_LOCK();
	// 轮询已准备好的技能Spec数组
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.InputID == InputID)
		{
			if (Spec.Ability != nullptr)
			{
				Spec.InputPressed = true;
				if (Spec.IsActive())
				{
					// Replicate Input Directly 这个选项会把Client的Input输入的press和release事件Replicate(RPC)到Server中去
					// Epic不推荐使用这个选项，而是推荐使用另一种方法：如果我们把Input绑定到了ASC上，我们可以使用已经内置到AbilityTask中的input函数; 就是在使用了Ability的InputBind之后(把Ability的激活绑定到用户的输入上的方式)，使用AbilityTask中的有一个监听用户输入的Task
					// 当且仅当GA实例的复制标志位是命中的且在客户端的情形下,同步客户端输入状态（例如按键单击或松开）到RPC到服务器
					if (Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
					{
						UAbilitySystemComponent::ServerSetInputPressed(Spec.Handle);
					}

					// 客户端先激发1次输入
					// 注意这里是 SpecInputPressed; 仅在客户端确认输入激发
					UAbilitySystemComponent::AbilitySpecInputPressed(Spec);
					// 在服务器上验证和处理输入事件; 使用 InvokeReplicatedEvent 进行同步; 用于触发在客户端和服务器之间同步的事件
					// InvokeReplicatedEvent 是 UAbilitySystemComponent 类中的一个方法。它通常在技能（能力）或其他游戏玩法类中使用，用来触发已注册的事件，并确保这些事件在服务器和所有相关客户端之间同步 (把输入行为关联到服务端);
					// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
					UAbilitySystemComponent::InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
				}
				else
				{
					UGRBGameplayAbility* GA = Cast<UGRBGameplayAbility>(Spec.Ability);
					// 如果你不想你的GameplayAbility在按键按下时自动激活, 但是仍想将它们绑定到输入以与AbilityTask一起使用, 你可以在UGameplayAbility子类中添加一个新的布尔变量, bActivateOnInput
					if (GA && GA->bActivateOnInput)
					{
						// Ability is not active, so try to activate it
						TryActivateAbility(Spec.Handle);
					}
				}
			}
		}
	}
}

///@brief 获取特定类型GA的技能句柄.
FGameplayAbilitySpecHandle UGRBAbilitySystemComponent::FindAbilitySpecHandleForClass(TSubclassOf<UGameplayAbility> AbilityClass, UObject* OptionalSourceObject)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		TSubclassOf<UGameplayAbility> SpecAbilityClass = Spec.Ability->GetClass();
		if (SpecAbilityClass == AbilityClass)
		{
			if (!OptionalSourceObject || (OptionalSourceObject && Spec.SourceObject == OptionalSourceObject))
			{
				return Spec.Handle;
			}
		}
	}
	return FGameplayAbilitySpecHandle();
}

// 虚函数; 决定了是否应该批处理来自客户端的 RPC 请求，将多个请求合并成一个，以减少网络通信的开销。在多人游戏中，这种优化非常重要，可以显著减少网络延迟和带宽使用
bool UGRBAbilitySystemComponent::ShouldDoServerAbilityRPCBatch() const
{
	// 你可以加入更多复杂的条件，比如根据网络状态、帧率等动态调整
	// 假设我们有一个获取当前网络条件的函数
	// int32 CurrentPing = GetCurrentPing(); // 伪代码
	// int32 FPS = GetCurrentFPS(); // 伪代码
	// 网络延迟较高或FPS较低时，启用批处理
	// if (CurrentPing > 100 || FPS < 30)
	// {
	// 	return true;
	// }
	return true;
}

//---------------------------------------------------  ------------------------------------------------
///@brief 采用一种思想:把同一帧内的所有RPC合批, 最佳情况是，我们将 ActivateAbility、SendTargetData 和 EndAbility 批处理为一个 RPC，而不是三个
///@brief 最坏情况是，我们将 ActivateAbility 和 SendTargetData 批处理为一个 RPC，而不是两个，然后在单独的 RPC 中调用 EndAbility
///@brief 单发（又或者是半自动）将 ActivateAbility、SendTargetData 和 EndAbility 组合成一个 RPC，而不是三个
///@brief 全自动射击模式: 首发子弹射击是 把ActivateAbility and SendTargetData合批为一个RPC; 从第2发开始的子弹都是每一发RPC for SendTargetData; 第30发即最后一发采用的是 RPC for the EndAbility 
//---------------------------------------------------  ------------------------------------------------
bool UGRBAbilitySystemComponent::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately)
{
	// ~Start Cutter ---------------------------------------------------  演示合批RPC激活GA 将激活两个能力的 RPC 请求合并为一个请求-------------------------------------------------- End Cutter~
	// 创建一个范围批处理器对象
	// FScopedServerAbilityRPCBatcher AbilityRPCBatcher(this);
	// 示例：激活多个能力
	// FGameplayAbilitySpecHandle AbilityHandle1; // 假设已经初始化了
	// FGameplayAbilitySpecHandle AbilityHandle2; // 假设已经初始化了
	// InternalServerTryActivateAbility(AbilityHandle1); // 内部方法激活能力1
	// InternalServerTryActivateAbility(AbilityHandle2); // 内部方法激活能力2
	// ~Start Cutter ---------------------------------------------------  演示合批RPC激活GA 将激活两个能力的 RPC 请求合并为一个请求-------------------------------------------------- End Cutter~

	bool AbilityActivatedStatus = false;
	if (InAbilityHandle.IsValid())
	{
		/** 1.为指定的GA句柄创建1个范围批处理器对象*/
		FScopedServerAbilityRPCBatcher GRBAbilityRPCBatcher(this, InAbilityHandle);
		/** 2.激活GA句柄*/
		AbilityActivatedStatus = TryActivateAbility(InAbilityHandle, true);
		/** 3.*/
		if (EndAbilityImmediately)
		{
			FGameplayAbilitySpec* AbilitySpec = UAbilitySystemComponent::FindAbilitySpecFromHandle(InAbilityHandle); //从GA句柄找到GA-SPEC
			if (AbilitySpec)
			{
				UGRBGameplayAbility* GRBAbility = Cast<UGRBGameplayAbility>(AbilitySpec->GetPrimaryInstance()); // 从GA-Spec转化为GA
				GRBAbility->ExternalEndAbility(); // 旨在与批处理系统合作,从外部结束技能, 类似于K2_EndAbility
			}
		}

		return AbilityActivatedStatus;
	}

	return AbilityActivatedStatus;
}

///@brief 检测ASC内预测Key,返回字符串通知
FString UGRBAbilitySystemComponent::GetCurrentPredictionKeyStatus()
{
	return ScopedPredictionKey.ToString() + " is valid for more prediction: " + (ScopedPredictionKey.IsValidForMorePrediction() ? TEXT("true") : TEXT("false"));
}

///@brief 给Self施加带预测性质效果的BUFF
FActiveGameplayEffectHandle UGRBAbilitySystemComponent::BP_ApplyGameplayEffectToSelfWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle EffectContext)
{
	if (GameplayEffectClass)
	{
		if (!EffectContext.IsValid())
		{
			EffectContext = MakeEffectContext();
		}

		UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();

		if (CanPredict())
		{
			return ApplyGameplayEffectToSelf(GameplayEffect, Level, EffectContext, ScopedPredictionKey);
		}

		return ApplyGameplayEffectToSelf(GameplayEffect, Level, EffectContext);
	}

	return FActiveGameplayEffectHandle();
}

///@brief 给目标ASC施加带预测性质效果的BUFF
FActiveGameplayEffectHandle UGRBAbilitySystemComponent::BP_ApplyGameplayEffectToTargetWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, UAbilitySystemComponent* Target, float Level, FGameplayEffectContextHandle Context)
{
	if (Target == nullptr)
	{
		ABILITY_LOG(Log, TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTargetWithPrediction called with null Target. %s. Context: %s"), *GetFullName(), *Context.ToString());
		return FActiveGameplayEffectHandle();
	}

	if (GameplayEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTargetWithPrediction called with null GameplayEffectClass. %s. Context: %s"), *GetFullName(), *Context.ToString());
		return FActiveGameplayEffectHandle();
	}

	UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();

	if (CanPredict())
	{
		return ApplyGameplayEffectToTarget(GameplayEffect, Target, Level, Context, ScopedPredictionKey);
	}

	return ApplyGameplayEffectToTarget(GameplayEffect, Target, Level, Context);
}


//---------------------------------------------------  ------------------------------------------------
//---------------------------------------------------  ------------------------------------------------
//---------------------------------------------------  ------------------------------------------------
#pragma region ~ 对外接口 ~
/** 获取在1个容器下某个Tag的出现次数*/
int32 UGRBAbilitySystemComponent::K2_GetTagCount(FGameplayTag TagToCheck) const
{
	return GetTagCount(TagToCheck);
}

// Exposes AddLooseGameplayTag to Blueprint. This tag is *not* replicated.
void UGRBAbilitySystemComponent::K2_AddLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	AddLooseGameplayTag(GameplayTag, Count);
}

// Exposes AddLooseGameplayTags to Blueprint. These tags are *not* replicated.
void UGRBAbilitySystemComponent::K2_AddLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	AddLooseGameplayTags(GameplayTags, Count);
}

// Exposes RemoveLooseGameplayTag to Blueprint. This tag is *not* replicated.
void UGRBAbilitySystemComponent::K2_RemoveLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	RemoveLooseGameplayTag(GameplayTag, Count);
}

// Exposes RemoveLooseGameplayTags to Blueprint. These tags are *not* replicated.
void UGRBAbilitySystemComponent::K2_RemoveLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	RemoveLooseGameplayTags(GameplayTags, Count);
}

///@brief 激活对应的Cue
void UGRBAbilitySystemComponent::ExecuteGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::Executed, GameplayCueParameters);
}

///@brief 激活对应的Cue-OnActive
void UGRBAbilitySystemComponent::AddGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::OnActive, GameplayCueParameters);
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::WhileActive, GameplayCueParameters);
}

///@brief 激活对应的Cue-Removed
void UGRBAbilitySystemComponent::RemoveGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::Removed, GameplayCueParameters);
}
#pragma endregion
//---------------------------------------------------  ------------------------------------------------
//---------------------------------------------------  ------------------------------------------------
//---------------------------------------------------  ------------------------------------------------


// ~Start Cutter --------------------------------------------------- 对ASC作用目标多骨骼的蒙太奇动画技术支持 如下-------------------------------------------------- End Cutter~
// ~Start Cutter --------------------------------------------------- 对ASC作用目标多骨骼的蒙太奇动画技术支持 如下-------------------------------------------------- End Cutter~
// ~Start Cutter --------------------------------------------------- 对ASC作用目标多骨骼的蒙太奇动画技术支持 如下-------------------------------------------------- End Cutter~
///--@brief 依赖于技能系统, 播放蒙太奇并着手处理传入技能信息的网络复制和预测--/
float UGRBAbilitySystemComponent::PlayMontageForMesh(UGameplayAbility* InAnimatingAbility, USkeletalMeshComponent* InMesh, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName, bool bReplicateMontage)
{
	// 0.预准备
	UGRBGameplayAbility* InAbility = Cast<UGRBGameplayAbility>(InAnimatingAbility); // 传入的技能
	float Duration = -1.f; // 蒙太奇待操作的时长
	UAnimInstance* ABPInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr; // 传入骨架的动画实例

	if (ABPInstance && NewAnimMontage)
	{
		Duration = ABPInstance->Montage_Play(NewAnimMontage, InPlayRate); // 播放给定蒙太奇,并拿取到播放时长
		if (Duration > 0.f)
		{
			// 提取出 传入骨架的本地蒙太奇信息
			FGameplayAbilityLocalAnimMontageForMesh& LocalAnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);

			// 1.意外情况排除
			if (LocalAnimMontageInfo.LocalMontageInfo.AnimatingAbility.IsValid() && LocalAnimMontageInfo.LocalMontageInfo.AnimatingAbility != InAnimatingAbility) // 当本地蒙太奇的关联技能不匹配传入的GA,则视作是一种意外情况
			{
				// The ability that was previously animating will have already gotten the 'interrupted' callback.
				// It may be a good idea to make this a global policy and 'cancel' the ability.
				// For now, we expect it to end itself when this happens.
			}
			if (NewAnimMontage->HasRootMotion() && ABPInstance->GetOwningActor()) // 且不承认根运动的蒙太奇
			{
				UE_LOG(LogRootMotion, Log, TEXT("UAbilitySystemComponent::PlayMontage %s, Role: %s")
				       , *GetNameSafe(NewAnimMontage)
				       , *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), ABPInstance->GetOwningActor()->GetLocalRole())
				);
			}

			// 2.填充本地蒙太奇Info 的关联GA和关联蒙太奇资产
			LocalAnimMontageInfo.LocalMontageInfo.AnimMontage = NewAnimMontage;
			LocalAnimMontageInfo.LocalMontageInfo.AnimatingAbility = InAnimatingAbility;
			if (InAbility)
			{
				InAbility->SetCurrentMontageForMesh(InMesh, NewAnimMontage);
			}

			// 3.催动骨架ABP跳转到蒙太奇的指定Section
			if (StartSectionName != NAME_None)
			{
				ABPInstance->Montage_JumpToSection(StartSectionName, NewAnimMontage);
			}

			// 4. 再从服务端 刷新骨架的蒙太奇动画复制信息并强力Replicate到客户端; 若是客户端则尝试按照预测Key去绑定1个预测处理机制回调(当预测Key拒绝的时候)
			if (IsOwnerActorAuthoritative())
			{
				if (bReplicateMontage)
				{
					// Those are static parameters, they are only set when the montage is played. They are not changed after that.
					FGameplayAbilityRepAnimMontageForMesh& AbilityRepMontageInfo = GetGameplayAbilityRepAnimMontageForMesh(InMesh);
					AbilityRepMontageInfo.RepMontageInfo.Animation = NewAnimMontage;

					// Update parameters that change during Montage life time.
					AnimMontage_UpdateReplicatedDataForMesh(InMesh);

					// Force net update on our avatar actor
					if (AbilityActorInfo->AvatarActor != nullptr)
					{
						AbilityActorInfo->AvatarActor->ForceNetUpdate();
					}
				}
			}
			else
			{
				// prediction key被拒绝的时候,强制给到一个处理失败的回调(///@brief 当本地正在播放的蒙太奇预测被rejected则立刻停止蒙太奇并淡出0.25秒混合)
				FPredictionKey PredictionKey = UAbilitySystemComponent::GetPredictionKeyForNewAction();
				if (PredictionKey.IsValidKey())
				{
					PredictionKey.NewRejectedDelegate().BindUObject(this, &UGRBAbilitySystemComponent::OnPredictiveMontageRejectedForMesh, InMesh, NewAnimMontage);
				}
			}
		}
	}

	return Duration;
}

///--@brief 在模拟端; 让骨架的动画实例播指定蒙太奇动画,同时将其更新至本地包体的蒙太奇资产--/
float UGRBAbilitySystemComponent::PlayMontageSimulatedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName)
{
	float Duration = -1.f;
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (Duration > 0.f)
		{
			FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
			AnimMontageInfo.LocalMontageInfo.AnimMontage = NewAnimMontage;
		}
	}
	return Duration;
}

///--@brief 停播骨架关联的蒙太奇,--/
void UGRBAbilitySystemComponent::CurrentMontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime)
{
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
	UAnimMontage* MontageToStop = AnimMontageInfo.LocalMontageInfo.AnimMontage;

	bool bShouldStopMontage = AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if (bShouldStopMontage)
	{
		const float BlendOutTime = (OverrideBlendOutTime >= 0.0f ? OverrideBlendOutTime : MontageToStop->BlendOut.GetBlendTime());

		AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);

		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedDataForMesh(InMesh);
		}
	}
}

/** 复位清楚本地动画信息集内的技能和蒙太奇数据.*/
void UGRBAbilitySystemComponent::ClearAnimatingAbilityForAllMeshes(UGameplayAbility* Ability)
{
	UGRBGameplayAbility* GRBAbility = Cast<UGRBGameplayAbility>(Ability);
	for (FGameplayAbilityLocalAnimMontageForMesh& GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		if (GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility == Ability)
		{
			GRBAbility->SetCurrentMontageForMesh(GameplayAbilityLocalAnimMontageForMesh.Mesh, nullptr); // 让GA设置其对应骨架的动画蒙太奇为空
			GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility = nullptr; // 并清掉技能
		}
	}
}

void UGRBAbilitySystemComponent::CurrentMontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName)
{
}

///--@brief 在本地蒙太奇包池子内查找 匹配给定动画技能的那个元素--/
bool UGRBAbilitySystemComponent::IsAnimatingAbilityForAnyMesh(UGameplayAbility* InAbility) const
{
	for (FGameplayAbilityLocalAnimMontageForMesh GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		if (GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility == InAbility)
		{
			return true;
		}
	}
	return false;
}

//---------------------------------------------------  ------------------------------------------------
#pragma region ~ 助手函数 ~
///--@brief 在本地蒙太奇包池子内 查找匹配特定骨架的那个池子元素;--/
FGameplayAbilityLocalAnimMontageForMesh& UGRBAbilitySystemComponent::GetLocalAnimMontageInfoForMesh(USkeletalMeshComponent* InMesh)
{
	for (FGameplayAbilityLocalAnimMontageForMesh& MontageInfo : LocalAnimMontageInfoForMeshes)
	{
		if (MontageInfo.Mesh == InMesh)
		{
			return MontageInfo;
		}
	}
	FGameplayAbilityLocalAnimMontageForMesh MontageInfo = FGameplayAbilityLocalAnimMontageForMesh(InMesh);
	LocalAnimMontageInfoForMeshes.Add(MontageInfo);
	return LocalAnimMontageInfoForMeshes.Last();
}

///--@brief 在联网蒙太奇包池子内 查找匹配特定骨架的那个池子元素;--/
FGameplayAbilityRepAnimMontageForMesh& UGRBAbilitySystemComponent::GetGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh)
{
	for (FGameplayAbilityRepAnimMontageForMesh& RepMontageInfo : RepAnimMontageInfoForMeshes)
	{
		if (RepMontageInfo.Mesh == InMesh)
		{
			return RepMontageInfo;
		}
	}
	FGameplayAbilityRepAnimMontageForMesh RepMontageInfo = FGameplayAbilityRepAnimMontageForMesh(InMesh);
	RepAnimMontageInfoForMeshes.Add(RepMontageInfo);
	return RepAnimMontageInfoForMeshes.Last();
}

///@brief 当本地正在播放的蒙太奇预测被rejected则立刻停止蒙太奇并淡出0.25秒混合
void UGRBAbilitySystemComponent::OnPredictiveMontageRejectedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* PredictiveMontage)
{
	static const float MONTAGE_PREDICTION_REJECT_FADETIME = 0.25f;

	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && PredictiveMontage)
	{
		// If this montage is still playing: kill it
		if (AnimInstance->Montage_IsPlaying(PredictiveMontage))
		{
			AnimInstance->Montage_Stop(MONTAGE_PREDICTION_REJECT_FADETIME, PredictiveMontage);
		}
	}
}

///--@brief 专门在服务端负责处理给定骨架关联包体的数据更新, 并复制到客户端, 并在网络端计算出下个section的数据--/
void UGRBAbilitySystemComponent::AnimMontage_UpdateReplicatedDataForMesh(USkeletalMeshComponent* InMesh)
{
	check(IsOwnerActorAuthoritative());
	AnimMontage_UpdateReplicatedDataForMesh(GetGameplayAbilityRepAnimMontageForMesh(InMesh));
}

///--@brief 专门负责处理给定网络蒙太奇包的数据更新, 并复制到客户端, 并在网络端计算出下个section的数据--/
void UGRBAbilitySystemComponent::AnimMontage_UpdateReplicatedDataForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo)
{
	// 网络蒙太奇包关联的动画实例
	UAnimInstance* AnimInstance = (IsValid(OutRepAnimMontageInfo.Mesh) && OutRepAnimMontageInfo.Mesh->GetOwner() == AbilityActorInfo->AvatarActor)
		                              ? OutRepAnimMontageInfo.Mesh->GetAnimInstance()
		                              : nullptr;
	// 找到关联的本地蒙太奇包
	FGameplayAbilityLocalAnimMontageForMesh& LocalMontagePak = GetLocalAnimMontageInfoForMesh(OutRepAnimMontageInfo.Mesh);

	if (AnimInstance && LocalMontagePak.LocalMontageInfo.AnimMontage)
	{
		// 1.联网蒙太奇包的动画序列 播放速率 位置 混合时长统统依据本地包覆写; 并强行同步到本地客户端
		OutRepAnimMontageInfo.RepMontageInfo.Animation = LocalMontagePak.LocalMontageInfo.AnimMontage;
		bool bIsStopped = AnimInstance->Montage_GetIsStopped(LocalMontagePak.LocalMontageInfo.AnimMontage);
		if (!bIsStopped)
		{
			OutRepAnimMontageInfo.RepMontageInfo.PlayRate = AnimInstance->Montage_GetPlayRate(LocalMontagePak.LocalMontageInfo.AnimMontage);
			OutRepAnimMontageInfo.RepMontageInfo.Position = AnimInstance->Montage_GetPosition(LocalMontagePak.LocalMontageInfo.AnimMontage);
			OutRepAnimMontageInfo.RepMontageInfo.BlendTime = AnimInstance->Montage_GetBlendTime(LocalMontagePak.LocalMontageInfo.AnimMontage);
		}
		if (OutRepAnimMontageInfo.RepMontageInfo.IsStopped != bIsStopped)
		{
			// Set this prior to calling UpdateShouldTick, so we start ticking if we are playing a Montage
			OutRepAnimMontageInfo.RepMontageInfo.IsStopped = bIsStopped;

			// When we start or stop an animation, update the clients right away for the Avatar Actor
			if (AbilityActorInfo->AvatarActor != nullptr)
			{
				AbilityActorInfo->AvatarActor->ForceNetUpdate();
			}

			// 2.更新标志位使之开始重新tick跟踪行为; When this changes, we should update whether or not we should be ticking
			UpdateShouldTick();
		}

		// 3. 依据网络蒙太奇包的section计算本地蒙太奇的下个section并据此刷新网络蒙太奇的section,往前推一步
		// Replicate NextSectionID to keep it in sync.
		// We actually replicate NextSectionID+1 on a BYTE to put INDEX_NONE in there.
		int32 CurrentSectionID = LocalMontagePak.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(OutRepAnimMontageInfo.RepMontageInfo.Position);
		if (CurrentSectionID != INDEX_NONE)
		{
			int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(LocalMontagePak.LocalMontageInfo.AnimMontage, CurrentSectionID);
			if (NextSectionID >= (256 - 1))
			{
				ABILITY_LOG(Error, TEXT("AnimMontage_UpdateReplicatedData. NextSectionID = %d.  RepAnimMontageInfo.Position: %.2f, CurrentSectionID: %d. LocalAnimMontageInfo.AnimMontage %s"),
				            NextSectionID, OutRepAnimMontageInfo.RepMontageInfo.Position, CurrentSectionID, *GetNameSafe(LocalMontagePak.LocalMontageInfo.AnimMontage));
				ensure(NextSectionID < (256 - 1));
			}
			OutRepAnimMontageInfo.RepMontageInfo.NextSectionID = uint8(NextSectionID + 1);
		}
		else
		{
			OutRepAnimMontageInfo.RepMontageInfo.NextSectionID = 0;
		}
	}
}

///--@brief Copy over playing flags for duplicate animation data--/
void UGRBAbilitySystemComponent::AnimMontage_UpdateForcedPlayFlagsForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo)
{
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(OutRepAnimMontageInfo.Mesh);
}

///--@brief 准备好进行同步蒙太奇包的判断函数, 可供子类覆写--/
bool UGRBAbilitySystemComponent::IsReadyForReplicatedMontageForMesh()
{
	/** Children may want to override this for additional checks (e.g, "has skin been applied") */
	return true;
}

///--@brief RPC, 在服务端执行, 设置动画实例下个section并手动校准一些错误section情形且校准时长位置, 并下发至各SimulatedProxy更新包体数据和section数据--/
void UGRBAbilitySystemComponent::ServerCurrentMontageSetNextSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName)
{
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);

	if (AnimInstance)
	{
		UAnimMontage* CurrentAnimMontage = AnimMontageInfo.LocalMontageInfo.AnimMontage;
		if (ClientAnimMontage == CurrentAnimMontage) // 当且仅当本地蒙太奇包的资产匹配给定的客户端蒙太奇资产
		{
			// 在当前动画片段播放完毕后，动态设置下一个要播放的动画片段
			AnimInstance->Montage_SetNextSection(SectionName, NextSectionName, CurrentAnimMontage);

			// 出现section错误的情形,则手动校准, 让蒙太奇去使用客户端位置, 并让动画实例跳转至客户端的时长
			float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
			int32 CurrentSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(CurrentPosition);
			FName CurrentSectionName = CurrentAnimMontage->GetSectionName(CurrentSectionID);
			int32 ClientSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(ClientPosition);
			FName ClientCurrentSectionName = CurrentAnimMontage->GetSectionName(ClientSectionID);
			if ((CurrentSectionName != ClientCurrentSectionName) || (CurrentSectionName != SectionName))
			{
				AnimInstance->Montage_SetPosition(CurrentAnimMontage, ClientPosition);
			}

			// 在服务端负责处理给定骨架关联包体的数据更新, 并复制到客户端, 并在网络端计算出下个section的数据--
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedDataForMesh(InMesh);
			}
		}
	}
}

bool UGRBAbilitySystemComponent::ServerCurrentMontageSetNextSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName)
{
	return true;
}

///--@brief RPC, 在服务端执行, 设置动画实例跳转至给定section, 并下发至各SimulatedProxy更新包体数据和section数据--/
void UGRBAbilitySystemComponent::ServerCurrentMontageJumpToSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName)
{
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);

	if (AnimInstance)
	{
		UAnimMontage* CurrentAnimMontage = AnimMontageInfo.LocalMontageInfo.AnimMontage;
		if (ClientAnimMontage == CurrentAnimMontage)
		{
			// Set NextSectionName
			AnimInstance->Montage_JumpToSection(SectionName, CurrentAnimMontage);

			// Update replicated version for Simulated Proxies if we are on the server.
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedDataForMesh(InMesh);
			}
		}
	}
}

bool UGRBAbilitySystemComponent::ServerCurrentMontageJumpToSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName)
{
	return true;
}

///--@brief RPC, 在服务端执行, 设置动画实例的蒙太奇播放速率, 并下发至各SimulatedProxy更新包体数据和section数据--/
void UGRBAbilitySystemComponent::ServerCurrentMontageSetPlayRateForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate)
{
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& LocalMontagePak = GetLocalAnimMontageInfoForMesh(InMesh);
	if (AnimInstance)
	{
		UAnimMontage* CurrentAnimMontage = LocalMontagePak.LocalMontageInfo.AnimMontage;
		if (ClientAnimMontage == CurrentAnimMontage)
		{
			// Set PlayRate
			AnimInstance->Montage_SetPlayRate(LocalMontagePak.LocalMontageInfo.AnimMontage, InPlayRate);

			// Update replicated version for Simulated Proxies if we are on the server.
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedDataForMesh(InMesh);
			}
		}
	}
}

bool UGRBAbilitySystemComponent::ServerCurrentMontageSetPlayRateForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate)
{
	return true;
}

//--------------------------------------------------- 动画蒙太奇同步重要入口 如下 ------------------------------------------------
///--@brief /** 客户端收到RepMontage包体数据刷新后自动同步至客户端的回调.--/
void UGRBAbilitySystemComponent::OnRep_ReplicatedAnimMontageForMesh()
{
	// 0.轮询同步下来的Rep蒙太奇包体
	for (int32 QueryIndex = 0; QueryIndex < RepAnimMontageInfoForMeshes.Num(); ++QueryIndex)
	{
		// 1.0数据预提取和预处理
		FGameplayAbilityRepAnimMontageForMesh& EachRepMontagePak = RepAnimMontageInfoForMeshes[QueryIndex];
		FGameplayAbilityLocalAnimMontageForMesh& EachLocalMontagePak = GetLocalAnimMontageInfoForMesh(EachRepMontagePak.Mesh);
		UWorld* World = GetWorld();
		if (EachRepMontagePak.RepMontageInfo.bSkipPlayRate) // Skip选项开启的时候 Rep包体会复位播放速率为1
		{
			EachRepMontagePak.RepMontageInfo.PlayRate = 1.f;
		}

		/*
		 * 1.1部分意外情况预处理和初始化数据提取
		 */
		const bool bIsPlayingReplay = World && World->IsPlayingReplay();
		const float MONTAGE_REP_POS_ERR_THRESH = bIsPlayingReplay ? CVarReplayMontageErrorThreshold.GetValueOnGameThread() : 0.1f;
		// rep包体内骨架关联到的动画实例
		UAnimInstance* AnimInstance = IsValid(EachRepMontagePak.Mesh) && EachRepMontagePak.Mesh->GetOwner() == AbilityActorInfo->AvatarActor ? EachRepMontagePak.Mesh->GetAnimInstance() : nullptr;
		// 意外情况处理：没有动画实例或者是在子类内禁用了同步标志位 --> 赋能是否中断蒙太奇包体网络同步
		if (AnimInstance == nullptr || !IsReadyForReplicatedMontageForMesh())
		{
			// We can't handle this yet
			bPendingMontageRep = true;
			return;
		}
		bPendingMontageRep = false;

		/*
		 * 针对在SimulateProxy上的蒙太奇动画处理
		 */
		if (!AbilityActorInfo->IsLocallyControlled())
		{
			// 留下一些调试信息打印rep包体下蒙太奇的信息,诸如速率位置section
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.Montage.Debug"));
			bool DebugMontage = (CVar && CVar->GetValueOnGameThread() == 1);
			if (DebugMontage)
			{
				ABILITY_LOG(Warning, TEXT("\n\nOnRep_ReplicatedAnimMontage, %s"), *GetNameSafe(this));
				ABILITY_LOG(Warning, TEXT("\tAnimMontage: %s\n\tPlayRate: %f\n\tPosition: %f\n\tBlendTime: %f\n\tNextSectionID: %d\n\tIsStopped: %d"),
				            *GetNameSafe(EachRepMontagePak.RepMontageInfo.GetAnimMontage()),
				            EachRepMontagePak.RepMontageInfo.PlayRate,
				            EachRepMontagePak.RepMontageInfo.Position,
				            EachRepMontagePak.RepMontageInfo.BlendTime,
				            EachRepMontagePak.RepMontageInfo.NextSectionID,
				            EachRepMontagePak.RepMontageInfo.IsStopped);
				ABILITY_LOG(Warning, TEXT("\tLocalAnimMontageInfo.AnimMontage: %s\n\tPosition: %f"),
				            *GetNameSafe(EachLocalMontagePak.LocalMontageInfo.AnimMontage), AnimInstance->Montage_GetPosition(EachLocalMontagePak.LocalMontageInfo.AnimMontage));
			}
			// 
			if (EachRepMontagePak.RepMontageInfo.GetAnimMontage() != nullptr)
			{
				// 如果rep包体和本地包体不匹配, 在模拟端播放动画的时候以REP包体的数据为基准
				if ((EachLocalMontagePak.LocalMontageInfo.AnimMontage != EachRepMontagePak.RepMontageInfo.Animation))
				{
					PlayMontageSimulatedForMesh(EachRepMontagePak.Mesh, EachRepMontagePak.RepMontageInfo.GetAnimMontage(), EachRepMontagePak.RepMontageInfo.PlayRate);
				}
				// 如若模拟端没有蒙太奇动画则视作播放失败
				if (EachLocalMontagePak.LocalMontageInfo.AnimMontage == nullptr)
				{
					ABILITY_LOG(Warning, TEXT("OnRep_ReplicatedAnimMontage: PlayMontageSimulated failed. Name: %s, AnimMontage: %s"), *GetNameSafe(this), *GetNameSafe(EachRepMontagePak.RepMontageInfo.GetAnimMontage()));
					return;
				}
				// 修改或者是校准模拟端的蒙太奇播放速率 以rep包体数据为准
				if (AnimInstance->Montage_GetPlayRate(EachLocalMontagePak.LocalMontageInfo.AnimMontage) != EachRepMontagePak.RepMontageInfo.PlayRate)
				{
					AnimInstance->Montage_SetPlayRate(EachLocalMontagePak.LocalMontageInfo.AnimMontage, EachRepMontagePak.RepMontageInfo.PlayRate);
				}

				// Compressed Flags
				const bool bIsLocalStopped = AnimInstance->Montage_GetIsStopped(EachLocalMontagePak.LocalMontageInfo.AnimMontage);
				const bool bIsReplicatedStopped = bool(EachRepMontagePak.RepMontageInfo.IsStopped);
				// Process stopping first, so we don't change sections and cause blending to pop.
				// 模拟端还在播但是远端停了则 停播REP蒙太奇并同步至到客户端
				if (bIsReplicatedStopped)
				{
					if (!bIsLocalStopped)
					{
						CurrentMontageStopForMesh(EachRepMontagePak.Mesh, EachRepMontagePak.RepMontageInfo.BlendTime);
					}
				}
				/// 服务端未停播 且 Rep包体未开启节省带宽
				else if (!EachRepMontagePak.RepMontageInfo.SkipPositionCorrection)
				{
					const int32 RepFakedSectionID_Now = EachLocalMontagePak.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(EachRepMontagePak.RepMontageInfo.Position); // 使用服务器数据 模拟出来的当前SectionID
					const int32 RepRealSectionID_Next = int32(EachRepMontagePak.RepMontageInfo.NextSectionID) - 1; // 服务器真正的 下个SectionID

					/**--@brief; 关于sectionID的处理与调校
					 * 2.0 由于各种原因发生模拟端SectionID和远端的不匹配,则尽可能的使用Rep包体数据来进行校准.
					 */
					if (RepFakedSectionID_Now != INDEX_NONE)
					{
						// And NextSectionID for the replicated SectionID.
						// 使用服务器数据 模拟出来的下一个SectionID
						const int32 LocalFakedSectionID_Next = AnimInstance->Montage_GetNextSectionID(EachLocalMontagePak.LocalMontageInfo.AnimMontage, RepFakedSectionID_Now);
						// 本地真正的SectionID
						const int32 LocalRealSectionID_Now = EachLocalMontagePak.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(AnimInstance->Montage_GetPosition(EachLocalMontagePak.LocalMontageInfo.AnimMontage));

						// 如若本地的下个ID和远端的不匹配, 则强行对模拟端的蒙太奇动画 用复制下来的那个section数据; If NextSectionID is different than the replicated one, then set it.
						if (LocalFakedSectionID_Next != RepRealSectionID_Next)
						{
							AnimInstance->Montage_SetNextSection(EachLocalMontagePak.LocalMontageInfo.AnimMontage->GetSectionName(RepFakedSectionID_Now), EachLocalMontagePak.LocalMontageInfo.AnimMontage->GetSectionName(RepRealSectionID_Next), EachLocalMontagePak.LocalMontageInfo.AnimMontage);
						}
						// 当意外情形比如网络问题导致客户端播放的比服务器过快, 则以服务端模拟出来的校准时刻为基准, 重新调整蒙太奇的播放Section与位置; Make sure we haven't received that update too late and the client hasn't already jumped to another section. 
						if ((LocalRealSectionID_Now != RepFakedSectionID_Now) && (LocalRealSectionID_Now != RepRealSectionID_Next))
						{
							// Client is in a wrong section, teleport him into the begining of the right section
							const float CorrectSectionStartTime = EachLocalMontagePak.LocalMontageInfo.AnimMontage->GetAnimCompositeSection(RepFakedSectionID_Now).GetTime();
							AnimInstance->Montage_SetPosition(EachLocalMontagePak.LocalMontageInfo.AnimMontage, CorrectSectionStartTime);
						}
					}

					/**--@brief; 回溯机制的处理
					 * 3.0 检测双端播放时刻差异并校准重设蒙太奇的权重 事件 notify; 仅承认服务端的时刻为基准, 最终应用到动画实例上
					 */
					// Update Position. If error is too great, jump to replicated position.
					const float LocalRealPos_Now = AnimInstance->Montage_GetPosition(EachLocalMontagePak.LocalMontageInfo.AnimMontage);
					const int32 LocalMatchSectionID_Now = EachLocalMontagePak.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(LocalRealPos_Now);// 本地蒙太奇section
					// 服务端上与模拟端本地蒙太奇播放位置的差异值
					const float DeltaDiff = EachRepMontagePak.RepMontageInfo.Position - LocalRealPos_Now;
					// 如若SectionID双端一致但却出现了 双端差异值大于容错阈值的意外情况
					if ((LocalMatchSectionID_Now == RepFakedSectionID_Now) && (FMath::Abs(DeltaDiff) > MONTAGE_REP_POS_ERR_THRESH) && (EachRepMontagePak.RepMontageInfo.IsStopped == 0))
					{
						// fast forward to server position and trigger notifies
						if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(EachRepMontagePak.RepMontageInfo.GetAnimMontage()))
						{
							// 如果意图上希望回溯, 则需要跳过已经触发过的Notifies
							const float DeltaTime = !FMath::IsNearlyZero(EachRepMontagePak.RepMontageInfo.PlayRate) ? (DeltaDiff / EachRepMontagePak.RepMontageInfo.PlayRate) : 0.f;
							if (DeltaTime >= 0.f)
							{
								MontageInstance->UpdateWeight(DeltaTime);// 通过更新蒙太奇实例的权重，可以实现动画的平滑切换、淡入淡出等效果
								MontageInstance->HandleEvents(LocalRealPos_Now, EachRepMontagePak.RepMontageInfo.Position, nullptr);// 重设各种事件,以服务端的时刻为基准
								AnimInstance->TriggerAnimNotifies(DeltaTime);// 动画实例以deltatime为依据 触发动画通知
							}
						}
						// 最终为了应对意外情形,会把模拟端的本地包体动画播放时刻校准至rep包体远端的蒙太奇播放位置.
						AnimInstance->Montage_SetPosition(EachLocalMontagePak.LocalMontageInfo.AnimMontage, EachRepMontagePak.RepMontageInfo.Position);
					}
				}
			}
		}
	}
}
#pragma endregion ~ 助手函数~
// ~Start Cutter --------------------------------------------------- 对ASC作用目标多骨骼的蒙太奇动画技术支持 如上-------------------------------------------------- End Cutter~
// ~Start Cutter --------------------------------------------------- 对ASC作用目标多骨骼的蒙太奇动画技术支持 如上-------------------------------------------------- End Cutter~
// ~Start Cutter --------------------------------------------------- 对ASC作用目标多骨骼的蒙太奇动画技术支持 如上-------------------------------------------------- End Cutter~
