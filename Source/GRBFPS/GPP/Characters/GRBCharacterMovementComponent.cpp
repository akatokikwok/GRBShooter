#include "GRBFPS/GPP/Characters/GRBCharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "GRBFPS/GPP/Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "GRBFPS/GPP/Characters/GRBCharacterBase.h"
#include "GameplayTagContainer.h"

void UGRBCharacterMovementComponent::FGRBSavedMove::Clear()
{
	Super::Clear();
	SavedRequestToStartSprinting = false;
	SavedRequestToStartADS = false;
}

uint8 UGRBCharacterMovementComponent::FGRBSavedMove::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (SavedRequestToStartSprinting)
	{
		Result |= FLAG_Custom_0;
	}

	if (SavedRequestToStartADS)
	{
		Result |= FLAG_Custom_1;
	}

	return Result;
}

bool UGRBCharacterMovementComponent::FGRBSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	//Set which moves can be combined together. This will depend on the bit flags that are used.
	if (SavedRequestToStartSprinting != ((FGRBSavedMove*)NewMove.Get())->SavedRequestToStartSprinting)
	{
		return false;
	}

	if (SavedRequestToStartADS != ((FGRBSavedMove*)NewMove.Get())->SavedRequestToStartADS)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void UGRBCharacterMovementComponent::FGRBSavedMove::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	UGRBCharacterMovementComponent* CharacterMovement = Cast<UGRBCharacterMovementComponent>(Character->GetCharacterMovement());
	if (CharacterMovement)
	{
		SavedRequestToStartSprinting = CharacterMovement->RequestToStartSprinting;
		SavedRequestToStartADS = CharacterMovement->RequestToStartADS;
	}
}

void UGRBCharacterMovementComponent::FGRBSavedMove::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	UGRBCharacterMovementComponent* CharacterMovement = Cast<UGRBCharacterMovementComponent>(Character->GetCharacterMovement());
	if (CharacterMovement)
	{
	}
}

UGRBCharacterMovementComponent::FGRBNetworkPredictionData_Client::FGRBNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr UGRBCharacterMovementComponent::FGRBNetworkPredictionData_Client::AllocateNewMove()
{
	return FSavedMovePtr(new FGRBSavedMove());
}

UGRBCharacterMovementComponent::UGRBCharacterMovementComponent()
{
	SprintSpeedMultiplier = 1.4f;
	ADSSpeedMultiplier = 0.8f;
	KnockedDownSpeedMultiplier = 0.4f;

	KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
	InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");
	InteractingRemovalTag = FGameplayTag::RequestGameplayTag("State.InteractingRemoval");
}

/** 根据不同业务情形获取玩家移速, Override UCharacterMovementComponent::GetMaxSpeed() */
float UGRBCharacterMovementComponent::GetMaxSpeed() const
{
	AGRBCharacterBase* Owner = Cast<AGRBCharacterBase>(GetOwner());
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("%s() UGRBCharacterMovementComponent doest not have Owner"), *FString(__FUNCTION__));
		return Super::GetMaxSpeed();
	}

	// 玩家死亡会失去所有移速.
	if (!Owner->IsAlive())
	{
		return 0.0f;
	}
	// Don't move while interacting or being interacted on (revived); 玩家使用交互键或者是被治疗过程中不给移动
	if (Owner->GetAbilitySystemComponent() && Owner->GetAbilitySystemComponent()->GetTagCount(InteractingTag) > Owner->GetAbilitySystemComponent()->GetTagCount(InteractingRemovalTag))
	{
		return 0.0f;
	}
	// 玩家处于被击倒状态 会影响移速
	if (Owner->GetAbilitySystemComponent() && Owner->GetAbilitySystemComponent()->HasMatchingGameplayTag(KnockedDownTag))
	{
		return Owner->GetMoveSpeed() * KnockedDownSpeedMultiplier;
	}
	// 玩家如果收到了外部开始冲刺请求, 会影响移速
	if (RequestToStartSprinting)
	{
		return Owner->GetMoveSpeed() * SprintSpeedMultiplier;
	}
	// 玩家如果收到了外部 AimDownSight瞄准请求, 会影响移速.
	if (RequestToStartADS)
	{
		return Owner->GetMoveSpeed() * ADSSpeedMultiplier;
	}

	return Owner->GetMoveSpeed();
}

/** Override UCharacterMovementComponent::UpdateFromCompressedFlags; 在服务端解包压缩的标识符(关乎到移动操作的移动队列FSavedMove_Character) */
void UGRBCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	//The Flags parameter contains the compressed input flags that are stored in the saved move.
	//UpdateFromCompressed flags simply copies the flags from the saved move into the movement component.
	//It basically just resets the movement component to the state when the move was made so it can simulate from there.
	RequestToStartSprinting = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	RequestToStartADS = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

FNetworkPredictionData_Client* UGRBCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != NULL);

	if (!ClientPredictionData)
	{
		UGRBCharacterMovementComponent* MutableThis = const_cast<UGRBCharacterMovementComponent*>(this);

		MutableThis->ClientPredictionData = new FGRBNetworkPredictionData_Client(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}

void UGRBCharacterMovementComponent::StartSprinting()
{
	RequestToStartSprinting = true;
}

void UGRBCharacterMovementComponent::StopSprinting()
{
	RequestToStartSprinting = false;
}

void UGRBCharacterMovementComponent::StartAimDownSights()
{
	RequestToStartADS = true;
}

void UGRBCharacterMovementComponent::StopAimDownSights()
{
	RequestToStartADS = false;
}
