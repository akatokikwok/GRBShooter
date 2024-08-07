// Copyright 2024 GRB.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/UMG/Public/Blueprint/UserWidget.h"
#include "GRBHUDReticle.generated.h"

/**
 * 绘制在玩家屏幕上的准星UI, 可以被武器 技能 修改.
 * A reticle on the player's HUD. Can be changed by weapons, abilities, etc.
 */
UCLASS()
class GRBSHOOTER_API UGRBHUDReticle : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Essentially an interface that all child classes in Blueprint will have to fill out
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetColor(FLinearColor Color);
};
