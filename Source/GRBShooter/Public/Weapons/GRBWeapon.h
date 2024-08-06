#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "GRBShooter/GRBShooter.h"
#include "GRBWeapon.generated.h"


UCLASS(Blueprintable, BlueprintType)
class GRBSHOOTER_API AGRBWeapon : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
};
