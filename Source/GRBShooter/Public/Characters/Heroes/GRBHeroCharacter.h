#pragma once

#include "CoreMinimal.h"
#include "Characters/GRBCharacterBase.h"
#include "Characters/Abilities/GRBInteractable.h"
#include "GameplayEffectTypes.h"
#include "GRBHeroCharacter.generated.h"

class AGRBWeapon;
/**
* A player or AI controlled hero character.
 */
UCLASS()
class GRBSHOOTER_API AGRBHeroCharacter : public AGRBCharacterBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
	AGRBWeapon* GetCurrentWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "GASShooter|GSHeroCharacter")
	virtual bool IsInFirstPersonPerspective() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GASShooter|GSHeroCharacter")
	class USkeletalMeshComponent* GetFirstPersonMesh() const;
};