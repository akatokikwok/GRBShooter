#include "Characters/Abilities/GRBGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Characters/GRBCharacterBase.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Abilities/GRBTargetType_BaseObj.h"
#include "Characters/Heroes/GRBHeroCharacter.h"

UGRBGameplayAbility::UGRBGameplayAbility()
{
	// InstancedPerActor 这种策略适用于那些需要在角色生命周期内保持状态的能力。例如，一个持续时间内增加角色速度的能力可以使用这种策略
	// InstancedPerExecution 适用于那些需要在每次执行时都有独立状态的复杂能力。例如，一个可以同时施放多次且每次都需要独立状态的投掷能力可以使用这种策略
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 用来判断是否Grant的时候就激活技能;
	bActivateAbilityOnGranted = false;

	// 此开关决策绑定输入时不激活GA
	bActivateOnInput = true;

	// 是否当武器装备好再激活SourceObject;
	bSourceObjectMustEqualCurrentWeaponToActivate = false;

	// 允许在交互时激活GA;
	bCannotActivateWhileInteracting = true;

	// ASC有这些状态的时候,本GA发动不了
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.KnockedDown"));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.BlocksInteraction"));

	// 存储人身上是否在交互
	InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");
	InteractingRemovalTag = FGameplayTag::RequestGameplayTag("State.InteractingRemoval");
}

/// @brief 虚方法,当avatarActor初始化绑定键位的时候会被调用
// ~Start Implements UGameplayAbility::OnAvatarSet 
void UGRBGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	if (bActivateAbilityOnGranted)
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle, false);
	}
}

///--@brief 虚接口： 检查技能发动条件--/
bool UGRBGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 禁用交互时刻释放技能的时候是以检查 交互标签次数是否少于移除交互标签次数
	if (bCannotActivateWhileInteracting)
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC->GetTagCount(InteractingTag) > ASC->GetTagCount(InteractingRemovalTag))
		{
			return false;
		}
	}
	// 检查装备武器的时 sourceobject是否匹配
	if (bSourceObjectMustEqualCurrentWeaponToActivate)
	{
		AGRBHeroCharacter* Hero = Cast<AGRBHeroCharacter>(ActorInfo->AvatarActor);
		if (Hero && Hero->GetCurrentWeapon() && reinterpret_cast<UObject*>(Hero->GetCurrentWeapon()) == GetSourceObject(Handle, ActorInfo))
		{
			return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
		}
		else
		{
			return false;
		}
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

///--@brief 虚接口: 检查技能发动消耗成本--/
bool UGRBGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

///--@brief 虚接口: 一个能力被激活时，ApplyCost 方法会实际扣除该能力所需的资源（例如魔法值、能量点等），确保该成本在系统中被正确处理--/
void UGRBGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	GRBApplyCost(Handle, *ActorInfo, ActivationInfo);
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

/// @brief 帮给定的一组目标actor组建targetdata
FGameplayAbilityTargetDataHandle UGRBGameplayAbility::MakeGameplayAbilityTargetDataHandleFromActorArray(const TArray<AActor*> TargetActors)
{
	//---------------------------------------------------  ------------------------------------------------
	// FGameplayAbilityTargetData 是一个抽象基类，不同的派生类可以存储不同类型的目标数据（比如单个目标、多个目标、位置等）
	// FGameplayAbilityTargetData_ActorArray存储一系列TargetActors, 是FGameplayAbilityTargetData的子类;可用于AOE攻击
	// FGameplayAbilityTargetDataHandle 是 Unreal Engine 的 Gameplay Ability System (GAS) 中用于传递目标数据的重要结构体。它广泛用于技能的瞄准、选择和应用目标效果等操作
	//---------------------------------------------------  ------------------------------------------------
	if (TargetActors.Num() > 0)
	{
		FGameplayAbilityTargetData_ActorArray* NewTargetData = new FGameplayAbilityTargetData_ActorArray();
		NewTargetData->TargetActorArray.Append(TargetActors);
		return FGameplayAbilityTargetDataHandle(NewTargetData);
	}
	return FGameplayAbilityTargetDataHandle();
}

/// @brief 给所有的命中hit组建TargetData
FGameplayAbilityTargetDataHandle UGRBGameplayAbility::MakeGameplayAbilityTargetDataHandleFromHitResults(const TArray<FHitResult> HitResults)
{
	FGameplayAbilityTargetDataHandle TargetData;
	for (const FHitResult& HitResult : HitResults)
	{
		FGameplayAbilityTargetData_SingleTargetHit* NewData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);
		TargetData.Add(NewData);
	}
	return TargetData;
}

///--@brief 给定索敌BUFF容器,并依据它的内建信息来生成1个对应的索敌BUFF处理器.--/
FGRBGameplayEffectContainerSpec UGRBGameplayAbility::MakeEffectContainerSpecFromContainer(const FGRBGameplayEffectContainer& Container, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	// 索敌BUFF处理器
	FGRBGameplayEffectContainerSpec ReturnSpec;
	// 预准备数据
	AActor* OwningActor = GetOwningActorFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AGRBCharacterBase* AvatarCharacter = Cast<AGRBCharacterBase>(AvatarActor);

	if (UGRBAbilitySystemComponent* OwningASC = UGRBAbilitySystemComponent::GetAbilitySystemComponentFromActor(OwningActor))
	{
		check(OwningASC);
		// 如果已经有了索敌目标类型则运行蓝图覆写的注册流程; If we have a target type, run the targeting logic. This is optional, targets can be added later
		if (UClass* pGRBTargetType = Container.TargetType.Get())
		{
			check(pGRBTargetType);
			TArray<FHitResult> OutHitResults;
			TArray<AActor*> OutTargetActors;
			TArray<FGameplayAbilityTargetDataHandle> OutTargetDataHandles;
			// 1.先提取索敌BUFF容器内的内建信息
			const UGRBTargetType_BaseObj* TargetTypeCDO = Container.TargetType.GetDefaultObject();
			TargetTypeCDO->GetTargets(AvatarCharacter, AvatarActor, EventData, OutTargetDataHandles, OutHitResults, OutTargetActors);
			// 2.再使用这些内建信息(如actors 射线命中集 索敌句柄) 来填充构建 索敌BUFF处理器
			ReturnSpec.AddTargets(OutTargetDataHandles, OutHitResults, OutTargetActors);
		}

		// 技能等级留一套方案; If we don't have an override level, use the ability level
		if (OverrideGameplayLevel == INDEX_NONE)
		{
			//OverrideGameplayLevel = OwningASC->GetDefaultAbilityLevel();
			OverrideGameplayLevel = GetAbilityLevel();
		}

		// 轮询索敌BUFF容器, 将所有的BUFF实例转化为GE-Handle并最终注册进 索敌BUFF处理器
		for (const TSubclassOf<UGameplayEffect>& EffectClass : Container.TargetGameplayEffectClasses)
		{
			ReturnSpec.TargetGameplayEffectSpecs.Add(MakeOutgoingGameplayEffectSpec(EffectClass, OverrideGameplayLevel));
		}
	}
	return ReturnSpec;
}

///--@brief 查找符合标签的索敌BUFF容器并据此构建1个索敌BUFF处理器.--/
FGRBGameplayEffectContainerSpec UGRBGameplayAbility::MakeEffectContainerSpec(const FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	const FGRBGameplayEffectContainer* FoundContainer = EffectContainerMap.Find(ContainerTag);
	if (FoundContainer != nullptr)
	{
		return MakeEffectContainerSpecFromContainer(*FoundContainer, EventData, OverrideGameplayLevel);
	}
	return FGRBGameplayEffectContainerSpec();
}

///--@brief 轮询指定的索敌BUFF处理器并把每一个BUFF句柄应用到每一个索敌目标句柄上 --/
TArray<FActiveGameplayEffectHandle> UGRBGameplayAbility::ApplyEffectContainerSpec(const FGRBGameplayEffectContainerSpec& ContainerSpec)
{
	TArray<FActiveGameplayEffectHandle> AllEffects;
	// Iterate list of effect specs and apply them to their target data
	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{
		AllEffects.Append(K2_ApplyGameplayEffectSpecToTarget(SpecHandle, ContainerSpec.TargetData));
	}
	return AllEffects;
}

///--@brief 对蓝图暴露GetSourceObject接口--/
UObject* UGRBGameplayAbility::K2_GetSourceObject(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	return GetSourceObject(Handle, &ActorInfo);
}

///--@brief 使用ASC内的优化RPC合批机制; 合批RPC激活技能.--/
bool UGRBGameplayAbility::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately)
{
	if (UGRBAbilitySystemComponent* GRBASC = Cast<UGRBAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		return GRBASC->BatchRPCTryActivateAbility(InAbilityHandle, EndAbilityImmediately);
	}
	return false;
}

///--@brief 旨在与批处理系统合作,从外部结束技能, 类似于K2_EndAbility.--/
void UGRBGameplayAbility::ExternalEndAbility()
{
	check(CurrentActorInfo);

	bool bReplicateEndAbility = true;
	bool bWasCancelled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

///--@brief 返回ASC宿主的预测Key状态--/
FString UGRBGameplayAbility::GetCurrentPredictionKeyStatus()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC->ScopedPredictionKey.ToString() + " is valid for more prediction: " + (ASC->ScopedPredictionKey.IsValidForMorePrediction() ? TEXT("true") : TEXT("false"));
}

///--@brief 检查预测Key是否已发送至服务端--/
bool UGRBGameplayAbility::IsPredictionKeyValidForMorePrediction() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC->ScopedPredictionKey.IsValidForMorePrediction();
}

///--@brief 允许蓝图重载, 检查消耗成本--/
bool UGRBGameplayAbility::GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	return true;
}

///--@brief 允许蓝图重载, 应用成本扣除--/
void UGRBGameplayAbility::GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
}

void UGRBGameplayAbility::SetHUDReticle(TSubclassOf<UGRBHUDReticle> ReticleClass)
{
	
}

void UGRBGameplayAbility::ResetHUDReticle()
{
	
}

///--@brief 在多人游戏内的带预测的客户端内 往服务器发送索敌数据--/
void UGRBGameplayAbility::SendTargetDataToServer(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (IsPredictingClient())
	{
		UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
		check(ASC);

		FScopedPredictionWindow ScopedPrediction(ASC, IsPredictingClient());

		FGameplayTag ApplicationTag; // Fixme: where would this be useful?
		CurrentActorInfo->AbilitySystemComponent->CallServerSetReplicatedTargetData(CurrentSpecHandle,
		                                                                            CurrentActivationInfo.GetActivationPredictionKey(), TargetData, ApplicationTag, ASC->ScopedPredictionKey);
	}
}

///--@brief 核验技能是否被按键激活的(未松开).--/
bool UGRBGameplayAbility::IsInputPressed() const
{
	FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	return Spec && Spec->InputPressed;
}

//--------------------------------------------------- ~ 动画与蒙太奇相关 ~ ------------------------------------------------
//---------------------------------------------------  ------------------------------------------------
#pragma region ~ 动画与蒙太奇相关 ~

///--@brief 从GRB蒙太奇池子里提取出对应骨架关联的蒙太奇资产--/
UAnimMontage* UGRBGameplayAbility::GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh)
{
	FAbilityMeshMontageGRB AbilityMeshMontage;
	if (FindAbillityMeshMontage(InMesh, AbilityMeshMontage))
	{
		return AbilityMeshMontage.Montage;
	}
	return nullptr;
}

///--@brief 查找GRB蒙太奇池子内符合对应骨架的蒙太奇,并替换它; 查找失败则手动为此骨架注册进池子.--/
void UGRBGameplayAbility::SetCurrentMontageForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated());// 确保此技能一定已经被实例化

	FAbilityMeshMontageGRB AbilityMeshMontage;
	if (FindAbillityMeshMontage(InMesh, AbilityMeshMontage))
	{
		AbilityMeshMontage.Montage = InCurrentMontage;
	}
	else
	{
		CurrentAbilityMeshMontages.Add(FAbilityMeshMontageGRB(InMesh, InCurrentMontage));
	}
}

///--@brief 从GRB蒙太奇池子里提取对应骨架的元素--/
bool UGRBGameplayAbility::FindAbillityMeshMontage(const USkeletalMeshComponent* InMesh, FAbilityMeshMontageGRB& InAbilityMeshMontage)
{
	for (FAbilityMeshMontageGRB& MeshMontage : CurrentAbilityMeshMontages)
	{
		if (MeshMontage.Mesh == InMesh)
		{
			InAbilityMeshMontage = MeshMontage;
			return true;
		}
	}
	return false;
}

///--@brief --/
void UGRBGameplayAbility::MontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName)
{
	check(CurrentActorInfo);

	UGRBAbilitySystemComponent* const AbilitySystemComponent = Cast<UGRBAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo_Checked());
	if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
	{
		AbilitySystemComponent->CurrentMontageJumpToSectionForMesh(InMesh, SectionName);
	}
}

///--@brief --/
void UGRBGameplayAbility::MontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, FName FromSectionName, FName ToSectionName)
{
	check(CurrentActorInfo);

	UGRBAbilitySystemComponent* const AbilitySystemComponent = Cast<UGRBAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo_Checked());
	if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
	{
		AbilitySystemComponent->CurrentMontageSetNextSectionNameForMesh(InMesh, FromSectionName, ToSectionName);
	}
}

///--@brief --/
void UGRBGameplayAbility::MontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime)
{
	check(CurrentActorInfo);

	UGRBAbilitySystemComponent* const AbilitySystemComponent = Cast<UGRBAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get());
	if (AbilitySystemComponent != nullptr)
	{
		// We should only stop the current montage if we are the animating ability
		if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
		{
			AbilitySystemComponent->CurrentMontageStopForMesh(InMesh, OverrideBlendOutTime);
		}
	}
}

///--@brief --/
void UGRBGameplayAbility::MontageStopForAllMeshes(float OverrideBlendOutTime)
{
	check(CurrentActorInfo);

	UGRBAbilitySystemComponent* const AbilitySystemComponent = Cast<UGRBAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get());
	if (AbilitySystemComponent != nullptr)
	{
		if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
		{
			AbilitySystemComponent->StopAllCurrentMontages(OverrideBlendOutTime);
		}
	}
}
#pragma endregion
