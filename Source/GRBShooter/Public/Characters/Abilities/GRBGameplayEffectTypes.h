// Copyright 2024 GRB.

#pragma once

#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GRBGameplayEffectTypes.generated.h"

/**
 * 项目自己的负责处理GEEC信息, 切换上下文关联的发起者和关联数据的数据结构
 * Data structure that stores an instigator and related data, such as positions and targets
 * Games can subclass this structure and add game-specific information
 * It is passed throughout effect execution so it is a great place to track transient information about an execution
 */
USTRUCT()
struct GRBSHOOTER_API FGRBGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_USTRUCT_BODY()

public:
	// virtual; Get单例
	virtual FGameplayAbilityTargetDataHandle GetTargetData()
	{
		return TargetData;
	}

	// virtual; 添加新的targethandle
	virtual void AddTargetData(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
	{
		TargetData.Append(TargetDataHandle);
	}

	/**
	* Functions that subclasses of FGameplayEffectContext need to override
	*/

	// virtual; 返回类struct
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGRBGameplayEffectContext::StaticStruct();
	}

	// virtual; 拷贝本类数据到1个副本实例上
	virtual FGRBGameplayEffectContext* Duplicate() const override
	{
		FGRBGameplayEffectContext* NewContext = new FGRBGameplayEffectContext();
		*NewContext = *this;
		NewContext->AddActors(this->Actors);
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*(this->GetHitResult()), true);
		}
		// Shallow copy of TargetData, is this okay?
		NewContext->TargetData.Append(this->TargetData);
		return NewContext;
	}

	// virtual; 网络同步系统中的一个重要方法，用于将对象的状态序列化为字节流，以便在客户端和服务器之间进行传输
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

protected:
	// FGameplayAbilityTargetDataHandle 是一个复合数据结构，可以包含多个 FGameplayAbilityTargetData 实例
	// 可以有效管理和处理游戏能力系统中的目标数据
	FGameplayAbilityTargetDataHandle TargetData;
};

/*
 * 使用特定行为类模板来定制可网络序列化的GRB-GameplayEffectContext
 * TStructOpsTypeTraits 是一个模板结构，它可以向 Unreal 引擎注册特定类型的操作特性。通过定义这些特性，可以让引擎知道如何处理你的自定义 USTRUCT 实例
 * 一些常见的特性标志，你可以用它们来定制你的 USTRUCT
 * WithNetSerializer: 启用自定义网络序列化功能。
 * WithCopy: 使用默认的拷贝构造函数
 */
template <>
struct TStructOpsTypeTraits<FGRBGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FGRBGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};
