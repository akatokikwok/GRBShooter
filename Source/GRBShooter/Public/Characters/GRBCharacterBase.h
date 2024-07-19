#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
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
	
};