#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "GRBFPS/GRBFPS.h"
#include "GRBWeapon.generated.h"


class UAnimMontage;
class UGRBAbilitySystemComponent;
class UGRBGameplayAbility;
class UPaperSprite;
class USkeletalMeshComponent;


/*
 * 武器基类.
 */
UCLASS(Blueprintable, BlueprintType)
class GRBFPS_API AGRBWeapon : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGRBWeapon();

	// Implement IAbilitySystemInterface
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
};
