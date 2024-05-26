#include "GRBCharacterBase.h"
#include "Abilities/GRBAbilitySystemComponent.h"

AGRBCharacterBase::AGRBCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UAbilitySystemComponent* AGRBCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
