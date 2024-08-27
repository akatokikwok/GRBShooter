// Copyright 2024 GRB

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRBProjectile.generated.h"

class UProjectileMovementComponent;

/*
 * 子弹类
 */
UCLASS()
class GRBSHOOTER_API AGRBProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGRBProjectile();

	//
	UFUNCTION(BlueprintImplementableEvent, Blueprintable, Category="Buss")
	void K2_PrepareWhenSpawned(const AActor* InOWner, const struct FGRBGameplayEffectContainerSpec& InGRBEffectSpecPak, bool IsHoming, const AActor* InHomingTarget, const APawn* InInstigator);
	
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "PBProjectile")
	UProjectileMovementComponent* ProjectileMovement;
};
