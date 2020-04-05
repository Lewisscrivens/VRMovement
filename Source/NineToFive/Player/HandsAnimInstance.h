// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HandsAnimInstance.generated.h"

/* This is set as the parent of the animation blueprint so I can communicate these variables from C++ into that animation blueprint. */
UCLASS(transient, Blueprintable, hideCategories = AnimInstance, BlueprintType)
class NINETOFIVE_API UHandsAnimInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

public:
	
	/* Current hand close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float handClosingAmount;

	/* Current hand lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float handLerpingAmount;

	/* Current hands finger close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float fingerClosingAmount;

	/* Current hand lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float fingerLerpingAmount;

	/* Speed to lerp in between animation states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float handLerpSpeed;

	/* Speed to lerp in between animation states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	bool pointing;
};
