// Copyright 2024 GRB.


#include "Characters/Abilities/Executions//GRBDamageExecutionCalc.h"
#include "Characters/Abilities/AttributeSets/GRBAttributeSetBase.h"
#include "Characters/Abilities/GRBAbilitySystemComponent.h"

// Declare the attributes to capture and define how we want to capture them from the Source and Target.
struct FGRBDamageStatics
{
public:
	// 定义要捕获的属性;最后宏返回的是 FGameplayEffectAttributeCaptureDefinition类型
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);

public:
	// 构造器
	FGRBDamageStatics()
	{
		// Snapshot happens at time of GESpec creation

		// We're not capturing anything from the Source in this example, but there could be like AttackPower attributes that you might want.

		/**
		* UYourAttributeSetClass: 这是一个包含你想要捕获的属性的 UAttributeSet 类。
		* YourAttributeName: 这是你想要捕获的属性的名字。
		* Source: EGameplayEffectAttributeCaptureSource 枚举类型，指定捕获属性时的来源，可以是 Source 或 Target，分别表示从效果的发起者（Source）或目标（Target）捕获属性。
		* false: 一个布尔值，即 bSnapshot，表示是否在效果应用时对属性进行快照。若为 true，则表示在效果应用时捕获属性值并保存；若为 false，则表示每次计算都重新评估属性值
		*/

		// Capture optional Damage set on the damage GE as a CalculationModifier under the ExecutionCalculation
		DEFINE_ATTRIBUTE_CAPTUREDEF(UGRBAttributeSetBase, Damage, Source, true);

		// Capture the Target's Armor. Don't snapshot.
		DEFINE_ATTRIBUTE_CAPTUREDEF(UGRBAttributeSetBase, Armor, Target, false);
	}
};

///--@brief 构造1个策略单例--/
static const FGRBDamageStatics& DamageStatics()
{
	static FGRBDamageStatics DStatics;
	return DStatics;
}

UGRBDamageExecutionCalc::UGRBDamageExecutionCalc()
{
	// 爆头额外伤害倍率
	HeadShotMultiplier = 1.5f;

	// GEEC属性捕获列表里把单例里的字段
	UGameplayEffectCalculation::RelevantAttributesToCapture.Add(::DamageStatics().DamageDef);
	UGameplayEffectCalculation::RelevantAttributesToCapture.Add(::DamageStatics().ArmorDef);
}

void UGRBDamageExecutionCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, OUT FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* TargetAbilitySystemComponent = ExecutionParams.GetTargetAbilitySystemComponent();
	UAbilitySystemComponent* SourceAbilitySystemComponent = ExecutionParams.GetSourceAbilitySystemComponent();

	AActor* SourceActor = SourceAbilitySystemComponent ? SourceAbilitySystemComponent->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetAbilitySystemComponent ? TargetAbilitySystemComponent->GetAvatarActor() : nullptr;

	// 提取本GEEC被挂载到的GE; 拿到这张蓝图GE的 资产标签 "Effect.Damage.CanHeadShot"
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FGameplayTagContainer AssetTags;// 伤害BUFF蓝图上装好的资产标签
	Spec.GetAllAssetTags(AssetTags);

	// 用聚合器参数拿到 枪手身上和枪击目标身上的所有Tag
	FAggregatorEvaluateParameters EvaluationParameters;
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	// 结合之前处理过的单例捕获步骤; 提取GE捕获到的 人物属性集上的护甲值
	float Armor = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvaluationParameters, Armor);
	Armor = FMath::Max<float>(Armor, 0.0f);

	// 结合之前处理过的单例捕获步骤; 提取GE捕获到的 人物属性集上的伤害值
	float Damage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DamageDef, EvaluationParameters, Damage);// Capture optional damage value set on the damage GE as a CalculationModifier under the ExecutionCalculation
	// 由于在之前GA里的 const FGameplayEffectSpecHandle& TheBuffToApply = UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(RifleDamageGESpecHandle, CauseTag, Magnitude)这一步里的Magnitude存的是技能内手动配置的mBulletDamage = 10
	// 使用 GetSetByCallerMagnitude API 解包出来GA那一步给到的子弹伤害 10
	Damage += FMath::Max<float>(Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), false, -1.0f), 0.0f);// Add SetByCaller damage if it exists

	float UnmitigatedDamage = Damage; // Can multiply any damage boosters here

	// 在副开火技能的这一句内已经把HitResult加到了BUFF的上下文内;
	// UAbilitySystemBlueprintLibrary::EffectContextAddHitResult(ContextHandle, HitResultApply, Reset);
	const FHitResult* Hit = Spec.GetContext().GetHitResult();
	
	// 资产标签内必须有"Effect.Damage.CanHeadShot" 且 有命中结果 且打到了头部骨骼 才会被视作是爆头情形
	if (AssetTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.CanHeadShot"))) && Hit && Hit->BoneName == "b_head")// Check for headshot. There's only one character mesh here, but you could have a function on your Character class to return the head bone name
	{
		UnmitigatedDamage *= HeadShotMultiplier;// 累加爆头倍率
		FGameplayEffectSpec* MutableSpec = ExecutionParams.GetOwningSpecForPreExecuteMod();// 拿到这张伤害蓝图BUFF的Spec
		MutableSpec->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.HeadShot")));// 给伤害BUFF蓝图再主动附着加上1个资产标签,暗示有爆头状态 "Effect.Damage.HeadShot"
	}

	// 按策划公式再加工一下伤害值
	float MitigatedDamage = (UnmitigatedDamage) * (100 / (100 + Armor));

	// 最终组建好了修改器OutputModifier-修改 捕获列表::伤害值为我们之前一系列加工出来的Final MitigatedDamage 并加到修改器Output输出流;
	// 最终会进到堆栈 UGRBAttributeSetBase::PostGameplayEffectExecute 内的 if (Data.EvaluatedData.Attribute == GetDamageAttribute())来把伤害值和血量综合处理扣血
	if (MitigatedDamage > 0.f)
	{
		// Set the Target's damage meta attribute
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatics().DamageProperty, EGameplayModOp::Additive, MitigatedDamage));
	}
}
