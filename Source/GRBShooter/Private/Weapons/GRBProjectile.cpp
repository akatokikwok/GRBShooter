// Copyright 2024 GRB.


#include "Weapons/GRBProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GRBBlueprintFunctionLibrary.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PlayerState.h"


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


	ProjectileMovement->bIsHomingProjectile = mIsHoming;
	if (mHomingTarget)
	{
		ProjectileMovement->HomingTargetComponent = mHomingTarget->GetRootComponent();
	}
}

void AGRBProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (AGRBHeroCharacter* GRBHero = Cast<AGRBHeroCharacter>(GetInstigator()))
	{
		APlayerState* PS = UGameplayStatics::GetPlayerController(this, 0)->PlayerState.Get();
		UAbilitySystemComponent* NowASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Cast<AActor>(PS));
		if (UGRBAbilitySystemComponent* PASC = Cast<UGRBAbilitySystemComponent>(NowASC))
		{
			mGRBASC = PASC;
			if (!GRBHero->IsLocallyControlled())
			{
				const FGameplayTag& CueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.RocketLauncher.Fire"));
				const FGameplayEffectContextHandle& EmptyEffectContextHandle = FGameplayEffectContextHandle();
				const FVector& CueLocation = GetActorLocation();
				const FVector& CueNormal = UKismetMathLibrary::GetForwardVector(GetActorRotation());
				const AActor* CueInstigator = GRBHero;
				const FGameplayCueParameters& CueParameters = UAbilitySystemBlueprintLibrary::MakeGameplayCueParameters(0, 0, EmptyEffectContextHandle,
				                                                                                                        FGameplayTag::EmptyTag, FGameplayTag::EmptyTag, FGameplayTagContainer(),
				                                                                                                        FGameplayTagContainer(), CueLocation, CueNormal,
				                                                                                                        const_cast<AActor*>(CueInstigator), nullptr, nullptr,
				                                                                                                        nullptr, 1, 1,
				                                                                                                        nullptr, false);
				mGRBASC->ExecuteGameplayCueLocal(CueTag, CueParameters);
			}
		}
	}
}

void AGRBProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const FGameplayTag& CueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.RocketLauncher.Impact"));
	const FGameplayEffectContextHandle& EmptyEffectContextHandle = FGameplayEffectContextHandle();
	const FVector& CueLocation = GetActorLocation();
	const FVector& CueNormal = FVector::ZeroVector;
	const AActor* CueInstigator = nullptr;
	const FGameplayCueParameters& CueParameters = UAbilitySystemBlueprintLibrary::MakeGameplayCueParameters(0, 0, EmptyEffectContextHandle,
	                                                                                                        FGameplayTag::EmptyTag, FGameplayTag::EmptyTag, FGameplayTagContainer(),
	                                                                                                        FGameplayTagContainer(), CueLocation, CueNormal,
	                                                                                                        const_cast<AActor*>(CueInstigator), nullptr, nullptr,
	                                                                                                        nullptr, 1, 1,
	                                                                                                        nullptr, false);
	mGRBASC->ExecuteGameplayCueLocal(CueTag, CueParameters);
	Super::EndPlay(EndPlayReason);
}

void AGRBProjectile::OnSphereCompOvlp(AActor* InOtherActor)
{
	if (!HasAuthority())
	{
		K2_DestroyActor();
	}
	else
	{
		if (GetInstigator() != InOtherActor)
		{
			TArray<FHitResult> OutHitResults;
			TArray<TEnumAsByte<EObjectTypeQuery>> PTypes;
			PTypes.AddUnique(UEngineTypes::ConvertToObjectType(ECC_Pawn));
			TArray<AActor*> IgnoreActors;
			UKismetSystemLibrary::SphereTraceMultiForObjects(this, GetActorLocation(), GetActorLocation() + FVector(0.01, 0.01, 0.01), mExplosionRadius,
			                                                 PTypes, false, IgnoreActors, EDrawDebugTrace::None, OutHitResults, true, FColor::Red, FColor::Green, 5.0f);
			for (FHitResult& PerHit : OutHitResults)
			{
				if (AGRBCharacterBase* GRBCharacterBase = Cast<AGRBCharacterBase>(PerHit.GetActor()))
				{
					if (GRBCharacterBase->IsAlive())
					{
						mHitTargets.AddUnique(GRBCharacterBase);
					}
				}
			}

			TArray<AActor*> pTargetActors;
			for (AGRBCharacterBase* PawnBase : mHitTargets)
			{
				pTargetActors.AddUnique(PawnBase);
			}
			UGRBBlueprintFunctionLibrary::AddTargetsToEffectContainerSpec(mGRBGEContainerSpecPak, TArray<FGameplayAbilityTargetDataHandle>(), TArray<FHitResult>(), pTargetActors);
			UGRBBlueprintFunctionLibrary::ApplyExternalEffectContainerSpec(mGRBGEContainerSpecPak);
			K2_DestroyActor();
		}
	}
}
