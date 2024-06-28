#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GRBFloatingStatusBarWidget.generated.h"

/**
 * 
 */
UCLASS()
class GRBFPS_API UGRBFloatingStatusBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly)
	class AGRBCharacterBase* OwningCharacter;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetHealthPercentage(float HealthPercentage);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetManaPercentage(float ManaPercentage);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetShieldPercentage(float ShieldPercentage);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetCharacterName(const FText& NewName);
};
