#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GRBFPS/GRBFPS.h"
#include "GRBGameplayAbility.generated.h"

/**
 * 项目所使用的GA 基类
 */
UCLASS()
class GRBFPS_API UGRBGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGRBGameplayAbility();

public:
	// 值将GA与插槽关联，而不将其绑定到自动激活的输入.
	// 被动技能不会与输入绑定，所以我们需要一种方法将技能与插槽关联起来.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	EGRBAbilityInputID m_AbilityID = EGRBAbilityInputID::None;

	// Abilities with this set will automatically activate when the input is pressed
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	EGRBAbilityInputID m_AbilityInputID = EGRBAbilityInputID::None;
};
