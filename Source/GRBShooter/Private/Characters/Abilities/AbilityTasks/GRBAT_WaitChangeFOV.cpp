// Copyright 2024 GRB.


#include "Characters/Abilities/AbilityTasks/GRBAT_WaitChangeFOV.h"
#include "Camera/CameraComponent.h"
#include "Curves/CurveFloat.h"
#include "GRBShooter/GRBShooter.h"

UGRBAT_WaitChangeFOV::UGRBAT_WaitChangeFOV(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bIsFinished = false;
}

UGRBAT_WaitChangeFOV* UGRBAT_WaitChangeFOV::WaitChangeFOV(UGameplayAbility* OwningAbility, FName TaskInstanceName, class UCameraComponent* CameraComponent, float TargetFOV, float Duration, UCurveFloat* OptionalInterpolationCurve)
{
	UGRBAT_WaitChangeFOV* MyObj = NewAbilityTask<UGRBAT_WaitChangeFOV>(OwningAbility, TaskInstanceName);

	MyObj->CameraComponent = CameraComponent;
	if (CameraComponent != nullptr)
	{
		MyObj->StartFOV = MyObj->CameraComponent->FieldOfView;
	}

	MyObj->TargetFOV = TargetFOV;
	MyObj->Duration = FMath::Max(Duration, 0.001f); // Avoid negative or divide-by-zero cases
	MyObj->TimeChangeStarted = MyObj->GetWorld()->GetTimeSeconds();
	MyObj->TimeChangeWillEnd = MyObj->TimeChangeStarted + MyObj->Duration;
	MyObj->LerpCurve = OptionalInterpolationCurve;

	return MyObj;
}

void UGRBAT_WaitChangeFOV::Activate()
{
}

void UGRBAT_WaitChangeFOV::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);

	if (CameraComponent)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();

		if (CurrentTime >= TimeChangeWillEnd)
		{
			bIsFinished = true;

			CameraComponent->SetFieldOfView(TargetFOV);

			if (ShouldBroadcastAbilityTaskDelegates())
			{
				OnTargetFOVReached.Broadcast();
			}
			EndTask();
		}
		else
		{
			float NewFOV;

			float MoveFraction = (CurrentTime - TimeChangeStarted) / Duration;
			if (LerpCurve)
			{
				MoveFraction = LerpCurve->GetFloatValue(MoveFraction);
			}

			NewFOV = FMath::Lerp<float, float>(StartFOV, TargetFOV, MoveFraction);

			CameraComponent->SetFieldOfView(NewFOV);
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

void UGRBAT_WaitChangeFOV::OnDestroy(bool AbilityIsEnding)
{
	Super::OnDestroy(AbilityIsEnding);
}
