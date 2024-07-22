#include "Characters/GRBCharacterMovementComponent.h"
#include "Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Characters/GRBCharacterBase.h"
#include "GameplayTagContainer.h"


UGRBCharacterMovementComponent::UGRBCharacterMovementComponent()
{
	SprintSpeedMultiplier = 1.4f;
	ADSSpeedMultiplier = 0.8f;
	KnockedDownSpeedMultiplier = 0.4f;

	KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
	InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");
	InteractingRemovalTag = FGameplayTag::RequestGameplayTag("State.InteractingRemoval");
}

/** 方法的主要作用是提供角色在当前状态下的最大可能移动速度。这个方法可能会根据多种状态和条件返回不同的速度值*/
float UGRBCharacterMovementComponent::GetMaxSpeed() const
{
	AGRBCharacterBase* Owner = Cast<AGRBCharacterBase>(GetOwner());
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("%s() No Owner"), *FString(__FUNCTION__));
		return Super::GetMaxSpeed();
	}

	// 玩家死亡不允许移动
	if (!Owner->IsAlive())
	{
		return 0.0f;
	}

	// 被治疗或者在交互中，不允许移动
	if (Owner->GetAbilitySystemComponent() && Owner->GetAbilitySystemComponent()->GetTagCount(InteractingTag) > Owner->GetAbilitySystemComponent()->GetTagCount(InteractingRemovalTag))
	{
		return 0.0f;
	}
	// 玩家被击倒
	if (Owner->GetAbilitySystemComponent() && Owner->GetAbilitySystemComponent()->HasMatchingGameplayTag(KnockedDownTag))
	{
		return Owner->GetMoveSpeed() * KnockedDownSpeedMultiplier;
	}
	// 玩家收到冲刺输入, 有倍率
	if (RequestToStartSprinting)
	{
		return Owner->GetMoveSpeed() * SprintSpeedMultiplier;
	}
	// 玩家收到ADS输入, 有倍率
	if (RequestToStartADS)
	{
		return Owner->GetMoveSpeed() * ADSSpeedMultiplier;
	}

	return Owner->GetMoveSpeed();
}

/** 主要功能是将压缩标志位解压，更新角色的相关状态。例如，标志位可以表示角色是否正在冲刺、是否正在跳跃、是否在蹲伏等; 会与移动队列的虚方法GetCompressedFlags关联*/
void UGRBCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// 注意这里2个业务的输入变量 是在移动组件的移动队列Saved_Move的虚方法里注册的 (UGRBCharacterMovementComponent::FGRBSavedMove::GetCompressedFlags())
	
	//The Flags parameter contains the compressed input flags that are stored in the saved move.
	//UpdateFromCompressed flags simply copies the flags from the saved move into the movement component.
	//It basically just resets the movement component to the state when the move was made so it can simulate from there.
	RequestToStartSprinting = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	RequestToStartADS = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

/**　返回一个数据包实例，这个实例用于存储客户端预测数据。这些数据包括角色的历史移动状态、时间戳、输入状态等信息 */
FNetworkPredictionData_Client* UGRBCharacterMovementComponent::GetPredictionData_Client() const
{
	// 这里就是把this指针主动创建出GRB移动组件，然后配置完客户端预测数据
	
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

///@brief Sets up the move before sending it to the server.
///@brief 该方法允许你将当前角色移动状态保存到 FSavedMove_Character 实例中，以便后来进行网络回放和纠正。当客户端预测角色移动，并将移动数据发送到服务器时，服务器会使用这些数据来验证和修正角色的位置和状态;
///@brief 通过重写和使用 FSavedMove_Character::SetMoveFor，我们能够捕获客户端角色的实时移动状态，并通过网络将这些数据与服务器同步
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

///@brief Sets variables on character movement component before making a predictive correction.
///@brief 方法的主要功能是恢复并应用之前在 FSavedMove_Character 实例中保存的移动数据至角色。这允许角色在不同帧之间，或在网络同步和回放时保持一致的状态
///@brief 方法的核心作用就是恢复和应用之前保存的角色移动状态，以确保网络回放过程中角色状态的一致性
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

///@brief 核心功能是动态分配和初始化一个新的 FSavedMove_Character 实例。在客户端预测角色移动时，每一帧都会调用这个方法来创建一个新的移动实例，并将其保存到移动历史记录中。之后，这些移动实例会被发送到服务器，或用于在客户端进行回放和校正
FSavedMovePtr UGRBCharacterMovementComponent::FGRBNetworkPredictionData_Client::AllocateNewMove()
{
	return FSavedMovePtr(new FGRBSavedMove());
}
