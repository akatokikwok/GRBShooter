// Copyright 2024 GRB.


#include "Characters/Abilities/GRBGameplayEffectTypes.h"

bool FGRBGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	return Super::NetSerialize(Ar, Map, bOutSuccess) && TargetData.NetSerialize(Ar, Map, bOutSuccess);
}
