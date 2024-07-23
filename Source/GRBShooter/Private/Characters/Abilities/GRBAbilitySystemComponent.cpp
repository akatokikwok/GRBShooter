// Copyright 2024 GRB.


#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "Animation/AnimInstance.h"
#include "Characters/Abilities/GRBGameplayAbility.h"
#include "GameplayCueManager.h"
#include "GRBBlueprintFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/GRBWeapon.h"

UGRBAbilitySystemComponent::UGRBAbilitySystemComponent()
{
}

void UGRBAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGRBAbilitySystemComponent, RepAnimMontageInfoForMeshes);
}

bool UGRBAbilitySystemComponent::GetShouldTick() const
{
	return Super::GetShouldTick();
}

void UGRBAbilitySystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

/** 用于初始化与特定角色和控制器相关联的能力信息。该方法通常在角色初始化完成、或者需要手动初始化能力信息时调用，以确保能力系统正确地关联到游戏角色和控制器.*/
void UGRBAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

/** 在一个能力（Ability）结束时调用，用于通知系统该能力已经完成、取消或中止。通过这一通知，系统可以进行一些清理工作、触发回调.*/
void UGRBAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);
}

UGRBAbilitySystemComponent* UGRBAbilitySystemComponent::GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent)
{
	return nullptr;
}

void UGRBAbilitySystemComponent::AbilityLocalInputPressed(int32 InputID)
{
	Super::AbilityLocalInputPressed(InputID);
}
