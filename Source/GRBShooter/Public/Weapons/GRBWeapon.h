// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "GRBShooter/GRBShooter.h"
#include "GRBWeapon.generated.h"

class AGRBHeroCharacter;
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

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GRBShooter|GRBWeapon")
	virtual USkeletalMeshComponent* GetWeaponMesh1P() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GRBShooter|GRBWeapon")
	virtual USkeletalMeshComponent* GetWeaponMesh3P() const;

	void SetOwningCharacter(AGRBHeroCharacter* InOwningCharacter);
	
	// Called when the player equips this weapon
	virtual void Equip();

	// Called when the player unequips this weapon
	virtual void UnEquip();
	
	virtual void AddAbilities();

	virtual void RemoveAbilities();

	virtual int32 GetAbilityLevel(EGRBAbilityInputID AbilityID);

	///--@brief 复位武器开火模式Tag--/
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual void ResetWeapon();

	UFUNCTION(NetMulticast, Reliable)
	void OnDropped(FVector NewLocation);
	virtual void OnDropped_Implementation(FVector NewLocation);
	virtual bool OnDropped_Validate(FVector NewLocation);

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual int32 GetPrimaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual int32 GetMaxPrimaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual int32 GetSecondaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual int32 GetMaxSecondaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual void SetPrimaryClipAmmo(int32 NewPrimaryClipAmmo);

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual void SetMaxPrimaryClipAmmo(int32 NewMaxPrimaryClipAmmo);

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual void SetSecondaryClipAmmo(int32 NewSecondaryClipAmmo);

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual void SetMaxSecondaryClipAmmo(int32 NewMaxSecondaryClipAmmo);

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	TSubclassOf<class UGRBHUDReticle> GetPrimaryHUDReticleClass() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	virtual bool HasInfiniteAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|Animation")
	UAnimMontage* GetEquip1PMontage() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|Animation")
	UAnimMontage* GetEquip3PMontage() const;
	
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|Audio")
	class USoundCue* GetPickupSound() const;

	UFUNCTION(BlueprintCallable, Category = "GRBShooter|GRBWeapon")
	FText GetDefaultStatusText() const;

	// Getter for LineTraceTargetActor. Spawns it if it doesn't exist yet.
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|Targeting")
	AGRBGATA_LineTrace* GetLineTraceTargetActor();

	// Getter for SphereTraceTargetActor. Spawns it if it doesn't exist yet.
	UFUNCTION(BlueprintCallable, Category = "GRBShooter|Targeting")
	AGRBGATA_SphereTrace* GetSphereTraceTargetActor();

protected:
	// Called when the player picks up this weapon
	virtual void PickUpOnTouch(AGRBHeroCharacter* InCharacter);

	UFUNCTION()
	virtual void OnRep_PrimaryClipAmmo(int32 OldPrimaryClipAmmo);

	UFUNCTION()
	virtual void OnRep_MaxPrimaryClipAmmo(int32 OldMaxPrimaryClipAmmo);

	UFUNCTION()
	virtual void OnRep_SecondaryClipAmmo(int32 OldSecondaryClipAmmo);

	UFUNCTION()
	virtual void OnRep_MaxSecondaryClipAmmo(int32 OldMaxSecondaryClipAmmo);

	
public:
	// 依据拾取模式设定是否启用碰撞, 枪支作为场景道具时候是拾取碰撞, 作为直接生成物的时候关闭碰撞
	// Whether or not to spawn this weapon with collision enabled (pickup mode).
	// Set to false when spawning directly into a player's inventory or true when spawning into the world in pickup mode.
	UPROPERTY(BlueprintReadWrite)
	bool bSpawnWithCollision;

	// 武器Tag, 会决策哪种ABP动画蓝图去播放
	// This tag is primarily used by the first person Animation Blueprint to determine which animations to play
	// (Rifle vs Rocket Launcher)
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GRBShooter|GRBWeapon")
	FGameplayTag WeaponTag;

	// 保存当异常情况发生会阻碍拾取武器的标签组
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GRBShooter|GRBWeapon")
	FGameplayTagContainer RestrictedPickupTags;
	
	// UI HUD Primary Icon when equipped. Using Sprites because of the texture atlas from ShooterGame.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|UI")
	UPaperSprite* PrimaryIcon;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|UI")
	UPaperSprite* SecondaryIcon;

	// UI HUD Primary Clip Icon when equipped
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|UI")
	UPaperSprite* PrimaryClipIcon;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|UI")
	UPaperSprite* SecondaryClipIcon;

	// 标签:当前开火模式
	UPROPERTY(BlueprintReadWrite, VisibleInstanceOnly, Category = "GRBShooter|GRBWeapon")
	FGameplayTag FireMode;

	// 主弹匣的关联TAG
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GRBShooter|GRBWeapon")
	FGameplayTag PrimaryAmmoType;

	// 备用弹匣的关联TAG
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GRBShooter|GRBWeapon")
	FGameplayTag SecondaryAmmoType;

	// Things like fire mode for rifle
	UPROPERTY(BlueprintReadWrite, VisibleInstanceOnly, Category = "GRBShooter|GRBWeapon")
	FText StatusText;

	UPROPERTY(BlueprintAssignable, Category = "GRBShooter|GRBWeapon")
	FWeaponAmmoChangedDelegate OnPrimaryClipAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "GRBShooter|GRBWeapon")
	FWeaponAmmoChangedDelegate OnMaxPrimaryClipAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "GRBShooter|GRBWeapon")
	FWeaponAmmoChangedDelegate OnSecondaryClipAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "GRBShooter|GRBWeapon")
	FWeaponAmmoChangedDelegate OnMaxSecondaryClipAmmoChanged;

protected:
	UPROPERTY()
	UGRBAbilitySystemComponent* AbilitySystemComponent;

	// 主弹匣
	// How much ammo in the clip the gun starts with
	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_PrimaryClipAmmo, Category = "GRBShooter|GRBWeapon|Ammo")
	int32 PrimaryClipAmmo;

	// 主弹匣最大载弹量
	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_MaxPrimaryClipAmmo, Category = "GRBShooter|GRBWeapon|Ammo")
	int32 MaxPrimaryClipAmmo;

	// 备用弹匣载弹量
	// How much ammo in the clip the gun starts with. Used for things like rifle grenades.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_SecondaryClipAmmo, Category = "GRBShooter|GRBWeapon|Ammo")
	int32 SecondaryClipAmmo;

	// 最大备用弹匣载弹量
	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_MaxSecondaryClipAmmo, Category = "GRBShooter|GRBWeapon|Ammo")
	int32 MaxSecondaryClipAmmo;

	// 是否开启作弊 无限弹匣
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|GRBWeapon|Ammo")
	bool bInfiniteAmmo;

	// 准星样式
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|UI")
	TSubclassOf<class UGRBHUDReticle> PrimaryHUDReticleClass;

	UPROPERTY()
	AGRBGATA_LineTrace* LineTraceTargetActor;

	UPROPERTY()
	AGRBGATA_SphereTrace* SphereTraceTargetActor;

	// 当为拾取模式时候的武器碰撞体
	// Collision capsule for when weapon is in pickup mode
	UPROPERTY(VisibleAnywhere)
	class UCapsuleComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "GRBShooter|GRBWeapon")
	USkeletalMeshComponent* WeaponMesh1P;

	UPROPERTY(VisibleAnywhere, Category = "GRBShooter|GRBWeapon")
	USkeletalMeshComponent* WeaponMesh3P;

	// Relative Location of weapon 3P Mesh when in pickup mode
	// 1P weapon mesh is invisible so it doesn't need one
	UPROPERTY(EditDefaultsOnly, Category = "GRBShooter|GRBWeapon")
	FVector WeaponMesh3PickupRelativeLocation;

	// Relative Location of weapon 1P Mesh when equipped
	UPROPERTY(EditDefaultsOnly, Category = "GRBShooter|GRBWeapon")
	FVector WeaponMesh1PEquippedRelativeLocation;

	// Relative Location of weapon 3P Mesh when equipped
	UPROPERTY(EditDefaultsOnly, Category = "GRBShooter|GRBWeapon")
	FVector WeaponMesh3PEquippedRelativeLocation;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "GRBShooter|GRBWeapon")
	AGRBHeroCharacter* OwningCharacter;

	UPROPERTY(EditAnywhere, Category = "GRBShooter|GRBWeapon")
	TArray<TSubclassOf<UGRBGameplayAbility>> Abilities;

	UPROPERTY(BlueprintReadOnly, Category = "GRBShooter|GRBWeapon")
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GRBShooter|GRBWeapon")
	FGameplayTag DefaultFireMode;

	// Things like fire mode for rifle
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|GRBWeapon")
	FText DefaultStatusText;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GRBShooter|Animation")
	UAnimMontage* Equip1PMontage;

	UPROPERTY(BlueprintReadonly, EditAnywhere, Category = "GRBShooter|Animation")
	UAnimMontage* Equip3PMontage;

	// Sound played when player picks it up
	UPROPERTY(EditDefaultsOnly, Category = "GRBShooter|Audio")
	class USoundCue* PickupSound;

	// Cache tags
	FGameplayTag WeaponPrimaryInstantAbilityTag;
	FGameplayTag WeaponSecondaryInstantAbilityTag;
	FGameplayTag WeaponAlternateInstantAbilityTag;
	FGameplayTag WeaponIsFiringTag;
};
