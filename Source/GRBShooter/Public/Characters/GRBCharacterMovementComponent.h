#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "GRBCharacterMovementComponent.generated.h"

/**
 * 定制的GRB人物移动组件
 */
UCLASS()
class GRBSHOOTER_API UGRBCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** FSavedMove_Character
	 * UE4中玩家的所有移动操作需要被保存在一个队列queue中（FSavedMove_Character），因此我们需要实现自定义的移动队列;
	 * 它是 FNetworkPredictionData_Client_Character 结构体的成员
	 * 1. Clear 用于清除move队列
	 * 2. GetCompressedFlags 用于获取跳跃、蹲以及自定义的移动状态信息
	 * 3. CanCombineWith 用于判断两个move是否能够被合并，用于减轻带宽使用（比如玩家很长时间内没有移动，这样的move可以合并）
	 * 4. SetMoveFor 用于获取玩家输入并且将move放入队列，修改相关移动状态标记
	 * 5. PrepMoveFor 用于玩家人物真实开始移动前的准备工作，比如设置移动方向等
	 */
	class FGRBSavedMove : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		///@brief Resets all saved variables.
		///@brief 清除或者复位运动队列内的业务变量
		virtual void Clear() override;

		///@brief Store input commands in the compressed flags.
		///@brief 获取移动输入业务变量的 状态标志位
		virtual uint8 GetCompressedFlags() const override;

		///@brief This is used to check whether or not two moves can be combined into one.	///Basically you just check to make sure that the saved variables are the same.
		///@brief 用于判断两个move是否能够被合并，用于减轻带宽使用（比如玩家很长时间内没有移动，这样的move可以合并）
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

		///@brief Sets up the move before sending it to the server.
		///@brief 该方法允许你将当前角色移动状态保存到 FSavedMove_Character 实例中，以便后来进行网络回放和纠正。当客户端预测角色移动，并将移动数据发送到服务器时，服务器会使用这些数据来验证和修正角色的位置和状态;
		///@brief 通过重写和使用 FSavedMove_Character::SetMoveFor，我们能够捕获客户端角色的实时移动状态，并通过网络将这些数据与服务器同步
		virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;

		///@brief Sets variables on character movement component before making a predictive correction.
		///@brief 方法的主要功能是恢复并应用之前在 FSavedMove_Character 实例中保存的移动数据至角色。这允许角色在不同帧之间，或在网络同步和回放时保持一致的状态
		///@brief 方法的核心作用就是恢复和应用之前保存的角色移动状态，以确保网络回放过程中角色状态的一致性
		virtual void PrepMoveFor(class ACharacter* Character) override;

	protected:
		// Sprint; 业务：是否启用冲刺
		uint8 SavedRequestToStartSprinting : 1;

		// Aim Down Sights；业务；是否开启ADS瞄准
		uint8 SavedRequestToStartADS : 1;
	};

	
	/** FNetworkPredictionData_Client_Character
	 * 用于网络运动预测的一个重要结构体。它主要用于在客户端保存和处理玩家角色的移动数据,以便与服务器进行同步,实现平滑的网络游戏体验
	 * 1. 客户端会实时记录玩家的移动输入并生成相应的 FSavedMove_Character 实例,存储在 SavedMoves 数组中。同时,客户端会将这些移动数据发送到服务器进行验证和调整
	 * 2. 服务器在接收到客户端的移动数据后,会根据自身的权威状态对移动数据进行校正,生成新的位置和旋转数据,并将这些数据回传给客户端。客户端收到服务器的回复后,会将校正后的位置和旋转数据应用到玩家角色上,从而实现平滑的移动同步
	 * 3. FNetworkPredictionData_Client_Character 结构体通过存储和管理客户端的移动数据,使得客户端能够在接收到服务器更新之前预测并渲染玩家角色的移动,从而减少网络延迟
	 */
	class FGRBNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
	{
	public:
		typedef FNetworkPredictionData_Client_Character Super;
		
		FGRBNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement);

		///@brief Allocates a new copy of our custom saved move
		///@brief 核心功能是动态分配和初始化一个新的 FSavedMove_Character 实例。在客户端预测角色移动时，每一帧都会调用这个方法来创建一个新的移动实例，并将其保存到移动历史记录中。之后，这些移动实例会被发送到服务器，或用于在客户端进行回放和校正
		virtual FSavedMovePtr AllocateNewMove() override;
	};

public:
	UGRBCharacterMovementComponent();
	// ~Start Implements UCharacterMovementComponent
	/** 方法的主要作用是提供角色在当前状态下的最大可能移动速度。这个方法可能会根据多种状态和条件返回不同的速度值*/
	virtual float GetMaxSpeed() const override;
	/** 主要功能是将压缩标志位解压，更新角色的相关状态。例如，标志位可以表示角色是否正在冲刺、是否正在跳跃、是否在蹲伏等; 会与移动队列的虚方法GetCompressedFlags关联*/
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	/**　返回一个数据包实例，这个实例用于存储客户端预测数据。这些数据包括角色的历史移动状态、时间戳、输入状态等信息 */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	// ~End Implements

public:
	// Sprint
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StartSprinting();
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StopSprinting();

	// Aim Down Sights
	UFUNCTION(BlueprintCallable, Category = "Aim Down Sights")
	void StartAimDownSights();
	UFUNCTION(BlueprintCallable, Category = "Aim Down Sights")
	void StopAimDownSights();
	
protected:
	// 各种输入操作的倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float SprintSpeedMultiplier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float ADSSpeedMultiplier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float KnockedDownSpeedMultiplier;

	// 执行某种业务输入(比如ADS/冲刺)的请求
	uint8 RequestToStartSprinting : 1;
	uint8 RequestToStartADS : 1;

	// 一些伴随移动组件会使用到的状态Tag
	FGameplayTag KnockedDownTag;
	FGameplayTag InteractingTag;
	FGameplayTag InteractingRemovalTag;
	
};
