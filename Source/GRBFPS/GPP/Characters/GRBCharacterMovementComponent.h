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

		///@brief Resets all saved variables.
		virtual void Clear() override;

		///@brief Store input commands in the compressed flaGRB.
		virtual uint8 GetCompressedFlags() const override;

		///@brief This is used to check whether or not two moves can be combined into one.
		///Basically you just check to make sure that the saved variables are the same.
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

		///@brief Sets up the move before sending it to the server. 
		virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
		
		///@brief Sets variables on character movement component before making a predictive correction.
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
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float SprintSpeedMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float ADSSpeedMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
	float KnockedDownSpeedMultiplier;

	uint8 RequestToStartSprinting : 1;
	
	uint8 RequestToStartADS : 1;

	FGameplayTag KnockedDownTag;
	
	FGameplayTag InteractingTag;
	
	FGameplayTag InteractingRemovalTag;
};
