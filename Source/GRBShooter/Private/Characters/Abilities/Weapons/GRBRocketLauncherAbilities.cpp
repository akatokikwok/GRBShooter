#include "Characters/Abilities/Weapons/GRBRocketLauncherAbilities.h"
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


#pragma region ~ 火箭筒开火 ~
UGA_GRBRocketLauncherPrimaryInstant::UGA_GRBRocketLauncherPrimaryInstant()
{
	// 本地预测：（Local Predicted）本地预测是指技能将立即在客户端执行，执行过程中不会询问服务端的正确与否，但是同时服务端也起着决定性的作用，如果服务端的技能执行失败了，客户端的技能也会随之停止并“回滚”。如果服务端和客户端执行的结果不矛盾，客户端会执行的非常流畅无延时。如果技能需要实时响应，可以考虑用这种预测模式。下文会着重介绍这个策略。
	// 仅限本地：（Local Only）仅在本地客户端运行的技能。这个策略不适用于专用服务器游戏，本地客户端执行了该技能，服务端任然没有被执行，在有些情况下可能出现拉扯情况，原因在于服务端技能没有被执行，技能中的某个状态和客户端不一致。
	// 服务器启动：（Server Initiated）技能将在服务器上启动并PRC到客户端也执行。可以更准确地复制服务器上实际发生的情况，但是客户端会因缺少本地预测而发生短暂延迟。这种延迟相对来说是比较低的，但是相对于Local Predicted来说还是会牺牲一点流畅性。之前我遇到一种情况，一个没有预测的技能A，和一个带有预测的技能B，A执行了后B不能执行， 现在同时执行技能A和技能B， 由于A需要等待服务器端做验证，B是本地预测技能所以B的客户端会瞬间执行，导致B的客户端被执行过了，服务端失败，出现了一些不可预料的问题了，这种情况需要将技能B的网络策略修改为Server Initiated，这样就会以服务端为权威运行，虽然会牺牲点延迟，但是也是可以在游戏接收的范围内，有时候需要在此权衡。
	// 仅限服务器：（Server Only）技能将在只在服务器上运行，客户端不会。服务端的技能被执行后，技能修改的任何变量都将被复制，并会将状态传递给客户端。缺点是比较服务端的每个影响都会由延迟同步到客户端端，Server Initiated只在运行时候会有一点滞后。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	/** 配置业务数据*/
	mAmmoCost = 1; // 单回合射击消耗的弹量 1
	mRocketDamage = 60.0f; // 单发伤害为60.f
	mTimeOfLastShot = 0.0f; // 上次的射击时刻
	mTraceFromPlayerViewPointg = false; // 禁用从视角摄像机追踪射线

	/** 配置技能输入ID*/
	UGRBGameplayAbility::AbilityInputID = EGRBAbilityInputID::None;
	UGRBGameplayAbility::AbilityID = EGRBAbilityInputID::PrimaryFire;

	/** 配置和技能架构相关联的参数*/
	bActivateAbilityOnGranted = false; // 禁用授权好技能后自动激活
	bActivateOnInput = true; // 启用当检测到外部键鼠输入,自动激活技能
	bSourceObjectMustEqualCurrentWeaponToActivate = true; // 启用当装备好武器后应用SourceObject
	bCannotActivateWhileInteracting = true; // 启用当交互时阻止激活技能

	/** 配置带来的火箭筒炮弹伤害BUFF*/
	UClass* pClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/GE_RocketLauncherDamage.GE_RocketLauncherDamage_C'"));
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
	TagContainer_AbilityTags.AddTag(FGameplayTag::EmptyTag);
	TagContainer_CancelAbilities.AddTag(FGameplayTag::EmptyTag);
	TagContainer_BlockAbilities.AddTag(FGameplayTag::EmptyTag);
	TagContainer_ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.BlocksInteraction")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.KnockedDown")));
	AbilityTags = TagContainer_AbilityTags;
	CancelAbilitiesWithTag = TagContainer_CancelAbilities;
	BlockAbilitiesWithTag = TagContainer_BlockAbilities;
	ActivationOwnedTags = TagContainer_ActivationOwnedTags;
	ActivationBlockedTags = TagContainer_ActivationBlockedTags;
}

void UGA_GRBRocketLauncherPrimaryInstant::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	/** 触发技能时候的数据预准备*/
	CheckAndSetupCacheables();

	// 重制当前场景探查器的弹道扩散参数状态; 持续射击不会复位扩散
	// Reset CurrentSpread back to 0 on Activate. Continuous fire does not reset spread.
	// mLineTraceTargetActor->ResetSpread();

	/** 勒令服务器等待复制的 技能目标数据并在服务端ASC预热好索敌时刻后广播绑定好的业务事件(在这里是HandleTargetData)。客户端只需从任务的 Activate() 返回即可。如果玩家是服务器（主机），则这不会执行任何操作，因为 TargetData 从未被 RPC 处理，子弹将通过 FireBullet 事件处理。*/
	// Tell the Server to start waiting for replicated TargetData. Client simply returns from the Task's Activate(). If the player is the Server (Host), this won't do anything since the TargetData is never RPC'd and the bullet will be handled through the FireBullet event.
	bool bTriggerOnce = false;
	UGRBAT_ServerWaitForClientTargetData* pAsyncTask = UGRBAT_ServerWaitForClientTargetData::ServerWaitForClientTargetData(this, FName("None"), bTriggerOnce);
	pAsyncTask->ValidDataDelegate.AddUniqueDynamic(this, &UGA_GRBRocketLauncherPrimaryInstant::HandleTargetData);
	pAsyncTask->ReadyForActivation();
	mServerWaitTargetDataTask = pAsyncTask;

	/** 客户端即刻进行调度综合射击业务*/
	// Client immediately fires
	FireRocket();
}

bool UGA_GRBRocketLauncherPrimaryInstant::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 确保多次激活副开火技能的时刻差要始终大于键鼠输入的射击间隔; 这样玩家就不能通过快速点击来非法绕过
	// Make sure time between activations is more than time between shots so the player can't just click really fast to skirt around the time between shots.
	if (IsValid(mGAPrimary))
	{
		const float& DeltaMinus = UGameplayStatics::GetTimeSeconds(this) - mTimeOfLastShot;
		return FMath::Abs(DeltaMinus) >= mGAPrimary->Getm_TimeBetweenShot();
	}
	else if (!IsValid(mGAPrimary))
	{
		return true;
	}
	return true;
}

void UGA_GRBRocketLauncherPrimaryInstant::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsValid(mServerWaitTargetDataTask))
	{
		mServerWaitTargetDataTask->EndTask();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_GRBRocketLauncherPrimaryInstant::GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
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

void UGA_GRBRocketLauncherPrimaryInstant::GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
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

void UGA_GRBRocketLauncherPrimaryInstant::FireRocket()
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
					TEnumAsByte<EGameplayAbilityTargetingLocationType::Type> LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;
					FTransform LiteralTransform = FTransform();
					TObjectPtr<AActor> SourceActor = TObjectPtr<AActor>();
					TObjectPtr<UGameplayAbility> SourceAbility = nullptr;
					FName SourceSocketName = FName("MuzzleFlashSocket");
					FGameplayAbilityTargetingLocationInfo pLocationInfo = FGameplayAbilityTargetingLocationInfo();
					pLocationInfo.LocationType = LocationType;
					pLocationInfo.LiteralTransform = LiteralTransform;
					pLocationInfo.SourceActor = SourceActor;
					pLocationInfo.SourceAbility = SourceAbility;
					pLocationInfo.SourceSocketName = SourceSocketName;

					if (mOwningHero->IsInFirstPersonPerspective()) // 仅在第一视角下
					{
						pLocationInfo.SourceComponent = TObjectPtr<UMeshComponent>(Weapon1PMesh);
						mTraceStartLocation = pLocationInfo;
						mTraceFromPlayerViewPointg = true;
					}
					else if (!mOwningHero->IsInFirstPersonPerspective()) // 仅在第三视角下
					{
						pLocationInfo.SourceComponent = TObjectPtr<UMeshComponent>(Weapon3PMesh);
						mTraceStartLocation = pLocationInfo;
						mTraceFromPlayerViewPointg = false;
					}

					mLineTraceTargetActor->Configure(mTraceStartLocation, FGameplayTag::EmptyTag, FGameplayTag::EmptyTag,
					                                 FCollisionProfileName(FName("Projectile")), FGameplayTargetDataFilterHandle(), nullptr,
					                                 FWorldReticleParameters(), false, false,
					                                 false, false, true,
					                                 mTraceFromPlayerViewPointg, false, 99999999.0f,
					                                 0, 0, 0,
					                                 0, 1, 1
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
					AsyncTaskNode->ValidDataDelegate.AddUniqueDynamic(this, &UGA_GRBRocketLauncherPrimaryInstant::HandleTargetData); // Handle shot locally - predict hit impact FX or apply damage if player is Host
					AsyncTaskNode->ReadyForActivation();

					/** 刷新上次计时*/
					mTimeOfLastShot = UGameplayStatics::GetTimeSeconds(this);
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

void UGA_GRBRocketLauncherPrimaryInstant::ManuallyKillInstantGA()
{
	K2_EndAbility();
}

void UGA_GRBRocketLauncherPrimaryInstant::HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle)
{
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	const FString& ScopedPredictionKeyMessage = UGRBGameplayAbility::GetCurrentPredictionKeyStatus();
	UE_LOG(LogTemp, Warning, TEXT("Warning: HandleTargetData, RocketLauncher, Message: {%s}"), *ScopedPredictionKeyMessage);

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

		//
		const FGRBGameplayEffectContainerSpec& GRBGEContainerSpecPak = UGRBGameplayAbility::MakeEffectContainerSpec(FGameplayTag::RequestGameplayTag(FName("Ability")), FGameplayEventData(), -1);
		const FGameplayEffectSpecHandle& RocketLauncherDamageBuffHandle = GRBGEContainerSpecPak.TargetGameplayEffectSpecs.IsValidIndex(0) ? GRBGEContainerSpecPak.TargetGameplayEffectSpecs[0] : FGameplayEffectSpecHandle();
		//
		const FGameplayTag& CauseTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
		const float& Magnitude = mRocketDamage;
		const FGameplayEffectSpecHandle& TheBuffToApply = UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(RocketLauncherDamageBuffHandle, CauseTag, Magnitude);
		//
		const FHitResult& TraceHit = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(InTargetDataHandle, 0);
		const FRotator& LookAtRotation = UKismetMathLibrary::FindLookAtRotation(TraceHit.TraceStart, TraceHit.Location);
		//
		if (mOwningHero->HasAuthority())
		{
			FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/BP_RocketLauncherProjectile.BP_RocketLauncherProjectile_C'"));
			UClass* GRBProjectileBP = UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(SoftObjectPaths_Actor1);
			const FVector& SpawnLoc = TraceHit.TraceStart;
			const FRotator& SpawnRot = LookAtRotation;
			FActorSpawnParameters PActorSpawnParameters;
			PActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			PActorSpawnParameters.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
			FTransform PSpawnTrans;
			PSpawnTrans.SetLocation(SpawnLoc);
			PSpawnTrans.SetRotation(FQuat(SpawnRot));
			AGRBProjectile* const SpawnedGRBProjectile = mOwningHero->GetWorld()->SpawnActorDeferred<AGRBProjectile>(GRBProjectileBP, PSpawnTrans, mOwningHero, mOwningHero, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, ESpawnActorScaleMethod::OverrideRootScale);
			SpawnedGRBProjectile->FinishSpawning(PSpawnTrans);
		}

		if (mOwningHero->IsLocallyControlled())
		{
			const FGameplayTag& CueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.RocketLauncher.Fire"));
			const FGameplayEffectContextHandle& EmptyEffectContextHandle = FGameplayEffectContextHandle();
			const FVector& CueLocation = TraceHit.Location;
			const FVector& CueNormal = UKismetMathLibrary::GetForwardVector(LookAtRotation);
			const AActor* CueInstigator = mOwningHero;
			const FGameplayCueParameters& CueParameters = UAbilitySystemBlueprintLibrary::MakeGameplayCueParameters(0, 0, EmptyEffectContextHandle,
			                                                                                                        FGameplayTag::EmptyTag, FGameplayTag::EmptyTag, FGameplayTagContainer(),
			                                                                                                        FGameplayTagContainer(), CueLocation, CueNormal,
			                                                                                                        const_cast<AActor*>(CueInstigator), nullptr, nullptr,
			                                                                                                        nullptr, 1, 1,
			                                                                                                        nullptr, false);
			if (UGRBAbilitySystemComponent* const GRBASC = Cast<UGRBAbilitySystemComponent>(this->GetAbilitySystemComponentFromActorInfo()))
			{
				GRBASC->ExecuteGameplayCueLocal(CueTag, CueParameters);
			}
		}
		// mTimeOfLastShot = UGameplayStatics::GetTimeSeconds(this);

		// FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/Rifle/GE_RifleDamage.GE_RifleDamage_C'"));
		// UClass* pBP_RifleDamageGE = UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(SoftObjectPaths_Actor1);
		// if (pBP_RifleDamageGE)
		// {
		// 	const FGameplayEffectSpecHandle& RifleDamageGESpecHandle = UGameplayAbility::MakeOutgoingGameplayEffectSpec(pBP_RifleDamageGE, 1);
		//
		// 	const TArray<FActiveGameplayEffectHandle>& ActiveGEHandles = K2_ApplyGameplayEffectSpecToTarget(TheBuffToApply, InTargetDataHandle);
		//
		// 	const FGameplayEffectContextHandle& ContextHandle = UAbilitySystemBlueprintLibrary::GetEffectContext(TheBuffToApply);
		//
		// 	bool Reset = false;
		// 	const FHitResult& HitResultApply = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(InTargetDataHandle, 0);
		// 	UAbilitySystemBlueprintLibrary::EffectContextAddHitResult(ContextHandle, HitResultApply, Reset);
		//
		// 	const FGameplayTag& CueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.RocketLauncher.Fire"));
		// 	const FGameplayCueParameters CueParameters = UAbilitySystemBlueprintLibrary::MakeGameplayCueParameters(0, 0, ContextHandle,
		// 	                                                                                                       FGameplayTag::EmptyTag, FGameplayTag::EmptyTag, FGameplayTagContainer(),
		// 	                                                                                                       FGameplayTagContainer(), FVector::ZeroVector, FVector::ZeroVector,
		// 	                                                                                                       nullptr, nullptr, nullptr,
		// 	                                                                                                       nullptr, 1, 1,
		// 	                                                                                                       nullptr, false);
		// 	UGameplayAbility::K2_ExecuteGameplayCueWithParams(CueTag, CueParameters);
		// }
	}
	else
	{
		// 负担不起技能消耗就暂时打断技能.
		UGameplayAbility::K2_CancelAbility();
	}
}

void UGA_GRBRocketLauncherPrimaryInstant::PlayFireMontage()
{
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	if (GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Weapon.RocketLauncher.Aiming"))) && !GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Weapon.RocketLauncher.AimingRemoval"))))
	{
		if (UAnimMontage* pMontageAsset = LoadObject<UAnimMontage>(mOwningHero, TEXT("/Script/Engine.AnimMontage'/Game/ShooterGame/Animations/FPP_Animations/FPP_LauncherAimFire_Montage.FPP_LauncherAimFire_Montage'")))
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
		if (UAnimMontage* pMontageAsset = LoadObject<UAnimMontage>(mOwningHero, TEXT("/Script/Engine.AnimMontage'/Game/ShooterGame/Animations/FPP_Animations/HeroFPP_LauncherFire_Montage.HeroFPP_LauncherFire_Montage'")))
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

void UGA_GRBRocketLauncherPrimaryInstant::CheckAndSetupCacheables()
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
		FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/BP_GA_RocketLauncherShoot.BP_GA_RocketLauncherShoot_C'"));
		UClass* pBPClassTmplate = UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(SoftObjectPaths_Actor1);
		if (pBPClassTmplate)
		{
			mGAPrimary = Cast<UGA_GRBRocketLauncherPrimary>(UGRBBlueprintFunctionLibrary::GetPrimaryAbilityInstanceFromClass(GetAbilitySystemComponentFromActorInfo(), pBPClassTmplate));
		}
	}
	if (!IsValid(mLineTraceTargetActor))
	{
		mLineTraceTargetActor = mSourceWeapon->GetLineTraceTargetActor();
	}
}

UGA_GRBRocketLauncherPrimary::UGA_GRBRocketLauncherPrimary()
{
	// 本地预测：（Local Predicted）本地预测是指技能将立即在客户端执行，执行过程中不会询问服务端的正确与否，但是同时服务端也起着决定性的作用，如果服务端的技能执行失败了，客户端的技能也会随之停止并“回滚”。如果服务端和客户端执行的结果不矛盾，客户端会执行的非常流畅无延时。如果技能需要实时响应，可以考虑用这种预测模式。下文会着重介绍这个策略。
	// 仅限本地：（Local Only）仅在本地客户端运行的技能。这个策略不适用于专用服务器游戏，本地客户端执行了该技能，服务端任然没有被执行，在有些情况下可能出现拉扯情况，原因在于服务端技能没有被执行，技能中的某个状态和客户端不一致。
	// 服务器启动：（Server Initiated）技能将在服务器上启动并PRC到客户端也执行。可以更准确地复制服务器上实际发生的情况，但是客户端会因缺少本地预测而发生短暂延迟。这种延迟相对来说是比较低的，但是相对于Local Predicted来说还是会牺牲一点流畅性。之前我遇到一种情况，一个没有预测的技能A，和一个带有预测的技能B，A执行了后B不能执行， 现在同时执行技能A和技能B， 由于A需要等待服务器端做验证，B是本地预测技能所以B的客户端会瞬间执行，导致B的客户端被执行过了，服务端失败，出现了一些不可预料的问题了，这种情况需要将技能B的网络策略修改为Server Initiated，这样就会以服务端为权威运行，虽然会牺牲点延迟，但是也是可以在游戏接收的范围内，有时候需要在此权衡。
	// 仅限服务器：（Server Only）技能将在只在服务器上运行，客户端不会。服务端的技能被执行后，技能修改的任何变量都将被复制，并会将状态传递给客户端。缺点是比较服务端的每个影响都会由延迟同步到客户端端，Server Initiated只在运行时候会有一点滞后。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 业务变量设置
	m_TimeBetweenShot = 0.3f; // 火箭筒射击间隔为0.3秒
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

void UGA_GRBRocketLauncherPrimary::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	/** 几个业务数据预处理*/
	CheckAndSetupCacheables();

	/** 按开火模式执行射击业务*/
	if (GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Sprinting"))))
	{
		// 若之前人物正在冲刺; 则延时0.03秒触发按模式开火; 因为要预留一小段时长给冲刺动画的淡出
		UAbilityTask_WaitDelay* const AsyncNodeTask_WaitDelay = UAbilityTask_WaitDelay::WaitDelay(this, 0.03f);
		AsyncNodeTask_WaitDelay->OnFinish.AddUniqueDynamic(this, &UGA_GRBRocketLauncherPrimary::ExecuteShootBussByFireMode);
		AsyncNodeTask_WaitDelay->ReadyForActivation();
	}
	else
	{
		// 除开冲刺运动,会下一帧执行开火业务(SetTimerForNextTick)
		UGRBAT_WaitDelayOneFrame* const AsyncNode_WaitDelayOneFrame = UGRBAT_WaitDelayOneFrame::WaitDelayOneFrame(this);
		AsyncNode_WaitDelayOneFrame->GetOnFinishDelegate().AddUniqueDynamic(this, &UGA_GRBRocketLauncherPrimary::ExecuteShootBussByFireMode);
		AsyncNode_WaitDelayOneFrame->ReadyForActivation();
	}
}

bool UGA_GRBRocketLauncherPrimary::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGA_GRBRocketLauncherPrimary::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_GRBRocketLauncherPrimary::GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
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

void UGA_GRBRocketLauncherPrimary::CheckAndSetupCacheables()
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
			FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/BP_GA_RocketLauncherShootInstant.BP_GA_RocketLauncherShootInstant_C'"));
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
		if (UGA_GRBRocketLauncherPrimaryInstant* const pGA_PrimaryInstant = Cast<UGA_GRBRocketLauncherPrimaryInstant>(pResultGameplayAbility))
		{
			m_InstantAbility = pGA_PrimaryInstant;
		}
	}
}

void UGA_GRBRocketLauncherPrimary::ExecuteShootBussByFireMode()
{
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	/** 在半自动模式下 合批激活副开火技能句柄并立刻杀掉它; 接着主动终止主开火技能*/
	bool bEndAbilityImmediately = true;
	bool Result = BatchRPCTryActivateAbility(m_InstantAbilityHandle, bEndAbilityImmediately);
	if (Result || !Result)
	{
		K2_EndAbility();
	}
}
#pragma endregion


#pragma region ~ 火箭筒右键索敌并等待开火 ~
UGA_GRBRocketLauncherSecondary::UGA_GRBRocketLauncherSecondary()
{
	// 本地预测：（Local Predicted）本地预测是指技能将立即在客户端执行，执行过程中不会询问服务端的正确与否，但是同时服务端也起着决定性的作用，如果服务端的技能执行失败了，客户端的技能也会随之停止并“回滚”。如果服务端和客户端执行的结果不矛盾，客户端会执行的非常流畅无延时。如果技能需要实时响应，可以考虑用这种预测模式。下文会着重介绍这个策略。
	// 仅限本地：（Local Only）仅在本地客户端运行的技能。这个策略不适用于专用服务器游戏，本地客户端执行了该技能，服务端任然没有被执行，在有些情况下可能出现拉扯情况，原因在于服务端技能没有被执行，技能中的某个状态和客户端不一致。
	// 服务器启动：（Server Initiated）技能将在服务器上启动并PRC到客户端也执行。可以更准确地复制服务器上实际发生的情况，但是客户端会因缺少本地预测而发生短暂延迟。这种延迟相对来说是比较低的，但是相对于Local Predicted来说还是会牺牲一点流畅性。之前我遇到一种情况，一个没有预测的技能A，和一个带有预测的技能B，A执行了后B不能执行， 现在同时执行技能A和技能B， 由于A需要等待服务器端做验证，B是本地预测技能所以B的客户端会瞬间执行，导致B的客户端被执行过了，服务端失败，出现了一些不可预料的问题了，这种情况需要将技能B的网络策略修改为Server Initiated，这样就会以服务端为权威运行，虽然会牺牲点延迟，但是也是可以在游戏接收的范围内，有时候需要在此权衡。
	// 仅限服务器：（Server Only）技能将在只在服务器上运行，客户端不会。服务端的技能被执行后，技能修改的任何变量都将被复制，并会将状态传递给客户端。缺点是比较服务端的每个影响都会由延迟同步到客户端端，Server Initiated只在运行时候会有一点滞后。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 是否可以从客户端来Cancel 服务器上的Ability
	bServerRespectsRemoteAbilityCancellation = false;

	/** 配置业务数据*/

	if (UClass* pClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/GE_RocketLauncherAiming.GE_RocketLauncherAiming_C'")))
	{
		mGE_Aiming_Class = pClass;
	}
	if (UClass* pClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/GE_RocketLauncherAimingRemoval.GE_RocketLauncherAimingRemoval_C'")))
	{
		mGE_AimingRemoval_Class = pClass;
	}
	if (UClass* pClass = LoadClass<UGRBHUDReticle>(nullptr, TEXT("/Script/UMGEditor.WidgetBlueprint'/Game/GRBShooter/UI/HUDReticles/HUDReticle_Spread.HUDReticle_Spread_C'")))
	{
		mAimingHUDReticle = pClass;
	}
	mTargeting1PFOV = 60.0f; // 欲变化到的视场角是60°
	mTraceFromPlayerViewPointg = false; // 禁用从视角摄像机追踪射线
	mMaxRange = 3000.0f;
	mRocketDamage = 60.0f; // 单发伤害为60.f
	mTimeToStartTargetingAgain = 0.4f;
	mTimeBetweenFastRockets = 0.2f;
	mTimeBetweenShots = 0.4f;
	mTimeOfLastShot = 0.0f; // 上次的射击时刻
	mAmmoCost = 1; // 单回合射击消耗的弹量 1
	mMaxTargets = 3.0f; // 最大索敌上限是3个敌人(会绘制UI)

	/** 配置技能输入ID*/
	UGRBGameplayAbility::AbilityInputID = EGRBAbilityInputID::SecondaryFire; // 配置为副开火/瞄准
	UGRBGameplayAbility::AbilityID = EGRBAbilityInputID::SecondaryFire;

	/** 配置和技能架构相关联的参数*/
	bActivateAbilityOnGranted = false; // 禁用授权好技能后自动激活
	bActivateOnInput = true; // 启用当检测到外部键鼠输入,自动激活技能
	bSourceObjectMustEqualCurrentWeaponToActivate = true; // 启用当装备好武器后应用SourceObject
	bCannotActivateWhileInteracting = true; // 启用当交互时阻止激活技能

	/** 配置带来的火箭筒炮弹伤害BUFF*/
	UClass* pClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/GE_RocketLauncherDamage.GE_RocketLauncherDamage_C'"));
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
	// 触发本技能, 有资产标签
	TagContainer_AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Secondary")));
	// 触发本技能 会打断重装子弹
	TagContainer_CancelAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Reload")));
	// 触发本技能 会后续阻碍这些技能发动
	TagContainer_BlockAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Sprint")));
	TagContainer_BlockAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging")));
	TagContainer_BlockAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Primary")));
	TagContainer_BlockAbilities.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Reload")));
	// Activation Owned Tags：激活该GA时，赋予ASC以下状态标签
	TagContainer_ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.BlocksInteraction")));
	TagContainer_ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.CancelsSprint")));
	// Activation Blocked Tags：激活GA时，ASC上不能有以下标签, 否则激活失败
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	TagContainer_ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.KnockedDown")));
	AbilityTags = TagContainer_AbilityTags;
	CancelAbilitiesWithTag = TagContainer_CancelAbilities;
	BlockAbilitiesWithTag = TagContainer_BlockAbilities;
	ActivationOwnedTags = TagContainer_ActivationOwnedTags;
	ActivationBlockedTags = TagContainer_ActivationBlockedTags;
}

void UGA_GRBRocketLauncherSecondary::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 清除已激活的 瞄准索敌移除BUFF
	mGE_AimingRemovalBuffHandle = FActiveGameplayEffectHandle();

	// 预准备和填充数据
	CheckAndSetupCacheables();

	// 施加一个索敌瞄准BUFF给自身ASC
	// 蓝图 GE_RocketLauncherAiming 是Infinite周期的BUFF; Components选项内执行了策略"GrandTagsToTargetActor" 即 给枪手ASC授予了 标签 "Weapon.RocketLauncher.Aiming"
	const FActiveGameplayEffectHandle& AppliedBuffHandle = UGameplayAbility::BP_ApplyGameplayEffectToOwner(mGE_Aiming_Class, GetAbilityLevel(), 1);
	mGE_AimingBuffHandle = AppliedBuffHandle;

	// 放大视场角
	m1PZoomInTask = UGRBAT_WaitChangeFOV::WaitChangeFOV(this, FName("None"), mOwningHero->GetFirstPersonCamera(), mTargeting1PFOV, 0.1f, nullptr);
	m1PZoomInTask->ReadyForActivation();
	mOwningHero->SetThirdPersonCameraBoom(100.0f);

	// 在索敌瞄准时候同时发动ADS瞄准请求, 勒令运动组件结合运动队列迟滞运动
	if (UGRBCharacterMovementComponent* const GRBMoveComp = Cast<UGRBCharacterMovementComponent>(GetActorInfo().MovementComponent))
	{
		GRBMoveComp->StartAimDownSights();
	}

	// 松开键鼠检测,结束索敌瞄准技能
	UAbilityTask_WaitInputRelease* const WaitInputReleaseNode = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitInputReleaseNode->OnRelease.AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::OnManuallyStopRocketSearch);
	WaitInputReleaseNode->ReadyForActivation();

	//
	UGRBAT_ServerWaitForClientTargetData* const ServerWaitForClientTargetDataNode = UGRBAT_ServerWaitForClientTargetData::ServerWaitForClientTargetData(this, FName("None"), false);
	ServerWaitForClientTargetDataNode->ValidDataDelegate.AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::HandleTargetData);
	mServerWaitTargetDataTask = ServerWaitForClientTargetDataNode;
	ServerWaitForClientTargetDataNode->ReadyForActivation();

	// 开始索敌技能数据和客户端行为校验
	// 制造场景探查器并构建技能目标数据
	StartTargeting();

	// 设置UI状态显示
	mSourceWeapon->StatusText = FText::FromString(TEXT("Homing"));
	if (IsValid(mGRBPlayerController))
	{
		mGRBPlayerController->SetEquippedWeaponStatusText(mSourceWeapon->StatusText);
	}
}

bool UGA_GRBRocketLauncherSecondary::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 确保多次激活副开火技能的时刻差要始终大于键鼠输入的射击间隔; 这样玩家就不能通过快速点击来非法绕过
	// Make sure time between activations is more than time between shots so the player can't just click really fast to skirt around the time between shots.
	if (IsValid(mSourceWeapon))
	{
		const float& DeltaMinus = UGameplayStatics::GetTimeSeconds(this) - mTimeOfLastShot;
		return FMath::Abs(DeltaMinus) >= mTimeBetweenShots;
	}
	else if (!IsValid(mSourceWeapon))
	{
		return true;
	}
	return true;
}

void UGA_GRBRocketLauncherSecondary::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 复位UI
	mSourceWeapon->StatusText = FText::FromString(TEXT("Default"));
	mGRBPlayerController->SetEquippedWeaponStatusText(mSourceWeapon->StatusText);
	UGRBGameplayAbility::ResetHUDReticle();

	// 等待技能目标数据异步任务和 服务器按任务流等待客户端技能目标数据异步任务都被清理
	if (IsValid(mWaitTargetDataWithActorTask))
	{
		mWaitTargetDataWithActorTask->EndTask();
	}
	if (IsValid(mServerWaitTargetDataTask))
	{
		mServerWaitTargetDataTask->EndTask();
	}

	// 移除瞄准的BUFF
	if (UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectStackCount(mGE_AimingRemovalBuffHandle) <= 0)
	{
		mGE_AimingRemovalBuffHandle = UGameplayAbility::BP_ApplyGameplayEffectToOwner(mGE_AimingRemoval_Class, GetAbilityLevel(), 1);
	}
	UGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithHandle(mGE_AimingBuffHandle, -1);
	UGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithHandle(mGE_AimingRemovalBuffHandle, -1);

	// 视场角复位处理
	if (IsValid(m1PZoomInTask))
	{
		m1PZoomInTask->EndTask();
	}
	if (IsValid(m1PZoomResetTask))
	{
		m1PZoomResetTask->EndTask();
	}
	mOwningHero->GetFirstPersonCamera()->SetFieldOfView(90);
	mOwningHero->SetThirdPersonCameraBoom(300.f);

	// 运动组件的ADS请求复位
	if (UGRBCharacterMovementComponent* const GRBMoveComp = Cast<UGRBCharacterMovementComponent>(GetActorInfo().MovementComponent))
	{
		GRBMoveComp->StopAimDownSights();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_GRBRocketLauncherSecondary::GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	// 仅承认SourceObject载体是挂载在枪支上的.且残余载弹量要满足单发消耗
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

void UGA_GRBRocketLauncherSecondary::GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::GRBApplyCost_Implementation(Handle, ActorInfo, ActivationInfo);

	// 每次都会扣除一发弹药
	if (AGRBWeapon* pGRBWeapon = Cast<AGRBWeapon>(GetSourceObject(Handle, &ActorInfo)))
	{
		if (!pGRBWeapon->HasInfiniteAmmo())
		{
			pGRBWeapon->SetPrimaryClipAmmo(pGRBWeapon->GetPrimaryClipAmmo() - mAmmoCost);
		}
	}
}

///--@brief 每回合/每次进行 右键索敌瞄准技能数据的综合入口; 制造场景探查器并构建技能目标数据--/
void UGA_GRBRocketLauncherSecondary::StartTargeting()
{
	if (GetActorInfo().PlayerController->IsLocalPlayerController()) // 仅承认本地客户端行为
	{
		// 配上索敌UI
		UGRBGameplayAbility::SetHUDReticle(mAimingHUDReticle);

		// 预配置可复用的球型探查器
		if (IsValid(mSourceWeapon))
		{
			if (mOwningHero->IsInFirstPersonPerspective())
			{
				FGameplayAbilityTargetingLocationInfo NewLocationInfo = FGameplayAbilityTargetingLocationInfo();
				NewLocationInfo.LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;
				NewLocationInfo.LiteralTransform = FTransform();
				NewLocationInfo.SourceActor = nullptr;
				NewLocationInfo.SourceComponent = Weapon1PMesh;
				NewLocationInfo.SourceAbility = nullptr;
				NewLocationInfo.SourceSocketName = FName("MuzzleFlashSocket");
				mTraceStartLocation = NewLocationInfo;

				mTraceFromPlayerViewPointg = true;
			}
			else if (!mOwningHero->IsInFirstPersonPerspective())
			{
				FGameplayAbilityTargetingLocationInfo NewLocationInfo = FGameplayAbilityTargetingLocationInfo();
				NewLocationInfo.LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;
				NewLocationInfo.LiteralTransform = FTransform();
				NewLocationInfo.SourceActor = nullptr;
				NewLocationInfo.SourceComponent = Weapon3PMesh;
				NewLocationInfo.SourceAbility = nullptr;
				NewLocationInfo.SourceSocketName = FName("MuzzleFlashSocket");
				mTraceStartLocation = NewLocationInfo;

				mTraceFromPlayerViewPointg = false;
			}

			const FGameplayTag& AimingTag = FGameplayTag::EmptyTag;
			const FGameplayTag& AimingRemovalTag = FGameplayTag::EmptyTag;
			const FCollisionProfileName& ProfileName = FCollisionProfileName(FName("Projectile"));
			const FGameplayTargetDataFilterHandle& FilterHandle = FGameplayTargetDataFilterHandle();
			const TSubclassOf<AGameplayAbilityWorldReticle>& ReticleClass = (UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Characters/Shared/Targeting/BP_SingleTargetReticle.BP_SingleTargetReticle_C'"))));
			const FWorldReticleParameters& WorldReticleParameters = FWorldReticleParameters();
			const bool& IgnoreBlockingHits = false;
			const bool& ShouldProduceTargetDataOnServer = false;
			const bool& UsePersisitentHitResults = true;
			const bool& OpenDebug = false;
			const bool& TraceAffectsAimPitch = true;
			const bool& TraceFromPlayerViewPoint = true;
			const bool& UseAimingSpreadMod = false;
			const float& MaxDetectRange = mMaxRange;
			const float& TraceSphereRadius = 32.0f;
			const float& BaseTargetingSpread = 0;
			const float& AimingSpreadMod = 0;
			const float& TargetingSpreadIncreasement = 0;
			const float& TargetingSpreadMax = 0;
			const int32& MaxHitResultsPerTrace = FMath::Min(mSourceWeapon->GetPrimaryClipAmmo(), mMaxTargets);
			const int32& NumberOfTraces = 1;
			mSphereTraceTargetActor->Configure(mTraceStartLocation, AimingTag, AimingRemovalTag, ProfileName, FilterHandle, (ReticleClass), WorldReticleParameters, IgnoreBlockingHits,
			                                   ShouldProduceTargetDataOnServer, UsePersisitentHitResults, OpenDebug, TraceAffectsAimPitch, TraceFromPlayerViewPoint, UseAimingSpreadMod, MaxDetectRange,
			                                   TraceSphereRadius, BaseTargetingSpread, AimingSpreadMod, TargetingSpreadIncreasement, TargetingSpreadMax, MaxHitResultsPerTrace, 1
			);

			/**
			 * 因为是UserConfirmed 模式,
			 * 所以UGRBAT_WaitTargetDataUsingActor节点是在玩家按下鼠标左键后确认了技能目标数据,才会进堆栈UGA_GRBRocketLauncherSecondary::HandleTargetData
			 */
			// 等待目标数据并HandleTargetData
			EGameplayTargetingConfirmation::Type ConfirmType = EGameplayTargetingConfirmation::UserConfirmed; // 非瞬发的, 需要等待用户后续确认的 索敌
			bool bCreateKeyIfNotValidForMorePredicting = true;
			UGRBAT_WaitTargetDataUsingActor* const WaitTargetDataUsingActorNode = UGRBAT_WaitTargetDataUsingActor::WaitTargetDataWithReusableActor(this, FName("None"), ConfirmType, mSphereTraceTargetActor, bCreateKeyIfNotValidForMorePredicting);
			WaitTargetDataUsingActorNode->ValidDataDelegate.AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::HandleTargetData);
			WaitTargetDataUsingActorNode->CancelledDelegate.AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::HandleTargetDataCancelled);
			WaitTargetDataUsingActorNode->ReadyForActivation();
			mWaitTargetDataWithActorTask = WaitTargetDataUsingActorNode;
		}
	}
}

void UGA_GRBRocketLauncherSecondary::ManuallyKillInstantGA()
{
	K2_EndAbility();
}

void UGA_GRBRocketLauncherSecondary::OnManuallyStopRocketSearch(float InTimeHeld)
{
	// 复位FOV
	const float& HeroOrigin1PFOV = 90.0f;
	UGRBAT_WaitChangeFOV* const WaitChangeFOVNode = UGRBAT_WaitChangeFOV::WaitChangeFOV(this, FName("None"), mOwningHero->GetFirstPersonCamera(), HeroOrigin1PFOV, 0.05f, nullptr);
	WaitChangeFOVNode->GetOnTargetFOVReached().AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::ManuallyKillInstantGA);
	WaitChangeFOVNode->ReadyForActivation();
	m1PZoomResetTask = WaitChangeFOVNode;

	// 应用 一张BUFF来添加移除ADSTag;
	// 这张BUFF是GE_RocketLauncherAimingRemoval
	// BUFF带来的效果是 Infinite周期; Components选项配置为给目标授予Tag Weapon.RocketLauncher.AimingRemoval
	const FActiveGameplayEffectHandle& TheBuffWillApply = UGameplayAbility::BP_ApplyGameplayEffectToOwner(mGE_AimingRemoval_Class, GetAbilityLevel(), 1);
	mGE_AimingRemovalBuffHandle = TheBuffWillApply;
}

void UGA_GRBRocketLauncherSecondary::HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle)
{
	/**
	 * 因为是UserConfirmed 模式,
	 * 所以UGRBAT_WaitTargetDataUsingActor节点是在玩家按下鼠标左键后确认了技能目标数据,才会进堆栈UGA_GRBRocketLauncherSecondary::HandleTargetData
	 */

	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	// 每次鼠标左键确认后进来都会保存一份技能目标数据副本
	CachedTargetDataHandleWhenConfirmed = InTargetDataHandle;

	// 先确认技能目标数据的目标个数
	const int32& AbilityTargetsNum = UAbilitySystemBlueprintLibrary::GetDataCountFromTargetData(InTargetDataHandle);
	const int32& SearchEnemyNums = FMath::Min(AbilityTargetsNum, mMaxTargets);

	// 依据索敌个数, 多次射击火箭弹;
	UAbilityTask_Repeat* const RepeatNode = UAbilityTask_Repeat::RepeatAction(this, mTimeBetweenFastRockets, SearchEnemyNums);
	RepeatNode->OnPerformAction.AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::HandleTargetData_PerformAction);
	RepeatNode->OnFinished.AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::HandleTargetData_OnFinishFireHomingRocketBullet);
	RepeatNode->ReadyForActivation();
	mFireRocketRepeatActionTask = RepeatNode;
}

void UGA_GRBRocketLauncherSecondary::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& InTargetDataHandle)
{
	OnManuallyStopRocketSearch(INDEX_NONE);
}

void UGA_GRBRocketLauncherSecondary::HandleTargetData_PerformAction(const int32 InActionNumber)
{
	// 射击多次的时候本次的射线检测结果;
	const FHitResult& CalcHitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(CachedTargetDataHandleWhenConfirmed, InActionNumber);
	AActor* HitActor = CalcHitResult.GetActor();
	if (AGRBCharacterBase* const HittedGRBCharacterBase = Cast<AGRBCharacterBase>(HitActor))
	{
		if (HittedGRBCharacterBase->IsAlive())
		{
			if (UGameplayAbility::K2_CommitAbilityCost(false))
			{
				PlayFireMontage();

				// 组织1个BUFF HANDLE, 用于计算弹丸伤害
				const FGRBGameplayEffectContainerSpec& GRBGEContainerSpecPak = UGRBGameplayAbility::MakeEffectContainerSpec(FGameplayTag::RequestGameplayTag(FName("Ability")), FGameplayEventData(), -1);
				const FGameplayEffectSpecHandle& RocketLauncherDamageBuffHandle = GRBGEContainerSpecPak.TargetGameplayEffectSpecs.IsValidIndex(0) ? GRBGEContainerSpecPak.TargetGameplayEffectSpecs[0] : FGameplayEffectSpecHandle();
				// 给这个BUFF HANDLE 动态修改一些数据, 例如组织CauseTag, 和调试伤害幅度
				// ("Data.Damage")这个辨识标签会和 GGEC:UGRBDamageExecutionCalc 内关联,用于伤害提取
				const FGameplayTag& CauseTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
				const float& Magnitude = mRocketDamage;
				const FGameplayEffectSpecHandle& TheBuffToApply = UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(RocketLauncherDamageBuffHandle, CauseTag, Magnitude);
				//
				const FHitResult& TraceHit = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(CachedTargetDataHandleWhenConfirmed, InActionNumber);
				const FRotator& LookAtRotation = UKismetMathLibrary::FindLookAtRotation(TraceHit.TraceStart, TraceHit.Location);
				// 生成追踪弹
				if (mOwningHero->HasAuthority())
				{
					FSoftObjectPath SoftObjectPaths_Actor1 = FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/Game/GRBShooter/Weapons/RocketLauncher/BP_RocketLauncherProjectile1.BP_RocketLauncherProjectile1_C'"));
					UClass* GRBProjectileBP = UAssetManager::GetStreamableManager().LoadSynchronous<UClass>(SoftObjectPaths_Actor1);
					const FVector& SpawnLoc = TraceHit.TraceStart;
					const FRotator& SpawnRot = LookAtRotation;
					FActorSpawnParameters PActorSpawnParameters;
					PActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					PActorSpawnParameters.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
					FTransform PSpawnTrans = FTransform(SpawnRot, SpawnLoc);
					AGRBProjectile* const SpawnedGRBProjectile = mOwningHero->GetWorld()->SpawnActorDeferred<AGRBProjectile>(GRBProjectileBP, PSpawnTrans, mOwningHero, mOwningHero, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, ESpawnActorScaleMethod::OverrideRootScale);
					SpawnedGRBProjectile->Owner = mOwningHero;
					SpawnedGRBProjectile->mGRBGEContainerSpecPak = GRBGEContainerSpecPak;
					SpawnedGRBProjectile->mIsHoming = true;
					SpawnedGRBProjectile->mHomingTarget = HittedGRBCharacterBase;
					SpawnedGRBProjectile->SetInstigator(mOwningHero);
					SpawnedGRBProjectile->FinishSpawning(PSpawnTrans);
				}

				// 本地开火特效
				if (mOwningHero->IsLocallyControlled())
				{
					const FGameplayTag& CueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.RocketLauncher.Fire"));
					const FGameplayEffectContextHandle& EmptyEffectContextHandle = FGameplayEffectContextHandle();
					const FVector& CueLocation = TraceHit.Location;
					const FVector& CueNormal = UKismetMathLibrary::GetForwardVector(LookAtRotation);
					const AActor* CueInstigator = mOwningHero;
					const FGameplayCueParameters& CueParameters = UAbilitySystemBlueprintLibrary::MakeGameplayCueParameters(0, 0, EmptyEffectContextHandle,
					                                                                                                        FGameplayTag::EmptyTag, FGameplayTag::EmptyTag, FGameplayTagContainer(),
					                                                                                                        FGameplayTagContainer(), CueLocation, CueNormal,
					                                                                                                        const_cast<AActor*>(CueInstigator), nullptr, nullptr,
					                                                                                                        nullptr, 1, 1,
					                                                                                                        nullptr, false);
					if (UGRBAbilitySystemComponent* const GRBASC = Cast<UGRBAbilitySystemComponent>(this->GetAbilitySystemComponentFromActorInfo()))
					{
						GRBASC->ExecuteGameplayCueLocal(CueTag, CueParameters);
					}
				}
				// 刷新开火时刻
				mTimeOfLastShot = UGameplayStatics::GetTimeSeconds(this);
			}
			else if (!UGameplayAbility::K2_CommitAbilityCost(false))
			{
				// 负担不起技能消耗就手动停止索敌业务
				OnManuallyStopRocketSearch(INDEX_NONE);
			}
		}
	}
}

void UGA_GRBRocketLauncherSecondary::HandleTargetData_OnFinishFireHomingRocketBullet(const int32 InActionNumber)
{
	CachedTargetDataHandleWhenConfirmed = FGameplayAbilityTargetDataHandle();
	if (K2_CheckAbilityCost())
	{
		if (mOwningHero->IsLocallyControlled())
		{
			/**
			 * 这里推荐的思想和机制
			 * 因为所有追踪导弹都打出去了
			 * 要确保之前UserConfirmed模式下的异步任务:等待索敌技能数据节点必须主动销毁 然后再 索敌一次
			 * 不推荐使用CustomMUlit的技能目标确认模式,因为此种模式下会导致在开火中仍然持续索敌持续StartTargeting,这并不是我们希望的
			 * 所以推荐必须开火了结束之后再进行下一轮的 索敌StartTargeting
			 ***/
			if (IsValid(mWaitTargetDataWithActorTask))
			{
				mWaitTargetDataWithActorTask->EndTask();
			}

			UAbilityTask_WaitDelay* const WaitDelayNode = UAbilityTask_WaitDelay::WaitDelay(this, mTimeToStartTargetingAgain);
			WaitDelayNode->OnFinish.AddUniqueDynamic(this, &UGA_GRBRocketLauncherSecondary::StartTargeting);
			WaitDelayNode->ReadyForActivation();
		}
	}
	else if (!K2_CheckAbilityCost())
	{
		OnManuallyStopRocketSearch(INDEX_NONE);
	}
}

void UGA_GRBRocketLauncherSecondary::PlayFireMontage()
{
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;

	if (GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Weapon.RocketLauncher.Aiming"))) && !GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Weapon.RocketLauncher.AimingRemoval"))))
	{
		if (UAnimMontage* pMontageAsset = LoadObject<UAnimMontage>(mOwningHero, TEXT("/Script/Engine.AnimMontage'/Game/ShooterGame/Animations/FPP_Animations/FPP_LauncherAimFire_Montage.FPP_LauncherAimFire_Montage'")))
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
		if (UAnimMontage* pMontageAsset = LoadObject<UAnimMontage>(mOwningHero, TEXT("/Script/Engine.AnimMontage'/Game/ShooterGame/Animations/FPP_Animations/HeroFPP_LauncherFire_Montage.HeroFPP_LauncherFire_Montage'")))
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

void UGA_GRBRocketLauncherSecondary::CheckAndSetupCacheables()
{
	const ENetRole& NowRole = GetAbilitySystemComponentFromActorInfo()->GetAvatarActor() != nullptr ? GetAvatarActorFromActorInfo()->GetLocalRole() : ENetRole::ROLE_None;
	UE_LOG(LogGRBGameplayAbility, Warning, TEXT("[%s] NowRole is [%s]"), *FString(__FUNCTION__), *UGRBStaticLibrary::GetEnumValueAsString<ENetRole>("ENetRole", NowRole));

	if (!IsValid(mOwningHero))
	{
		mOwningHero = Cast<AGRBHeroCharacter>(GetAvatarActorFromActorInfo());
	}
	if (!IsValid(mSourceWeapon))
	{
		mSourceWeapon = Cast<AGRBWeapon>(GetCurrentSourceObject()); // 因为这个技能是装载在枪支上, 所以SourceObject是枪支
	}
	if (!IsValid(Weapon1PMesh))
	{
		Weapon1PMesh = mSourceWeapon->GetWeaponMesh1P();
	}
	if (!IsValid(Weapon3PMesh))
	{
		Weapon3PMesh = mSourceWeapon->GetWeaponMesh3P();
	}
	if (!IsValid(ASC))
	{
		ASC = Cast<UGRBAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	}
	if (!IsValid(mSphereTraceTargetActor))
	{
		mSphereTraceTargetActor = mSourceWeapon->GetSphereTraceTargetActor();
	}
	if (!IsValid(mGRBPlayerController))
	{
		mGRBPlayerController = Cast<AGRBPlayerController>(GetActorInfo().PlayerController);
	}
}

#pragma endregion
