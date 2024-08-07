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
