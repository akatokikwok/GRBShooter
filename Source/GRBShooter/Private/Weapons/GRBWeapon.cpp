// Copyright 2024 GRB.


#include "Weapons/GRBWeapon.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"
#include "Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "Characters/Abilities/GRBGameplayAbility.h"
#include "Characters/Abilities/GRBGATA_LineTrace.h"
#include "Characters/Abilities/GRBGATA_SphereTrace.h"
#include "Characters/Heroes/GRBHeroCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GRBBlueprintFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Player/GRBPlayerController.h"

// 将网络类型转化为字符串
#define GET_ACTOR_ROLE_FSTRING(Actor) *(FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ENetRole"), true)->GetNameStringByValue(Actor->GetLocalRole()))

AGRBWeapon::AGRBWeapon()
{
	// 永不tick
	PrimaryActorTick.bCanEverTick = false;
	// 开启网络复制
	bReplicates = true;
	// 设置为 true，该对象将按照其拥有者（Owner）对象的网络相关性进行同步更新。也就是说，只有能够接收拥有者数据的客户端才能接收到该对象的数据
	// bNetUseOwnerRelevancy 属性通常在下面的情况下使用：
	//优化网络流量：当一个对象与另一个对象（例如玩家角色）具有逻辑上的关联，只需要在拥有者相关的客户端进行更新时，这个属性可以减小不必要的网络流量。
	//关联子对象：对于一些附属于玩家的对象（例如，玩家的武器、装备等），它们的状态和位置只是对拥有该对象的玩家以及其他观测到该玩家行为的玩家相关。在这种情况下，将这些对象的网络相关性设置为依赖于拥有者是很合理的
	// 意味着武器枪支的网络相关性将依赖于其拥有者，即 PlayerCharacter 对象
	bNetUseOwnerRelevancy = true;
	// 网络复制频率
	NetUpdateFrequency = 100.0f;
	// 依据拾取模式设定是否启用碰撞, 枪支作为场景道具时候是拾取碰撞, 作为直接生成物的时候关闭碰撞
	bSpawnWithCollision = true;
	// 弹匣载弹相关设置
	PrimaryClipAmmo = 0;
	MaxPrimaryClipAmmo = 0;
	SecondaryClipAmmo = 0;
	MaxSecondaryClipAmmo = 0;
	bInfiniteAmmo = false;
	PrimaryAmmoType = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.None"));
	SecondaryAmmoType = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.None"));

	// 设置武器碰撞体
	CollisionComp = CreateDefaultSubobject<UCapsuleComponent>(FName("CollisionComponent"));
	CollisionComp->InitCapsuleSize(40.0f, 50.0f);
	CollisionComp->SetCollisionObjectType(COLLISION_PICKUP);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Manually enable when in pickup mode
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = CollisionComp;

	// 处理Mesh1P
	WeaponMesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh1P"));
	WeaponMesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh1P->CastShadow = false;
	WeaponMesh1P->SetVisibility(false, true);
	WeaponMesh1P->SetupAttachment(CollisionComp);
	WeaponMesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose; // 用来决定动画组件是否需要在不可见时继续进行动画更新

	// 处理Mesh3P
	WeaponMesh3PickupRelativeLocation = FVector(0.0f, -25.0f, 0.0f);
	WeaponMesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh3P"));
	WeaponMesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh3P->SetupAttachment(CollisionComp);
	WeaponMesh3P->SetRelativeLocation(WeaponMesh3PickupRelativeLocation);
	WeaponMesh3P->CastShadow = true;
	WeaponMesh3P->SetVisibility(true, true);
	WeaponMesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	// 武器各状态的标签初始化
	WeaponPrimaryInstantAbilityTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.Primary.Instant");
	WeaponSecondaryInstantAbilityTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.Secondary.Instant");
	WeaponAlternateInstantAbilityTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.Alternate.Instant");
	WeaponIsFiringTag = FGameplayTag::RequestGameplayTag("Weapon.IsFiring");
	FireMode = FGameplayTag::RequestGameplayTag("Weapon.FireMode.None");
	StatusText = DefaultStatusText;

	// 保存当异常情况发生会阻碍拾取武器的标签组
	RestrictedPickupTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
	RestrictedPickupTags.AddTag(FGameplayTag::RequestGameplayTag("State.KnockedDown"));
}

void AGRBWeapon::BeginPlay()
{
	// 复位武器开火模式Tag
	ResetWeapon();

	// 没有宿主且允许拾取 则给到一个默认碰撞
	// Spawned into the world without an owner, enable collision as we are in pickup mode
	if (!OwningCharacter && bSpawnWithCollision)
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	Super::BeginPlay();
}

void AGRBWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (LineTraceTargetActor)
	{
		LineTraceTargetActor->Destroy();
	}

	if (SphereTraceTargetActor)
	{
		SphereTraceTargetActor->Destroy();
	}
	Super::EndPlay(EndPlayReason);
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

UAbilitySystemComponent* AGRBWeapon::GetAbilitySystemComponent() const
{
	return nullptr;
}

USkeletalMeshComponent* AGRBWeapon::GetWeaponMesh1P() const
{
	return WeaponMesh1P;
}

USkeletalMeshComponent* AGRBWeapon::GetWeaponMesh3P() const
{
	return WeaponMesh3P;
}

void AGRBWeapon::AddAbilities()
{
	// 确保枪有装备了ASC的枪手
	if (!IsValid(OwningCharacter) || !OwningCharacter->GetAbilitySystemComponent())
	{
		return;
	}
	UGRBAbilitySystemComponent* GRBASC = Cast<UGRBAbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());

	// 意外情况下,打印ASC的复制类型
	if (!GRBASC)
	{
		UE_LOG(LogTemp, Error, TEXT("%s %s Role: %s ASC is null"), *FString(__FUNCTION__), *GetName(), GET_ACTOR_ROLE_FSTRING(OwningCharacter));
		return;
	}

	// 仅承认在服务端授权 枪支携带的技能
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// 初始化这些蓝图技能(如持续射击, 换弹)并注册到技能句柄池子内
	for (TSubclassOf<UGRBGameplayAbility>& Ability : Abilities)
	{
		AbilitySpecHandles.Add(GRBASC->GiveAbility(
			FGameplayAbilitySpec(
				Ability,
				GetAbilityLevel(Ability.GetDefaultObject()->AbilityID), // 该技能等级
				static_cast<int32>(Ability.GetDefaultObject()->AbilityInputID), // 该技能输入ID, 每个蓝图技能都有自己独一无二的技能输入ID, 如PrimaryFire, Reload
				this)
		));
	}
}

void AGRBWeapon::RemoveAbilities()
{
	if (!IsValid(OwningCharacter) || !OwningCharacter->GetAbilitySystemComponent())
	{
		return;
	}
	UGRBAbilitySystemComponent* GRBASC = Cast<UGRBAbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());
	if (!GRBASC)
	{
		return;
	}
	// Remove abilities, but only on the server	
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// 从武器句柄池子内移除所有蓝图装配的 技能句柄
	for (FGameplayAbilitySpecHandle& SpecHandle : AbilitySpecHandles)
	{
		GRBASC->ClearAbility(SpecHandle);
	}
}

void AGRBWeapon::SetOwningCharacter(AGRBHeroCharacter* InOwningCharacter)
{
	OwningCharacter = InOwningCharacter;
	if (OwningCharacter)
	{
		// Called when added to inventory
		AbilitySystemComponent = Cast<UGRBAbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());
		SetOwner(InOwningCharacter);
		AttachToComponent(OwningCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 已装备其他枪支的情形;
		if (OwningCharacter->GetCurrentWeapon() != this)
		{
			WeaponMesh3P->CastShadow = false;
			WeaponMesh3P->SetVisibility(true, true);
			WeaponMesh3P->SetVisibility(false, true);
		}
	}
	else
	{
		AbilitySystemComponent = nullptr;
		SetOwner(nullptr);
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}
