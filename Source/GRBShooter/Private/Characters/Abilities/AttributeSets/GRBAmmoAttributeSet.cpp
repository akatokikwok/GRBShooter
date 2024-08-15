// Copyright 2024 GRB.

#include "Characters/Abilities/AttributeSets/GRBAmmoAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"

UGRBAmmoAttributeSet::UGRBAmmoAttributeSet()
{
	RifleAmmoTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Rifle"));
	RocketAmmoTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Rocket"));
	ShotgunAmmoTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Shotgun"));
}

void UGRBAmmoAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
}

void UGRBAmmoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 各种弹药剩余量要约束在 最大载弹量内
	if (Data.EvaluatedData.Attribute == GetRifleReserveAmmoAttribute())
	{
		float Ammo = GetRifleReserveAmmo();
		SetRifleReserveAmmo(FMath::Clamp<float>(Ammo, 0, GetMaxRifleReserveAmmo()));
	}
	else if (Data.EvaluatedData.Attribute == GetRocketReserveAmmoAttribute())
	{
		float Ammo = GetRocketReserveAmmo();
		SetRocketReserveAmmo(FMath::Clamp<float>(Ammo, 0, GetMaxRocketReserveAmmo()));
	}
	else if (Data.EvaluatedData.Attribute == GetShotgunReserveAmmoAttribute())
	{
		float Ammo = GetShotgunReserveAmmo();
		SetShotgunReserveAmmo(FMath::Clamp<float>(Ammo, 0, GetMaxShotgunReserveAmmo()));
	}
}

void UGRBAmmoAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//c: 类类型（应为带有该属性的类）。
	//v: 需要被复制的属性（variable）。
	//cond: 复制条件（condition），指定在何种条件下进行属性复制。
	//notify: 当复制属性触发时调用的通知函数

	DOREPLIFETIME_CONDITION_NOTIFY(UGRBAmmoAttributeSet, RifleReserveAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGRBAmmoAttributeSet, MaxRifleReserveAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGRBAmmoAttributeSet, RocketReserveAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGRBAmmoAttributeSet, MaxRocketReserveAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGRBAmmoAttributeSet, ShotgunReserveAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGRBAmmoAttributeSet, MaxShotgunReserveAmmo, COND_None, REPNOTIFY_Always);
}

FGameplayAttribute UGRBAmmoAttributeSet::GetReserveAmmoAttributeFromTag(FGameplayTag& PrimaryAmmoTag)
{
	if (PrimaryAmmoTag == FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Rifle")))
	{
		return GetRifleReserveAmmoAttribute();
	}
	else if (PrimaryAmmoTag == FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Rocket")))
	{
		return GetRocketReserveAmmoAttribute();
	}
	else if (PrimaryAmmoTag == FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Shotgun")))
	{
		return GetShotgunReserveAmmoAttribute();
	}

	return FGameplayAttribute();
}

FGameplayAttribute UGRBAmmoAttributeSet::GetMaxReserveAmmoAttributeFromTag(FGameplayTag& PrimaryAmmoTag)
{
	if (PrimaryAmmoTag == FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Rifle")))
	{
		return GetMaxRifleReserveAmmoAttribute();
	}
	else if (PrimaryAmmoTag == FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Rocket")))
	{
		return GetMaxRocketReserveAmmoAttribute();
	}
	else if (PrimaryAmmoTag == FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.Shotgun")))
	{
		return GetMaxShotgunReserveAmmoAttribute();
	}

	return FGameplayAttribute();
}

// 助手方法, 用于当最大属性发生之后自动应用百分比变化的方法.
void UGRBAmmoAttributeSet::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		// Change current value to maintain the current Val / Max percent
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		float NewDelta = (CurrentMaxValue > 0.f) ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
}

void UGRBAmmoAttributeSet::OnRep_RifleReserveAmmo(const FGameplayAttributeData& OldRifleReserveAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGRBAmmoAttributeSet, RifleReserveAmmo, OldRifleReserveAmmo);
}

void UGRBAmmoAttributeSet::OnRep_MaxRifleReserveAmmo(const FGameplayAttributeData& OldMaxRifleReserveAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGRBAmmoAttributeSet, MaxRifleReserveAmmo, OldMaxRifleReserveAmmo);
}

void UGRBAmmoAttributeSet::OnRep_RocketReserveAmmo(const FGameplayAttributeData& OldRocketReserveAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGRBAmmoAttributeSet, RocketReserveAmmo, OldRocketReserveAmmo);
}

void UGRBAmmoAttributeSet::OnRep_MaxRocketReserveAmmo(const FGameplayAttributeData& OldMaxRocketReserveAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGRBAmmoAttributeSet, MaxRocketReserveAmmo, OldMaxRocketReserveAmmo);
}

void UGRBAmmoAttributeSet::OnRep_ShotgunReserveAmmo(const FGameplayAttributeData& OldShotgunReserveAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGRBAmmoAttributeSet, ShotgunReserveAmmo, OldShotgunReserveAmmo);
}

void UGRBAmmoAttributeSet::OnRep_MaxShotgunReserveAmmo(const FGameplayAttributeData& OldMaxShotgunReserveAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGRBAmmoAttributeSet, MaxShotgunReserveAmmo, OldMaxShotgunReserveAmmo);
}
