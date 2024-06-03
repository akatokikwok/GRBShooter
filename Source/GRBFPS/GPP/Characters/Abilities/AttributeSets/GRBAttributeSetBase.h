#pragma once
#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GRBAttributeSetBase.generated.h"

//使用来自 AttributeSet.h 头文件默认携带的宏
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/*
 * GRB项目自带的AttributeSetBase基类
 */
UCLASS()
class GRBFPS_API UGRBAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGRBAttributeSetBase();
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma region 内部工具方法
protected:
	/** 当一个属性的最大属性发生变化时，助手函数按比例调整属性的值. (即当MaxHealth增加时，当前生命值增加量与之前保持等百分比) */
	void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty);
#pragma endregion 内部工具方法
	
#pragma region AS属性同步方法
protected:
	/**
	 * 这些属性同步方法是为了保障在网络复制的时候 在ASC框架内部能同步;
	 **/
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_HealthRegenRate(const FGameplayAttributeData& OldHealthRegenRate);

	UFUNCTION()
	virtual void OnRep_Mana(const FGameplayAttributeData& OldMana);

	UFUNCTION()
	virtual void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana);

	UFUNCTION()
	virtual void OnRep_ManaRegenRate(const FGameplayAttributeData& OldManaRegenRate);

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	UFUNCTION()
	virtual void OnRep_StaminaRegenRate(const FGameplayAttributeData& OldStaminaRegenRate);

	UFUNCTION()
	virtual void OnRep_Shield(const FGameplayAttributeData& OldShield);

	UFUNCTION()
	virtual void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);

	UFUNCTION()
	virtual void OnRep_ShieldRegenRate(const FGameplayAttributeData& OldShieldRegenRate);

	UFUNCTION()
	virtual void OnRep_Armor(const FGameplayAttributeData& OldArmor);

	UFUNCTION()
	virtual void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

	UFUNCTION()
	virtual void OnRep_CharacterLevel(const FGameplayAttributeData& OldCharacterLevel);

	UFUNCTION()
	virtual void OnRep_XP(const FGameplayAttributeData& OldXP);

	UFUNCTION()
	virtual void OnRep_XPBounty(const FGameplayAttributeData& OldXPBounty);

	UFUNCTION()
	virtual void OnRep_Gold(const FGameplayAttributeData& OldGold);

	UFUNCTION()
	virtual void OnRep_GoldBounty(const FGameplayAttributeData& OldGoldBounty);
#pragma endregion AS属性同步方法


protected:
	// 为爆头行为单独设立地标签；
	FGameplayTag HeadShotTag;

#pragma region AS属性字段
public:
	// 当前生命值，当为0时，我们期望拥有者死亡，除非被技能阻止。上限是MaxHealth。
	// 正向变化可以直接使用this。
	// 负面变化应该通过伤害元属性。
	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, Health);

	// 最大生命值; 是它自己的属性，因为GameplayEffects可以修改它
	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, MaxHealth)

	// 生命回复速度; 将被动地每秒增加生命值
	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_HealthRegenRate)
	FGameplayAttributeData HealthRegenRate;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, HealthRegenRate)

	// 当前法力值，用于执行特殊技能。上限是MaxMana
	UPROPERTY(BlueprintReadOnly, Category = "Mana", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, Mana)

	// 最大蓝量； 是它自己的属性; 因为GameplayEffects可以修改它
	UPROPERTY(BlueprintReadOnly, Category = "Mana", ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, MaxMana)

	// 蓝量恢复速度； 被动地影响每秒恢复的法力值
	UPROPERTY(BlueprintReadOnly, Category = "Mana", ReplicatedUsing = OnRep_ManaRegenRate)
	FGameplayAttributeData ManaRegenRate;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, ManaRegenRate)

	// 当前耐力;用于执行特殊技能。以MaxStamina为上限
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, Stamina)

	// 最大体力值； 是它自己的属性; 因为GameplayEffects可以修改它
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, MaxStamina)

	// 体力值恢复速度； 将被动地每秒恢复体力值
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_StaminaRegenRate)
	FGameplayAttributeData StaminaRegenRate;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, StaminaRegenRate)

	// 当前护盾; 的作用类似于临时生命值。当耗尽时，伤害将消耗常规生命值。
	UPROPERTY(BlueprintReadOnly, Category = "Shield", ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, Shield)

	// 最大护盾值； 
	UPROPERTY(BlueprintReadOnly, Category = "Shield", ReplicatedUsing = OnRep_MaxShield)
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, MaxShield)

	// 护盾恢复速率； 被动地影响每秒恢复护盾
	UPROPERTY(BlueprintReadOnly, Category = "Shield", ReplicatedUsing = OnRep_ShieldRegenRate)
	FGameplayAttributeData ShieldRegenRate;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, ShieldRegenRate)

	// 护甲值； 减少攻击者造成的伤害
	UPROPERTY(BlueprintReadOnly, Category = "Armor", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, Armor)

	// 伤害值； 是一个元属性，由DamageExecution用来计算最终伤害，然后转化为-生命值
	// 只存在于服务端的临时值, 不承认网络复制。
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, Damage)

	// 移动速度; 影响角色移动的速度。
	UPROPERTY(BlueprintReadOnly, Category = "MoveSpeed", ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, MoveSpeed)

	// 
	UPROPERTY(BlueprintReadOnly, Category = "Character Level", ReplicatedUsing = OnRep_CharacterLevel)
	FGameplayAttributeData CharacterLevel;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, CharacterLevel)

	// 击杀敌人获得经验值; 用于升级(在本项目中未实现)
	UPROPERTY(BlueprintReadOnly, Category = "XP", ReplicatedUsing = OnRep_XP)
	FGameplayAttributeData XP;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, XP)

	// 经验值奖励给枪手, 用于升级(在本项目中未实现).
	UPROPERTY(BlueprintReadOnly, Category = "XP", ReplicatedUsing = OnRep_XPBounty)
	FGameplayAttributeData XPBounty;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, XPBounty)

	// 击杀敌人获得的金币; 用于购买物品(在本项目中未实现).
	UPROPERTY(BlueprintReadOnly, Category = "Gold", ReplicatedUsing = OnRep_Gold)
	FGameplayAttributeData Gold;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, Gold)

	// 金币奖励给枪手； 用于购买物品(在本项目中未实现).
	UPROPERTY(BlueprintReadOnly, Category = "Gold", ReplicatedUsing = OnRep_GoldBounty)
	FGameplayAttributeData GoldBounty;
	ATTRIBUTE_ACCESSORS(UGRBAttributeSetBase, GoldBounty)
#pragma endregion AS属性字段

};
