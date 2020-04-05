// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/HandsAnimInstance.h"

UHandsAnimInstance::UHandsAnimInstance(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	handClosingAmount = 0.0f;
	handLerpingAmount = 0.0f;
	fingerClosingAmount = 0.0f;
	fingerLerpingAmount = 0.0f;
	handLerpSpeed = 15.0f;
	pointing = false;
}