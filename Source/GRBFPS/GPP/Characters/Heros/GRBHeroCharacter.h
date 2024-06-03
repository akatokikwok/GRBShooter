#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GRBFPS/GPP/Characters/GRBCharacterBase.h"
#include "GRBFPS/GPP/Characters/Abilities/GRBInteractable.h"
#include "GRBHeroCharacter.generated.h"

UCLASS()
class AGRBHeroCharacter : public AGRBCharacterBase, public IGRBInteractable
{
	GENERATED_BODY()

public:
	AGRBHeroCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// ~Begin ACharacter::SetupPlayerInputComponent
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// ~End ACharacter::SetupPlayerInputComponent

	// ~Begin ACharacter::PossessedBy
	virtual void PossessedBy(AController* NewController) override;
	// ~End ACharacter::PossessedBy

	/** ~ Begin From : GRBCharacterBase::FinishDying*/
	// virtual void FinishDying() override;
	/** ~ End From : GRBCharacterBase::FinishDying*/

public:
	virtual void KnockDown();
	
	virtual void PlayKnockDownEffects();
	
	virtual void PlayReviveEffects();
	
	UFUNCTION(BlueprintCallable, Category = "GASShooter|GSHeroCharacter")
	virtual bool IsInFirstPersonPerspective() const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GASShooter|GSHeroCharacter")
	USkeletalMeshComponent* GetFirstPersonMesh() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GASShooter|GSHeroCharacter")
	USkeletalMeshComponent* GetThirdPersonMesh() const;
	
#pragma region ~ 一些对外界可调用的接口 ~
	
public:
	//
	UFUNCTION(BlueprintCallable, Category = "GASShooter|GSHeroCharacter")
	virtual bool IsInFirstPersonPerspective() const;
	
protected:

private:
	
#pragma endregion ~ 一些对外界可调用的接口 ~

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
#pragma region 字段
public:
	// 允许在第一人称开火的开关
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GRBFPS|GRBHeroCharacter")
	bool bStartInFirstPersonPerspective;

	// 当前英雄开火时候会携带的技能标签
	FGameplayTag CurrentWeaponTag;
#pragma endregion 字段
	
};
