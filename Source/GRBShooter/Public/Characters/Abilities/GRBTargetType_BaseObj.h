#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "Characters/Abilities/GRBAbilityTypes.h"
#include "GRBTargetType_BaseObj.generated.h"

class AGRBCharacterBase;
class AActor;
struct FGameplayEventData;


/** 用于决策 技能目标的类; 专为蓝图逻辑设计; 
 * Class that is used to determine targeting for abilities
 * It is meant to be blueprinted to run target logic
 * This does not subclass GameplayAbilityTargetActor because this class is never instanced into the world
 * This can be used as a basis for a game-specific targeting blueprint
 * If your targeting is more complicated you may need to instance into the world once or as a pooled actor
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin))
class GRBSHOOTER_API UGRBTargetType_BaseObj : public UObject
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UGRBTargetType_BaseObj()
	{
	}

	/** @brief 虚方法; 负责索敌机制后注册内部数据; 决策技能目标并应用BUFF, 蓝图可覆写; Called to determine targets to apply gameplay effects to */
	UFUNCTION(BlueprintNativeEvent)
	void GetTargets(AGRBCharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const;
	virtual void GetTargets_Implementation(AGRBCharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const;
};

/** 琐碎技能目标 类型
 * Trivial target type that uses the owner
 */
UCLASS(NotBlueprintable)
class GRBSHOOTER_API UGRBTargetType_UseOwner : public UGRBTargetType_BaseObj
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UGRBTargetType_UseOwner()
	{
	}

	/** @brief 虚方法覆写; 负责索敌机制后注册内部数据; Uses the passed in event data */
	virtual void GetTargets_Implementation(AGRBCharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const override;
};

/* 负责从EventData内拉选定目标的 目标类型
 * Trivial target type that pulls the target out of the event data
 * */
UCLASS(NotBlueprintable)
class GRBSHOOTER_API UGRBTargetType_UseEventData : public UGRBTargetType_BaseObj
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UGRBTargetType_UseEventData()
	{
	}

	/** @brief 虚方法覆写; 负责索敌机制后注册内部数据; 从EventData内提出Hit结果亦或是命中的Actor; Uses the passed in event data */
	virtual void GetTargets_Implementation(AGRBCharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const override;
};
