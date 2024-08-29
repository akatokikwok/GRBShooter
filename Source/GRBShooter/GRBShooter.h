// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GRBShooter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGRBShooter, Warning, All);

#define COLLISION_PICKUP						ECollisionChannel::ECC_GameTraceChannel4


/*
 * 技能输入ID分类
 */
UENUM(BlueprintType)
enum class EGRBAbilityInputID : uint8
{
	// 0 None
	None				UMETA(DisplayName = "None"),
	// 1 Confirm
	Confirm				UMETA(DisplayName = "Confirm"),
	// 2 Cancel
	Cancel				UMETA(DisplayName = "Cancel"),
	// 3 Sprint
	Sprint				UMETA(DisplayName = "Sprint"),
	// 4 Jump
	Jump				UMETA(DisplayName = "Jump"),
	// 5 PrimaryFire
	PrimaryFire			UMETA(DisplayName = "Primary Fire"),
	// 6 SecondaryFire
	SecondaryFire		UMETA(DisplayName = "Secondary Fire"),
	// 7 Alternate Fire
	AlternateFire		UMETA(DisplayName = "Alternate Fire"),
	// 8 Reload
	Reload				UMETA(DisplayName = "Reload"),
	// 9 NextWeapon
	NextWeapon			UMETA(DisplayName = "Next Weapon"), 
	// 10 PrevWeapon
	PrevWeapon			UMETA(DisplayName = "Previous Weapon"),
	// 11 Interact
	Interact			UMETA(DisplayName = "Interact")
};

UCLASS()
class GRBSHOOTER_API UGRBStaticLibrary : public UObject
{
	GENERATED_BODY()

public:
	template<typename TEnum>
	static FString GetEnumValueAsString(const FString& InEnumName, TEnum InEnumValue);
};

template <typename TEnum>
FString UGRBStaticLibrary::GetEnumValueAsString(const FString& InEnumName, TEnum InEnumValue)
{
	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, *InEnumName, true);
	if (!EnumPtr)
	{
		return FString("InValid");
	}
	return EnumPtr->GetNameStringByIndex(static_cast<int32>(InEnumValue));
}
