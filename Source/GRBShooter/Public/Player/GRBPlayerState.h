// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GRBPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGRBOnGameplayAttributeValueChangedDelegate, FGameplayAttribute, Attribute, float, NewValue, float, OldValue);

class UGRBAttributeSetBase;
class UGRBAmmoAttributeSet;


/**
 * 玩家状态存档类 PlayerState
 * 玩家状态类仍需要继承 IASC虚接口 来管理ASC
 */
UCLASS()
class GRBSHOOTER_API AGRBPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGRBPlayerState();
	virtual void BeginPlay() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	class UGRBAttributeSetBase* GetAttributeSetBase() const;
	class UGRBAmmoAttributeSet* GetAmmoAttributeSet() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|UI")
	void ShowAbilityConfirmPrompt(bool bShowPrompt);

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|UI")
	void ShowInteractionPrompt(float InteractionDuration);

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|UI")
	void HideInteractionPrompt();

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|UI")
	void StartInteractionTimer(float InteractionDuration);

	// Interaction interrupted, cancel and hide HUD interact timer
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|UI")
	void StopInteractionTimer();


#pragma region ~ 对外预留的一些属性集Getter接口 ~
	/**
	* Getters for attributes from GDAttributeSetBase. Returns Current Value unless otherwise specified.
	*/
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetHealthRegenRate() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetMana() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetMaxMana() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetManaRegenRate() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetStamina() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetMaxStamina() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetStaminaRegenRate() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetShield() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetMaxShield() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetShieldRegenRate() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetArmor() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	float GetMoveSpeed() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	int32 GetCharacterLevel() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	int32 GetXP() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	int32 GetXPBounty() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	int32 GetGold() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	int32 GetGoldBounty() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	int32 GetPrimaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBPlayerState|Attributes")
	int32 GetPrimaryReserveAmmo() const;
#pragma endregion
	
protected:
	virtual void OnAttribute_HealthChangedCallback(const FOnAttributeChangeData& Data);
	virtual void KnockDownTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	
protected:
	FGameplayTag DeadTag;
	FGameplayTag KnockedDownTag;

	// 玩家存档会持有管理 1个 GRB-ASC
	UPROPERTY()
	class UGRBAbilitySystemComponent* m_PlayerStateASCComponent;

	// 管理1个 人物属性集
	UPROPERTY()
	UGRBAttributeSetBase* AttributeSetBase;

	// 管理1个 武器弹药属性集
	UPROPERTY()
	UGRBAmmoAttributeSet* AmmoAttributeSet;

	// Attribute changed delegate handles
	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle PawnKnockdownAddOrRemoveHandle;
};
