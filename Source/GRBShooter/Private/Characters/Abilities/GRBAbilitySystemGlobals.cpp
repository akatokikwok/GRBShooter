// Copyright 2024 GRB


#include "Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "Characters/Abilities/GRBGameplayEffectTypes.h"

UGRBAbilitySystemGlobals::UGRBAbilitySystemGlobals()
{

}

///--@brief virtual; 覆写 构建自己的定制网络序列化GRBEffectContext. --/
FGameplayEffectContext* UGRBAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FGRBGameplayEffectContext();
}

void UGRBAbilitySystemGlobals::InitGlobalTags()
{
	Super::InitGlobalTags();

	DeadTag = FGameplayTag::RequestGameplayTag("State.Dead");
	KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
	InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");
	InteractingRemovalTag = FGameplayTag::RequestGameplayTag("State.InteractingRemoval");
}

