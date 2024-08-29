// Copyright 2024 GRB.


#include "Player/GRBPlayerController.h"
#include "Characters/Abilities/AttributeSets/GRBAmmoAttributeSet.h"
#include "Characters/Abilities/AttributeSets/GRBAttributeSetBase.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "Player/GRBPlayerState.h"
#include "UI/GRBHUDWidget.h"
#include "Weapons/GRBWeapon.h"

UGRBHUDWidget* AGRBPlayerController::GetGRBHUD()
{
	return nullptr;
}

void AGRBPlayerController::SetHUDReticle(TSubclassOf<UGRBHUDReticle> ReticleClass)
{
	
}

void AGRBPlayerController::SetEquippedWeaponStatusText(const FText& StatusText)
{
	
}

void AGRBPlayerController::ShowDamageNumber_Implementation(float DamageAmount, AGRBCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags)
{
	
}

bool AGRBPlayerController::ShowDamageNumber_Validate(float DamageAmount, AGRBCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags)
{
	return true;
}
