﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttributeSet.h"

#include "UltraAttributeSet.generated.h"

class AActor;
class UUltraAbilitySystemComponent;
class UObject;
class UWorld;
struct FGameplayEffectSpec;


/**
 * This macro defines a set of helper functions for accessing and initializing attributes.
 *
 * The following example of the macro:
 *		ATTRIBUTE_ACCESSORS(UUltraStyleSet, Style)
 * will create the following functions:
 *		static FGameplayAttribute GetHealthAttribute();
 *		float GetHealth() const;
 *		void SetHealth(float NewVal);
 *		void InitHealth(float NewVal);
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)


// Delegate used to broadcast attribute events.
DECLARE_MULTICAST_DELEGATE_FourParams(FUltraAttributeEvent, AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/, const FGameplayEffectSpec& /*EffectSpec*/, float /*EffectMagnitude*/);


/**
 * UUltraAttributeSet
 *
 *	Base attribute set class for the project.
 */
UCLASS()
class ULTRAGAME_API UUltraAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	UUltraAttributeSet();

	UWorld* GetWorld() const override;

	UUltraAbilitySystemComponent* GetUltraAbilitySystemComponent() const;
};
