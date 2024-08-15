// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "WorldCollision.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GRBGATA_Trace.generated.h"

/**
 * TargetActor可以理解为一个场景信息探测器，用来获取场景中数据。它主要用于存储目标数据(一般是TArray >)、FHitResult
 * 是一个目标选择器
 *
 ** 可重复使用、可配置的跟踪 TargetActor。可供子类覆写.
 * 旨在与 GRBAT_WaitTargetDataUsingActor 一起使用，而不是默认的 WaitTargetData AbilityTask
 *
 * AGameplayAbilityTargetActor 是 Unreal Engine 中 GameplayAbilities 系统的一部分，用于处理技能的目标选择逻辑。
 * 它是一个抽象的基类，派生类可以实现具体的目标选择逻辑，例如选择一个位置、一个物体或一个区域
 *
 * 一般流程为：使用WaitTargetData_AbilityTask生成TargetActor，之后通过TargetActor的内部函数或者射线获取场景信息，
 * 最后通过委托传递携带这些信息构建的FGameplayAbilityTargetDataHandle
 * 一般供 Task Wait Target Data这些AbilityTask调用
 * Reusable, configurable trace TargetActor. Subclass with your own trace shapes.
 * Meant to be used with GRBAT_WaitTargetDataUsingActor instead of the default WaitTargetData AbilityTask as the default
 * one will destroy the TargetActor.
 */
UCLASS()
class GRBSHOOTER_API AGRBGATA_Trace : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

private:
	// 在目标选择行为完成后，触发相应的回调，通知能力系统目标数据已经准备好，可以继续执行与该目标相关的游戏逻辑
	// FAbilityTargetData	TargetDataReadyDelegate;
	// 在玩家取消目标选择时触发CanceledDelegate。
	// FAbilityTargetData	CanceledDelegate;
	
public:
	AGRBGATA_Trace();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartTargeting(UGameplayAbility* Ability) override;///--@brief 覆写; 开始探查器的探查准备工作并开启tick.--/
	virtual void ConfirmTargetingAndContinue() override;
	virtual void CancelTargeting() override;

	///--@brief 复位弹道扩散参数状态 --/
	UFUNCTION(BlueprintCallable)
	virtual void ResetSpread();

	///--@brief 解算出当下的扩散系数 --/
	virtual float GetCurrentSpread() const;

	// 设置 瞄准开始位置参数包
	// Expose to Blueprint
	UFUNCTION(BlueprintCallable)
	void SetStartLocation(const FGameplayAbilityTargetingLocationInfo& InStartLocation);

	///--@brief 设置是否在服务端生成目标数据--/
	// Expose to Blueprint
	UFUNCTION(BlueprintCallable)
	virtual void SetShouldProduceTargetDataOnServer(bool bInShouldProduceTargetDataOnServer);

	///--@brief 用于控制目标选择 Actor 的生命周期管理。这一标志决定了当目标选择得到确认后，是否自动销毁目标选择器--/
	// Expose to Blueprint
	UFUNCTION(BlueprintCallable)
	void SetDestroyOnConfirmation(bool bInDestroyOnConfirmation = false);

	///--@brief 会先做复合射线检测并同时筛选出命中了actor的那些Hits--/
	// Traces as normal, but will manually filter all hit actors
	virtual void LineTraceWithFilter(TArray<FHitResult>& OutHitResults, const UWorld* World, const FGameplayTargetDataFilterHandle FilterHandle, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams Params);

	///--@brief 工具方法: 裁剪相机Ray在射距内--/
	virtual bool ClipCameraRayToAbilityRange(FVector CameraLocation, FVector CameraDirection, FVector CustomPos, float AbilityRange, FVector& ClippedPosition);

	///--@brief 经过一系列对于瞄朝向的射击调调校; 输出最终的TraceEnd位置--/
	virtual void AimWithPlayerController(const AActor* InSourceActor, FCollisionQueryParams Params, const FVector& TraceStart, FVector& OutTraceEnd, bool bIgnorePitch = false);

	///--@brief 主动停止目标选择并进行一系列数据/委托清理--/
	virtual void StopTargeting();

protected:
	///--@brief 为一组命中hit制作 目标数据句柄 并存储它们.--/
	virtual FGameplayAbilityTargetDataHandle MakeTargetData(const TArray<FHitResult>& HitResults) const;

	///--@brief 重要函数; 场景探查器每帧都会执行的trace最终入口. --/
	virtual TArray<FHitResult> PerformTrace(AActor* InSourceActor);

	///--@brief 执行trace, 纯虚函数--/
	virtual void DoTrace(TArray<FHitResult>& HitResults, const UWorld* World, const FGameplayTargetDataFilterHandle FilterHandle, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams Params) PURE_VIRTUAL(AGRBGATA_Trace, return;);

	///--@brief 是否debug trace, 纯虚函数--/
	virtual void ShowDebugTrace(TArray<FHitResult>& HitResults, EDrawDebugTrace::Type DrawDebugType, float Duration = 2.0f) PURE_VIRTUAL(AGRBGATA_Trace, return;);
	
	///--@brief 在指定位置生成1个可视化的3D准星--/
	virtual AGameplayAbilityWorldReticle* SpawnReticleActor(FVector Location, FRotator Rotation);
	
	///--@brief 把一组可视化3D准星从后到前挨个销毁--/
	virtual void DestroyReticleActors();

public:
	// 基础扩散因子  Base targeting spread (degrees)
	UPROPERTY(BlueprintReadWrite, Category = "Accuracy")
	float BaseSpread;

	// 瞄准扩散模组器,可修改进阶系数; Aiming spread modifier
	UPROPERTY(BlueprintReadWrite, Category = "Accuracy")
	float AimingSpreadMod;

	// Continuous targeting: spread increment
	UPROPERTY(BlueprintReadWrite, Category = "Accuracy")
	float TargetingSpreadIncrement;

	// Continuous targeting: max increment
	UPROPERTY(BlueprintReadWrite, Category = "Accuracy")
	float TargetingSpreadMax;

	// 由持续射击造成的当前扩散因子; Current spread from continuous targeting
	float CurrentTargetingSpread;

	// 是否启用瞄准扩散
	UPROPERTY(BlueprintReadWrite, Category = "Accuracy")
	bool bUseAimingSpreadMod;

	// 瞄准时候给到的标签
	UPROPERTY(BlueprintReadWrite, Category = "Accuracy")
	FGameplayTag AimingTag;

	// 取消瞄准的时候给到的标签
	UPROPERTY(BlueprintReadWrite, Category = "Accuracy")
	FGameplayTag AimingRemovalTag;

	// 最大射距
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Trace")
	float MaxRange;

	// trace业务用到的 碰撞profile
	UPROPERTY(BlueprintReadWrite, EditAnywhere, config, meta = (ExposeOnSpawn = true), Category = "Trace")
	FCollisionProfileName TraceProfile;

	// 是否启用瞄准影响pitch
	// Does the trace affect the aiming pitch
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Trace")
	bool bTraceAffectsAimPitch;

	// 可配置的承认的本回合的最大命中个数
	// Maximum hit results to return per trace. 0 just returns the trace end point.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Trace")
	int32 m_MaxAcknowledgeHitNums;

	// 可配置的每回合射线次数; 比如步枪单回合就是Trace1次, 霰弹枪则是单回合Trace多次
	// Number of traces to perform at one time. Single bullet weapons like rilfes will only do one trace.
	// Multi-bullet weapons like shotguns can do multiple traces. Not intended to be used with PersistentHits.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Trace")
	int32 NumberOfTraces;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Trace")
	bool bIgnoreBlockingHits;

	// 是否从视角开始Trace
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Trace")
	bool bTraceFromPlayerViewPoint;

	// 在目标选中/Cancel前是否采用持续trace
	// HitResults will persist until Confirmation/Cancellation or until a new HitResult takes its place
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Trace")
	bool bUseMechanism_PersistHits;

protected:
	// Trace End point, useful for debug drawing
	FVector CurrentTraceEnd;
	
	// 一组可视化的3D准星Actor
	TArray<TWeakObjectPtr<AGameplayAbilityWorldReticle>> ReticleActors;
	
	// 在持续trace情况下, 保存到的一组hit结果(这里设计为队列, 认新加进来的命中结果)
	TArray<FHitResult> m_QueuePersistHits;
};
