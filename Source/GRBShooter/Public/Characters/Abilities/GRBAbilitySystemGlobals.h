// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "GRBAbilitySystemGlobals.generated.h"

/**
 * UAbilitySystemGlobals子类
 * 不要在uobject构造器里调用,否则会引发崩溃.
 * Child class of UAbilitySystemGlobals.
 * Do not try to get a reference to this or call into it during constructors of other UObjects. It will crash in packaged games.
 */
UCLASS()
class GRBSHOOTER_API UGRBAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

public:
	UGRBAbilitySystemGlobals();
	
	// ~Start Implements UAbilitySystemGlobals
	///--@brief virtual; 覆写 构建自己的定制网络序列化GRBEffectContext. Should allocate a project specific GameplayEffectContext struct. Caller is responsible for deallocation --/
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
	///--@brief virtual; 覆写 初始化构建一些全局可用到的tags. --/
	virtual void InitGlobalTags() override;
	// ~End Implements

	// 拿取类单例引用
	static UGRBAbilitySystemGlobals& GRBGet()
	{
		return dynamic_cast<UGRBAbilitySystemGlobals&>(UAbilitySystemGlobals::Get());
	}

	/**
	 * 保存一些常见的标签, 便于在外界静态访问
	* Cache commonly used tags here. This has the benefit of one place to set the tag FName in case tag names change and
	* the function call into UGRBAbilitySystemGlobals::GRBGet() is cheaper than calling FGameplayTag::RequestGameplayTag().
	* Classes can access them by UGRBAbilitySystemGlobals::GRBGet().DeadTag
	* We're not using this in this sample project (classes are manually caching in their constructors), but it's here as a reference.
	*/
	UPROPERTY()
	FGameplayTag DeadTag;

	UPROPERTY()
	FGameplayTag KnockedDownTag;

	UPROPERTY()
	FGameplayTag InteractingTag;

	UPROPERTY()
	FGameplayTag InteractingRemovalTag;
};
