#include "Characters/Heroes/GRBHeroCharacter.h"
#include "Weapons/GRBWeapon.h"

AGRBWeapon* AGRBHeroCharacter::GetCurrentWeapon() const
{
	return nullptr;
}

bool AGRBHeroCharacter::IsInFirstPersonPerspective() const
{
	return true;
}

USkeletalMeshComponent* AGRBHeroCharacter::GetFirstPersonMesh() const
{
	return nullptr;
}
