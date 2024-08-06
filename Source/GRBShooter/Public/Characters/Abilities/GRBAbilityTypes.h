#pragma once
// ----------------------------------------------------------------------------------------------------------------
// This header is for Ability-specific structures and enums that are shared across a project
// Every game will probably need a file like this to handle their extensions to the system
// This file is a good place for subclasses of FGameplayEffectContext and FGameplayAbilityTargetData
// ----------------------------------------------------------------------------------------------------------------

#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectStackChange.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectRemoved.h"
#include "GRBAbilityTypes.generated.h"

class UGRBAbilitySystemComponent;
class UGameplayEffect;
class UGRBTargetType_BaseObj;

/** 索敌BUFF容器--管理索敌类型和一组BUFF实例
 * 当选定技能目标的业务发生时, 应用的BUFF数据结构
 * Struct defining a list of gameplay effects, a tag, and targeting info
 * These containers are defined statically in blueprints or assets and then turn into Specs at runtime
 */
USTRUCT(BlueprintType)
struct FGRBGameplayEffectContainer
{
	GENERATED_BODY()

public:
	FGRBGameplayEffectContainer()
	{
	}

	/** 关心的选定目标类型; Sets the way that targeting happens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	TSubclassOf<UGRBTargetType_BaseObj> TargetType;

	/** 要应用到技能目标群上的一组BUFF; List of gameplay effects to apply to the targets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	TArray<TSubclassOf<UGameplayEffect>> TargetGameplayEffectClasses;
};

/* 索敌BUFF处理器--管理索敌目标容器句柄和一组BUFF句柄; 负责处理 选定目标并应用BUFF实例 的数据结构
 * A "processed" version of GRBGameplayEffectContainer that can be passed around and eventually applied
 */
USTRUCT(BlueprintType)
struct FGRBGameplayEffectContainerSpec
{
	GENERATED_BODY()

public:
	FGRBGameplayEffectContainerSpec()
	{
	}

	/** Returns true if this has any valid effect specs */
	bool HasValidEffects() const;

	/** Returns true if this has any valid targets */
	bool HasValidTargets() const;

	/** 把外界额度技能目标数据 命中Hit数据, ACTOR数据统统缓存进本结构的TargetDataHandle;  Adds new targets to target data */
	void AddTargets(const TArray<FGameplayAbilityTargetDataHandle>& TargetData, const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors);

	/** Clears target data */
	void ClearTargets();

	/** FGameplayAbilityTargetDataHandle 是一个目标数据的容器，它管理多个 FGameplayAbilityTargetData 对象;
	 * 计算好的命中目标数据; Computed target data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
	FGameplayAbilityTargetDataHandle TargetData;

	/** 要应用给选定目标集的BUFF句柄; List of gameplay effects to apply to the targets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
	TArray<FGameplayEffectSpecHandle> TargetGameplayEffectSpecs;
};
