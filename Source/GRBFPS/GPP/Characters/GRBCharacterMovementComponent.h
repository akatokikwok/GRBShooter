#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "GRBCharacterMovementComponent.generated.h"

/**
 * GRB项目 人物移动组件
 */
UCLASS()
class GRBFPS_API UGRBCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

private:
	/**
	 * UE4中玩家的所有移动操作需要被保存在一个队列queue中（FSavedMove_Character），因此我们需要实现自定义的移动队列
	 */
	class FGRBSavedMove : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		///@brief Resets all saved variables. 复位所有的移动队列内的标识符与属性
		virtual void Clear() override;

		///@brief Store input commands in the compressed flags.
		/** 作用是获取当前移动数据的压缩标志。这些压缩标志决定了在网络传输时,移动数据中的各个字段将采用何种压缩方式。合理的压缩策略可以显著降低网络带宽的占用*/
		virtual uint8 GetCompressedFlags() const override;

		///@brief This is used to check whether or not two moves can be combined into one.
		///Basically you just check to make sure that the saved variables are the same.
		/**
		 * 作用是判断当前的移动数据是否可以与另一个移动数据合并。该函数通常在客户端发送移动数据给服务器之前被调用,用于减少网络带宽的消耗
		 */
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

		///@brief Sets up the move before sending it to the server.
		/**
		 * 作用是在客户端应用服务器发送回来的网络校正数据。具体来说,它会根据校正数据调整客户端角色的位置、旋转和速度,以使其与服务器端的模拟结果保持一致
		 * 这个过程通常会配合平滑校正功能一起使用,以避免校正时出现突兀的位移或旋转
		 * SetMoveFor 函数只是应用了网络校正数据,实际的模拟过程仍然需要由 UCharacterMovementComponent 中的其他函数来执行。不过,正确地应用校正数据是保证后续模拟结果与服务器保持一致的关键步骤
		 */
		virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
		
		///@brief Sets variables on character movement component before making a predictive correction.
		/**
		 * 作用是准备好这个保存的移动数据,使其可以在服务器端被重放和模拟。这个函数通常在服务器接收到客户端发送的移动数据后被调用
		 * 通过 PrepMoveFor 函数的执行,服务器就可以基于从客户端接收到的移动数据,准确地重现客户端的运动状态和输入。服务器会利用这些准备好的数据进行模拟和重放,并与客户端的预测结果进行比较,从而确定是否需要发送校正数据给客户端
		 * PrepMoveFor 函数只是完成了移动数据的准备工作,实际的模拟过程需要由 UCharacterMovementComponent 中的其他函数来执行
		 * 正确地准备好移动数据是保证模拟过程正确性的前提条件
		 * PrepMoveFor 函数是 UE4 客户端网络预测系统中一个非常关键的环节,它为服务器端提供了可靠的输入数据和初始状态,从而支持了整个重放和校正的过程
		 */
		virtual void PrepMoveFor(class ACharacter* Character) override;

		// 移动队列里会使用到的 标识符 Sprint
		uint8 SavedRequestToStartSprinting : 1;

		// 移动队列里会使用到的 标识符 Aim Down Sights
		uint8 SavedRequestToStartADS : 1;
	};


	/**
	 * 实现自定义的NetworkPredictionData类，用于客户端预测移动，可以重写GetPredictionData_Client函数进行自定义预测相关参数的设置
	 * FNetworkPredictionData_Client_Character 是 UE4 中用于实现客户端角色网络预测的一个结构体。它是 ACharacter 类的一个成员变量,主要用于存储和传输客户端角色的运动状态和预测数据。
     * 在网络游戏中,由于网络延迟的存在,客户端必须先于服务器预测角色的运动状态,以获得平滑的移动效果。FNetworkPredictionData_Client_Character 就是用于存储和传输这些预测数据的载体。它包含以下主要成员
	 * CurrentClientPosition - 当前客户端角色预测的位置。
	 * CurrentClientRotation - 当前客户端角色预测的旋转。
	 * CurrentClientVelocity - 当前客户端角色预测的速度。
	 * CurrentClientInput - 客户端输入的当前状态。
	 * SavedMoves - 客户端输入的历史记录,用于服务器端回滚和重放。
	 * PendingMove - 当前正在等待服务器确认的输入。
	 * SmoothCorrections - 用于平滑处理服务器校正的位移和旋转。
	 * 当客户端接收到新的用户输入时,它会将输入存储在 PendingMove 中,并根据输入预测出新的 CurrentClientPosition、CurrentClientRotation 和 CurrentClientVelocity。同时会将旧的 PendingMove 添加到 SavedMoves 中,以备服务器回滚使用。
	 * 客户端会定期将 FNetworkPredictionData_Client_Character 结构体的内容复制到一个 FSavedMove_Character 实例中并发送给服务器。服务器会根据客户端发送的 SavedMoves 重演客户端的输入,计算出"正确"的运动状态。如果发现客户端预测与服务器重演结果不同,服务器就会发送校正数据给客户端,客户端接收到后会使用 SmoothCorrections 平滑角色的位置和旋转。
	 * FNetworkPredictionData_Client_Character 结构体的设计使得客户端可以独立地预测角色运动,同时又能与服务器保持一致,从而在网络延迟的情况下提供流畅的体验。它是 UE4 实现客户端网络预测的核心数据结构。
	 */
	class FGRBNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
	{
	public:
		FGRBNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement);

		typedef FNetworkPredictionData_Client_Character Super;

		///@brief Allocates a new copy of our custom saved move
		/**
		 * 作用是为客户端角色分配一个新的移动数据结构 FSavedMove_Character。这个新的移动数据结构将被用于存储当前的输入和运动状态,并最终发送给服务器用于重放和校正
		 */
		virtual FSavedMovePtr AllocateNewMove() override;
	};
	
public:
	//
	UGRBCharacterMovementComponent();

	/** 根据不同业务情形获取玩家移速, Override UCharacterMovementComponent::GetMaxSpeed() */
	virtual float GetMaxSpeed() const override;
	
	/** Override UCharacterMovementComponent::UpdateFromCompressedFlags; 在服务端解包压缩的标识符(关乎到移动操作的移动队列FSavedMove_Character) */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

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
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	
	// 发送启用冲刺标识符 请求
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StartSprinting();

	// 终止发送启用冲刺标识符 请求
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StopSprinting();

	// 发送ADS瞄准标识符 请求
	UFUNCTION(BlueprintCallable, Category = "Aim Down Sights")
	void StartAimDownSights();

	// 终止发送ADS瞄准 标识符请求
	UFUNCTION(BlueprintCallable, Category = "Aim Down Sights")
	void StopAimDownSights();
	
public:
	// 冲刺情形下会影响移速的倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float SprintSpeedMultiplier;

	// ADS瞄准下情形下会影响移速的倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float ADSSpeedMultiplier;

	// 被击倒情形下会影响移速的倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float KnockedDownSpeedMultiplier;

	// 移动组件标识符: 是否启用冲刺
	uint8 RequestToStartSprinting : 1;

	// 移动组件标识符: 是否开启ADS瞄准
	uint8 RequestToStartADS : 1;

	// 玩家身上会携带的被击倒Tag
	FGameplayTag KnockedDownTag;

	// 玩家身上会携带的 交互中Tag
	FGameplayTag InteractingTag;

	// 玩家身上会携带的 交互完成Tag
	FGameplayTag InteractingRemovalTag;
};
