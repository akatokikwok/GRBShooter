// Copyright 2024 GRB.

#include "Weapons/GRBWeapon.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "Characters/Abilities/GRBGameplayAbility.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GRBBlueprintFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Player/GRBPlayerController.h"

AGRBWeapon::AGRBWeapon()
{
}

void AGRBWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AGRBWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* AGRBWeapon::GetAbilitySystemComponent() const
{
	return nullptr;
}

void AGRBWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AGRBWeapon::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
}

void AGRBWeapon::NotifyActorBeginOverlap(AActor* Other)
{
	Super::NotifyActorBeginOverlap(Other);
}
