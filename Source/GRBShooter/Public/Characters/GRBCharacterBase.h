#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GRBShooter/GRBShooter.h"
#include "TimerManager.h"
#include "GRBCharacterBase.generated.h"


/** 动态多播事件委托：玩家死亡 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCharacterDiedDelegate, AGRBCharacterBase*, Character);

/*
 * 玩家基类
 */
UCLASS()
class GRBSHOOTER_API AGRBCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

private:
	// 事件：当玩家死亡
	UPROPERTY(BlueprintAssignable, Category = "GRBShooter|GRBCharacter")
	FCharacterDiedDelegate OnCharacterDied;
	
public:
	// ~Start Implements IAbilitySystemInterface
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// ~End Implements
	
public:
	AGRBCharacterBase(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter")
	virtual bool IsAlive() const;

	
#pragma region ~ 类对外的接口
public:
	/**
	* Getters for attributes from GRBAttributeSetBase
	**/
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	int32 GetCharacterLevel() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetHealth() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetMaxHealth() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetMana() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetMaxMana() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetStamina() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetMaxStamina() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetShield() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetMaxShield() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetMoveSpeed() const;
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBCharacter|Attributes")
	float GetMoveSpeedBaseValue() const;

	
	/**
	* Setters for Attributes. Only use these in special cases like Respawning, otherwise use a GE to change Attributes.
	* These change the Attribute's Base Value.
	*/
	virtual void SetHealth(float Health);
	virtual void SetMana(float Mana);
	virtual void SetStamina(float Stamina);
	virtual void SetShield(float Shield);
#pragma endregion


protected:
	// Reference to the AttributeSetBase. It will live on the PlayerState or here if the character doesn't have a PlayerState.
	UPROPERTY()
	class UGRBAttributeSetBase* AttributeSetBase = nullptr;
};