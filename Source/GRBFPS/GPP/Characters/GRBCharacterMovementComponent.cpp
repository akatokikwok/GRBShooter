#include "GRBFPS/GPP/Characters/GRBCharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "GRBFPS/GPP/Characters/Abilities/GRBAbilitySystemGlobals.h"
#include "GRBFPS/GPP/Characters/GRBCharacterBase.h"
#include "GameplayTagContainer.h"

///@brief Resets all saved variables. 复位所有的移动队列内的标识符与属性
void UGRBCharacterMovementComponent::FGRBSavedMove::Clear()
{
	Super::Clear();
	SavedRequestToStartSprinting = false;
	SavedRequestToStartADS = false;
}

/** 作用是获取当前移动数据的压缩标志。这些压缩标志决定了在网络传输时,移动数据中的各个字段将采用何种压缩方式。合理的压缩策略可以显著降低网络带宽的占用*/
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

/**
 * 作用是判断当前的移动数据是否可以与另一个移动数据合并。该函数通常在客户端发送移动数据给服务器之前被调用,用于减少网络带宽的消耗
 */
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

/**
 * 作用是在客户端应用服务器发送回来的网络校正数据。具体来说,它会根据校正数据调整客户端角色的位置、旋转和速度,以使其与服务器端的模拟结果保持一致
 * 这个过程通常会配合平滑校正功能一起使用,以避免校正时出现突兀的位移或旋转
 * SetMoveFor 函数只是应用了网络校正数据,实际的模拟过程仍然需要由 UCharacterMovementComponent 中的其他函数来执行。不过,正确地应用校正数据是保证后续模拟结果与服务器保持一致的关键步骤
 */
void UGRBCharacterMovementComponent::FGRBSavedMove::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	UGRBCharacterMovementComponent* CharacterMovement = Cast<UGRBCharacterMovementComponent>(Character->GetCharacterMovement());
	if (CharacterMovement)
	{
		//在客户端应用服务器发送回来的网络校正数据 (在客户端应用服务器校验好的是否冲刺和开启ADS)
		SavedRequestToStartSprinting = CharacterMovement->RequestToStartSprinting;
		SavedRequestToStartADS = CharacterMovement->RequestToStartADS;
	}
}

/**
 * 作用是准备好这个保存的移动数据,使其可以在服务器端被重放和模拟。这个函数通常在服务器接收到客户端发送的移动数据后被调用
 * 通过 PrepMoveFor 函数的执行,服务器就可以基于从客户端接收到的移动数据,准确地重现客户端的运动状态和输入。服务器会利用这些准备好的数据进行模拟和重放,并与客户端的预测结果进行比较,从而确定是否需要发送校正数据给客户端
 * PrepMoveFor 函数只是完成了移动数据的准备工作,实际的模拟过程需要由 UCharacterMovementComponent 中的其他函数来执行
 * 正确地准备好移动数据是保证模拟过程正确性的前提条件
 * PrepMoveFor 函数是 UE4 客户端网络预测系统中一个非常关键的环节,它为服务器端提供了可靠的输入数据和初始状态,从而支持了整个重放和校正的过程
 */
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

/** 作用是为客户端角色分配一个新的移动数据结构 FSavedMove_Character。这个新的移动数据结构将被用于存储当前的输入和运动状态,并最终发送给服务器用于重放和校正 */
FSavedMovePtr UGRBCharacterMovementComponent::FGRBNetworkPredictionData_Client::AllocateNewMove()
{
	// 主动构建1个移动队列FGRBSavedMove
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

	// Flags入参携带 那些被压缩好的且储存在移动队列内的移动输入标识符
	// UpdateFromCompressed函数只是将标志从SavedMove(即移动队列)复制到移动组件中
	// 它基本上只是将移动组件重置为移动时的状态，以便它可以从那里进行模拟
	RequestToStartSprinting = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	RequestToStartADS = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

/*
 * UCharacterMovementComponent::GetPredictionData_Client 是 UE4 中 UCharacterMovementComponent 类的一个重要成员函数,它的作用是获取当前角色的网络预测数据,并将其填充到由 ACharacter::GetPredictionData_Client 函数返回的 FNetworkPredictionData_Client_Character 结构体中
 * 实现过程大致如下:
 * 1. 检查预测状态 函数首先会检查当前是否启用了网络预测功能,如果没有启用,则直接返回。
 * 2. 获取根组件的位置和旋转 函数会获取根组件的位置和旋转,并将其存储到预测数据结构体中。
 * 3. 计算角色速度 通过角色的位置变化和时间间隔,函数会计算出角色的当前速度,并存储到预测数据结构体中。
 * 4. 填充输入状态 函数会获取当前角色的输入状态,包括移动输入向量、视角输入等,并将其存储到预测数据结构体的 CurrentClientInput 字段中。
 * 5. 处理未确认的移动数据 如果存在未被服务器确认的移动数据,函数会将其复制到预测数据结构体的 PendingMove 字段中。
 * 6. 处理历史移动数据 函数会遍历已保存的移动数据列表,将其复制到预测数据结构体的 SavedMoves 字段中。
 * 7. 更新校正数据 如果服务器发送了校正数据,函数会更新预测数据结构体中的 SmoothCorrections 字段,用于平滑处理校正后的位置和旋转。
 */
FNetworkPredictionData_Client* UGRBCharacterMovementComponent::GetPredictionData_Client() const
{
	check(UPawnMovementComponent::PawnOwner != NULL);

	if (!UCharacterMovementComponent::ClientPredictionData)
	{
		// 将this指针设为可编辑的指针, 然后设置自身移动组件的客户端预测的部分字段以处理平滑效果
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
