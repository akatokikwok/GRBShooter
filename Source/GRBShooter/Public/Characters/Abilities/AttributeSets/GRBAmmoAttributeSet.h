// Copyright 2024 GRB

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GRBAmmoAttributeSet.generated.h"

// Uses macros from AttributeSet.h
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 子弹(作为一个技能道具)也有自己的属性集
 */
UCLASS()
class GRBSHOOTER_API UGRBAmmoAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGRBAmmoAttributeSet();
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 对外静态接口: 使用标签来访问属性集内的剩余载弹量
	static FGameplayAttribute GetReserveAmmoAttributeFromTag(FGameplayTag& PrimaryAmmoTag);

	// 对外静态接口: 使用标签来访问属性集内的剩余最大载弹量
	static FGameplayAttribute GetMaxReserveAmmoAttributeFromTag(FGameplayTag& PrimaryAmmoTag);

protected:
	// 助手方法, 用于当最大属性发生之后自动应用百分比变化的方法.
	// Helper function to proportionally adjust the value of an attribute when it's associated max attribute changes.
	// (i.e. When MaxHealth increases, Health increases by an amount that maintains the same percentage as before)
	void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty);


	/**
	* These OnRep functions exist to make sure that the ability system internal representations are synchronized properly during replication
	**/

	UFUNCTION()
	virtual void OnRep_RifleReserveAmmo(const FGameplayAttributeData& OldRifleReserveAmmo);

	UFUNCTION()
	virtual void OnRep_MaxRifleReserveAmmo(const FGameplayAttributeData& OldMaxRifleReserveAmmo);

	UFUNCTION()
	virtual void OnRep_RocketReserveAmmo(const FGameplayAttributeData& OldRocketReserveAmmo);

	UFUNCTION()
	virtual void OnRep_MaxRocketReserveAmmo(const FGameplayAttributeData& OldMaxRocketReserveAmmo);

	UFUNCTION()
	virtual void OnRep_ShotgunReserveAmmo(const FGameplayAttributeData& OldShotgunReserveAmmo);

	UFUNCTION()
	virtual void OnRep_MaxShotgunReserveAmmo(const FGameplayAttributeData& OldMaxShotgunReserveAmmo);

public:
	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_RifleReserveAmmo)
	FGameplayAttributeData RifleReserveAmmo;
	ATTRIBUTE_ACCESSORS(UGRBAmmoAttributeSet, RifleReserveAmmo)

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxRifleReserveAmmo)
	FGameplayAttributeData MaxRifleReserveAmmo;
	ATTRIBUTE_ACCESSORS(UGRBAmmoAttributeSet, MaxRifleReserveAmmo)

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_RocketReserveAmmo)
	FGameplayAttributeData RocketReserveAmmo;
	ATTRIBUTE_ACCESSORS(UGRBAmmoAttributeSet, RocketReserveAmmo)

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxRocketReserveAmmo)
	FGameplayAttributeData MaxRocketReserveAmmo;
	ATTRIBUTE_ACCESSORS(UGRBAmmoAttributeSet, MaxRocketReserveAmmo)

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_ShotgunReserveAmmo)
	FGameplayAttributeData ShotgunReserveAmmo;
	ATTRIBUTE_ACCESSORS(UGRBAmmoAttributeSet, ShotgunReserveAmmo)

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxShotgunReserveAmmo)
	FGameplayAttributeData MaxShotgunReserveAmmo;
	ATTRIBUTE_ACCESSORS(UGRBAmmoAttributeSet, MaxShotgunReserveAmmo)

protected:
	// Cache tags
	FGameplayTag RifleAmmoTag;
	FGameplayTag RocketAmmoTag;
	FGameplayTag ShotgunAmmoTag;
};
