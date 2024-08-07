// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "GRBShooter/GRBShooter.h"
#include "GRBWeapon.generated.h"

class UAbilitySystemComponent;

UCLASS(Blueprintable, BlueprintType)
class GRBSHOOTER_API AGRBWeapon : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGRBWeapon();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void NotifyActorBeginOverlap(class AActor* Other) override;

};
