#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Characters/Abilities/GRBAbilityTypes.h"
#include "GRBShooter/GRBShooter.h"
#include "GRBGameplayAbility.generated.h"

class UGRBHUDReticle;
class USkeletalMeshComponent;

/*
 * GRB项目内 技能蒙太奇包体, 负责维护关联的骨架和蒙太奇资产
 */
USTRUCT()
struct GRBSHOOTER_API FAbilityMeshMontageGRB
{
	GENERATED_BODY()

public:
	FAbilityMeshMontageGRB() : Mesh(nullptr), Montage(nullptr)
	{
	}

	FAbilityMeshMontageGRB(class USkeletalMeshComponent* InMesh, class UAnimMontage* InMontage)
		: Mesh(InMesh), Montage(InMontage)
	{
	}

	UPROPERTY()
	class USkeletalMeshComponent* Mesh = nullptr;

	UPROPERTY()
	class UAnimMontage* Montage = nullptr;
};

/**
 * 项目使用的技能(GA)基类
 */
UCLASS()
class GRBSHOOTER_API UGRBGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGRBGameplayAbility();

#pragma region ~ 虚接口 ~
	// ~Start Implements UGameplayAbility 
	/// @brief 虚方法,当avatarActor初始化绑定键位的时候会被调用
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	///--@brief 虚接口： 检查技能发动条件--/
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	///--@brief 虚接口: 检查技能发动消耗成本--/
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	///--@brief 虚接口: 一个能力被激活时，ApplyCost 方法会实际扣除该能力所需的资源（例如魔法值、能量点等），确保该成本在系统中被正确处理--/
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// ~End Implements
#pragma endregion


	/// @brief 帮给定的一组目标actor组建targetdata
	UFUNCTION(BlueprintCallable, Category = "Ability")
	FGameplayAbilityTargetDataHandle MakeGameplayAbilityTargetDataHandleFromActorArray(const TArray<AActor*> TargetActors);

	/// @brief 给所有的命中hit组建TargetData
	UFUNCTION(BlueprintCallable, Category = "Ability")
	FGameplayAbilityTargetDataHandle MakeGameplayAbilityTargetDataHandleFromHitResults(const TArray<FHitResult> HitResults);

	///--@brief 给定索敌BUFF容器,并依据它的内建信息来生成1个对应的索敌BUFF处理器.--/
	// Make gameplay effect container spec to be applied later, using the passed in container
	UFUNCTION(BlueprintCallable, Category = "Ability", meta = (AutoCreateRefTerm = "EventData"))
	virtual FGRBGameplayEffectContainerSpec MakeEffectContainerSpecFromContainer(const FGRBGameplayEffectContainer& Container, const FGameplayEventData& EventData, int32 OverrideGameplayLevel = -1);

	///--@brief 查找符合标签的索敌BUFF容器并据此构建1个索敌BUFF处理器.--/
	// Search for and make a gameplay effect container spec to be applied later, from the EffectContainerMap
	UFUNCTION(BlueprintCallable, Category = "Ability", meta = (AutoCreateRefTerm = "EventData"))
	virtual FGRBGameplayEffectContainerSpec MakeEffectContainerSpec(const FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel = -1);

	///--@brief 轮询指定的索敌BUFF处理器并把每一个BUFF句柄应用到每一个索敌目标句柄上 --/
	// Applies a gameplay effect container spec that was previously created
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual TArray<FActiveGameplayEffectHandle> ApplyEffectContainerSpec(const FGRBGameplayEffectContainerSpec& ContainerSpec);

	///--@brief 对蓝图暴露GetSourceObject接口--/
	// Expose GetSourceObject to Blueprint. Retrieves the SourceObject associated with this ability. Callable on non instanced abilities.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability", meta = (DisplayName = "Get Source Object"))
	UObject* K2_GetSourceObject(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const;

	///--@brief 使用ASC内的优化RPC合批机制; 合批RPC激活技能.--/
	// Attempts to activate the given ability handle and batch all RPCs into one. This will only batch all RPCs that happen
	// in one frame. Best case scenario we batch ActivateAbility, SendTargetData, and EndAbility into one RPC instead of three.
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual bool BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately);

	///--@brief 旨在与批处理系统合作,从外部结束技能, 类似于K2_EndAbility.--/
	virtual void ExternalEndAbility();

	///--@brief 返回ASC宿主的预测Key状态--/
	// Returns the current prediction key and if it's valid for more predicting. Used for debugging ability prediction windows.
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual FString GetCurrentPredictionKeyStatus();

	///--@brief 检查预测Key是否已发送至服务端--/
	// Returns if the current prediction key is valid for more predicting.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability")
	virtual bool IsPredictionKeyValidForMorePrediction() const;

	///--@brief 允许蓝图重载, 检查消耗成本--/
	// Allows C++ and Blueprint abilities to override how cost is checked in case they don't use a GE like weapon ammo
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ability")
	bool GRBCheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const;
	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const;

	///--@brief 允许蓝图重载, 应用成本扣除--/
	// Allows C++ and Blueprint abilities to override how cost is applied in case they don't use a GE like weapon ammo
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ability")
	void GRBApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const;
	virtual void GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void SetHUDReticle(TSubclassOf<UGRBHUDReticle> ReticleClass);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void ResetHUDReticle();

	///--@brief 在多人游戏内的带预测的客户端内 往服务器发送索敌数据--/
	// Sends TargetData from the client to the Server and creates a new Prediction Window
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void SendTargetDataToServer(const FGameplayAbilityTargetDataHandle& TargetData);

	///--@brief 核验技能是否被按键激活的(未松开).--/
	// Is the player's input currently pressed? Only works if the ability is bound to input.
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual bool IsInputPressed() const;

#pragma region ~ 动画与蒙太奇相关 ~
	// ----------------------------------------------------------------------------------------------------------------
	//	Animation Support for multiple USkeletalMeshComponents on the AvatarActor
	// ----------------------------------------------------------------------------------------------------------------

	///--@brief 从GRB蒙太奇池子里提取出对应骨架关联的蒙太奇资产--/
	/** Returns the currently playing montage for this ability, if any */
	UFUNCTION(BlueprintCallable, Category = Animation)
	UAnimMontage* GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh);

	///--@brief 查找GRB蒙太奇池子内符合对应骨架的蒙太奇,并替换它; 查找失败则手动为此骨架注册进池子.--/
	/** ; Call to set/get the current montage from a montage task. Set to allow hooking up montage events to ability events */
	virtual void SetCurrentMontageForMesh(USkeletalMeshComponent* InMesh, class UAnimMontage* InCurrentMontage);

protected:
	// ----------------------------------------------------------------------------------------------------------------
	//	Animation Support for multiple USkeletalMeshComponents on the AvatarActor
	// ----------------------------------------------------------------------------------------------------------------

	///--@brief 在GRB池子内找关联特定骨架的池子元素--/
	bool FindAbillityMeshMontage(const USkeletalMeshComponent* InMesh, FAbilityMeshMontageGRB& InAbilityMontage);

	///--@brief 检查本技能是否隶属于ASC的池子元素(已激活的), 并跳转蒙太奇到给定section且维持双端统一.--/
	/** Immediately jumps the active montage to a section */
	UFUNCTION(BlueprintCallable, Category = "Ability|Animation")
	void MontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName);

	/** Sets pending section on active montage */
	UFUNCTION(BlueprintCallable, Category = "Ability|Animation")
	void MontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, FName FromSectionName, FName ToSectionName);

	/**
	 * Stops the current animation montage.
	 *
	 * @param OverrideBlendOutTime If >= 0, will override the BlendOutTime parameter on the AnimMontage instance
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Animation", Meta = (AdvancedDisplay = "OverrideBlendOutTime"))
	void MontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime = -1.0f);

	/**
	* Stops all currently animating montages
	*
	* @param OverrideBlendOutTime If >= 0, will override the BlendOutTime parameter on the AnimMontage instance
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Animation", Meta = (AdvancedDisplay = "OverrideBlendOutTime"))
	void MontageStopForAllMeshes(float OverrideBlendOutTime = -1.0f);
#pragma endregion

public:
	// Abilities with this set will automatically activate when the input is pressed
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	EGRBAbilityInputID AbilityInputID = EGRBAbilityInputID::None;

	// Value to associate an ability with an slot without tying it to an automatically activated input.
	// Passive abilities won't be tied to an input so we need a way to generically associate abilities with slots.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	EGRBAbilityInputID AbilityID = EGRBAbilityInputID::None;

	// 用来判断是否Grant的时候就激活技能; Tells an ability to activate immediately when its granted. Used for passive abilities and abilites forced on others.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ability")
	bool bActivateAbilityOnGranted;

	// 此开关决策绑定输入时不激活GA; If true, this ability will activate when its bound input is pressed. Disable if you want to bind an ability to an input but not have it activate when pressed.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ability")
	bool bActivateOnInput = true;

	// 是否当武器装备好再激活SourceObject; If true, only activate this ability if the weapon that granted it is the currently equipped weapon.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ability")
	bool bSourceObjectMustEqualCurrentWeaponToActivate;

	// 在交互时禁用激活GA; If true, only activate this ability if not interacting with something via GA_Interact.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ability")
	bool bCannotActivateWhileInteracting;

	// 本技能带来 多组<Tag-索敌BUFF>的容器
	// Map of gameplay tags to gameplay effect containers
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameplayEffects")
	TMap<FGameplayTag, FGRBGameplayEffectContainer> EffectContainerMap;

protected:
	/** 由此技能引发且激活的GRB蒙太奇池子 */
	UPROPERTY()
	TArray<FAbilityMeshMontageGRB> CurrentAbilityMeshMontages;

	// 在执行交互时候的标签
	FGameplayTag InteractingTag;

	// 在执行交互移除时刻的标签
	FGameplayTag InteractingRemovalTag;
};
//
//
// /**
//  * 步枪持续射击从属技能;
//  * 和UGA_GRBRiflePrimary主射击技能搭配使用;
//  */
// UCLASS()
// class GRBSHOOTER_API UGA_GRBRiflePrimaryInstant : public UGRBGameplayAbility
// {
// 	GENERATED_BODY()
//
// public:
// 	UGA_GRBRiflePrimaryInstant();
// 	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
// 	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
// 	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
//
// 	///--@brief 负担技能消耗的检查--/
// 	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;
//
// 	///--@brief 技能消耗成本扣除: 刷新残余载弹量扣除每回合发动时候的弹药消耗量--/
// 	virtual void GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
//
// public:
// 	// 每回合/每次射击子弹的调度业务
// 	UFUNCTION(BlueprintCallable)
// 	void FireBullet();
//
// 	// 手动终止技能以及异步任务
// 	UFUNCTION(BlueprintCallable)
// 	void ManuallyKillInstantGA();
//
// private:
// 	///--@brief 双端都会调度到的 处理技能目标数据的复合逻辑入口--/
// 	/**
//  	 * 播放蒙太奇动画
//  	 * 伤害BUFF应用
//  	 * 播放诸如关联枪支火焰CueTag的所有特效
//  	 */
// 	UFUNCTION(BlueprintCallable)
// 	void HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle);
//
// 	// 播放项目定制的蒙太奇.
// 	UFUNCTION(BlueprintCallable)
// 	void PlayFireMontage();
//
// 	// 触发技能时候的数据预准备
// 	UFUNCTION(BlueprintCallable)
// 	void CheckAndSetupCacheables();
//
// public:
// 	// 武器1P视角下的枪皮
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class USkeletalMeshComponent* Weapon1PMesh = nullptr;
//
// 	// 武器3P视角下的枪皮
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class USkeletalMeshComponent* Weapon3PMesh = nullptr;
//
// 	// 开火枪支
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class AGRBWeapon* mSourceWeapon = nullptr;
//
// 	// 枪手
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class AGRBHeroCharacter* mOwningHero = nullptr;
//
// 	// 主开火技能
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class UGA_GRBRiflePrimary* mGAPrimary = nullptr;
//
// 	// 异步任务: 服务器等待客户端发送来的目标数据并执行绑定的"索敌目标"委托
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class UGRBAT_ServerWaitForClientTargetData* mServerWaitTargetDataTask = nullptr;
//
// 	// 单回合射击消耗的弹量
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	int32 mAmmoCost = 1;
//
// 	// 扩散调幅
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mAimingSpreadMod = 0.15f;
//
// 	// 单发伤害值
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mBulletDamage = 10.0f;
//
// 	// 扩散增幅
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mFiringSpreadIncrement = 1.2f;
//
// 	// 扩散阈值
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mFiringSpreadMax = 10.0f;
//
// 	// 上次的射击时刻
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mTimeOfLastShot = -99999999.0f;
//
// 	// 武器扩散基准系数
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mWeaponSpread = 1.0f;
//
// 	// 和技能目标数据强关联的目标位置信息
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	struct FGameplayAbilityTargetingLocationInfo mTraceStartLocation;
//
// 	// 是否启用从视角摄像机追踪射线
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	bool mTraceFromPlayerViewPointg = true;
//
// 	// 瞄准时会授予的标签
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	FGameplayTag mAimingTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.Aiming"));
//
// 	// 瞄准移除时候会授予的标签
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	FGameplayTag mAimingRemovealTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.AimingRemoval"));
//
// 	// 副开火技能会用到的场景探查器
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class AGRBGATA_LineTrace* mLineTraceTargetActor = nullptr;
// };
//
//
// /**
//  * 步枪持续射击主技能;
//  * 和UGA_GRBRiflePrimaryInstant从属射击技能搭配使用;
//  */
// UCLASS()
// class GRBSHOOTER_API UGA_GRBRiflePrimary : public UGRBGameplayAbility
// {
// 	GENERATED_BODY()
//
// public:
// 	UGA_GRBRiflePrimary();
// 	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
// 	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
// 	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
//
// public:
// 	///--@brief 专门做的技能消耗检查; 载弹量不足以支撑本回合射击的情形,会主动激活Reload技能--/
// 	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;
//
// 	//
// 	const float& Getm_TimeBetweenShot() const { return m_TimeBetweenShot; }
//
// protected:
// 	///--@brief 业务数据预处理--/
// 	void CheckAndSetupCacheables();
//
// private:
// 	///--@brief 按开火模式执行开火业务--/
// 	UFUNCTION()
// 	void ExecuteShootBussByFireMode();
//
// 	///--@brief 玩家松开键鼠输入后的回调业务--/
// 	UFUNCTION()
// 	void OnReleaseBussCallback(float InPayload_TimeHeld);
//
// 	///--@brief 循环射击回调入口; 会反复递归异步任务UAbilityTask_WaitDelay--/
// 	UFUNCTION()
// 	void ContinuouslyFireOneBulletCallback();
//
// 	//
// 	UFUNCTION()
// 	void ContinuouslyFireOneBulletCallbackV2();
//
// 	///--@brief 爆炸开火模式下 除开首发合批的后两次合批走的业务--/
// 	///--@brief 注意这里的机制射击; 爆炸射击(以三连发为例), 这回合的第1发是走的半自动的第一波合批, 第2发和第3发才是走的下下一波合批--/
// 	UFUNCTION()
// 	void PerRoundFireBurstBulletsCallback();
//
// 	///--@brief 爆炸开火多次Repeat情形下的单次业务--/
// 	UFUNCTION()
// 	void PerTimeBurstShootBussCallback(int32 PerformActionNumber);
//
// 	///--@brief 爆炸开火多次Repeat结束后的业务--/
// 	UFUNCTION()
// 	void OnFinishBurstShootBussCallback(int32 ActionNumber);
//
// protected:
// 	// 与技能相关联的武器
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
// 	class AGRBWeapon* m_SourceWeapon = nullptr;
//
// 	// 射击间隔时长, 蓝图可配置
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
// 	float m_TimeBetweenShot = 0.1f;
//
// 	// 与开火主技能关联的 Instant副技能句柄
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
// 	struct FGameplayAbilitySpecHandle m_InstantAbilityHandle;
//
// 	// 与开火主技能关联的 Instant副技能
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
// 	class UGA_GRBRiflePrimaryInstant* m_InstantAbility = nullptr;
//
// 	// 单次射击消耗的子弹个数
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
// 	int32 m_AmmoCost = 1;
//
// private:
// 	// 异步节点WaitDelay:递归循环射击
// 	UPROPERTY()
// 	class UAbilityTask_WaitDelay* AsyncWaitDelayNode_ContinousShoot = nullptr;
// };
//
//
// //--------------------------------------------------- 火箭筒 ------------------------------------------------
// //--------------------------------------------------- 火箭筒 ------------------------------------------------
//
//
// /**
//  * 火箭筒开火副技能;
//  * 和UGA_GRBRocketLauncherPrimary主射击技能搭配使用;
//  */
// UCLASS()
// class GRBSHOOTER_API UGA_GRBRocketLauncherPrimaryInstant : public UGRBGameplayAbility
// {
// 	GENERATED_BODY()
//
// public:
// 	UGA_GRBRocketLauncherPrimaryInstant();
// 	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
// 	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
// 	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
//
// 	///--@brief 负担技能消耗的检查--/
// 	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;
//
// 	///--@brief 技能消耗成本扣除: 刷新残余载弹量扣除每回合发动时候的弹药消耗量--/
// 	virtual void GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
//
// public:
// 	// 每回合/每次射击子弹的调度业务
// 	UFUNCTION(BlueprintCallable)
// 	void FireRocket();
//
// 	// 手动终止技能以及异步任务
// 	UFUNCTION(BlueprintCallable)
// 	void ManuallyKillInstantGA();
//
// private:
// 	///--@brief 双端都会调度到的 处理技能目标数据的复合逻辑入口--/
// 	/**
//  	 * 播放蒙太奇动画
//  	 * 伤害BUFF应用
//  	 * 播放诸如关联枪支火焰CueTag的所有特效
//  	 */
// 	UFUNCTION(BlueprintCallable)
// 	void HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle);
//
// 	// 播放项目定制的蒙太奇.
// 	UFUNCTION(BlueprintCallable)
// 	void PlayFireMontage();
//
// 	// 触发技能时候的数据预准备
// 	UFUNCTION(BlueprintCallable)
// 	void CheckAndSetupCacheables();
//
// public:
// 	// 武器1P视角下的枪皮
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class USkeletalMeshComponent* Weapon1PMesh = nullptr;
//
// 	// 武器3P视角下的枪皮
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class USkeletalMeshComponent* Weapon3PMesh = nullptr;
//
// 	// 开火枪支
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class AGRBWeapon* mSourceWeapon = nullptr;
//
// 	// 枪手
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class AGRBHeroCharacter* mOwningHero = nullptr;
//
// 	// 主开火技能
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class UGA_GRBRocketLauncherPrimary* mGAPrimary = nullptr;
//
// 	// 异步任务: 服务器等待客户端发送来的目标数据并执行绑定的"索敌目标"委托
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class UGRBAT_ServerWaitForClientTargetData* mServerWaitTargetDataTask = nullptr;
//
// 	// 上次的射击时刻
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mTimeOfLastShot = 0.f;
//
// 	// 单回合射击消耗的弹量
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	int32 mAmmoCost = 1;
//
// 	// 单发火箭筒弹丸伤害值
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	float mRocketDamage = 60.0f;
//
// 	// 和技能目标数据强关联的目标位置信息
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	struct FGameplayAbilityTargetingLocationInfo mTraceStartLocation;
//
// 	// 是否启用从视角摄像机追踪射线-默认为否
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	bool mTraceFromPlayerViewPointg = false;
//
// 	// 副开火技能会用到的场景探查器
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
// 	class AGRBGATA_LineTrace* mLineTraceTargetActor = nullptr;
// };
//
//
// /**
//  * 火箭筒的开火主技能
//  */
// UCLASS(Blueprintable, BlueprintType)
// class GRBSHOOTER_API UGA_GRBRocketLauncherPrimary : public UGRBGameplayAbility
// {
// 	GENERATED_BODY()
//
// public:
// 	UGA_GRBRocketLauncherPrimary();
// 	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
// 	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
// 	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
//
// 	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;
//
// 	const float& Getm_TimeBetweenShot() const { return m_TimeBetweenShot; }
//
// protected:
// 	///--@brief 业务数据预处理--/
// 	void CheckAndSetupCacheables();
//
// private:
// 	///--@brief 按开火模式执行开火业务--/
// 	UFUNCTION()
// 	void ExecuteShootBussByFireMode();
//
// protected:
// 	// 与技能相关联的武器
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
// 	class AGRBWeapon* m_SourceWeapon = nullptr;
//
// 	// 射击间隔时长, 蓝图可配置
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
// 	float m_TimeBetweenShot = 0.3f;
//
// 	// 与开火主技能关联的 Instant副技能句柄
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
// 	struct FGameplayAbilitySpecHandle m_InstantAbilityHandle;
//
// 	// 与开火主技能关联的 Instant副技能
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
// 	class UGA_GRBRocketLauncherPrimaryInstant* m_InstantAbility = nullptr;
//
// 	// 单次射击消耗的子弹个数
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
// 	int32 m_AmmoCost = 1;
// };
//
//
// /**
//  * 火箭筒的瞄准次要技能
//  */
// UCLASS(Blueprintable, BlueprintType)
// class GRBSHOOTER_API UGA_GRBRocketLauncherSecondary : public UGRBGameplayAbility
// {
// 	GENERATED_BODY()
//
// public:
// 	UGA_GRBRocketLauncherSecondary();
// 	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
// 	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
// 	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
//
// 	///--@brief 负担技能消耗的检查--/
// 	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;
//
// 	///--@brief 技能消耗成本扣除: 刷新残余载弹量扣除每回合发动时候的弹药消耗量--/
// 	virtual void GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
//
// public:
// 	// 每回合/每次进行 右键索敌瞄准技能数据的综合入口
// 	// 制造场景探查器并构建技能目标数据
// 	UFUNCTION(BlueprintCallable)
// 	void StartTargeting();
//
// 	// 手动终止技能以及异步任务
// 	UFUNCTION(BlueprintCallable)
// 	void ManuallyKillInstantGA();
//
// private:
// 	///--@brief 双端都会调度到的 处理技能目标数据的复合逻辑入口--/
// 	/**
//  	 * 播放蒙太奇动画
//  	 * 伤害BUFF应用
//  	 * 播放诸如关联枪支火焰CueTag的所有特效
//  	 */
// 	UFUNCTION(BlueprintCallable)
// 	void HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle);
//
// 	UFUNCTION(BlueprintCallable)
// 	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& InTargetDataHandle);
//
// 	///--@brief 当右键索敌确认后,左键按下发射多次追踪弹的单次业务--/
// 	UFUNCTION(Blueprintable)
// 	void HandleTargetData_PerformAction(const int32 InActionNumber);
//
// 	///--@brief 当发射多个追踪弹结束之后的检测业务--/
// 	UFUNCTION(Blueprintable)
// 	void HandleTargetData_OnFinishFireHomingRocketBullet(const int32 InActionNumber);
//
// 	// 播放项目定制的蒙太奇.
// 	UFUNCTION(BlueprintCallable)
// 	void PlayFireMontage();
//
// 	// 触发技能时候的数据预准备
// 	UFUNCTION(BlueprintCallable)
// 	void CheckAndSetupCacheables();
//
// 	UFUNCTION()
// 	void OnManuallyStopRocketSearch(float InTimeHeld);
//
// public:
// 	// 武器1P视角下的枪皮
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness|Components")
// 	class USkeletalMeshComponent* Weapon1PMesh = nullptr;
//
// 	// 武器3P视角下的枪皮
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness|Components")
// 	class USkeletalMeshComponent* Weapon3PMesh = nullptr;
//
// 	// 关联的GRB-ASC
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness|Components")
// 	class UGRBAbilitySystemComponent* ASC = nullptr;
//
// 	// 枪手
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class AGRBHeroCharacter* mOwningHero = nullptr;
//
// 	// 开火枪支
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class AGRBWeapon* mSourceWeapon = nullptr;
//
// 	// 已激活的瞄准BUFF
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	struct FActiveGameplayEffectHandle mGE_AimingBuffHandle = FActiveGameplayEffectHandle();
//
// 	// 已激活的瞄准移除BUFF
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	struct FActiveGameplayEffectHandle mGE_AimingRemovalBuffHandle = FActiveGameplayEffectHandle();
//
// 	// 异步任务: 第一人称下的 等待切换FOV
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class UGRBAT_WaitChangeFOV* m1PZoomInTask = nullptr;
//
// 	// 异步任务: 第一人称下的 复位视角
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class UGRBAT_WaitChangeFOV* m1PZoomResetTask = nullptr;
//
// 	// 欲切换到的FOV视场角1p
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	float mTargeting1PFOV = 60.f;
//
// 	// 哪一张瞄准BUFF
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	TSubclassOf<UGameplayEffect> mGE_Aiming_Class;
//
// 	// 哪一张瞄准移除BUFF
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	TSubclassOf<UGameplayEffect> mGE_AimingRemoval_Class;
//
// 	// 哪一张瞄准HUD准星UI
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	TSubclassOf<class UGRBHUDReticle> mAimingHUDReticle;
//
// 	// 异步任务:用来模拟重复射击火箭筒弹丸
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class UAbilityTask_Repeat* mFireRocketRepeatActionTask = nullptr;
//
// 	// 异步任务: 服务器等待客户端发送来的目标数据并执行绑定的"索敌目标"委托
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class UGRBAT_ServerWaitForClientTargetData* mServerWaitTargetDataTask = nullptr;
//
// 	// 异步任务: 等待探查器数据并据此完成双端射击任务
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class UGRBAT_WaitTargetDataUsingActor* mWaitTargetDataWithActorTask = nullptr;
//
// 	// 和技能目标数据强关联的目标位置信息
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	struct FGameplayAbilityTargetingLocationInfo mTraceStartLocation;
//
// 	// 是否启用从视角摄像机追踪射线
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	bool mTraceFromPlayerViewPointg = false;
//
// 	// 最大射距
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	float mMaxRange = 3000.f;
//
// 	// 单发弹丸伤害
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	float mRocketDamage = 60.0f;
//
// 	// 索敌数据销毁后再次StartTargeting的容忍延迟间隔时长
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GSSecondaryInstantBussiness")
// 	float mTimeToStartTargetingAgain = 0.4f;
//
// 	// 多个目标锁定后重发多次发射导弹,每次之间的间隔
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GSSecondaryInstantBussiness")
// 	float mTimeBetweenFastRockets = 0.2f;
//
// 	// 射击间隔(键鼠点按最低容忍间隔)
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GSSecondaryInstantBussiness")
// 	float mTimeBetweenShots = 0.4f;
// 	
// 	// 上次射击的具体时刻
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	float mTimeOfLastShot = 0.0f;
//
// 	// 单回合射击消耗的弹量
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	int32 mAmmoCost = 1;
//
// 	// 最大选定目标个数; 若设置为3,则瞄准第4个的时候会把最旧的第一个销毁掉, 转移至第4个
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	int32 mMaxTargets = 3;
//
// 	// 球型场景探查器
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class AGRBGATA_SphereTrace* mSphereTraceTargetActor = nullptr;
//
// 	// 关联的玩家控制器
// 	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
// 	class AGRBPlayerController* mGRBPlayerController = nullptr;
//
// private:
// 	UPROPERTY()
// 	FGameplayAbilityTargetDataHandle CachedTargetDataHandleWhenConfirmed = FGameplayAbilityTargetDataHandle();
// };
