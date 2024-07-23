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
	GENERATED_BODY();

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

	// ~Start Implements ActorComponent::GetLifetimeReplicatedProps
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// ~End Implements

	// ~Start Implements UGameplayTasksComponent::GetShouldTick
	virtual bool GetShouldTick() const override;
	// ~End Implements

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ~Start Implements UAbilitySystemComponent::InitAbilityActorInfo
	/** 用于初始化与特定角色和控制器相关联的能力信息。该方法通常在角色初始化完成、或者需要手动初始化能力信息时调用，以确保能力系统正确地关联到游戏角色和控制器.*/
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	// ~End Implements

	// ~Start Implements UAbilitySystemComponent::NotifyAbilityEnded 
	/** 在一个能力（Ability）结束时调用，用于通知系统该能力已经完成、取消或中止。通过这一通知，系统可以进行一些清理工作、触发回调.*/
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	// ~End Implements

	// Version of function in AbilitySystemGlobals that returns correct type
	/** 从 Actor 获取能力系统组件：该方法用来快速获取一个 Actor 的能力系统组件，无需手动遍历 Actor 的组件.*/
	static UGRBAbilitySystemComponent* GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent = false);

	// Input bound to an ability is pressed
	// ~Start Implements UAbilitySystemComponent::AbilityLocalInputPressed
	/** 0.主要功能是响应本地输入,
	 * 1.并尝试激活对应的游戏能力; 检查输入绑定：根据 InputID 查找是否有与之关联的能力。
	 * 2.尝试激活能力：如果找到绑定的能力，尝试激活它。这涉及检查能力是否满足激活条件，诸如冷却时间是否完成、资源是否充足等。
	 * 3.调试信息：输出和记录输入相关的调试信息
	 */
	virtual void AbilityLocalInputPressed(int32 InputID) override;
	// ~End Implements

	// Exposes GetTagCount to Blueprint
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Abilities", Meta = (DisplayName = "GetTagCount", ScriptName = "GetTagCount"))
	int32 K2_GetTagCount(FGameplayTag TagToCheck) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Abilities")
	FGameplayAbilitySpecHandle FindAbilitySpecHandleForClass(TSubclassOf<UGameplayAbility> AbilityClass, UObject* OptionalSourceObject=nullptr);

	// Turn on RPC batching in ASC. Off by default.
	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }

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

	// Attempts to activate the given ability handle and batch all RPCs into one. This will only batch all RPCs that happen
	// in one frame. Best case scenario we batch ActivateAbility, SendTargetData, and EndAbility into one RPC instead of three.
	// Worst case we batch ActivateAbility and SendTargetData into one RPC instead of two and call EndAbility later in a separate
	// RPC. If we can't batch SendTargetData or EndAbility with ActivateAbility because they don't happen in the same frame due to
	// latent ability tasks for example, then batching doesn't help and we should just activate normally.
	// Single shots (semi auto fire) combine ActivateAbility, SendTargetData, and EndAbility into one RPC instead of three.
	// Full auto shots combine ActivateAbility and SendTargetData into one RPC instead of two for the first bullet. Each subsequent
	// bullet is one RPC for SendTargetData. We then send one final RPC for the EndAbility when we're done firing.
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	virtual bool BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately);

	UFUNCTION(BlueprintCallable, Category = "GameplayCue", Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void ExecuteGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters);

	UFUNCTION(BlueprintCallable, Category = "GameplayCue", Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void AddGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters);

	UFUNCTION(BlueprintCallable, Category = "GameplayCue", Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void RemoveGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual FString GetCurrentPredictionKeyStatus();

	/**
	* If this ASC has a valid prediction key, attempt to predictively apply this GE. Used in Pre/PostInteract() on the Interacter's ASC.
	* If the key is not valid, it will apply the GE without prediction.
	*/
	UFUNCTION(BlueprintCallable, Category = "GameplayEffects", Meta = (DisplayName = "ApplyGameplayEffectToSelfWithPrediction"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectToSelfWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle EffectContext);

	/**
	* If this ASC has a valid prediction key, attempt to predictively apply this GE to the target. Used in Pre/PostInteract() on the Interacter's ASC.
	* If the key is not valid, it will apply the GE to the target without prediction.
	*/
	UFUNCTION(BlueprintCallable, Category = "GameplayEffects", Meta = (DisplayName = "ApplyGameplayEffectToTargetWithPrediction"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectToTargetWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, UAbilitySystemComponent* Target, float Level, FGameplayEffectContextHandle Context);


	// ----------------------------------------------------------------------------------------------------------------
	//	AnimMontage Support for multiple USkeletalMeshComponents on the AvatarActor.
	//  Only one ability can be animating at a time though?
	// ----------------------------------------------------------------------------------------------------------------	

	// Plays a montage and handles replication and prediction based on passed in ability/activation info
	virtual float PlayMontageForMesh(UGameplayAbility* AnimatingAbility, class USkeletalMeshComponent* InMesh, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None, bool bReplicateMontage = true);

	// Plays a montage without updating replication/prediction structures. Used by simulated proxies when replication tells them to play a montage.
	virtual float PlayMontageSimulatedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	// Stops whatever montage is currently playing. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime = -1.0f);

	// Stops all montages currently playing
	virtual void StopAllCurrentMontages(float OverrideBlendOutTime = -1.0f);

	// Stops current montage if it's the one given as the Montage param
	virtual void StopMontageIfCurrentForMesh(USkeletalMeshComponent* InMesh, const UAnimMontage& Montage, float OverrideBlendOutTime = -1.0f);

	// Clear the animating ability that is passed in, if it's still currently animating
	virtual void ClearAnimatingAbilityForAllMeshes(UGameplayAbility* Ability);

	// Jumps current montage to given section. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName);

	// Sets current montages next section name. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, FName FromSectionName, FName ToSectionName);

	// Sets current montage's play rate
	virtual void CurrentMontageSetPlayRateForMesh(USkeletalMeshComponent* InMesh, float InPlayRate);

	// Returns true if the passed in ability is the current animating ability
	bool IsAnimatingAbilityForAnyMesh(UGameplayAbility* Ability) const;

	// Returns the current animating ability
	UGameplayAbility* GetAnimatingAbilityFromAnyMesh();

	// Returns montages that are currently playing
	TArray<UAnimMontage*> GetCurrentMontages() const;

	// Returns the montage that is playing for the mesh
	UAnimMontage* GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh);

	// Get SectionID of currently playing AnimMontage
	int32 GetCurrentMontageSectionIDForMesh(USkeletalMeshComponent* InMesh);

	// Get SectionName of currently playing AnimMontage
	FName GetCurrentMontageSectionNameForMesh(USkeletalMeshComponent* InMesh);

	// Get length in time of current section
	float GetCurrentMontageSectionLengthForMesh(USkeletalMeshComponent* InMesh);

	// Returns amount of time left in current section
	float GetCurrentMontageSectionTimeLeftForMesh(USkeletalMeshComponent* InMesh);


protected:
	// ----------------------------------------------------------------------------------------------------------------
	//	AnimMontage Support for multiple USkeletalMeshComponents on the AvatarActor.
	//  Only one ability can be animating at a time though?
	// ----------------------------------------------------------------------------------------------------------------	


	// Finds the existing FGameplayAbilityLocalAnimMontageForMesh for the mesh or creates one if it doesn't exist
	FGameplayAbilityLocalAnimMontageForMesh& GetLocalAnimMontageInfoForMesh(USkeletalMeshComponent* InMesh);
	// Finds the existing FGameplayAbilityRepAnimMontageForMesh for the mesh or creates one if it doesn't exist
	FGameplayAbilityRepAnimMontageForMesh& GetGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh);

	// Called when a prediction key that played a montage is rejected
	void OnPredictiveMontageRejectedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* PredictiveMontage);

	// Copy LocalAnimMontageInfo into RepAnimMontageInfo
	void AnimMontage_UpdateReplicatedDataForMesh(USkeletalMeshComponent* InMesh);
	void AnimMontage_UpdateReplicatedDataForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo);

	// Copy over playing flags for duplicate animation data
	void AnimMontage_UpdateForcedPlayFlagsForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo);	

	UFUNCTION()
	virtual void OnRep_ReplicatedAnimMontageForMesh();

	// Returns true if we are ready to handle replicated montage information
	virtual bool IsReadyForReplicatedMontageForMesh();

	// RPC function called from CurrentMontageSetNextSectionName, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);
	void ServerCurrentMontageSetNextSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);
	bool ServerCurrentMontageSetNextSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);

	// RPC function called from CurrentMontageJumpToSection, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageJumpToSectionNameForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);
	void ServerCurrentMontageJumpToSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);
	bool ServerCurrentMontageJumpToSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);

	// RPC function called from CurrentMontageSetPlayRate, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageSetPlayRateForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);
	void ServerCurrentMontageSetPlayRateForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);
	bool ServerCurrentMontageSetPlayRateForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);

#pragma region ~ 公有变量 ~
public:
	bool bCharacterAbilitiesGiven = false;
	
	bool bStartupEffectsApplied = false;
#pragma endregion

	// ~Start Cutter ---------------------------------------------------  -------------------------------------------------- End Cutter~
	
#pragma region ~ 保护变量 ~
protected:
	// Set if montage rep happens while we don't have the animinstance associated with us yet
	UPROPERTY()
	bool bPendingMontageRepForMesh;

	// Data structure for montages that were instigated locally (everything if server, predictive if client. replicated if simulated proxy)
	// Will be max one element per skeletal mesh on the AvatarActor
	UPROPERTY()
	TArray<FGameplayAbilityLocalAnimMontageForMesh> LocalAnimMontageInfoForMeshes;
	
	// Data structure for replicating montage info to simulated clients
	// Will be max one element per skeletal mesh on the AvatarActor
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedAnimMontageForMesh)
	TArray<FGameplayAbilityRepAnimMontageForMesh> RepAnimMontageInfoForMeshes;
#pragma endregion 
};