#include "Characters/GRBCharacterBase.h"
#include "Characters/GRBCharacterMovementComponent.h"


UAbilitySystemComponent* AGRBCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}

AGRBCharacterBase::AGRBCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGRBCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))// 设定本Pawn的移动组件为定制GRB移动组件
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AGRBCharacterBase::IsAlive() const
{
	return false;
}
