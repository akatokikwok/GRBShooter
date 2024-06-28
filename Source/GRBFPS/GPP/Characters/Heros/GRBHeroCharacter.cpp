#include "GRBHeroCharacter.h"
#include "GRBFPS/GPP/UI/GRBFloatingStatusBarWidget.h"
#include "Net/UnrealNetwork.h"

AGRBHeroCharacter::AGRBHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AGRBHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGRBHeroCharacter, Inventory);

	// Only replicate CurrentWeapon to simulated clients and manually sync Current Weeapon with Owner when we're ready.
	// This allows us to predict weapon changing.
	DOREPLIFETIME_CONDITION(AGRBHeroCharacter, CurrentWeapon, COND_SimulatedOnly);
}

void AGRBHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AGRBHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void AGRBHeroCharacter::FinishDying()
{
	Super::FinishDying();
}

UGRBFloatingStatusBarWidget* AGRBHeroCharacter::GetFloatingStatusBar()
{
	return UIFloatingStatusBar;
}

bool AGRBHeroCharacter::IsInFirstPersonPerspective() const
{

	return false;
}

USkeletalMeshComponent* AGRBHeroCharacter::GetFirstPersonMesh() const
{

	return nullptr;
}

USkeletalMeshComponent* AGRBHeroCharacter::GetThirdPersonMesh() const
{

	return nullptr;
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

void AGRBHeroCharacter::OnRep_CurrentWeapon(AGRBWeapon* LastWeapon)
{
	
}

void AGRBHeroCharacter::OnRep_Inventory()
{
	
}
