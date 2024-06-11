#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Abilities/AttributeSets/GRBAttributeSetBase.h"
#include "GRBCharacterBase.generated.h"

class UGRBAbilitySystemComponent;

/***/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCharacterDiedDelegate, AGRBCharacterBase*, ParamCharacter);

UCLASS()
class AGRBCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGRBCharacterBase(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	/** 仅在服务端才会调用; 调用顺序在函数 Server's AcknowledgePossession之前. */
	virtual void PossessedBy(AController* NewController) override;

	/** 处理输入回调的入口. */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// ~ Begin Implement IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// ~ End Implement IAbilitySystemInterface

public:
	// Switch on AbilityID to return individual ability levels.
	UFUNCTION(BlueprintCallable, Category = "GASShooter|GSCharacter")
	virtual int32 GetAbilityLevel(EGRBAbilityInputID AbilityID) const;
	
	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase")
	virtual bool IsAlive() const;
	
	virtual void Die();

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase")
	virtual void FinishDying();

public:
	//  一些AS属性的Getters.
	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	int32 GetCharacterLevel() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetMana() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetMaxMana() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetStamina() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetMaxStamina() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetShield() const;

	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetMaxShield() const;
	
	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetMoveSpeed() const;

	// 注意这里是拿取的AS-移速里的BaseValue.
	UFUNCTION(BlueprintCallable, Category = "GRBFPS|GRBCharacterBase|Attributes")
	float GetMoveSpeedBaseValue() const;
	
protected:
	// 在服务端授权技能, 并且GA-Hanle会被网络复制到所属客户端
	virtual void AddCharacterAbilities();

	// 初始化英雄的AS, 仅承认运行在服务端, 但是此工程亦会运行在服务端.
	// AS的网络复制并不是很重要, 因为在服务端和在客户端它应该都是一致相同的
	virtual void InitializeAttributes();

	// 处理Respawn的BUFF和一组英雄固有携带的BUFF组
	virtual void AddStartupEffects();

	// 一些AS属性的Setters, 仅在一些特殊案例下会被使用，如重生； 否则更合理的手段我推荐是使用GE去修改.
	// 这些Setters仅修改AS的 BaseValue.
	virtual void SetHealth(float Health);
	virtual void SetMana(float Mana);
	virtual void SetStamina(float Stamina);
	virtual void SetShield(float Shield);

	
protected:
	// 携带的ASC组件
	UPROPERTY()
	UGRBAbilitySystemComponent* AbilitySystemComponent;

	// AS属性集; 它一般会在玩家状态里会被使用的多.
	UPROPERTY()
	UGRBAttributeSetBase* AttributeSetBase;
	
	// 英雄固有携带的技能组，英雄死亡时会被移除，且会重新授权当英雄重生后.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GASShooter|Abilities")
	TArray<TSubclassOf<class UGRBGameplayAbility>> CharacterAbilities;

	// 给1个Pawn在 spawn/respawn 时机下会执行的默认属性GE; 瞬发的GE.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GASShooter|Abilities")
	TSubclassOf<class UGameplayEffect> DefaultAttributes;

	// 仅在英雄的初始阶段才会应用一次.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GASShooter|Abilities")
	TArray<TSubclassOf<class UGameplayEffect>> StartupEffects;
	
	// 英雄死亡时候会携带的死亡标签
	FGameplayTag DeadTag;

};
