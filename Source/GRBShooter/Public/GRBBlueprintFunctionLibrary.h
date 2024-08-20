// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Abilities/GRBAbilityTypes.h"
#include "Characters/Abilities/GRBGameplayAbility.h"
#include "Characters/Abilities/GRBGameplayEffectTypes.h"
#include "GRBBlueprintFunctionLibrary.generated.h"


/**
 * 
 */
UCLASS()
class GRBSHOOTER_API UGRBBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability")
	static UGRBGameplayAbility* GetPrimaryAbilityInstanceFromClass(UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> InAbilityClass);

	///--@brief 查看技能是否仍然在激活中--/
	UFUNCTION(BlueprintCallable, Category = "Ability")
	static bool IsPrimaryAbilityInstanceActive(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle);

	///--@brief 检查技能句柄是否合法--/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability")
	static bool IsAbilitySpecHandleValid(FGameplayAbilitySpecHandle Handle);

	///--@brief 把技能句柄转化成技能--/
	UFUNCTION(BlueprintCallable, Category = "Ability")
	static UGRBGameplayAbility* GetPrimaryAbilityInstanceFromHandle(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle);
};
