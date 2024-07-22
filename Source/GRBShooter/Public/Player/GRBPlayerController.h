// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Characters/GRBCharacterBase.h"
#include "GRBPlayerController.generated.h"

class UPaperSprite;

/**
 * 玩家控制器
 */
UCLASS()
class GRBSHOOTER_API AGRBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	UFUNCTION(Client, Reliable, WithValidation)
	void ShowDamageNumber(float DamageAmount, AGRBCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags);
	void ShowDamageNumber_Implementation(float DamageAmount, AGRBCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags);
	bool ShowDamageNumber_Validate(float DamageAmount, AGRBCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags);
	
};
