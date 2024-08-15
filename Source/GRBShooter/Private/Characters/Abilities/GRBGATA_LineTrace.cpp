// Copyright 2024 GRB


#include "Characters/Abilities/GRBGATA_LineTrace.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"

AGRBGATA_LineTrace::AGRBGATA_LineTrace()
{
}

void AGRBGATA_LineTrace::Configure(
	const FGameplayAbilityTargetingLocationInfo& InStartLocation,
	FGameplayTag InAimingTag,
	FGameplayTag InAimingRemovalTag,
	FCollisionProfileName InTraceProfile,
	FGameplayTargetDataFilterHandle InFilter,
	TSubclassOf<AGameplayAbilityWorldReticle> InReticleClass,
	FWorldReticleParameters InReticleParams,
	bool bInIgnoreBlockingHits,
	bool bInShouldProduceTargetDataOnServer,
	bool bInUsePersistentHitResults,
	bool bInDebug,
	bool bInTraceAffectsAimPitch,
	bool bInTraceFromPlayerViewPoint,
	bool bInUseAimingSpreadMod,
	float InMaxRange,
	float InBaseSpread,
	float InAimingSpreadMod,
	float InTargetingSpreadIncrement,
	float InTargetingSpreadMax,
	int32 InMaxHitResultsPerTrace,
	int32 InNumberOfTraces)
{
	AGameplayAbilityTargetActor::StartLocation = InStartLocation;
	AGRBGATA_Trace::AimingTag = InAimingTag;
	AGRBGATA_Trace::AimingRemovalTag = InAimingRemovalTag;
	AGRBGATA_Trace::TraceProfile = InTraceProfile;
	AGameplayAbilityTargetActor::Filter = InFilter;
	AGameplayAbilityTargetActor::ReticleClass = InReticleClass;
	AGameplayAbilityTargetActor::ReticleParams = InReticleParams;
	AGRBGATA_Trace::bIgnoreBlockingHits = bInIgnoreBlockingHits;
	AGameplayAbilityTargetActor::ShouldProduceTargetDataOnServer = bInShouldProduceTargetDataOnServer;
	AGRBGATA_Trace::bUseMechanism_PersistHits = bInUsePersistentHitResults;
	AGameplayAbilityTargetActor::bDebug = bInDebug;
	AGRBGATA_Trace::bTraceAffectsAimPitch = bInTraceAffectsAimPitch;
	AGRBGATA_Trace::bTraceFromPlayerViewPoint = bInTraceFromPlayerViewPoint;
	AGRBGATA_Trace::bUseAimingSpreadMod = bInUseAimingSpreadMod;
	AGRBGATA_Trace::MaxRange = InMaxRange;
	AGRBGATA_Trace::BaseSpread = InBaseSpread;
	AGRBGATA_Trace::AimingSpreadMod = InAimingSpreadMod;
	AGRBGATA_Trace::TargetingSpreadIncrement = InTargetingSpreadIncrement;
	AGRBGATA_Trace::TargetingSpreadMax = InTargetingSpreadMax;
	AGRBGATA_Trace::m_MaxAcknowledgeHitNums = InMaxHitResultsPerTrace;
	AGRBGATA_Trace::NumberOfTraces = InNumberOfTraces;

	if (AGRBGATA_Trace::bUseMechanism_PersistHits)
	{
		AGRBGATA_Trace::NumberOfTraces = 1;
	}
}

void AGRBGATA_LineTrace::DoTrace(TArray<FHitResult>& HitResults, const UWorld* World, const FGameplayTargetDataFilterHandle FilterHandle, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams Params)
{
	LineTraceWithFilter(HitResults, World, FilterHandle, Start, End, ProfileName, Params);
}

void AGRBGATA_LineTrace::ShowDebugTrace(TArray<FHitResult>& HitResults, EDrawDebugTrace::Type DrawDebugType, float Duration)
{
#if ENABLE_DRAW_DEBUG
	if (bDebug)
	{
		FVector ViewStart = StartLocation.GetTargetingTransform().GetLocation();
		FRotator ViewRot;
		if (PrimaryPC && bTraceFromPlayerViewPoint)
		{
			PrimaryPC->GetPlayerViewPoint(ViewStart, ViewRot);
		}

		FVector TraceEnd = HitResults[0].TraceEnd;
		if (NumberOfTraces > 1 || bUseMechanism_PersistHits)
		{
			TraceEnd = CurrentTraceEnd;
		}

		DrawDebugLineTraceMulti(GetWorld(), ViewStart, TraceEnd, DrawDebugType, true, HitResults, FLinearColor::Green, FLinearColor::Red, Duration);
	}
#endif
}

#if ENABLE_DRAW_DEBUG
// Copied from KismetTraceUtils.cpp
void AGRBGATA_LineTrace::DrawDebugLineTraceMulti(const UWorld* World, const FVector& Start, const FVector& End, EDrawDebugTrace::Type DrawDebugType, bool bHit, const TArray<FHitResult>& OutHits, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration) ? DrawTime : 0.f;

		// @fixme, draw line with thickness = 2.f?
		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().ImpactPoint;
			::DrawDebugLine(World, Start, BlockingHitPoint, TraceColor.ToFColor(true), bPersistent, LifeTime);
			::DrawDebugLine(World, BlockingHitPoint, End, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx = 0; HitIdx < OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, 16.0f, (Hit.bBlockingHit ? TraceColor.ToFColor(true) : TraceHitColor.ToFColor(true)), bPersistent, LifeTime);
		}
	}
}
#endif // ENABLE_DRAW_DEBUG
