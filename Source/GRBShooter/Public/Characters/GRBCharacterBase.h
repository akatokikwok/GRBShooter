#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GRBShooter/GRBShooter.h"
#include "TimerManager.h"
#include "GRBCharacterBase.generated.h"

UCLASS()
class GRBSHOOTER_API AGRBCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// ~Start Implements IAbilitySystemInterface
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// ~End Implements
	
public:
	
};