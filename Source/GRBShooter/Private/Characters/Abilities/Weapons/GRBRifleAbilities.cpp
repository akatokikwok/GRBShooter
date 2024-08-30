// Copyright 2024 Dan Kestranek.

#include "Characters/Abilities/Weapons/GRBRifleAbilities.h"
#include "GRBShooter/GRBShooter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "Characters/Abilities/GRBTargetType_BaseObj.h"
#include "Characters/GRBCharacterBase.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "GameplayTagContainer.h"
#include "GRBBlueprintFunctionLibrary.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Camera/CameraComponent.h"
#include "Characters/GRBCharacterMovementComponent.h"
#include "Characters/Abilities/GRBGATA_LineTrace.h"
#include "Characters/Abilities/GRBGATA_SphereTrace.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_PlayMontageForMeshAndWaitForEvent.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_ServerWaitForClientTargetData.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_WaitChangeFOV.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_WaitDelayOneFrame.h"
#include "Characters/Abilities/AbilityTasks/GRBAT_WaitTargetDataUsingActor.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Player/GRBPlayerController.h"
#include "Weapons/GRBProjectile.h"
#include "Weapons/GRBWeapon.h"
#include "UI/GRBHUDReticle.h"


#pragma region ~ 步枪开火技能 ~
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

	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	K2_EndAbility();
}

///--@brief 综合射击业务--/
void UGA_GRBRiflePrimaryInstant::FireBullet()
{
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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

						//---------------------------------------------------  ------------------------------------------------
						//---------------------------------------------------  ------------------------------------------------

						/**
						 * 可复用的异步等候技能目标数据 选中/反选 的 任务节点
						 * WaitTargetDataUsingActor管控 三部分重要概念
						 *  1.场景探查器 TraceActor 2. 节点外部绑定业务 ValidDataDelegate/CancelledDelegate 3.选中目标的模式 Instant/UserConfirmed/CustomMulti
						 * 
						 * 异步节点Activate的时候
						 * TargetActor->TargetDataReadyDelegate-探查器已确认目标事件              <=> Callback: 仅本地预测 把技能目标数据RPC到服务器 接着会广播异步节点外部绑好的确认目标逻辑(HandleTargetData)
						 * TargetActor->CanceledDelegate-探查器反选目标事件                       <=> Callback: 仅本地预测 RPC到服务器勒令反选目标 接着会广播异步节点外绑好的取消目标逻辑
						 *
						 * 
						 * 非本机客户端 这个端的ASC确认目标->AbilityTargetDataSetDelegate          <=> Callback: 剔除意外情况非主机端未收到目标数据,自动反选; 否则正常广播外部的选中业务
						 * 非本机客户端 这个端的ASC反选目标->AbilityTargetDataCancelledDelegate    <=> Callback: 广播异步节点外绑好的取消目标逻辑
						 * 非本机客户端会 等待远程目标数据任务流 SetWaitingOnRemotePlayerData()
						 *
						 *  TargetActor->StartTargeting: 开始探查器的探查准备工作并开启tick
						 *  接下来按 目标选中模式 ConfirmationType == Instant亦或是UserConfirmed来决策探查器的后续制作目标句柄的复杂业务
						 *  假若是瞬发的Instant 选择模式
						 *		间接触发多态的 AGRBGATA_Trace::ConfirmTargetingAndContinue()
						 * 		执行探查器的核心业务; 三步走
						 * 		执行探查器PerformTrace-- TArray<FHitResult> HitResults = PerformTrace(SourceActor);
						 * 		为一组命中hit制作 目标数据句柄 并存储它们-- FGameplayAbilityTargetDataHandle Handle = MakeTargetData(HitResults);
						 * 		为探查器的 "ready"事件广播; 并传入组好的payload 目标数据句柄-- AGameplayAbilityTargetActor::TargetDataReadyDelegate.Broadcast(Handle);
						 *  假若是UserConfirmed多阶段选择模式
						 *		TargetActor->BindToConfirmCancelInputs()
						 */

						//---------------------------------------------------  ------------------------------------------------
						//---------------------------------------------------  ------------------------------------------------

						/**
						 * 官方注释明确说每次都重新生成TargetActor是低效的，复用才是王道;
						 * GARBhooter为了实现复用做了以下修改定制版本的探查器 AGRBGATA_Trace
						 * 1.实现休眠状态
						 *		增加StopTargeting方法，与StartTargeting形成一对控制Actor休眠状态的函数，相当于反初始化，为的是进入休眠状态，以备下次使用。
						 * 2.避免Actor被销毁
						 *		重写CancelTargeting——避免取消操作时被销毁
						 *		构造函数中，bDestroyOnConfirmation = false——避免确认目标时被销毁
						 *
						 */


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
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;
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
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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

//---------------------------------------------------  ------------------------------------------------
//---------------------------------------------------  ------------------------------------------------

///--@brief 构造器--/
UGA_GRBRiflePrimary::UGA_GRBRiflePrimary()
{
	// 本地预测：（Local Predicted）本地预测是指技能将立即在客户端执行，执行过程中不会询问服务端的正确与否，但是同时服务端也起着决定性的作用，如果服务端的技能执行失败了，客户端的技能也会随之停止并“回滚”。如果服务端和客户端执行的结果不矛盾，客户端会执行的非常流畅无延时。如果技能需要实时响应，可以考虑用这种预测模式。下文会着重介绍这个策略。
	// 仅限本地：（Local Only）仅在本地客户端运行的技能。这个策略不适用于专用服务器游戏，本地客户端执行了该技能，服务端任然没有被执行，在有些情况下可能出现拉扯情况，原因在于服务端技能没有被执行，技能中的某个状态和客户端不一致。
	// 服务器启动：（Server Initiated）技能将在服务器上启动并PRC到客户端也执行。可以更准确地复制服务器上实际发生的情况，但是客户端会因缺少本地预测而发生短暂延迟。这种延迟相对来说是比较低的，但是相对于Local Predicted来说还是会牺牲一点流畅性。之前我遇到一种情况，一个没有预测的技能A，和一个带有预测的技能B，A执行了后B不能执行， 现在同时执行技能A和技能B， 由于A需要等待服务器端做验证，B是本地预测技能所以B的客户端会瞬间执行，导致B的客户端被执行过了，服务端失败，出现了一些不可预料的问题了，这种情况需要将技能B的网络策略修改为Server Initiated，这样就会以服务端为权威运行，虽然会牺牲点延迟，但是也是可以在游戏接收的范围内，有时候需要在此权衡。
	// 仅限服务器：（Server Only）技能将在只在服务器上运行，客户端不会。服务端的技能被执行后，技能修改的任何变量都将被复制，并会将状态传递给客户端。缺点是比较服务端的每个影响都会由延迟同步到客户端端，Server Initiated只在运行时候会有一点滞后。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;


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

	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

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
