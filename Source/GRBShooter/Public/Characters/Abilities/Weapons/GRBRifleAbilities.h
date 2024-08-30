// Copyright 2024 Dan Kestranek.

#pragma once

#include "Characters/Abilities/GRBGameplayAbility.h"
#include "GRBRifleAbilities.generated.h"


/**
 * 步枪持续射击从属技能;
 * 和UGA_GRBRiflePrimary主射击技能搭配使用;
 */
UCLASS()
class GRBSHOOTER_API UGA_GRBRiflePrimaryInstant : public UGRBGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_GRBRiflePrimaryInstant();
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
	void FireBullet();

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
	class UGA_GRBRiflePrimary* mGAPrimary = nullptr;

	// 异步任务: 服务器等待客户端发送来的目标数据并执行绑定的"索敌目标"委托
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class UGRBAT_ServerWaitForClientTargetData* mServerWaitTargetDataTask = nullptr;

	// 单回合射击消耗的弹量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	int32 mAmmoCost = 1;

	// 扩散调幅
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mAimingSpreadMod = 0.15f;

	// 单发伤害值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mBulletDamage = 10.0f;

	// 扩散增幅
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mFiringSpreadIncrement = 1.2f;

	// 扩散阈值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mFiringSpreadMax = 10.0f;

	// 上次的射击时刻
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mTimeOfLastShot = -99999999.0f;

	// 武器扩散基准系数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	float mWeaponSpread = 1.0f;

	// 和技能目标数据强关联的目标位置信息
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	struct FGameplayAbilityTargetingLocationInfo mTraceStartLocation;

	// 是否启用从视角摄像机追踪射线
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	bool mTraceFromPlayerViewPointg = true;

	// 瞄准时会授予的标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	FGameplayTag mAimingTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.Aiming"));

	// 瞄准移除时候会授予的标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	FGameplayTag mAimingRemovealTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Rifle.AimingRemoval"));

	// 副开火技能会用到的场景探查器
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="GRBPrimaryInstantBussiness")
	class AGRBGATA_LineTrace* mLineTraceTargetActor = nullptr;
};


/**
 * 步枪持续射击主技能;
 * 和UGA_GRBRiflePrimaryInstant从属射击技能搭配使用;
 */
UCLASS()
class GRBSHOOTER_API UGA_GRBRiflePrimary : public UGRBGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_GRBRiflePrimary();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

public:
	///--@brief 专门做的技能消耗检查; 载弹量不足以支撑本回合射击的情形,会主动激活Reload技能--/
	virtual bool GRBCheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const override;

	//
	const float& Getm_TimeBetweenShot() const { return m_TimeBetweenShot; }

protected:
	///--@brief 业务数据预处理--/
	void CheckAndSetupCacheables();

protected:
	///--@brief 按开火模式执行开火业务--/
	UFUNCTION()
	void ExecuteShootBussByFireMode();

	///--@brief 玩家松开键鼠输入后的回调业务--/
	UFUNCTION()
	void OnReleaseBussCallback(float InPayload_TimeHeld);

	///--@brief 循环射击回调入口; 会反复递归异步任务UAbilityTask_WaitDelay--/
	UFUNCTION()
	void ContinuouslyFireOneBulletCallback();

	//
	UFUNCTION()
	void ContinuouslyFireOneBulletCallbackV2();

	///--@brief 爆炸开火模式下 除开首发合批的后两次合批走的业务--/
	///--@brief 注意这里的机制射击; 爆炸射击(以三连发为例), 这回合的第1发是走的半自动的第一波合批, 第2发和第3发才是走的下下一波合批--/
	UFUNCTION()
	void PerRoundFireBurstBulletsCallback();

	///--@brief 爆炸开火多次Repeat情形下的单次业务--/
	UFUNCTION()
	void PerTimeBurstShootBussCallback(int32 PerformActionNumber);

	///--@brief 爆炸开火多次Repeat结束后的业务--/
	UFUNCTION()
	void OnFinishBurstShootBussCallback(int32 ActionNumber);

protected:
	// 与技能相关联的武器
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
	class AGRBWeapon* m_SourceWeapon = nullptr;

	// 射击间隔时长, 蓝图可配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
	float m_TimeBetweenShot = 0.1f;

	// 与开火主技能关联的 Instant副技能句柄
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
	struct FGameplayAbilitySpecHandle m_InstantAbilityHandle;

	// 与开火主技能关联的 Instant副技能
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
	class UGA_GRBRiflePrimaryInstant* m_InstantAbility = nullptr;

	// 单次射击消耗的子弹个数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RiflePrimary|Buss")
	int32 m_AmmoCost = 1;

private:
	// 异步节点WaitDelay:递归循环射击
	UPROPERTY()
	class UAbilityTask_WaitDelay* AsyncWaitDelayNode_ContinousShoot = nullptr;
};
