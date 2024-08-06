// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GRBAbilitySystemComponent.generated.h"

class USkeletalMeshComponent;

/**
* Data about montages that were played locally (all montages in case of server. predictive montages in case of client). Never replicated directly.
* 所有关联本地蒙太奇的数据(所有蒙太奇都在服务器上, 客户端仅负责预测); 并非直接网络同步
*/
USTRUCT()
struct GRBSHOOTER_API FGameplayAbilityLocalAnimMontageForMesh
{
	GENERATED_BODY()

public:
	UPROPERTY()
	USkeletalMeshComponent* Mesh;

	///@brief （GRB）中用于处理本地动画蒙太奇（montage）的结构体;当你在GA执行过程中需要触发某些动画效果时，可以使用这个结构体来处理动画播放、暂停、停止等操作
	UPROPERTY()
	FGameplayAbilityLocalAnimMontage LocalMontageInfo;

	FGameplayAbilityLocalAnimMontageForMesh() : Mesh(nullptr), LocalMontageInfo()
	{
	}

	FGameplayAbilityLocalAnimMontageForMesh(USkeletalMeshComponent* InMesh)
		: Mesh(InMesh), LocalMontageInfo()
	{
	}

	FGameplayAbilityLocalAnimMontageForMesh(USkeletalMeshComponent* InMesh, FGameplayAbilityLocalAnimMontage& InLocalMontageInfo)
		: Mesh(InMesh), LocalMontageInfo(InLocalMontageInfo)
	{
	}
};

/**
* Data about montages that is replicated to simulated clients.
* 专门用于处理在网络环境下的动画蒙太奇（montage）同步; 支持在多玩家游戏中同步动画蒙太奇的播放状态，包括播放、停止、暂停和恢复等操作。FGameplayAbilityRepAnimMontage 确保了客户端和服务器之间动画状态的一致性
* 通过自定义和扩展 ActivateAbility、StartMontage、StopMontage 和 EndAbility 方法，可以实现复杂的动画同步逻辑，提供更丰富的游戏体验
*/
USTRUCT()
struct GRBSHOOTER_API FGameplayAbilityRepAnimMontageForMesh
{
	GENERATED_BODY()
	;

public:
	UPROPERTY()
	USkeletalMeshComponent* Mesh;

	UPROPERTY()
	FGameplayAbilityRepAnimMontage RepMontageInfo;

	FGameplayAbilityRepAnimMontageForMesh() : Mesh(nullptr), RepMontageInfo()
	{
	}

	FGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh)
		: Mesh(InMesh), RepMontageInfo()
	{
	}
};


/**
 * 工程自己用的ASC技能组件
 */
UCLASS()
class GRBSHOOTER_API UGRBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UGRBAbilitySystemComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetShouldTick() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** 用于初始化与特定角色和控制器相关联的能力信息。该方法通常在角色初始化完成、或者需要手动初始化能力信息时调用，以确保能力系统正确地关联到游戏角色和控制器.*/
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	/** 在一个能力（Ability）结束时调用，用于通知系统该能力已经完成、取消或中止。通过这一通知，系统可以进行一些清理工作、触发回调.*/
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	/** 0.主要功能是响应本地输入,
	 * 1.并尝试激活对应的游戏能力; 检查输入绑定：根据 InputID 查找是否有与之关联的能力。
	 * 2.尝试激活能力：如果找到绑定的能力，尝试激活它。这涉及检查能力是否满足激活条件，诸如冷却时间是否完成、资源是否充足等。
	 * 3.调试信息：输出和记录输入相关的调试信息
	 */
	virtual void AbilityLocalInputPressed(int32 InputID) override;
	// 决定了是否应该批处理来自客户端的 RPC 请求，将多个请求合并成一个，以减少网络通信的开销。在多人游戏中，这种优化非常重要，可以显著减少网络延迟和带宽使用; Turn on RPC batching in ASC. Off by default.
	virtual bool ShouldDoServerAbilityRPCBatch() const override;

	// Version of function in AbilitySystemGlobals that returns correct type
	/** 从 Actor 获取能力系统组件：该方法用来快速获取一个 Actor 的能力系统组件，无需手动遍历 Actor 的组件.*/
	static UGRBAbilitySystemComponent* GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent = false);

	/** 获取特定类型GA的技能句柄.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Abilities")
	FGameplayAbilitySpecHandle FindAbilitySpecHandleForClass(TSubclassOf<UGameplayAbility> AbilityClass, UObject* OptionalSourceObject = nullptr);

	///@brief 采用一种思想:把同一帧内的所有RPC合批, 最佳情况是，我们将 ActivateAbility、SendTargetData 和 EndAbility 批处理为一个 RPC，而不是三个
	///@brief 最坏情况是，我们将 ActivateAbility 和 SendTargetData 批处理为一个 RPC，而不是两个，然后在单独的 RPC 中调用 EndAbility
	///@brief 单发（又或者是半自动）将 ActivateAbility、SendTargetData 和 EndAbility 组合成一个 RPC，而不是三个
	///@brief 全自动射击模式: 首发子弹射击是 把ActivateAbility and SendTargetData合批为一个RPC; 从第2发开始的子弹都是每一发RPC for SendTargetData; 第30发即最后一发采用的是 RPC for the EndAbility 
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	virtual bool BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately);

	///@brief 检测ASC内预测Key,返回字符串通知
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual FString GetCurrentPredictionKeyStatus();

	/** ///@brief 给Self施加带预测性质效果的BUFF
	* If this ASC has a valid prediction key, attempt to predictively apply this GE. Used in Pre/PostInteract() on the Interacter's ASC.
	* If the key is not valid, it will apply the GE without prediction.
	*/
	UFUNCTION(BlueprintCallable, Category = "GameplayEffects", Meta = (DisplayName = "ApplyGameplayEffectToSelfWithPrediction"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectToSelfWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle EffectContext);

	/** ///@brief 给目标ASC施加带预测性质效果的BUFF
	* If this ASC has a valid prediction key, attempt to predictively apply this GE to the target. Used in Pre/PostInteract() on the Interacter's ASC.
	* If the key is not valid, it will apply the GE to the target without prediction.
	*/
	UFUNCTION(BlueprintCallable, Category = "GameplayEffects", Meta = (DisplayName = "ApplyGameplayEffectToTargetWithPrediction"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectToTargetWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, UAbilitySystemComponent* Target, float Level, FGameplayEffectContextHandle Context);


#pragma region ~ 对ASC作用目标多骨骼的蒙太奇动画技术支持 ~
	// ----------------------------------------------------------------------------------------------------------------
	//  以下部分是 对ASC作用目标多骨骼的蒙太奇动画技术支持
	// ----------------------------------------------------------------------------------------------------------------	

	///--@brief 依赖于技能系统, 播放蒙太奇并着手处理传入技能信息的网络复制和预测--/
	// Plays a montage and handles replication and prediction based on passed in ability/activation info
	virtual float PlayMontageForMesh(UGameplayAbility* AnimatingAbility, class USkeletalMeshComponent* InMesh, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None, bool bReplicateMontage = true);

	///--@brief 在模拟端; 让骨架的动画实例播指定蒙太奇动画,同时将其更新至本地包体的蒙太奇资产--/
	// Plays a montage without updating replication/prediction structures. Used by simulated proxies when replication tells them to play a montage.
	virtual float PlayMontageSimulatedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	///--@brief 停播骨架关联的蒙太奇,--/
	// Stops whatever montage is currently playing. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime = -1.0f);

	///--@brief 停播本地所有蒙太奇动画.--/
	// Stops all montages currently playing
	virtual void StopAllCurrentMontages(float OverrideBlendOutTime = -1.0f);

	///--@brief 停掉指定骨架的指定蒙太奇播放.--/
	// Stops current montage if it's the one given as the Montage param
	virtual void StopMontageIfCurrentForMesh(USkeletalMeshComponent* InMesh, const UAnimMontage& Montage, float OverrideBlendOutTime = -1.0f);

	///--@brief 给定1各技能, 复位其关联的本地蒙太奇数据.--/
	// Clear the animating ability that is passed in, if it's still currently animating
	virtual void ClearAnimatingAbilityForAllMeshes(UGameplayAbility* Ability);

	///--@brief 把给定骨架的蒙太奇跳转至指定section播放, 在双端做到数据统一.--/
	// Jumps current montage to given section. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName);

	///--@brief 把给定骨架的蒙太奇设置下一个section, 并在双端做到数据统一.--/
	// Sets current montages next section name. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, FName FromSectionName, FName ToSectionName);

	///--@brief 把给定骨架的蒙太奇设置指定的播放速率, 并在双端做到数据统一.--/
	// Sets current montage's play rate
	virtual void CurrentMontageSetPlayRateForMesh(USkeletalMeshComponent* InMesh, float InPlayRate);

	///--@brief 校验给定的动画技能是否隶属于本地包体池子内的某个元素. --/
	// Returns true if the passed in ability is the current animating ability
	bool IsAnimatingAbilityForAnyMesh(UGameplayAbility* Ability) const;

	///--@brief 在本地包体池子内提取出当前唯一一个合法的动画技能.--/
	// Returns the current animating ability
	UGameplayAbility* GetAnimatingAbilityFromAnyMesh();

	///--@brief 收集本地包体池子内正在播放已激活的蒙太奇资产.--/
	// Returns montages that are currently playing
	TArray<UAnimMontage*> GetCurrentMontages() const;

	///--@brief 拿取给定骨架正在播放的蒙太奇资产.--/
	// Returns the montage that is playing for the mesh
	UAnimMontage* GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh);

	///--@brief 拿取给定骨架本地动画正在播放的SectionID--/
	// Get SectionID of currently playing AnimMontage
	int32 GetCurrentMontageSectionIDForMesh(USkeletalMeshComponent* InMesh);

	///--@brief 拿取给定骨架本地动画正在播放的Section资产名字--/
	// Get SectionName of currently playing AnimMontage
	FName GetCurrentMontageSectionNameForMesh(USkeletalMeshComponent* InMesh);

	///--@brief 拿取指定骨架的正在播放的这段section总时长. --/
	// Get length in time of current section
	float GetCurrentMontageSectionLengthForMesh(USkeletalMeshComponent* InMesh);

	///--@brief 拿取给定骨架的的section的剩余未播放时长.--/
	// Returns amount of time left in current section
	float GetCurrentMontageSectionTimeLeftForMesh(USkeletalMeshComponent* InMesh);

protected:
	// ----------------------------------------------------------------------------------------------------------------
	//	AnimMontage Support for multiple USkeletalMeshComponents on the AvatarActor.
	//  Only one ability can be animating at a time though?
	// ----------------------------------------------------------------------------------------------------------------	

	///--@brief 在本地蒙太奇包池子内 查找匹配特定骨架的那个池子元素;--/
	// Finds the existing FGameplayAbilityLocalAnimMontageForMesh for the mesh or creates one if it doesn't exist
	FGameplayAbilityLocalAnimMontageForMesh& GetLocalAnimMontageInfoForMesh(USkeletalMeshComponent* InMesh);

	///--@brief 在联网蒙太奇包池子内 查找匹配特定骨架的那个池子元素;--/
	// Finds the existing FGameplayAbilityRepAnimMontageForMesh for the mesh or creates one if it doesn't exist
	FGameplayAbilityRepAnimMontageForMesh& GetGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh);

	///@brief 当本地正在播放的蒙太奇预测被rejected则立刻停止蒙太奇并淡出0.25秒混合
	void OnPredictiveMontageRejectedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* PredictiveMontage);

	///--@brief 专门在服务端负责处理给定骨架关联包体的数据更新, 并复制到客户端, 并在网络端计算出下个section的数据--/
	void AnimMontage_UpdateReplicatedDataForMesh(USkeletalMeshComponent* InMesh);

	///--@brief 专门负责处理给定网络蒙太奇包的数据更新, 并复制到客户端, 并在网络端计算出下个section的数据--/
	// Copy LocalAnimMontageInfo into RepAnimMontageInfo
	void AnimMontage_UpdateReplicatedDataForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo);

	// Copy over playing flags for duplicate animation data
	void AnimMontage_UpdateForcedPlayFlagsForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo);

	///--@brief 准备好进行同步蒙太奇包的判断函数, 可供子类覆写--/
	// Returns true if we are ready to handle replicated montage information
	virtual bool IsReadyForReplicatedMontageForMesh();

	///--@brief RPC, 在服务端执行, 设置动画实例下个section并手动校准一些错误section情形且校准时长位置, 并下发至各SimulatedProxy更新包体数据和section数据--/
	// RPC function called from CurrentMontageSetNextSectionName, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);
	void ServerCurrentMontageSetNextSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);
	bool ServerCurrentMontageSetNextSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);

	///--@brief RPC, 在服务端执行, 设置动画实例跳转至给定section, 并下发至各SimulatedProxy更新包体数据和section数据--/
	// RPC function called from CurrentMontageJumpToSection, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageJumpToSectionNameForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);
	void ServerCurrentMontageJumpToSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);
	bool ServerCurrentMontageJumpToSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);

	///--@brief RPC, 在服务端执行, 设置动画实例的蒙太奇播放速率, 并下发至各SimulatedProxy更新包体数据和section数据--/
	// RPC function called from CurrentMontageSetPlayRate, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageSetPlayRateForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);
	void ServerCurrentMontageSetPlayRateForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);
	bool ServerCurrentMontageSetPlayRateForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);

	///--@brief 当动画蒙太奇同步下来后会进到的客户端回调--/
	UFUNCTION()
	virtual void OnRep_ReplicatedAnimMontageForMesh();
#pragma endregion ~ 对ASC作用目标多骨骼的蒙太奇动画技术支持 ~

	//---------------------------------------------------  ------------------------------------------------
	//---------------------------------------------------  ------------------------------------------------
	//---------------------------------------------------  ------------------------------------------------

#pragma region ~ 对外接口 ~

public:
	/** 获取在1个容器下某个Tag的出现次数*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Abilities", Meta = (DisplayName = "GetTagCount", ScriptName = "GetTagCount"))
	int32 K2_GetTagCount(FGameplayTag TagToCheck) const;

	// Exposes AddLooseGameplayTag to Blueprint. This tag is *not* replicated.
	UFUNCTION(BlueprintCallable, Category = "Abilities", Meta = (DisplayName = "AddLooseGameplayTag"))
	void K2_AddLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count = 1);

	// Exposes AddLooseGameplayTags to Blueprint. These tags are *not* replicated.
	UFUNCTION(BlueprintCallable, Category = "Abilities", Meta = (DisplayName = "AddLooseGameplayTags"))
	void K2_AddLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count = 1);

	// Exposes RemoveLooseGameplayTag to Blueprint. This tag is *not* replicated.
	UFUNCTION(BlueprintCallable, Category = "Abilities", Meta = (DisplayName = "RemoveLooseGameplayTag"))
	void K2_RemoveLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count = 1);

	// Exposes RemoveLooseGameplayTags to Blueprint. These tags are *not* replicated.
	UFUNCTION(BlueprintCallable, Category = "Abilities", Meta = (DisplayName = "RemoveLooseGameplayTags"))
	void K2_RemoveLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count = 1);

	///@brief 激活对应的Cue-Executed
	UFUNCTION(BlueprintCallable, Category = "GameplayCue", Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void ExecuteGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters);

	///@brief 激活对应的Cue-OnActive
	UFUNCTION(BlueprintCallable, Category = "GameplayCue", Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void AddGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters);

	///@brief 激活对应的Cue-Removed
	UFUNCTION(BlueprintCallable, Category = "GameplayCue", Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void RemoveGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters);
#pragma endregion

	//---------------------------------------------------  ------------------------------------------------
	//---------------------------------------------------  ------------------------------------------------

#pragma region ~ 字段 ~

public:
	// 是否授权技能
	bool bCharacterAbilitiesGiven = false;

	// 是否玩家初始化时刻给予固有BUFF
	bool bStartupEffectsApplied = false;

protected:
	// Set if montage rep happens while we don't have the animinstance associated with us yet
	UPROPERTY()
	bool bPendingMontageRepForMesh;

	// 本地发起的蒙太奇的数据结构（如果是服务器则为所有内容，如果是客户端则为预测内容，如果是模拟端则为复制内容; AvatarActor 上每个骨架网格最多一个元素
	UPROPERTY()
	TArray<FGameplayAbilityLocalAnimMontageForMesh> LocalAnimMontageInfoForMeshes;

	// 接收网络复制; 用于将蒙太奇信息从服务端复制到模拟客户端的数据结构；AvatarActor 上每个骨架网格最多一个元素
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedAnimMontageForMesh)
	TArray<FGameplayAbilityRepAnimMontageForMesh> RepAnimMontageInfoForMeshes;
#pragma endregion
};
