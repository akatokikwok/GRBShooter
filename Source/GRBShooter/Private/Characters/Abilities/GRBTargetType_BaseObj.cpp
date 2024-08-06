#include "Characters/Abilities/GRBTargetType_BaseObj.h"
#include "Characters/GRBCharacterBase.h"

void UGRBTargetType_BaseObj::GetTargets_Implementation(AGRBCharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	return;
}

void UGRBTargetType_UseOwner::GetTargets_Implementation(AGRBCharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	OutActors.Add(TargetingCharacter);
}

void UGRBTargetType_UseEventData::GetTargets_Implementation(AGRBCharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	const FHitResult* FoundHitResult = EventData.ContextHandle.GetHitResult();
	if (FoundHitResult)
	{
		OutHitResults.Add(*FoundHitResult);
	}
	else if (EventData.Target)
	{
		OutActors.Add(const_cast<AActor*>(EventData.Target.Get()));
	}
}
