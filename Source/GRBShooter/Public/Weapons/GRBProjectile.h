// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/GRBAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "GRBProjectile.generated.h"

UCLASS()
class GRBSHOOTER_API AGRBProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGRBProjectile();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable)
	void OnSphereCompOvlp(AActor* InOtherActor);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buss", meta=(ExposeOnSpawn="true"))
	class UGRBAbilitySystemComponent* mGRBASC = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buss", meta=(ExposeOnSpawn="true"))
	FGRBGameplayEffectContainerSpec mGRBGEContainerSpecPak = FGRBGameplayEffectContainerSpec();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buss", meta=(ExposeOnSpawn="true"))
	float mExplosionRadius = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buss", meta=(ExposeOnSpawn="true"))
	TArray<class AGRBCharacterBase*> mHitTargets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buss", meta=(ExposeOnSpawn="true"))
	bool mIsHoming = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buss", meta=(ExposeOnSpawn="true"))
	AActor* mHomingTarget = nullptr;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "PBProjectile")
	class UProjectileMovementComponent* ProjectileMovement;
};
