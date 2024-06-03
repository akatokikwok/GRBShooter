#include "GRBHeroCharacter.h"

AGRBHeroCharacter::AGRBHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AGRBHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AGRBHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AGRBHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void AGRBHeroCharacter::KnockDown()
{
}

void AGRBHeroCharacter::PlayKnockDownEffects()
{
}

void AGRBHeroCharacter::PlayReviveEffects()
{
}
