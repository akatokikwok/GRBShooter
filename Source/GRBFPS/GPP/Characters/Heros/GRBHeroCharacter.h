#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GRBFPS/GPP/Characters/GRBCharacterBase.h"
#include "GRBFPS/GPP/Characters/Abilities/GRBInteractable.h"
#include "GRBHeroCharacter.generated.h"

/**
 * 真正使用的HeroCharacter
 */
UCLASS()
class AGRBHeroCharacter : public AGRBCharacterBase, public IGRBInteractable
{
	GENERATED_BODY()

public:
	AGRBHeroCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// ~Begin ACharacter::SetupPlayerInputComponent /** 处理输入回调的入口. */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// ~End ACharacter::SetupPlayerInputComponent

	// ~Begin ACharacter::PossessedBy //** 仅在服务端才会调用; 调用顺序在函数 Server's AcknowledgePossession之前. */
	virtual void PossessedBy(AController* NewController) override;
	// ~End ACharacter::PossessedBy

	/** ~ Begin From : GRBCharacterBase::FinishDying*/
	// virtual void FinishDying() override;
	/** ~ End From : GRBCharacterBase::FinishDying*/

public:
	virtual void KnockDown();
	
	virtual void PlayKnockDownEffects();
	
	virtual void PlayReviveEffects();
	
#pragma region ~ 一些对外界可调用的接口 ~
	
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
