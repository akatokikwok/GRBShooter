#include "GRBCharacterBase.h"
#include "Abilities/GRBAbilitySystemComponent.h"
#include "Abilities/GRBGameplayAbility.h"

AGRBCharacterBase::AGRBCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void AGRBCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void AGRBCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	InitializeAttributes();

	AddStartupEffects();

	AddCharacterAbilities();
	
}

void AGRBCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UAbilitySystemComponent* AGRBCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

int32 AGRBCharacterBase::GetAbilityLevel(EGRBAbilityInputID AbilityID) const
{
	return 1;
}

bool AGRBCharacterBase::IsAlive() const
{
	return GetHealth() > 0.0f;
}

void AGRBCharacterBase::Die()
{
	
}

void AGRBCharacterBase::FinishDying()
{
	AActor::Destroy();
}

int32 AGRBCharacterBase::GetCharacterLevel() const
{
	// TODO
	return 1;
}

float AGRBCharacterBase::GetHealth() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetHealth();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetMaxHealth() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetMaxHealth();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetMana() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetMana();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetMaxMana() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetMaxMana();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetStamina() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetStamina();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetMaxStamina() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetMaxStamina();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetShield() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetShield();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetMaxShield() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetMaxShield();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetMoveSpeed() const
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetMoveSpeed();
	}
	return 0.0f;
}

float AGRBCharacterBase::GetMoveSpeedBaseValue() const
{
	if (IsValid(AttributeSetBase))
	{
		// 注意这里是拿取的AS-移速里的BaseValue.
		return AttributeSetBase->GetMoveSpeedAttribute().GetGameplayAttributeData(AttributeSetBase)->GetBaseValue();
	}
	return 0.0f;
}

// 在服务端授权技能, 并且GA-Hanle会被网络复制到所属客户端
void AGRBCharacterBase::AddCharacterAbilities()
{
	// 仅在权威服务器里才承认授权GA, 且不允许多次授权
	if (GetLocalRole() != ROLE_Authority || !IsValid(AbilitySystemComponent) || AbilitySystemComponent->bCharacterAbilitiesGiven)
	{
		return;
	}

	// 对英雄固有携带的技能组内的每个GA进行 构建句柄并授权.
	for (TSubclassOf<UGRBGameplayAbility>& StartupAbility : CharacterAbilities)
	{
		AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(StartupAbility, GetAbilityLevel(StartupAbility.GetDefaultObject()->m_AbilityID)/*GA Level-ID*/, static_cast<int32>(StartupAbility.GetDefaultObject()->m_AbilityInputID)/*GA input-ID*/, this)
		);
	}
	// 确认已授权.
	AbilitySystemComponent->bCharacterAbilitiesGiven = true;
}

/** 巧妙借助1个BUFF处理且初始化英雄固有携带的AS. */
void AGRBCharacterBase::InitializeAttributes()
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	// 未配置人物默认的用于重生的GE.
	if (!DefaultAttributes)
	{
		UE_LOG(LogTemp, Error, TEXT("%s() Missing DefaultAttributes for %s. Please fill in the character's Blueprint."), *FString(__FUNCTION__), *GetName());
		return;
	}

	// 使用ASC的MakeEffectContext接口组1个BUFF-Context, 并设置这个BUFF的源OBJ是英雄基类.(客户端和服务端都会执行)
	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	// 利用BUFF-Context构建并应用真正的BUFF(即这个RespawnBuff)
	FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributes, GetCharacterLevel(), EffectContext);
	if (NewHandle.IsValid())
	{
		FActiveGameplayEffectHandle ActiveGEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*NewHandle.Data.Get());
	}
}

// 处理Respawn的BUFF和一组英雄固有携带的BUFF组
void AGRBCharacterBase::AddStartupEffects()
{
	// 仅承认服务端并检查反复授权初始BUFF.
	if (GetLocalRole() != ROLE_Authority || !IsValid(AbilitySystemComponent) || AbilitySystemComponent->bStartupEffectsApplied)
	{
		return;
	}
	// 为英雄身上固有携带的BUFF组配置并应用这些BUFF到自身.
	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	for (TSubclassOf<UGameplayEffect> GameplayEffect : StartupEffects)
	{
		FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(GameplayEffect, GetCharacterLevel(), EffectContext);
		if (NewHandle.IsValid())
		{
			FActiveGameplayEffectHandle ActiveGEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*NewHandle.Data.Get(), AbilitySystemComponent);
		}
	}
	AbilitySystemComponent->bStartupEffectsApplied = true;
}

void AGRBCharacterBase::SetHealth(float Health)
{
	
}

void AGRBCharacterBase::SetMana(float Mana)
{
	
}

void AGRBCharacterBase::SetStamina(float Stamina)
{
	
}

void AGRBCharacterBase::SetShield(float Shield)
{
	
}
