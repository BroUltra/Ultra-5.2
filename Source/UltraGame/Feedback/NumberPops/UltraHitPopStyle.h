// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "UltraHitPopStyle.generated.h"

class UObject;
class UStaticMesh;

UCLASS()
class UUltraHitPopStyle : public UDataAsset
{
	GENERATED_BODY()

public:

	UUltraHitPopStyle();

	UPROPERTY(EditDefaultsOnly, Category="HitPop")
	FString DisplayText;

	UPROPERTY(EditDefaultsOnly, Category="HitPop")
	FGameplayTagQuery MatchPattern;

	UPROPERTY(EditDefaultsOnly, Category="HitPop", meta=(EditCondition=bOverrideColor))
	FLinearColor Color;

	UPROPERTY(EditDefaultsOnly, Category="HitPop", meta=(EditCondition=bOverrideColor))
	FLinearColor CriticalColor;

	UPROPERTY(EditDefaultsOnly, Category="HitPop", meta=(EditCondition=bOverrideMesh))
	TObjectPtr<UStaticMesh> TextMesh;

	UPROPERTY()
	bool bOverrideColor = false;

	UPROPERTY()
	bool bOverrideMesh = false;
};
