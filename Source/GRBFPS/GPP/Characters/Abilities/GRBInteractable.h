#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "GRBInteractable.generated.h"

UINTERFACE()
class UGRBInteractable : public UInterface
{
	GENERATED_BODY()
};

/*
 * Interface for Actors that can be interacted with through the GameplayAbilitySystem.
 */
class IGRBInteractable
{
	GENERATED_BODY()

public:
	
};
