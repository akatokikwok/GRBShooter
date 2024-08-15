// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/UMG/Public/Blueprint/UserWidget.h"
#include "GRBHUDWidget.generated.h"

class UPaperSprite;
class UTexture2D;

/**
 * 
 */
UCLASS()
class GRBSHOOTER_API UGRBHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void ShowAbilityConfirmPrompt(bool bShowText);
};
