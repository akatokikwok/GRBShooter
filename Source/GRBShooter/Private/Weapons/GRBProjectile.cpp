// Copyright 2024 GRB

#include "Weapons/GRBProjectile.h"
#include "Runtime/Engine/Classes/GameFramework/ProjectileMovementComponent.h"

// Sets default values
AGRBProjectile::AGRBProjectile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(FName("ProjectileMovement"));
	ProjectileMovement->ProjectileGravityScale = 0;
	ProjectileMovement->InitialSpeed = 7000.0f;

	bReplicates = true;

	//TODO change this to a better value
	NetUpdateFrequency = 100.0f;
}
