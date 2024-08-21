// Copyright 2024 GRB.


#include "Characters/Abilities/GRBGATA_Trace.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"

AGRBGATA_Trace::AGRBGATA_Trace()
{
	bDestroyOnConfirmation = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	m_MaxAcknowledgeHitNums = 1;
	NumberOfTraces = 1;
	bIgnoreBlockingHits = false;
	bTraceAffectsAimPitch = true;
	bTraceFromPlayerViewPoint = false;
	MaxRange = 999999.0f;
	bUseAimingSpreadMod = false;
	BaseSpread = 0.0f;
	AimingSpreadMod = 0.0f;
	TargetingSpreadIncrement = 0.0f;
	TargetingSpreadMax = 0.0f;
	CurrentTargetingSpread = 0.0f;
	bUseMechanism_PersistHits = false;
}

#pragma region ~ Override ~
void AGRBGATA_Trace::BeginPlay()
{
	Super::BeginPlay();

	// 仅在StartTargeting时候开启tick
	// Start with Tick disabled. We'll enable it in StartTargeting() and disable it again in StopTargeting().
	// For instant confirmations, tick will never happen because we StartTargeting(), ConfirmTargeting(), and immediately StopTargeting().
	SetActorTickEnabled(false);
}

void AGRBGATA_Trace::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 把一组可视化3D准星从后到前挨个销毁
	DestroyReticleActors();
	
	Super::EndPlay(EndPlayReason);
}

void AGRBGATA_Trace::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TArray<FHitResult> HitResults;
	if (bDebug || bUseMechanism_PersistHits)
	{
		// Only need to trace on Tick if we're showing debug or if we use persistent hit results, otherwise we just use the confirmation trace
		HitResults = PerformTrace(SourceActor);
	}

#if ENABLE_DRAW_DEBUG
	if (SourceActor && bDebug)
	{
		ShowDebugTrace(HitResults, EDrawDebugTrace::Type::ForOneFrame);
	}
#endif
}

///--@brief 覆写; 开始探查器的探查准备工作并开启tick.--/
void AGRBGATA_Trace::StartTargeting(UGameplayAbility* Ability)
{
	// Don't call to Super because we can have more than one Reticle

	SetActorTickEnabled(true);
	OwningAbility = Ability;
	SourceActor = Ability->GetCurrentActorInfo()->AvatarActor.Get();

	// This is a lazy way of emptying and repopulating the ReticleActors.
	// We could come up with a solution that reuses them.
	DestroyReticleActors();

	// 为累计 单回合频次 * 单次命中数 总个数生成匹配的 3D准星物体
	if (AGameplayAbilityTargetActor::ReticleClass)
	{
		for (int32 i = 0; i < m_MaxAcknowledgeHitNums * NumberOfTraces; i++)
		{
			SpawnReticleActor(GetActorLocation(), GetActorRotation());
		}
	}

	if (bUseMechanism_PersistHits)
	{
		m_QueuePersistHits.Empty();
	}
}

///--@brief 覆写入口; 在玩家确认目标之后调用，以继续能力的执行逻辑--/
void AGRBGATA_Trace::ConfirmTargetingAndContinue()
{
	check(AGameplayAbilityTargetActor::ShouldProduceTargetData());
	if (SourceActor)
	{
		// 执行探查器trace
		TArray<FHitResult> HitResults = PerformTrace(SourceActor);
		// 为一组命中hit制作 目标数据句柄 并存储它们
		FGameplayAbilityTargetDataHandle Handle = MakeTargetData(HitResults);
		// 为探查器的 "已确认选择射击目标"事件广播; 并传入组好的payload 目标数据句柄
		AGameplayAbilityTargetActor::TargetDataReadyDelegate.Broadcast(Handle);

#if ENABLE_DRAW_DEBUG
		if (bDebug)
		{
			ShowDebugTrace(HitResults, EDrawDebugTrace::Type::ForDuration, 2.0f);
		}
#endif
	}

	if (bUseMechanism_PersistHits)
	{
		m_QueuePersistHits.Empty();
	}
}

///--@brief 覆写入口; 当区域探查器取消选中目标的时候会进入.--/
void AGRBGATA_Trace::CancelTargeting()
{
	const FGameplayAbilityActorInfo* ActorInfo = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr);
	UAbilitySystemComponent* ASC = (ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
	if (ASC)
	{
		// 设置通用的网络复制事件为 cancel状态并清除数据
		ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericCancel, OwningAbility->GetCurrentAbilitySpecHandle(), OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey()).Remove(GenericCancelHandle);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("AGameplayAbilityTargetActor::CancelTargeting called with null ASC! Actor %s"), *GetName());
	}

	// 广播target actor的 “取消事件”.
	AGameplayAbilityTargetActor::CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());

	SetActorTickEnabled(false);
	// 清空持续队列
	if (bUseMechanism_PersistHits)
	{
		m_QueuePersistHits.Empty();
	}
}
#pragma endregion


///--@brief 复位弹道扩散参数状态 --/
void AGRBGATA_Trace::ResetSpread()
{
	bUseAimingSpreadMod = false;
	BaseSpread = 0.0f;
	AimingSpreadMod = 0.0f;
	TargetingSpreadIncrement = 0.0f;
	TargetingSpreadMax = 0.0f;
	CurrentTargetingSpread = 0.0f;
}

///--@brief 解算出当下的扩散系数 --/
float AGRBGATA_Trace::GetCurrentSpread() const
{
	float FinalSpread = BaseSpread + CurrentTargetingSpread;

	if (bUseAimingSpreadMod && AimingTag.IsValid() && AimingRemovalTag.IsValid())
	{
		UAbilitySystemComponent* ASC = OwningAbility->GetCurrentActorInfo()->AbilitySystemComponent.Get();
		if (ASC && (ASC->GetTagCount(AimingTag) > ASC->GetTagCount(AimingRemovalTag)))
		{
			FinalSpread *= AimingSpreadMod;
		}
	}

	return FinalSpread;
}

///--@brief 设置 瞄准开始位置参数包 Expose to Blueprint --/
void AGRBGATA_Trace::SetStartLocation(const FGameplayAbilityTargetingLocationInfo& InStartLocation)
{
	AGameplayAbilityTargetActor::StartLocation = InStartLocation;
}

///--@brief 设置是否在服务端生成目标数据--/
void AGRBGATA_Trace::SetShouldProduceTargetDataOnServer(bool bInShouldProduceTargetDataOnServer)
{
	AGameplayAbilityTargetActor::ShouldProduceTargetDataOnServer = bInShouldProduceTargetDataOnServer;
}

///--@brief 用于控制目标选择 Actor 的生命周期管理。这一标志决定了当目标选择得到确认后，是否自动销毁目标选择器--/
void AGRBGATA_Trace::SetDestroyOnConfirmation(bool bInDestroyOnConfirmation)
{
	AGameplayAbilityTargetActor::bDestroyOnConfirmation = bInDestroyOnConfirmation;
}

///--@brief 会先做复合射线检测并同时筛选出命中了actor的那些Hits--/
void AGRBGATA_Trace::LineTraceWithFilter(TArray<FHitResult>& OutHitResults, const UWorld* World, const FGameplayTargetDataFilterHandle FilterHandle, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams Params)
{
	check(World);

	TArray<FHitResult> HitResults;
	World->LineTraceMultiByProfile(HitResults, Start, End, ProfileName, Params);

	TArray<FHitResult> FilteredHitResults;

	// Start param could be player ViewPoint. We want HitResult to always display the StartLocation.
	FVector TraceStart = AGameplayAbilityTargetActor::StartLocation.GetTargetingTransform().GetLocation();

	for (int32 HitIdx = 0; HitIdx < HitResults.Num(); ++HitIdx)
	{
		FHitResult& Hit = HitResults[HitIdx];

		if (!Hit.GetActor() || FilterHandle.FilterPassesForActor(Hit.GetActor()))
		{
			Hit.TraceStart = TraceStart;
			Hit.TraceEnd = End;

			FilteredHitResults.Add(Hit);
		}
	}

	OutHitResults = FilteredHitResults;

	return;
}

///--@brief 工具方法: 裁剪相机Ray在射距内--/
bool AGRBGATA_Trace::ClipCameraRayToAbilityRange(FVector CameraLocation, FVector CameraDirection, FVector CustomPos, float AbilityRange, FVector& ClippedPosition)
{
	// 相机到自定义点的 "朝向Custom"
	FVector CamToCusPosVec = CustomPos - CameraLocation;
	// "朝向Custom" 落在	相机朝向 的 阶段投影长度(标量); 会制造出一个垂足
	float ProjectionOnCamDis = FVector::DotProduct(CamToCusPosVec, CameraDirection);
	// 必须是同方向
	if (ProjectionOnCamDis >= 0) //If this fails, we're pointed away from the center, but we might be inside the sphere and able to find a good exit point.
	{
		// 原理: 点乘结果表示向量在另一个向量上投影的标量长度，而平方通常用于计算这种投影的平方长度

		// 最大射距 平方
		float MaxShootRadiusSquared = (AbilityRange * AbilityRange);
		// 自定义点 到 垂足 的距离平方
		float DistanceSquared = CamToCusPosVec.SizeSquared() - (ProjectionOnCamDis * ProjectionOnCamDis);

		if (DistanceSquared <= MaxShootRadiusSquared)
		{
			// 现在有 最大射距长 垂足长; 利用前2个因子组三角形再组出来一个长度C
			float CorrectTriangleThirdLineSize = FMath::Sqrt(MaxShootRadiusSquared - DistanceSquared);
			// 能得到相机的预设朝向射线长度; 最终算出裁剪位置
			float DistanceAlongRay = ProjectionOnCamDis + CorrectTriangleThirdLineSize; //Subtracting instead of adding will get the other intersection point
			ClippedPosition = CameraLocation + (DistanceAlongRay * CameraDirection); //Cam aim point clipped to range sphere
			return true;
		}
	}
	return false;
}

///--@brief 经过一系列对于瞄朝向的射击调调校; 输出最终的TraceEnd位置--/
void AGRBGATA_Trace::AimWithPlayerController(const AActor* InSourceActor, FCollisionQueryParams Params, const FVector& TraceStart, FVector& OutTraceEnd, bool bIgnorePitch)
{
	if (!AGameplayAbilityTargetActor::OwningAbility) // Server and launching client only
	{
		return;
	}

	/**--@brief 0.组建trace的起始位置和最大射距终止位置.*/
	FVector ViewStart = TraceStart;
	// AGameplayAbilityTargetActor::StartLocation解释: 用于定义目标选择过程的起始位置。这通常对于技能或能力的目标选择非常关键，例如从角色位置开始的射线或投掷
	FRotator ViewRot = AGameplayAbilityTargetActor::StartLocation.GetTargetingTransform().GetRotation().Rotator();
	if (PrimaryPC)
	{
		PrimaryPC->GetPlayerViewPoint(ViewStart, ViewRot);
	}
	const FVector ViewDir = ViewRot.Vector();
	FVector ViewEnd = ViewStart + (ViewDir * MaxRange);

	// 裁剪一下相机ray在射距内; 注意! 这里的viewstart可能是自定义位置(如枪口位置) 亦或者是 真正的相机坐标
	ClipCameraRayToAbilityRange(ViewStart, ViewDir, TraceStart, MaxRange, ViewEnd);

	/**--@brief 1.做综合筛选性质的复合射线检测 */
	TArray<FHitResult> HitResults;
	// @Filter 滤器机制通常通过 Filter 成员变量实现，允许开发者定义一些过滤规则来限制目标选择的结果。这样可以确保能力只能作用于特定类型的对象，增加了系统的灵活性和精确度。
	LineTraceWithFilter(HitResults, InSourceActor->GetWorld(), AGameplayAbilityTargetActor::Filter, ViewStart, ViewEnd, TraceProfile.Name, Params);

	// 按照自定义规则,扩散因子取 Min(目标最大扩散, 增量扩散)
	CurrentTargetingSpread = FMath::Min(TargetingSpreadMax, CurrentTargetingSpread + TargetingSpreadIncrement);

	/**--@brief 2.按命中的第一个结果来校准瞄准朝向 */
	const bool bUseTraceResult = HitResults.Num() > 0 && (FVector::DistSquared(TraceStart, HitResults[0].Location) <= (MaxRange * MaxRange)); // trace出的第一目标在射距内
	const FVector AdjustedEnd = (bUseTraceResult) ? HitResults[0].Location : ViewEnd;
	FVector AdjustedAimDir = (AdjustedEnd - TraceStart).GetSafeNormal(); // 调校后的瞄准朝向
	if (AdjustedAimDir.IsZero())
	{
		AdjustedAimDir = ViewDir;
	}

	/**--@brief 3.在常见情形下 输出以原生pitch为准的调准版本瞄准朝向 */
	if (!bTraceAffectsAimPitch && bUseTraceResult)
	{
		FVector OriginalAimDir = (ViewEnd - TraceStart).GetSafeNormal();

		if (!OriginalAimDir.IsZero())
		{
			// Convert to angles and use original pitch
			const FRotator OriginalAimRot = OriginalAimDir.Rotation();

			FRotator AdjustedAimRot = AdjustedAimDir.Rotation();
			AdjustedAimRot.Pitch = OriginalAimRot.Pitch;

			AdjustedAimDir = AdjustedAimRot.Vector();
		}
	}

	/**--@brief 4. 构建射击圆锥. */
	const float CurrentSpread = GetCurrentSpread();
	const float ConeHalfAngle = FMath::DegreesToRadians(CurrentSpread * 0.5f);
	const int32 RandomSeed = FMath::Rand();
	FRandomStream WeaponRandomStream(RandomSeed);
	const FVector ShootDir = WeaponRandomStream.VRandCone(AdjustedAimDir, ConeHalfAngle, ConeHalfAngle);

	// 最终输出制作出本次射击的射击位置
	OutTraceEnd = TraceStart + (ShootDir * MaxRange);
}

///--@brief 主动停止目标选择并进行一系列数据/委托清理--/
void AGRBGATA_Trace::StopTargeting()
{
	SetActorTickEnabled(false);
	DestroyReticleActors();
	// 清除target actor上的目标选中/选中取消委托
	AGameplayAbilityTargetActor::TargetDataReadyDelegate.Clear();
	AGameplayAbilityTargetActor::CanceledDelegate.Clear();
	// 清除ASC上绑定/取消输入 的绑定回调
	if (GenericDelegateBoundASC)
	{
		GenericDelegateBoundASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &AGameplayAbilityTargetActor::ConfirmTargeting);
		GenericDelegateBoundASC->GenericLocalCancelCallbacks.RemoveDynamic(this, &AGameplayAbilityTargetActor::CancelTargeting);
		GenericDelegateBoundASC = nullptr;
	}
}


#pragma region ~ 内部方法 ~
///--@brief 为一组命中hit制作 目标数据句柄 并存储它们.--/
FGameplayAbilityTargetDataHandle AGRBGATA_Trace::MakeTargetData(const TArray<FHitResult>& HitResults) const
{
	// 技能可能需要一个或多个目标对象，而这些目标对象的数据通常由 FGameplayAbilityTargetDataHandle 来存储和管理。它允许技能逻辑在客户端和服务器之间高效而灵活地传递目标信息
	// FGameplayAbilityTargetDataHandle 包含多个 FGameplayAbilityTargetData 的实例，每个实例代表一个目标或一组目标的数据。

	FGameplayAbilityTargetDataHandle ReturnDataHandle;

	for (int32 i = 0; i < HitResults.Num(); i++)
	{
		/** Note: These are cleaned up by the FGameplayAbilityTargetDataHandle (via an internal TSharedPtr) */
		FGameplayAbilityTargetData_SingleTargetHit* SingleTargetHitDataPtr = new FGameplayAbilityTargetData_SingleTargetHit();
		SingleTargetHitDataPtr->HitResult = HitResults[i];
		ReturnDataHandle.Add(SingleTargetHitDataPtr);
	}

	return ReturnDataHandle;
}

///--@brief 重要函数; 场景探查器每帧都会执行的trace最终入口. --/
TArray<FHitResult> AGRBGATA_Trace::PerformTrace(AActor* InSourceActor)
{
	// 准备参数
	bool bTraceComplex = false;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(InSourceActor);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AGRBGATA_LineTrace), bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	Params.bIgnoreBlocks = bIgnoreBlockingHits;

	// trace的起点和终点
	FVector TraceStart = AGameplayAbilityTargetActor::StartLocation.GetTargetingTransform().GetLocation(); // AGameplayAbilityTargetActor::StartLocation解释: 用于定义目标选择过程的起始位置。这通常对于技能或能力的目标选择非常关键，例如从角色位置开始的射线或投掷
	FVector TraceEnd;

	// 组织一下trace起点
	if (PrimaryPC)
	{
		FVector ViewStart;
		FRotator ViewRot;
		PrimaryPC->GetPlayerViewPoint(ViewStart, ViewRot);
		TraceStart = bTraceFromPlayerViewPoint ? ViewStart : TraceStart;
	}

	// 在目标选中/Cancel前是否采用持续trace; 从队尾清空持续命中的hit
	if (bUseMechanism_PersistHits)
	{
		// Clear any blocking hit results, invalid Actors, or actors out of range
		//TODO Check for visibility if we add AIPerceptionComponent in the future
		for (int32 i = m_QueuePersistHits.Num() - 1; i >= 0; i--)
		{
			FHitResult& PerHit = m_QueuePersistHits[i];
			if (PerHit.bBlockingHit || !PerHit.GetActor() || FVector::DistSquared(TraceStart, PerHit.GetActor()->GetActorLocation()) > (MaxRange * MaxRange))
			{
				m_QueuePersistHits.RemoveAt(i);
			}
		}
	}

	// 轮询可配置的每回合射线次数; 比如步枪单回合就是Trace1次, 霰弹枪则是单回合Trace多次
	TArray<FHitResult> ReturnHitResults;
	for (int32 PerRoundIndex = 0; PerRoundIndex < NumberOfTraces; PerRoundIndex++)
	{
		// 经过一系列对于瞄朝向的射击调调校; 输出最终的TraceEnd位置
		// Effective on server and launching client only
		AimWithPlayerController(InSourceActor, Params, TraceStart, TraceEnd);

		// ------------------------------------------------------


		// 把本目标选择器重新定位至trace终点
		SetActorLocationAndRotation(TraceEnd, AGameplayAbilityTargetActor::SourceActor->GetActorRotation());
		// 缓存一下trace终点
		CurrentTraceEnd = TraceEnd;

		// 派生类目标探查器的trace结果
		TArray<FHitResult> DoTraceHits;
		DoTrace(DoTraceHits, InSourceActor->GetWorld(), Filter, TraceStart, TraceEnd, TraceProfile.Name, Params);

		// 轮询 (从最近一次) --而非++ 派生类目标探查器的过滤trace结果
		for (int32 BackwardsIndex = DoTraceHits.Num() - 1; BackwardsIndex >= 0; BackwardsIndex--)
		{
			// 本次的命中结果
			const FHitResult& BackwardsHit = DoTraceHits[BackwardsIndex];
			const AActor* BackwardsHitActor = BackwardsHit.GetActor();

			// 修剪命中个数; 剪除靠后的,保留旧的(比如先后命中了士兵和宝箱,移除出宝箱,保留那个旧的结果,命中的士兵);限制在最大命中个数范围内
			if (m_MaxAcknowledgeHitNums >= 0 && (BackwardsIndex + 1 > m_MaxAcknowledgeHitNums))
			{
				DoTraceHits.RemoveAt(BackwardsIndex);
				continue; // 结束本次不需要的情形, 继续下一次for迭代
			}

			// Reminder: if bUsePersistentHitResults, Number of Traces = 1
			if (bUseMechanism_PersistHits) // 启用持续trace
			{
				/**--@brief 让持续命中队列 优先缓存最近的一次命中结果  */
				// This is looping backwards so that further objects from player are added first to the queue.
				// This results in closer actors taking precedence as the further actors will get bumped out of the TArray.
				if (BackwardsHit.GetActor() && (!BackwardsHit.bBlockingHit || m_QueuePersistHits.Num() < 1))
				{
					// 某个actor是否已在持续搜索队列
					bool bActorAlreadyInPersistentHits = false;
					// 希望 持续队列里不应该有已经命中的actor
					for (int32 k = 0; k < m_QueuePersistHits.Num(); k++)
					{
						FHitResult& PersistentHitResult = m_QueuePersistHits[k];
						if (PersistentHitResult.GetActor() == BackwardsHit.GetActor())
						{
							bActorAlreadyInPersistentHits = true;
							break;
						}
					}
					if (bActorAlreadyInPersistentHits)
					{
						continue;
					}

					if (m_QueuePersistHits.Num() >= m_MaxAcknowledgeHitNums)
					{
						// Treat PersistentHitResults like a queue, remove first element
						m_QueuePersistHits.RemoveAt(0);
					}
					m_QueuePersistHits.Add(BackwardsHit);
				}
			}
			else if (bUseMechanism_PersistHits == false) // 不启用持续trace机制
			{
				// 注意这里是 有后向前的单词命中序数+(步枪/霰弹枪)的单回合序数 * max上限
				// 处理非持续机制下的命中准星
				// ReticleActors for PersistentHitResults are handled later
				int32 ReticleIndex = PerRoundIndex * m_MaxAcknowledgeHitNums + BackwardsIndex;
				if (ReticleIndex < ReticleActors.Num())
				{
					if (AGameplayAbilityWorldReticle* LocalReticleActor = ReticleActors[ReticleIndex].Get())
					{
						const bool bHitActor = BackwardsHit.GetActor() != nullptr;

						if (bHitActor && !BackwardsHit.bBlockingHit)
						{
							LocalReticleActor->SetActorHiddenInGame(false);

							const FVector ReticleLocation = (bHitActor && LocalReticleActor->bSnapToTargetedActor) ? BackwardsHit.GetActor()->GetActorLocation() : BackwardsHit.Location;
							LocalReticleActor->SetActorLocation(ReticleLocation);
							LocalReticleActor->SetIsTargetAnActor(bHitActor);
						}
						else
						{
							LocalReticleActor->SetActorHiddenInGame(true);
						}
					}
				}
			}
		} // for TraceHitResults


		// 在不适用持续trace机制的情况下: 仅显示超出DoTracehits数量的 那部分额外的准星
		if (!bUseMechanism_PersistHits)
		{
			if (DoTraceHits.Num() < ReticleActors.Num())
			{
				// We have less hit results than ReticleActors, hide the extra ones
				for (int32 j = DoTraceHits.Num(); j < ReticleActors.Num(); j++)
				{
					if (AGameplayAbilityWorldReticle* LocalReticleActor = ReticleActors[j].Get())
					{
						LocalReticleActor->SetIsTargetAnActor(false);
						LocalReticleActor->SetActorHiddenInGame(true);
					}
				}
			}
		}

		// 若DoTrace没有命中任何结果; 则互动按trace起点终点构造一次命中结果注入到持续队列里
		if (DoTraceHits.Num() < 1)
		{
			// If there were no hits, add a default HitResult at the end of the trace
			FHitResult HitResult;
			// Start param could be player ViewPoint. We want HitResult to always display the StartLocation.
			HitResult.TraceStart = StartLocation.GetTargetingTransform().GetLocation();
			HitResult.TraceEnd = TraceEnd;
			HitResult.Location = TraceEnd;
			HitResult.ImpactPoint = TraceEnd;
			DoTraceHits.Add(HitResult);

			if (bUseMechanism_PersistHits && m_QueuePersistHits.Num() < 1)
			{
				m_QueuePersistHits.Add(HitResult);
			}
		}
		// 真正的载入合理的命中结果队列
		ReturnHitResults.Append(DoTraceHits);
	} // for NumberOfTraces


	// Reminder: if bUsePersistentHitResults, Number of Traces = 1
	if (bUseMechanism_PersistHits && m_MaxAcknowledgeHitNums > 0)
	{
		// Handle ReticleActors
		for (int32 PersistentHitResultIndex = 0; PersistentHitResultIndex < m_QueuePersistHits.Num(); PersistentHitResultIndex++)
		{
			FHitResult& Hit = m_QueuePersistHits[PersistentHitResultIndex];

			// 由于玩家可能已经移动位置,则刷新Hit的起始位置
			// Update TraceStart because old persistent HitResults will have their original TraceStart and the player could have moved since then
			Hit.TraceStart = StartLocation.GetTargetingTransform().GetLocation();
			// 处理准星actor逻辑
			if (AGameplayAbilityWorldReticle* LocalReticleActor = ReticleActors[PersistentHitResultIndex].Get())
			{
				const bool bHitActor = Hit.GetActor() != nullptr;
				if (bHitActor && !Hit.bBlockingHit)
				{
					LocalReticleActor->SetActorHiddenInGame(false);

					const FVector ReticleLocation = (bHitActor && LocalReticleActor->bSnapToTargetedActor) ? Hit.GetActor()->GetActorLocation() : Hit.Location;

					LocalReticleActor->SetActorLocation(ReticleLocation);
					LocalReticleActor->SetIsTargetAnActor(bHitActor);
				}
				else
				{
					LocalReticleActor->SetActorHiddenInGame(true);
				}
			}
		}

		// 处理持续队列内比准星数组少的情况,修剪多出来的准星,将他们隐藏并移除选中状态
		if (m_QueuePersistHits.Num() < ReticleActors.Num())
		{
			// We have less hit results than ReticleActors, hide the extra ones
			for (int32 PersistentHitResultIndex = m_QueuePersistHits.Num(); PersistentHitResultIndex < ReticleActors.Num(); PersistentHitResultIndex++)
			{
				if (AGameplayAbilityWorldReticle* LocalReticleActor = ReticleActors[PersistentHitResultIndex].Get())
				{
					LocalReticleActor->SetIsTargetAnActor(false);
					LocalReticleActor->SetActorHiddenInGame(true);
				}
			}
		}
		return m_QueuePersistHits;
	}

	return ReturnHitResults;
}

///--@brief 在指定位置生成1个可视化的3D准星--/
AGameplayAbilityWorldReticle* AGRBGATA_Trace::SpawnReticleActor(FVector Location, FRotator Rotation)
{
	if (AGameplayAbilityTargetActor::ReticleClass)
	{
		AGameplayAbilityWorldReticle* SpawnedReticleActor = GetWorld()->SpawnActor<AGameplayAbilityWorldReticle>(ReticleClass, Location, Rotation);
		if (SpawnedReticleActor)
		{
			SpawnedReticleActor->InitializeReticle(this, PrimaryPC, ReticleParams);
			SpawnedReticleActor->SetActorHiddenInGame(true);
			ReticleActors.Add(SpawnedReticleActor);

			// 它决定目标数据（例如玩家选择的目标位置或对象）是应该在服务器上生成，还是可以在客户端生成并由服务器进行验证
			// This is to catch cases of playing on a listen server where we are using a replicated reticle actor.
			// (In a client controlled player, this would only run on the client and therefor never replicate. If it runs
			// on a listen server, the reticle actor may replicate. We want consistancy between client/listen server players.
			// Just saying 'make the reticle actor non replicated' isnt a good answer, since we want to mix and match reticle
			// actors and there may be other targeting types that want to replicate the same reticle actor class).
			if (!AGameplayAbilityTargetActor::ShouldProduceTargetDataOnServer)
			{
				SpawnedReticleActor->SetReplicates(false);
			}

			return SpawnedReticleActor;
		}
	}

	return nullptr;
}

///--@brief 把一组可视化3D准星从后到前挨个销毁--/
void AGRBGATA_Trace::DestroyReticleActors()
{
	for (int32 i = ReticleActors.Num() - 1; i >= 0; i--)
	{
		if (ReticleActors[i].IsValid())
		{
			ReticleActors[i].Get()->Destroy();
		}
	}

	ReticleActors.Empty();
}
#pragma endregion
