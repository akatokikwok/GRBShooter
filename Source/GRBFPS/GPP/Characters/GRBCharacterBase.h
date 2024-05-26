#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "GRBCharacterBase.generated.h"

class UGRBAbilitySystemComponent;

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
	UPROPERTY()
	UGRBAbilitySystemComponent* AbilitySystemComponent;
	
};
