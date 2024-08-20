#include "Characters/Abilities/GRBGameplayAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GRBBlueprintFunctionLibrary.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Characters/GRBCharacterBase.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Abilities/GRBGATA_LineTrace.h"
#include "Characters/Abilities/GRBTargetType_BaseObj.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_PlayMontageForMeshAndWaitForEvent.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_ServerWaitForClientTargetData.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_WaitDelayOneFrame.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_WaitTargetDataUsingActor.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/GRBPlayerController.h"
#include "Weapons/GRBWeapon.h"

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
	/** 这里需要额外注意; GRBCheckCost检测机制会在子类覆写*/
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && GRBCheckCost(Handle, *ActorInfo);
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
	// FGameplayAbilityTargetDataHandle 是 Unreal Engine 的 Gameplay Ability System (GRB) 中用于传递目标数据的重要结构体。它广泛用于技能的瞄准、选择和应用目标效果等操作
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
	// to do
}

void UGRBGameplayAbility::SetHUDReticle(TSubclassOf<UGRBHUDReticle> ReticleClass)
{
	AGRBPlayerController* PC = Cast<AGRBPlayerController>(CurrentActorInfo->PlayerController);
	if (PC)
	{
		PC->SetHUDReticle(ReticleClass);
	}
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
///--@brief 从GRB技能池子里提取骨架关联的蒙太奇资产.--/
UAnimMontage* UGRBGameplayAbility::GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh)
{
	FAbilityMeshMontageGRB AbilityMeshMontagePak;
	if (bool QuerySuccess = FindAbillityMeshMontage(InMesh, AbilityMeshMontagePak))
	{
		return AbilityMeshMontagePak.Montage;
	}
	return nullptr;
}

///--@brief 设置GRB技能池子内关联骨架的蒙太奇资产, 如果查找失败则手动构建一组注册到技能池子内--/
void UGRBGameplayAbility::SetCurrentMontageForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated()); // 确保此技能一定已经被实例化

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

///--@brief 在GRB池子内找关联特定骨架的池子元素--/
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

///--@brief 检查本技能是否隶属于ASC的池子元素(已激活的), 并跳转蒙太奇到给定section且维持双端统一.--/
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


#pragma region ~ 副开火技能 ~
///--@brief 构造器--/
UGA_GRBRiflePrimaryInstant::UGA_GRBRiflePrimaryInstant()
{
	/** 配置业务数据*/
	mAmmoCost = 1; // 单回合射击消耗的弹量 1
	mAimingSpreadMod = 0.15; // 扩散调幅 == 0.15
	mBulletDamage = 10.0f; // 单发伤害为10
	mFiringSpreadIncrement = 1.2; // 扩散增幅 == 1.2
	mFiringSpreadMax = 10; // 扩散阈值 == 10
	mTimeOfLastShot = -99999999.0f; // 上次的射击时刻
	mWeaponSpread = 1.0f; // 武器扩散基准系数
	mTraceFromPlayerViewPointg = true; // 启用从视角摄像机追踪射线
	mAimingTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.Aiming"));
	mAimingRemovealTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.AimingRemoval"));

	/** 配置技能输入ID*/
	UGRBGameplayAbility::AbilityInputID = EGRBAbilityInputID::None;
	UGRBGameplayAbility::AbilityID = EGRBAbilityInputID::PrimaryFire;

	/** 配置和技能架构相关联的参数*/
	bActivateAbilityOnGranted = false; // 禁用授权好技能后自动激活
	bActivateOnInput = true; // 启用当检测到外部键鼠输入,自动激活技能
	bSourceObjectMustEqualCurrentWeaponToActivate = true; // 启用当装备好武器后应用SourceObject
	bCannotActivateWhileInteracting = true; // 启用当交互时阻止激活技能

	/** 配置带来的射击伤害BUFF*/
	UClass* pClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/Rifle/GE_RifleDamage.GE_RifleDamage_C'"));
	if (pClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("pClass name:%s"), *pClass->GetName());
		UClass* const DamagedBuffClass = pClass;
		if (DamagedBuffClass)
		{
			FGameplayTag InducedTag = FGameplayTag::RequestGameplayTag(FName("Ability"));
			FGRBGameplayEffectContainer GRBGEContainerToApply = FGRBGameplayEffectContainer();
			GRBGEContainerToApply.TargetType = nullptr;
			GRBGEContainerToApply.TargetGameplayEffectClasses.AddUnique(TSubclassOf<UGameplayEffect>(DamagedBuffClass));
			TTuple<FGameplayTag, FGRBGameplayEffectContainer> MakedPair(InducedTag, GRBGEContainerToApply);
			EffectContainerMap.Add(MakedPair);
		}
	}

	/** 配置诸标签打断关系*/
	FGameplayTagContainer TagContainer_AbilityTags;
	FGameplayTagContainer TagContainer_CancelAbilities;
	FGameplayTagContainer TagContainer_BlockAbilities;
	FGameplayTagContainer TagContainer_ActivationOwnedTags;
	FGameplayTagContainer TagContainer_ActivationBlockedTags;
	TagContainer_AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Primary.Instant")));
	TagContainer_CancelAbilities.AddTag(FGameplayTag::EmptyTag);
	TagContainer_BlockAbilities.AddTag(FGameplayTag::EmptyTag);
	TagContainer_ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.BlocksInteraction")));
	TagContainer_ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Weapon.IsFiring")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.KnockedDown")));
	AbilityTags = TagContainer_AbilityTags;
	CancelAbilitiesWithTag = TagContainer_CancelAbilities;
	BlockAbilitiesWithTag = TagContainer_BlockAbilities;
	ActivationOwnedTags = TagContainer_ActivationOwnedTags;
	ActivationBlockedTags = TagContainer_ActivationBlockedTags;
}

///--@brief 激活副开火技能--/
void UGA_GRBRiflePrimaryInstant::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	/** 触发技能时候的数据预准备*/
	CheckAndSetupCacheables();

	// 重制当前场景探查器的弹道扩散参数状态; 持续射击不会复位扩散
	// Reset CurrentSpread back to 0 on Activate. Continuous fire does not reset spread.
	mLineTraceTargetActor->ResetSpread();

	/** 勒令服务器等待复制的 技能目标数据并在服务端ASC预热好索敌时刻后广播绑定好的业务事件(在这里是HandleTargetData)。客户端只需从任务的 Activate() 返回即可。如果玩家是服务器（主机），则这不会执行任何操作，因为 TargetData 从未被 RPC 处理，子弹将通过 FireBullet 事件处理。*/
	// Tell the Server to start waiting for replicated TargetData. Client simply returns from the Task's Activate(). If the player is the Server (Host), this won't do anything since the TargetData is never RPC'd and the bullet will be handled through the FireBullet event.
	bool bTriggerOnce = false;
	UGRBAT_ServerWaitForClientTargetData* pAsyncTask = UGRBAT_ServerWaitForClientTargetData::ServerWaitForClientTargetData(this, FName("None"), bTriggerOnce);
	pAsyncTask->ValidDataDelegate.AddUniqueDynamic(this, &UGA_GRBRiflePrimaryInstant::HandleTargetData);
	pAsyncTask->ReadyForActivation();
	mServerWaitTargetDataTask = pAsyncTask;

	/** 客户端即刻进行调度综合射击业务*/
	// Client immediately fires
	FireBullet();
}

///--@brief 检查发动技能是否成功--/
bool UGA_GRBRiflePrimaryInstant::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);

	// 确保多次激活副开火技能的时刻差要始终大于键鼠输入的射击间隔; 这样玩家就不能通过快速点击来非法绕过
	// Make sure time between activations is more than time between shots so the player can't just click really fast to skirt around the time between shots.
	if (IsValid(mGAPrimary))
	{
		const float& DeltaMinus = UGameplayStatics::GetTimeSeconds(this) - mTimeOfLastShot;
		if (FMath::Abs(DeltaMinus) >= mGAPrimary->Getm_TimeBetweenShot())
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (!IsValid(mGAPrimary))
	{
		return true;
	}
	return true;
}

///--@brief 结束技能; 清理工作--/
void UGA_GRBRiflePrimaryInstant::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsValid(mServerWaitTargetDataTask))
	{
		mServerWaitTargetDataTask->EndTask();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

///--@brief 负担技能消耗的检查--/
bool UGA_GRBRiflePrimaryInstant::GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	// return Super::GRBCheckCost_Implementation(Handle, ActorInfo);

	// 仅承认副开火的技能SourceObject载体是挂载在枪支上的.且残余载弹量要满足单发消耗
	if (AGRBWeapon* pGRBWeapon = Cast<AGRBWeapon>(GetSourceObject(Handle, &ActorInfo)))
	{
		if ((pGRBWeapon->GetPrimaryClipAmmo() >= mAmmoCost || pGRBWeapon->HasInfiniteAmmo()) && Super::GRBCheckCost_Implementation(Handle, ActorInfo))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	return true;
}

///--@brief 技能消耗成本扣除: 刷新残余载弹量扣除每回合发动时候的弹药消耗量--/
void UGA_GRBRiflePrimaryInstant::GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::GRBApplyCost_Implementation(Handle, ActorInfo, ActivationInfo);

	if (AGRBWeapon* pGRBWeapon = Cast<AGRBWeapon>(GetSourceObject(Handle, &ActorInfo)))
	{
		if (!pGRBWeapon->HasInfiniteAmmo())
		{
			pGRBWeapon->SetPrimaryClipAmmo(pGRBWeapon->GetPrimaryClipAmmo() - mAmmoCost);
		}
	}
}

///--@brief 手动终止技能以及异步任务--/
void UGA_GRBRiflePrimaryInstant::ManuallyKillInstantGA()
{
	K2_EndAbility();
}

///--@brief 综合射击业务--/
void UGA_GRBRiflePrimaryInstant::FireBullet()
{
	// 仅承认在主控端构建技能目标数据
	if (GetActorInfo().PlayerController->IsLocalPlayerController())
	{
		// 确保键鼠按下频率要大于配置好的射击间隔，否则触发失败
		// Only fire a bullet if not on cooldown (enough time has passed >= to TimeBetweenShots from managing Ability)
		if (FMath::Abs(UGameplayStatics::GetTimeSeconds(this) - mTimeOfLastShot) >= mGAPrimary->Getm_TimeBetweenShot() - 0.01)
		{
			if (UGameplayAbility::CheckCost(CurrentSpecHandle, CurrentActorInfo))
			{
				/** 组织并构建可复用的射线类型 场景探查器*/
				if (IsValid(mSourceWeapon))
				{
					// 仅在第一视角下
					if (mOwningHero->IsInFirstPersonPerspective())
					{
						TEnumAsByte<EGameplayAbilityTargetingLocationType::Type> LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;
						FTransform LiteralTransform = FTransform();
						TObjectPtr<AActor> SourceActor = TObjectPtr<AActor>();
						TObjectPtr<UMeshComponent> SourceComponent = TObjectPtr<UMeshComponent>(Weapon1PMesh);
						TObjectPtr<UGameplayAbility> SourceAbility = nullptr;
						FName SourceSocketName = FName("MuzzleFlashSocket");

						FGameplayAbilityTargetingLocationInfo pLocationInfo = FGameplayAbilityTargetingLocationInfo();
						pLocationInfo.LocationType = LocationType;
						pLocationInfo.LiteralTransform = LiteralTransform;
						pLocationInfo.SourceActor = SourceActor;
						pLocationInfo.SourceComponent = SourceComponent;
						pLocationInfo.SourceAbility = SourceAbility;
						pLocationInfo.SourceSocketName = SourceSocketName;


						mTraceStartLocation = pLocationInfo;
						mTraceFromPlayerViewPointg = true;
						mLineTraceTargetActor->Configure(mTraceStartLocation, mAimingTag, mAimingRemovealTag,
						                                 FCollisionProfileName(FName("Projectile")), FGameplayTargetDataFilterHandle(), nullptr,
						                                 FWorldReticleParameters(), false, false,
						                                 false, false, true,
						                                 mTraceFromPlayerViewPointg, true, 99999999.0f,
						                                 mWeaponSpread, mAimingSpreadMod, mFiringSpreadIncrement,
						                                 mFiringSpreadMax, 1, 1
						);

						/** RPC 探查器的技能目标数据到服务器; 并绑定好预热索敌的回调 HandleTargetData;*/
						UGRBAT_WaitTargetDataUsingActor* AsyncTaskNode = UGRBAT_WaitTargetDataUsingActor::WaitTargetDataWithReusableActor(this, FName("None"), EGameplayTargetingConfirmation::Instant, mLineTraceTargetActor, true);
						AsyncTaskNode->ValidDataDelegate.AddUniqueDynamic(this, &UGA_GRBRiflePrimaryInstant::HandleTargetData); // Handle shot locally - predict hit impact FX or apply damage if player is Host
						AsyncTaskNode->ReadyForActivation();

						/** 刷新上次计时*/
						mTimeOfLastShot = UGameplayStatics::GetTimeSeconds(this);
					}
					else
					{
					}
				}
			}
			else if (!UGameplayAbility::CheckCost(CurrentSpecHandle, CurrentActorInfo)) // 负担不起消耗就打断副开火
			{
				UGameplayAbility::CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false);
			}
		}
		else
		{
			// 键鼠操作频率过高,射击过快会进入异常情况
			UE_LOG(LogTemp, Warning, TEXT("Error: Tried to fire a bullet too fast"))
		}
	}
}

///--@brief 双端都会调度到的 处理技能目标数据的复合逻辑入口--/
/**
 * 播放蒙太奇动画
 * 伤害BUFF应用
 * 播放诸如关联枪支火焰CueTag的所有特效
 */
void UGA_GRBRiflePrimaryInstant::HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle)
{
	/**
	 * 播放蒙太奇动画
	 * 伤害BUFF应用
	 * 播放诸如关联枪支火焰CueTag的所有特效
	 */
	bool BroadcastCommitEvent = false;
	if (UGameplayAbility::K2_CommitAbilityCost(BroadcastCommitEvent))
	{
		// 播放项目定制的蒙太奇.
		PlayFireMontage();

		// Functionally equivalent. Container path does have an insignificant couple more function calls.
		FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/Rifle/GE_RifleDamage.GE_RifleDamage_C'"));
		UClass* pBP_RifleDamageGE = UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(SoftObjectPaths_Actor1);
		if (pBP_RifleDamageGE)
		{
			const FGameplayEffectSpecHandle& RifleDamageGESpecHandle = UGameplayAbility::MakeOutgoingGameplayEffectSpec(pBP_RifleDamageGE, 1);
			const FGameplayTag& CauseTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
			const float& Magnitude = mBulletDamage;
			const FGameplayEffectSpecHandle& TheBuffToApply = UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(RifleDamageGESpecHandle, CauseTag, Magnitude);

			const TArray<FActiveGameplayEffectHandle>& ActiveGEHandles = K2_ApplyGameplayEffectSpecToTarget(TheBuffToApply, InTargetDataHandle);

			const FGameplayEffectContextHandle& ContextHandle = UAbilitySystemBlueprintLibrary::GetEffectContext(TheBuffToApply);

			bool Reset = false;
			const FHitResult& HitResultApply = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(InTargetDataHandle, 0);
			UAbilitySystemBlueprintLibrary::EffectContextAddHitResult(ContextHandle, HitResultApply, Reset);

			const FGameplayTag& CueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.Rifle.Fire"));
			const FGameplayCueParameters CueParameters = UAbilitySystemBlueprintLibrary::MakeGameplayCueParameters(0, 0, ContextHandle,
			                                                                                                       FGameplayTag::EmptyTag, FGameplayTag::EmptyTag, FGameplayTagContainer(),
			                                                                                                       FGameplayTagContainer(), FVector::ZeroVector, FVector::ZeroVector,
			                                                                                                       nullptr, nullptr, nullptr,
			                                                                                                       nullptr, 1, 1,
			                                                                                                       nullptr, false);
			UGameplayAbility::K2_ExecuteGameplayCueWithParams(CueTag, CueParameters);
		}
	}
	else
	{
		// 负担不起技能消耗就暂时打断技能.
		UGameplayAbility::K2_CancelAbility();
	}
}

///--@brief 播放项目定制的蒙太奇.--/
void UGA_GRBRiflePrimaryInstant::PlayFireMontage()
{
	if (GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(mAimingTag) && !GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(mAimingRemovealTag))
	{
		if (UAnimMontage* pMontageAsset = LoadObject<UAnimMontage>(mOwningHero, TEXT("/Script/Engine.AnimMontage'/Game/ShooterGame/Animations/FPP_Animations/FPP_RifleAimFire_Montage.FPP_RifleAimFire_Montage'")))
		{
			UGRBAT_PlayMontageForMeshAndWaitForEvent* Node = UGRBAT_PlayMontageForMeshAndWaitForEvent::PlayMontageForMeshAndWaitForEvent(
				this,
				FName("None"),
				mOwningHero->GetFirstPersonMesh(),
				pMontageAsset,
				FGameplayTagContainer(),
				1,
				FName("None"),
				false,
				1,
				false,
				-1,
				-1
			);
			Node->ReadyForActivation();
		}
	}
	else
	{
		if (UAnimMontage* pMontageAsset = LoadObject<UAnimMontage>(mOwningHero, TEXT("/Script/Engine.AnimMontage'/Game/ShooterGame/Animations/FPP_Animations/HerroFPP_RifleFire_Montage.HerroFPP_RifleFire_Montage'")))
		{
			UGRBAT_PlayMontageForMeshAndWaitForEvent* Node = UGRBAT_PlayMontageForMeshAndWaitForEvent::PlayMontageForMeshAndWaitForEvent(
				this,
				FName("None"),
				mOwningHero->GetFirstPersonMesh(),
				pMontageAsset,
				FGameplayTagContainer(),
				1,
				FName("None"),
				false,
				1,
				false,
				-1,
				-1
			);
			Node->ReadyForActivation();
		}
	}
}

///--@brief 触发技能时候的数据预准备--/
void UGA_GRBRiflePrimaryInstant::CheckAndSetupCacheables()
{
	if (!IsValid(mSourceWeapon))
	{
		mSourceWeapon = Cast<AGRBWeapon>(GetCurrentSourceObject());
	}
	if (!IsValid(Weapon1PMesh))
	{
		Weapon1PMesh = mSourceWeapon->GetWeaponMesh1P();
	}
	if (!IsValid(Weapon3PMesh))
	{
		Weapon3PMesh = mSourceWeapon->GetWeaponMesh3P();
	}
	if (!IsValid(mOwningHero))
	{
		mOwningHero = Cast<AGRBHeroCharacter>(GetAvatarActorFromActorInfo());
	}
	if (!IsValid(mGAPrimary))
	{
		FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/Rifle/BP_GA_RifleContinouslyShoot.BP_GA_RifleContinouslyShoot_C'"));
		UClass* pBPClassTmplate = UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(SoftObjectPaths_Actor1);
		if (pBPClassTmplate)
		{
			mGAPrimary = Cast<UGA_GRBRiflePrimary>(UGRBBlueprintFunctionLibrary::GetPrimaryAbilityInstanceFromClass(GetAbilitySystemComponentFromActorInfo(), pBPClassTmplate));
		}
	}
	if (!IsValid(mLineTraceTargetActor))
	{
		mLineTraceTargetActor = mSourceWeapon->GetLineTraceTargetActor();
	}
}
#pragma endregion


#pragma region ~ 主开火技能 ~
///--@brief 构造器--/
UGA_GRBRiflePrimary::UGA_GRBRiflePrimary()
{
	// 业务变量设置
	m_TimeBetweenShot = 0.1f; // 射击间隔为0.1秒
	m_AmmoCost = 1; // 技能消耗一次1颗弹药

	// 设置技能输入类型为:主动开火
	UGRBGameplayAbility::AbilityInputID = EGRBAbilityInputID::PrimaryFire;
	UGRBGameplayAbility::AbilityID = EGRBAbilityInputID::PrimaryFire;

	// 设置一些技能的基础配置
	bActivateAbilityOnGranted = false; // 禁用授权好技能后自动激活
	bActivateOnInput = true; // 启用当检测到外部键鼠输入,自动激活技能
	bSourceObjectMustEqualCurrentWeaponToActivate = true; // 启用当装备好武器后应用SourceObject
	bCannotActivateWhileInteracting = true; // 启用当交互时阻止激活技能

	// 设置诸多标签关系
	FGameplayTagContainer TagContainer_AbilityTags;
	FGameplayTagContainer TagContainer_CancelAbilities;
	FGameplayTagContainer TagContainer_BlockAbilities;
	FGameplayTagContainer TagContainer_ActivationOwnedTags;
	FGameplayTagContainer TagContainer_ActivationBlockedTags;
	TagContainer_AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Primary")));
	TagContainer_CancelAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Reload")));
	TagContainer_BlockAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Sprint")));
	TagContainer_BlockAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging")));
	TagContainer_ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.BlocksInteraction")));
	TagContainer_ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.CancelsSprint")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.KnockedDown")));
	AbilityTags = TagContainer_AbilityTags;
	CancelAbilitiesWithTag = TagContainer_CancelAbilities;
	BlockAbilitiesWithTag = TagContainer_BlockAbilities;
	ActivationOwnedTags = TagContainer_ActivationOwnedTags;
	ActivationBlockedTags = TagContainer_ActivationBlockedTags;
}

///--@brief ActivateAbility--/
void UGA_GRBRiflePrimary::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	/** 几个业务数据预处理*/
	CheckAndSetupCacheables();

	/** 按开火模式执行射击业务*/
	if (GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Sprinting"))))
	{
		// 若之前人物正在冲刺; 则延时0.03秒触发按模式开火; 因为要预留一小段时长给冲刺动画的淡出
		UAbilityTask_WaitDelay* const AsyncNodeTask_WaitDelay = UAbilityTask_WaitDelay::WaitDelay(this, 0.03f);
		AsyncNodeTask_WaitDelay->OnFinish.AddUniqueDynamic(this, &UGA_GRBRiflePrimary::ExecuteShootBussByFireMode);
		AsyncNodeTask_WaitDelay->ReadyForActivation();
	}
	else
	{
		// 除开冲刺运动,会下一帧执行开火业务(SetTimerForNextTick)
		UGRBAT_WaitDelayOneFrame* const AsyncNode_WaitDelayOneFrame = UGRBAT_WaitDelayOneFrame::WaitDelayOneFrame(this);
		AsyncNode_WaitDelayOneFrame->GetOnFinishDelegate().AddUniqueDynamic(this, &UGA_GRBRiflePrimary::ExecuteShootBussByFireMode);
		AsyncNode_WaitDelayOneFrame->ReadyForActivation();
	}
}

///--@brief 激活条件校验--/
bool UGA_GRBRiflePrimary::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	bool Result = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
	return Result;
}

///--@brief 结束技能清理业务--/
void UGA_GRBRiflePrimary::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	AsyncWaitDelayNode_ContinousShoot = nullptr;
	m_InstantAbility = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

///--@brief 专门做的技能消耗检查; 载弹量不足以支撑本回合射击的情形,会主动激活Reload技能--/
bool UGA_GRBRiflePrimary::GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	if (AGRBWeapon* const pGRBWeapon = Cast<AGRBWeapon>(GetSourceObject(Handle, &ActorInfo)))
	{
		// 残余载弹量必须大于单回合射击数
		bool CorrectCond1 = (pGRBWeapon->GetPrimaryClipAmmo() >= m_AmmoCost || pGRBWeapon->HasInfiniteAmmo());
		bool CorrectCond2 = Super::GRBCheckCost_Implementation(Handle, ActorInfo) && CorrectCond1;
		// 载弹量不足以支撑本回合射击的情形,会主动激活Reload技能
		if (!CorrectCond2)
		{
			if (!UGRBBlueprintFunctionLibrary::IsPrimaryAbilityInstanceActive(ActorInfo.AbilitySystemComponent.Get(), Handle))
			{
				FGameplayTagContainer pTagContainer;
				pTagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Reload")));
				bool bAllowRemoteActivation = true;
				bool TryActivateGAResult = ActorInfo.AbilitySystemComponent.Get()->TryActivateAbilitiesByTag(pTagContainer, bAllowRemoteActivation);
				return false;
			}
		}
	}
	return true;
}

///--@brief 业务数据预处理--/
void UGA_GRBRiflePrimary::CheckAndSetupCacheables()
{
	// 设置武器为发动此技能的SourceObjectActor
	if (!IsValid(m_SourceWeapon))
	{
		if (AGRBWeapon* const pCastGRBWeapon = Cast<AGRBWeapon>(GetCurrentSourceObject()))
		{
			m_SourceWeapon = pCastGRBWeapon;
		}
	}

	// 设置副开火技能句柄
	if (!UGRBBlueprintFunctionLibrary::IsAbilitySpecHandleValid(m_InstantAbilityHandle))
	{
		if (UGRBAbilitySystemComponent* const pASC_HeroPlayerState = Cast<UGRBAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo())) // 注意这里的ASC是挂载在PlayerState上的
		{
			FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/Rifle/BP_GA_RifleContinouslyShootInstant.BP_GA_RifleContinouslyShootInstant_C'"));
			UClass* pBPClassTmplate = UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(SoftObjectPaths_Actor1);
			if (pBPClassTmplate)
			{
				UE_LOG(LogTemp, Warning, TEXT("pClass_YouWant Name:%s"), *pBPClassTmplate->GetName());
				const TSubclassOf<UGameplayAbility> pParamSubclass = pBPClassTmplate;
				m_InstantAbilityHandle = pASC_HeroPlayerState->FindAbilitySpecHandleForClass(pParamSubclass, m_SourceWeapon); // 查找满足以下条件的技能句柄; 1.SourceObject是当前武器 2.符合指定的蓝图BP_GA_RifleContinouslyShootInstant_C
			}
		}
	}

	// 利用上一步设置好的副开火句柄,转化为副开火技能
	if (!IsValid((m_InstantAbility)))
	{
		UGRBGameplayAbility* const pResultGameplayAbility = UGRBBlueprintFunctionLibrary::GetPrimaryAbilityInstanceFromHandle(GetAbilitySystemComponentFromActorInfo(), m_InstantAbilityHandle);
		if (UGA_GRBRiflePrimaryInstant* const pGA_PrimaryInstant = Cast<UGA_GRBRiflePrimaryInstant>(pResultGameplayAbility))
		{
			m_InstantAbility = pGA_PrimaryInstant;
		}
	}
}

///--@brief 按开火模式执行开火业务--/
void UGA_GRBRiflePrimary::ExecuteShootBussByFireMode()
{
	if (IsValid(m_SourceWeapon))
	{
		if (m_SourceWeapon->FireMode == FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.FireMode.SemiAuto")))
		{
			/** 在半自动模式下 合批激活副开火技能句柄并立刻杀掉它; 接着主动终止主开火技能*/
			bool bEndAbilityImmediately = true;
			bool Result = BatchRPCTryActivateAbility(m_InstantAbilityHandle, bEndAbilityImmediately);
			if (Result || !Result)
			{
				bool bReplicateEndAbility = true;
				bool bWasCancelled = false;
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
		}
		else if (m_SourceWeapon->FireMode == FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.FireMode.FullAuto")))
		{
			/** 在全自动开火模式下 合批激活副开火技能; 合批失败则会杀掉主开火技能*/
			bool bEndAbilityImmediately = false;
			bool Result = BatchRPCTryActivateAbility(m_InstantAbilityHandle, bEndAbilityImmediately);
			if (Result)
			{
				// 激活异步节点:用于检测玩家键鼠输入松开,等待触发松开回调
				UAbilityTask_WaitInputRelease* const AsyncWaitInputReleaseNode = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
				AsyncWaitInputReleaseNode->OnRelease.AddUniqueDynamic(this, &UGA_GRBRiflePrimary::OnReleaseBussCallback);
				AsyncWaitInputReleaseNode->ReadyForActivation();

				// 持续射击入口; 按射击间隔处理循环射击业务
				AsyncWaitDelayNode_ContinousShoot = UAbilityTask_WaitDelay::WaitDelay(this, m_TimeBetweenShot);
				AsyncWaitDelayNode_ContinousShoot->OnFinish.AddUniqueDynamic(this, &UGA_GRBRiflePrimary::ContinuouslyFireOneBulletCallback);
				AsyncWaitDelayNode_ContinousShoot->ReadyForActivation();
			}
			else
			{
				bool bReplicateEndAbility = true;
				bool bWasCancelled = false;
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
		}
		else if (m_SourceWeapon->FireMode == FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.FireMode.Burst")))
		{
			/** 爆炸开火模式下; 主动激活副开火技能且不中断它; 接入异步节点UAbilityTask_Repeat*/
			/** !!! 注意这里的机制射击; 爆炸射击(以三连发为例), 这回合的第1发是走的半自动的第一波合批, 第2发和第3发才是走的下下一波合批*/
			bool bEndAbilityImmediately = false;
			bool Result = BatchRPCTryActivateAbility(m_InstantAbilityHandle, bEndAbilityImmediately);
			if (Result)
			{
				// First bullet is in batch with ActivateAbility, delay for bullets 2 and 3
				/** !!! 注意这里的机制射击; 爆炸射击(以三连发为例), 这回合的第1发是走的半自动的第一波合批, 第2发和第3发才是走的下下一波合批*/
				UAbilityTask_WaitDelay* const AsyncWaitDelayNode = UAbilityTask_WaitDelay::WaitDelay(this, m_TimeBetweenShot);
				AsyncWaitDelayNode->OnFinish.AddUniqueDynamic(this, &UGA_GRBRiflePrimary::PerRoundFireBurstBulletsCallback);
				AsyncWaitDelayNode->ReadyForActivation();
			}
			else
			{
				bool bReplicateEndAbility = true;
				bool bWasCancelled = false;
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
		}
		else
		{
			/** 未配置枪支开火模式则主动终止射击主技能*/
			bool bReplicateEndAbility = true;
			bool bWasCancelled = false;
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
		}
	}
}

///--@brief 玩家松开键鼠输入后的回调业务--/
void UGA_GRBRiflePrimary::OnReleaseBussCallback(float InPayload_TimeHeld)
{
	UE_LOG(LogTemp, Log, TEXT("停止射击子弹"));

	m_InstantAbility->ManuallyKillInstantGA();
	K2_EndAbility();
}

///--@brief 循环射击回调入口; 会反复递归异步任务UAbilityTask_WaitDelay--/
void UGA_GRBRiflePrimary::ContinuouslyFireOneBulletCallback()
{
	if (UGameplayAbility::CheckCost(CurrentSpecHandle, CurrentActorInfo))
	{
		m_InstantAbility->FireBullet();
		UE_LOG(LogTemp, Log, TEXT("射了一发子弹"));
		AsyncWaitDelayNode_ContinousShoot->OnFinish.RemoveAll(this);
		AsyncWaitDelayNode_ContinousShoot->EndTask();
		AsyncWaitDelayNode_ContinousShoot = nullptr;
		AsyncWaitDelayNode_ContinousShoot = UAbilityTask_WaitDelay::WaitDelay(this, m_TimeBetweenShot);
		AsyncWaitDelayNode_ContinousShoot->OnFinish.AddUniqueDynamic(this, &UGA_GRBRiflePrimary::ContinuouslyFireOneBulletCallback);
		AsyncWaitDelayNode_ContinousShoot->ReadyForActivation();
	}
	else
	{
		OnReleaseBussCallback(-1);
	}
}

///--@brief 弃用--/
void UGA_GRBRiflePrimary::ContinuouslyFireOneBulletCallbackV2()
{
	ContinuouslyFireOneBulletCallback();
}

///--@brief 爆炸开火模式下 除开首发合批的后两次合批走的业务--/
///--@brief 注意这里的机制射击; 爆炸射击(以三连发为例), 这回合的第1发是走的半自动的第一波合批, 第2发和第3发才是走的下下一波合批--/
void UGA_GRBRiflePrimary::PerRoundFireBurstBulletsCallback()
{
	/** !!! 注意这里的机制射击; 爆炸射击(以三连发为例), 这回合的第1发是走的半自动的第一波合批, 第2发和第3发才是走的下下一波合批*/
	// 所以ActionCount设置为了2而不是三连发的3
	UAbilityTask_Repeat* const AsyncNode_Repeat = UAbilityTask_Repeat::RepeatAction(this, m_TimeBetweenShot, 2);
	AsyncNode_Repeat->OnPerformAction.AddUniqueDynamic(this, &UGA_GRBRiflePrimary::PerTimeBurstShootBussCallback); // 单次做的业务
	AsyncNode_Repeat->OnFinished.AddUniqueDynamic(this, &UGA_GRBRiflePrimary::OnFinishBurstShootBussCallback); // 总共2发结束后的业务
	AsyncNode_Repeat->ReadyForActivation();
}

///--@brief 爆炸开火多次Repeat情形下的单次业务--/
void UGA_GRBRiflePrimary::PerTimeBurstShootBussCallback(int32 PerformActionNumber)
{
	if (CheckCost(CurrentSpecHandle, CurrentActorInfo))
	{
		// Repeat2次,每次会调度副开火技能的射击子弹
		m_InstantAbility->FireBullet();
	}
	else
	{
		// 检查消耗不满足的话(如载弹量不够),则主动杀掉副开火技能
		m_InstantAbility->ManuallyKillInstantGA();
		K2_EndAbility();
	}
}

///--@brief 爆炸开火多次Repeat结束后的业务--/
void UGA_GRBRiflePrimary::OnFinishBurstShootBussCallback(int32 ActionNumber)
{
	// 在每回合Burst开火后主动杀掉副开火技能
	m_InstantAbility->ManuallyKillInstantGA();
	K2_EndAbility();
}

#pragma endregion
