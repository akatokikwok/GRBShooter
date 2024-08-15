// Copyright 2024 GRB.

#include "Player/GRBPlayerState.h"
#include "Characters/Abilities/AttributeSets/GRBAmmoAttributeSet.h"
#include "Characters/Abilities/AttributeSets/GRBAttributeSetBase.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "Player/GRBPlayerController.h"
#include "UI/GRBHUDWidget.h"
#include "Weapons/GRBWeapon.h"

AGRBPlayerState::AGRBPlayerState()
{
	// 创建ASC并显式地执行网络复制
	m_PlayerStateASCComponent = CreateDefaultSubobject<UGRBAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	m_PlayerStateASCComponent->SetIsReplicated(true);

	// ASC的网络复制模式设置为混合模式的作用: 服务端下发过来的BUFF仅发给自己,不发给模拟端, 且不会通知本机, 而TAG CUE则是正常复制给我们
	// Mixed mode means we only are replicated the GEs to ourself, not the GEs to simulated proxies. If another GDPlayerState (Hero) receives a GE,
	// we won't be told about it by the Server. Attributes, GameplayTags, and GameplayCues will still replicate to us.
	m_PlayerStateASCComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create the attribute set, this replicates by default
	// Adding it as a subobject of the owning actor of an AbilitySystemComponent
	// automatically registers the AttributeSet with the AbilitySystemComponent
	AttributeSetBase = CreateDefaultSubobject<UGRBAttributeSetBase>(TEXT("AttributeSetBase"));

	AmmoAttributeSet = CreateDefaultSubobject<UGRBAmmoAttributeSet>(TEXT("AmmoAttributeSet"));

	// Set PlayerState's NetUpdateFrequency to the same as the Character.
	// Default is very low for PlayerStates and introduces perceived lag in the ability system.
	// 100 is probably way too high for a shipping game, you can adjust to fit your needs.
	NetUpdateFrequency = 100.0f;

	DeadTag = FGameplayTag::RequestGameplayTag("State.Dead");

	KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
}

void AGRBPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (m_PlayerStateASCComponent)
	{
		// 给 角色属性-生命值变化委托注册一个监听回调
		HealthChangedDelegateHandle = m_PlayerStateASCComponent->GetGameplayAttributeValueChangeDelegate(AttributeSetBase->GetHealthAttribute())
		                                                       .AddUObject(this, &AGRBPlayerState::OnAttribute_HealthChangedCallback);

		// 在ASC上注册一个监听回调, 用于处理监听 角色击倒标签(添加/移除)后做出对应的逻辑处理
		PawnKnockdownAddOrRemoveHandle = m_PlayerStateASCComponent->RegisterGameplayTagEvent(KnockedDownTag, EGameplayTagEventType::NewOrRemoved)
		                                                          .AddUObject(this, &AGRBPlayerState::KnockDownTagChanged);
	}
}

UAbilitySystemComponent* AGRBPlayerState::GetAbilitySystemComponent() const
{
	return m_PlayerStateASCComponent;
}

UGRBAttributeSetBase* AGRBPlayerState::GetAttributeSetBase() const
{
	return AttributeSetBase;
}

UGRBAmmoAttributeSet* AGRBPlayerState::GetAmmoAttributeSet() const
{
	return AmmoAttributeSet;
}

bool AGRBPlayerState::IsAlive() const
{
	return GetHealth() > 0.0f;
}

void AGRBPlayerState::ShowAbilityConfirmPrompt(bool bShowPrompt)
{
	AGRBPlayerController* PC = Cast<AGRBPlayerController>(GetOwner());
	if (PC)
	{
		UGRBHUDWidget* HUD = PC->GetGRBHUD();
		if (HUD)
		{
			HUD->ShowAbilityConfirmPrompt(bShowPrompt);
		}
	}
}

void AGRBPlayerState::ShowInteractionPrompt(float InteractionDuration)
{
	AGRBPlayerController* PC = Cast<AGRBPlayerController>(GetOwner());
	if (PC)
	{
		UGRBHUDWidget* HUD = PC->GetGRBHUD();
		if (HUD)
		{
			HUD->ShowInteractionPrompt(InteractionDuration);
		}
	}
}

void AGRBPlayerState::HideInteractionPrompt()
{
	AGRBPlayerController* PC = Cast<AGRBPlayerController>(GetOwner());
	if (PC)
	{
		UGRBHUDWidget* HUD = PC->GetGRBHUD();
		if (HUD)
		{
			HUD->HideInteractionPrompt();
		}
	}
}

void AGRBPlayerState::StartInteractionTimer(float InteractionDuration)
{
	AGRBPlayerController* PC = Cast<AGRBPlayerController>(GetOwner());
	if (PC)
	{
		UGRBHUDWidget* HUD = PC->GetGRBHUD();
		if (HUD)
		{
			HUD->StartInteractionTimer(InteractionDuration);
		}
	}
}

void AGRBPlayerState::StopInteractionTimer()
{
	AGRBPlayerController* PC = Cast<AGRBPlayerController>(GetOwner());
	if (PC)
	{
		UGRBHUDWidget* HUD = PC->GetGRBHUD();
		if (HUD)
		{
			HUD->StopInteractionTimer();
		}
	}
}

#pragma region ~ 对外预留的一些属性集Getter接口 ~
float AGRBPlayerState::GetHealth() const
{
	return AttributeSetBase->GetHealth();
}

float AGRBPlayerState::GetMaxHealth() const
{
	return AttributeSetBase->GetMaxHealth();
}

float AGRBPlayerState::GetHealthRegenRate() const
{
	return AttributeSetBase->GetHealthRegenRate();
}

float AGRBPlayerState::GetMana() const
{
	return AttributeSetBase->GetMana();
}

float AGRBPlayerState::GetMaxMana() const
{
	return AttributeSetBase->GetMaxMana();
}

float AGRBPlayerState::GetManaRegenRate() const
{
	return AttributeSetBase->GetManaRegenRate();
}

float AGRBPlayerState::GetStamina() const
{
	return AttributeSetBase->GetStamina();
}

float AGRBPlayerState::GetMaxStamina() const
{
	return AttributeSetBase->GetMaxStamina();
}

float AGRBPlayerState::GetStaminaRegenRate() const
{
	return AttributeSetBase->GetStaminaRegenRate();
}

float AGRBPlayerState::GetShield() const
{
	return AttributeSetBase->GetShield();
}

float AGRBPlayerState::GetMaxShield() const
{
	return AttributeSetBase->GetMaxShield();
}

float AGRBPlayerState::GetShieldRegenRate() const
{
	return AttributeSetBase->GetShieldRegenRate();
}

float AGRBPlayerState::GetArmor() const
{
	return AttributeSetBase->GetArmor();
}

float AGRBPlayerState::GetMoveSpeed() const
{
	return AttributeSetBase->GetMoveSpeed();
}

int32 AGRBPlayerState::GetCharacterLevel() const
{
	return AttributeSetBase->GetCharacterLevel();
}

int32 AGRBPlayerState::GetXP() const
{
	return AttributeSetBase->GetXP();
}

int32 AGRBPlayerState::GetXPBounty() const
{
	return AttributeSetBase->GetXPBounty();
}

int32 AGRBPlayerState::GetGold() const
{
	return AttributeSetBase->GetGold();
}

int32 AGRBPlayerState::GetGoldBounty() const
{
	return AttributeSetBase->GetGoldBounty();
}

int32 AGRBPlayerState::GetPrimaryClipAmmo() const
{
	AGRBHeroCharacter* Hero = GetPawn<AGRBHeroCharacter>();
	if (Hero)
	{
		return Hero->GetPrimaryClipAmmo();
	}

	return 0;
}

int32 AGRBPlayerState::GetPrimaryReserveAmmo() const
{
	AGRBHeroCharacter* Hero = GetPawn<AGRBHeroCharacter>();
	if (Hero && Hero->GetCurrentWeapon() && AmmoAttributeSet)
	{
		FGameplayAttribute Attribute = AmmoAttributeSet->GetReserveAmmoAttributeFromTag(Hero->GetCurrentWeapon()->PrimaryAmmoType);
		if (Attribute.IsValid())
		{
			return m_PlayerStateASCComponent->GetNumericAttribute(Attribute);
		}
	}

	return 0;
}
#pragma endregion

void AGRBPlayerState::OnAttribute_HealthChangedCallback(const FOnAttributeChangeData& Data)
{
	// Check for and handle knockdown and death
	AGRBHeroCharacter* Hero = Cast<AGRBHeroCharacter>(GetPawn());
	if (IsValid(Hero) && !IsAlive() && !m_PlayerStateASCComponent->HasMatchingGameplayTag(DeadTag))
	{
		if (Hero)
		{
			if (!m_PlayerStateASCComponent->HasMatchingGameplayTag(KnockedDownTag))
			{
				Hero->KnockDown();
			}
			else
			{
				Hero->FinishDying();
			}
		}
	}
}

void AGRBPlayerState::KnockDownTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	AGRBHeroCharacter* Hero = Cast<AGRBHeroCharacter>(GetPawn());
	if (!IsValid(Hero))
	{
		return;
	}

	if (NewCount > 0)
	{
		Hero->PlayKnockDownEffects();
	}
	else if (NewCount < 1 && !m_PlayerStateASCComponent->HasMatchingGameplayTag(DeadTag))
	{
		Hero->PlayReviveEffects();
	}
}
