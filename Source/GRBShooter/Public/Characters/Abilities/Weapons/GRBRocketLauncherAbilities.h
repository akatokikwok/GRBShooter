// Copyright 2024 Dan Kestranek.

#pragma once

#include "Characters/Abilities/GRBGameplayAbility.h"
#include "GRBRocketLauncherAbilities.generated.h"


#pragma region ~ 火箭筒开火 ~
/**
 * 火箭筒开火副技能;
 * 和UGA_GRBRocketLauncherPrimary主射击技能搭配使用;
 */
UCLASS()
class GRBSHOOTER_API UGA_GRBRocketLauncherPrimaryInstant : public UGRBGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_GRBRocketLauncherPrimaryInstant();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	///--@brief 负担技能消耗的检查--/
	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;

	///--@brief 技能消耗成本扣除: 刷新残余载弹量扣除每回合发动时候的弹药消耗量--/
	virtual void GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

public:
	// 每回合/每次射击子弹的调度业务
	UFUNCTION(BlueprintCallable)
	void FireRocket();

	// 手动终止技能以及异步任务
	UFUNCTION(BlueprintCallable)
	void ManuallyKillInstantGA();

private:
	///--@brief 双端都会调度到的 处理技能目标数据的复合逻辑入口--/
	/**
 	 * 播放蒙太奇动画
 	 * 伤害BUFF应用
 	 * 播放诸如关联枪支火焰CueTag的所有特效
 	 */
	UFUNCTION(BlueprintCallable)
	void HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle);

	// 播放项目定制的蒙太奇.
	UFUNCTION(BlueprintCallable)
	void PlayFireMontage();

	// 触发技能时候的数据预准备
	UFUNCTION(BlueprintCallable)
	void CheckAndSetupCacheables();

public:
	// 武器1P视角下的枪皮
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class USkeletalMeshComponent* Weapon1PMesh = nullptr;

	// 武器3P视角下的枪皮
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class USkeletalMeshComponent* Weapon3PMesh = nullptr;

	// 开火枪支
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class AGRBWeapon* mSourceWeapon = nullptr;

	// 枪手
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class AGRBHeroCharacter* mOwningHero = nullptr;

	// 主开火技能
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class UGA_GRBRocketLauncherPrimary* mGAPrimary = nullptr;

	// 异步任务: 服务器等待客户端发送来的目标数据并执行绑定的"索敌目标"委托
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class UGRBAT_ServerWaitForClientTargetData* mServerWaitTargetDataTask = nullptr;

	// 上次的射击时刻
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mTimeOfLastShot = 0.f;

	// 单回合射击消耗的弹量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	int32 mAmmoCost = 1;

	// 单发火箭筒弹丸伤害值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mRocketDamage = 60.0f;

	// 和技能目标数据强关联的目标位置信息
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	struct FGameplayAbilityTargetingLocationInfo mTraceStartLocation;

	// 是否启用从视角摄像机追踪射线-默认为否
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	bool mTraceFromPlayerViewPointg = false;

	// 副开火技能会用到的场景探查器
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class AGRBGATA_LineTrace* mLineTraceTargetActor = nullptr;
};


/**
 * 火箭筒的开火主技能
 */
UCLASS(Blueprintable, BlueprintType)
class GRBSHOOTER_API UGA_GRBRocketLauncherPrimary : public UGRBGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_GRBRocketLauncherPrimary();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;

	const float& Getm_TimeBetweenShot() const { return m_TimeBetweenShot; }

protected:
	///--@brief 业务数据预处理--/
	void CheckAndSetupCacheables();

private:
	///--@brief 按开火模式执行开火业务--/
	UFUNCTION()
	void ExecuteShootBussByFireMode();

protected:
	// 与技能相关联的武器
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
	class AGRBWeapon* m_SourceWeapon = nullptr;

	// 射击间隔时长, 蓝图可配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
	float m_TimeBetweenShot = 0.3f;

	// 与开火主技能关联的 Instant副技能句柄
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
	struct FGameplayAbilitySpecHandle m_InstantAbilityHandle;

	// 与开火主技能关联的 Instant副技能
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
	class UGA_GRBRocketLauncherPrimaryInstant* m_InstantAbility = nullptr;

	// 单次射击消耗的子弹个数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RocketLauncherPrimary|Buss")
	int32 m_AmmoCost = 1;
};
#pragma endregion


#pragma region ~ 火箭筒瞄准索敌并等待发射 ~
/**
 * 火箭筒的瞄准次要技能
 */
UCLASS(Blueprintable, BlueprintType)
class GRBSHOOTER_API UGA_GRBRocketLauncherSecondary : public UGRBGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_GRBRocketLauncherSecondary();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	///--@brief 负担技能消耗的检查--/
	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;

	///--@brief 技能消耗成本扣除: 刷新残余载弹量扣除每回合发动时候的弹药消耗量--/
	virtual void GRBApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

public:
	// 每回合/每次进行 右键索敌瞄准技能数据的综合入口
	// 制造场景探查器并构建技能目标数据
	UFUNCTION(BlueprintCallable)
	void StartTargeting();

	// 手动终止技能以及异步任务
	UFUNCTION(BlueprintCallable)
	void ManuallyKillInstantGA();

private:
	///--@brief 双端都会调度到的 处理技能目标数据的复合逻辑入口--/
	/**
 	 * 播放蒙太奇动画
 	 * 伤害BUFF应用
 	 * 播放诸如关联枪支火焰CueTag的所有特效
 	 */
	UFUNCTION(BlueprintCallable)
	void HandleTargetData(const FGameplayAbilityTargetDataHandle& InTargetDataHandle);

	UFUNCTION(BlueprintCallable)
	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& InTargetDataHandle);

	///--@brief 当右键索敌确认后,左键按下发射多次追踪弹的单次业务--/
	UFUNCTION(Blueprintable)
	void HandleTargetData_PerformAction(const int32 InActionNumber);

	///--@brief 当发射多个追踪弹结束之后的检测业务--/
	UFUNCTION(Blueprintable)
	void HandleTargetData_OnFinishFireHomingRocketBullet(const int32 InActionNumber);

	// 播放项目定制的蒙太奇.
	UFUNCTION(BlueprintCallable)
	void PlayFireMontage();

	// 触发技能时候的数据预准备
	UFUNCTION(BlueprintCallable)
	void CheckAndSetupCacheables();

	UFUNCTION()
	void OnManuallyStopRocketSearch(float InTimeHeld);

public:
	// 武器1P视角下的枪皮
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness|Components")
	class USkeletalMeshComponent* Weapon1PMesh = nullptr;

	// 武器3P视角下的枪皮
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness|Components")
	class USkeletalMeshComponent* Weapon3PMesh = nullptr;

	// 关联的GRB-ASC
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness|Components")
	class UGRBAbilitySystemComponent* ASC = nullptr;

	// 枪手
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class AGRBHeroCharacter* mOwningHero = nullptr;

	// 开火枪支
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class AGRBWeapon* mSourceWeapon = nullptr;

	// 已激活的瞄准BUFF
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	struct FActiveGameplayEffectHandle mGE_AimingBuffHandle = FActiveGameplayEffectHandle();

	// 已激活的瞄准移除BUFF
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	struct FActiveGameplayEffectHandle mGE_AimingRemovalBuffHandle = FActiveGameplayEffectHandle();

	// 异步任务: 第一人称下的 等待切换FOV
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class UGRBAT_WaitChangeFOV* m1PZoomInTask = nullptr;

	// 异步任务: 第一人称下的 复位视角
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class UGRBAT_WaitChangeFOV* m1PZoomResetTask = nullptr;

	// 欲切换到的FOV视场角1p
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	float mTargeting1PFOV = 60.f;

	// 哪一张瞄准BUFF
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	TSubclassOf<UGameplayEffect> mGE_Aiming_Class;

	// 哪一张瞄准移除BUFF
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	TSubclassOf<UGameplayEffect> mGE_AimingRemoval_Class;

	// 哪一张瞄准HUD准星UI
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	TSubclassOf<class UGRBHUDReticle> mAimingHUDReticle;

	// 异步任务:用来模拟重复射击火箭筒弹丸
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class UAbilityTask_Repeat* mFireRocketRepeatActionTask = nullptr;

	// 异步任务: 服务器等待客户端发送来的目标数据并执行绑定的"索敌目标"委托
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class UGRBAT_ServerWaitForClientTargetData* mServerWaitTargetDataTask = nullptr;

	// 异步任务: 等待探查器数据并据此完成双端射击任务
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class UGRBAT_WaitTargetDataUsingActor* mWaitTargetDataWithActorTask = nullptr;

	// 和技能目标数据强关联的目标位置信息
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	struct FGameplayAbilityTargetingLocationInfo mTraceStartLocation;

	// 是否启用从视角摄像机追踪射线
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	bool mTraceFromPlayerViewPointg = false;

	// 最大射距
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	float mMaxRange = 3000.f;

	// 单发弹丸伤害
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	float mRocketDamage = 60.0f;

	// 索敌数据销毁后再次StartTargeting的容忍延迟间隔时长
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	float mTimeToStartTargetingAgain = 0.4f;

	// 多个目标锁定后重发多次发射导弹,每次之间的间隔
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	float mTimeBetweenFastRockets = 0.2f;

	// 射击间隔(键鼠点按最低容忍间隔)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	float mTimeBetweenShots = 0.4f;

	// 上次射击的具体时刻
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	float mTimeOfLastShot = 0.0f;

	// 单回合射击消耗的弹量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	int32 mAmmoCost = 1;

	// 最大选定目标个数; 若设置为3,则瞄准第4个的时候会把最旧的第一个销毁掉, 转移至第4个
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	int32 mMaxTargets = 3;

	// 球型场景探查器
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class AGRBGATA_SphereTrace* mSphereTraceTargetActor = nullptr;

	// 关联的玩家控制器
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBSecondaryInstantBussiness")
	class AGRBPlayerController* mGRBPlayerController = nullptr;

private:
	UPROPERTY()
	FGameplayAbilityTargetDataHandle CachedTargetDataHandleWhenConfirmed = FGameplayAbilityTargetDataHandle();
};
#pragma endregion
