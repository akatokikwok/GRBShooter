#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GRBAbilitySystemComponent.generated.h"

class USkeletalMeshComponent;

/*
 * 自己独有的项目GRB 技能组件
 */
UCLASS()
class UGRBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UGRBAbilitySystemComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// ~ From UAbilitySystemComponent Virtual Function
	virtual bool GetShouldTick() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	// ~ From UAbilitySystemComponent Virtual Function

public:
	// GRB-ASC 已授权状态 (预防反复授权)
	bool bCharacterAbilitiesGiven = false;

	// 检查初始BUFF是否反复授权.
	bool bStartupEffectsApplied = false;
};
