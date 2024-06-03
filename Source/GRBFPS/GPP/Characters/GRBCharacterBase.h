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

	// ~ Begin Implement IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// ~ End Implement IAbilitySystemInterface

protected:
	// 携带的ASC组件
	UPROPERTY()
	UGRBAbilitySystemComponent* AbilitySystemComponent;

	// AS属性集; 它一般会在玩家状态里会被使用的多.
	UPROPERTY()
	UGRBAttributeSetBase* AttributeSetBase;
	
	// 英雄死亡时候会携带的死亡标签
	FGameplayTag DeadTag;

};
