#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GRBFPS/GPP/Characters/GRBCharacterBase.h"
#include "GRBFPS/GPP/Characters/Abilities/GRBInteractable.h"
#include "GRBHeroCharacter.generated.h"

UCLASS()
class AGRBHeroCharacter : public AGRBCharacterBase, public IGRBInteractable
{
	GENERATED_BODY()

public:
	AGRBHeroCharacter(const FObjectInitializer& ObjectInitializer);
};
