#include "Player/GRBPlayerController.h"
#include "Characters/Abilities/AttributeSets/GRBAttributeSetBase.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "Player/GRBPlayerState.h"
#include "Weapons/GRBWeapon.h"



void AGRBPlayerController::ShowDamageNumber_Implementation(float DamageAmount, AGRBCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags)
{
	
}

bool AGRBPlayerController::ShowDamageNumber_Validate(float DamageAmount, AGRBCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags)
{
	return true;
}
