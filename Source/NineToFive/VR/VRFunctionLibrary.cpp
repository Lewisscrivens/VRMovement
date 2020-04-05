// Fill out your copyright notice in the Description page of Project Settings.

#include "VR/VRFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/MeshComponent.h"
#include "Haptics//HapticFeedbackEffect_Base.h"
#include "Engine/World.h"
#include "Player/VRPawn.h"
#include "Player/VRHand.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

DEFINE_LOG_CATEGORY(LogVRFunctionLibrary);

UVRFunctionLibrary::UVRFunctionLibrary(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	//...
}

FRotator UVRFunctionLibrary::GetRelativeRotationFromWorld(FRotator currentWorldRotation, FTransform parentTransform)
{
	FTransform startRotationAsTransform = FTransform(currentWorldRotation, FVector::ZeroVector, FVector(1.0f, 1.0f, 1.0f));
	FTransform localRotationAsTransform = startRotationAsTransform * parentTransform.Inverse();
	return localRotationAsTransform.Rotator();
}

FRotator UVRFunctionLibrary::GetWorldRotationFromRelative(FRotator currentRelativeRotation, FTransform parentTransform)
{
	FTransform localSpaceRotationAsTransform = FTransform(currentRelativeRotation, FVector::ZeroVector, FVector(1.0f, 1.0f, 1.0f));
	FTransform worldSpaceRotationAsTransform = localSpaceRotationAsTransform * parentTransform;
	return worldSpaceRotationAsTransform.Rotator();
}

float UVRFunctionLibrary::GetYawAngle(FVector vector)
{
	// Calculate the angle of the vector.
	float localAngle = FMath::RadiansToDegrees(FMath::Atan(vector.Y / vector.X));

	if (vector.X < 0.0f && vector.Y > 0.0f)
	{
		localAngle += 180.0f;
	}
	else if (vector.X < 0.0f && vector.Y < 0.0f)
	{
		localAngle -= 180.0f;
	}

	// Return the yaw angle.
	return localAngle;
}

float UVRFunctionLibrary::GetAngleFromCumulativeAngle(float angle)
{
	bool bNegative = angle < 0.0f;
	float rotatorAngle;
	float angleRemainder;
	UKismetMathLibrary::FMod(angle, 360.0f, angleRemainder);

	// If the angle is negative use different formula to convert the angle.
	if (!bNegative)
	{
		if (angleRemainder > 180.0f) rotatorAngle = angleRemainder - 360.0f;
		else rotatorAngle = angleRemainder;
	}
	else
	{
		if (angleRemainder < -180.0f) rotatorAngle = angleRemainder + 360.0f;
		else rotatorAngle = angleRemainder;
	}

	return rotatorAngle;
}

FVector UVRFunctionLibrary::RotateAround(FVector toRotate, float amountToRotate, FVector axis, FVector pivotLocation)
{
	FVector rotatedRelativeLocation = toRotate.RotateAngleAxis(amountToRotate, axis);
	FVector rotatedWorldLocation = pivotLocation + rotatedRelativeLocation;
	return rotatedWorldLocation;
}

FVector UVRFunctionLibrary::CalculateActorLocalExtent(AActor* actor)
{
	// As the getting bounds is effected by world rotation its adjusted when updating.
	FRotator originalRotation = actor->GetActorRotation();
	actor->SetActorRotation(FRotator::ZeroRotator);
	FVector extent = FVector::ZeroVector;
	FVector boundsOrigin = FVector::ZeroVector;
	actor->GetActorBounds(true, boundsOrigin, extent);
	actor->SetActorRotation(originalRotation);
	return extent;
}

void UVRFunctionLibrary::FillObjectArray(TArray<TEnumAsByte<EObjectTypeQuery>>& array)
{
	for (uint8 i = EObjectTypeQuery::ObjectTypeQuery1; i < EObjectTypeQuery::ObjectTypeQuery_MAX; i++)
	{
		array.Add(EObjectTypeQuery(i));
	}
}

void UVRFunctionLibrary::ResetCurrentLevel(UObject* worldContext)
{
	FName currentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(worldContext, true));
	UGameplayStatics::OpenLevel(worldContext, currentLevelName, true);
}

void UVRFunctionLibrary::SetAngularConstraintOptions(UPhysicsConstraintComponent* constraint, float swing1Limit, EAngularConstraintMotion swing1Mode, float swing2Limit, EAngularConstraintMotion swing2Mode, float twistLimit, EAngularConstraintMotion twistMode)
{
	constraint->SetAngularSwing1Limit(swing1Mode, swing1Limit);
	constraint->SetAngularSwing2Limit(swing2Mode, swing2Limit);
	constraint->SetAngularTwistLimit(twistMode, twistLimit);
}

void UVRFunctionLibrary::SetLinearConstraintOptions(UPhysicsConstraintComponent* constraint, float xLimit, ELinearConstraintMotion xMode, float yLimit, ELinearConstraintMotion yMode, float zLimit, ELinearConstraintMotion zMode)
{
	constraint->SetLinearXLimit(xMode, xLimit);
	constraint->SetLinearYLimit(yMode, yLimit);
	constraint->SetLinearZLimit(zMode, zLimit);
}

FTransform UVRFunctionLibrary::LerpT(FTransform startTransform, FTransform endTransform, float alpha)
{
	FVector lerpingPosition = FMath::Lerp<FVector>(startTransform.GetLocation(), endTransform.GetLocation(), alpha);
	FVector lerpingScale = FMath::Lerp<FVector>(startTransform.GetScale3D(), endTransform.GetScale3D(), alpha);
	FRotator lerpingRotation = FMath::Lerp<FRotator>(startTransform.GetRotation().Rotator(), endTransform.GetRotation().Rotator(), alpha);
	return FTransform(lerpingRotation, lerpingPosition, lerpingScale);
}

FTransform UVRFunctionLibrary::LerpTSpeed(FTransform startTransform, FTransform endTransform, float speed, float deltaTime)
{
	float alpha = FMath::Clamp(deltaTime * speed, 0.0f, 1.0f);
	return LerpT(startTransform, endTransform, alpha);
}

bool UVRFunctionLibrary::ComponentOverlapComponentsByObject(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, ECollisionChannel blockingChannel, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	if (Component != nullptr)
	{
		FComponentQueryParams Params(SCENE_QUERY_STAT(ComponentOverlapComponents));
		Params.AddIgnoredActors(ActorsToIgnore);
		TArray<FOverlapResult> Overlaps;

		FCollisionObjectQueryParams ObjectParams;
		for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
		{
			const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
			ObjectParams.AddObjectTypesToQuery(Channel);
		}

		check(Component->GetWorld());
		Component->GetWorld()->ComponentOverlapMulti(Overlaps, Component, ComponentTransform.GetTranslation(), ComponentTransform.GetRotation(), Params, ObjectParams);

		for (int32 OverlapIdx = 0; OverlapIdx < Overlaps.Num(); ++OverlapIdx)
		{
			FOverlapResult const& O = Overlaps[OverlapIdx];
			if (O.Component.IsValid() && O.Component.Get()->GetCollisionResponseToChannel(blockingChannel) == ECR_Block)
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UVRFunctionLibrary::ComponentOverlapComponentsByChannel(UPrimitiveComponent* comp, const FTransform& transformToCheck, ECollisionChannel channel, const TArray<AActor*>& ignoredActors, TArray<UPrimitiveComponent*>& overlappingComponents, bool blockOnly)
{
	// Empty the array in-case it has items inside.
	overlappingComponents.Empty();

	// Ensure the component to check is not null.
	if (comp != nullptr)
	{
		// Create overlap query parameters.
		FComponentQueryParams Params(SCENE_QUERY_STAT(ComponentOverlapComponents));
		Params.AddIgnoredActors(ignoredActors);
		TArray<FOverlapResult> Overlaps;

		// Perform overlap check.
		check(comp->GetWorld());
		comp->GetWorld()->ComponentOverlapMultiByChannel(Overlaps, comp, transformToCheck.GetTranslation(), transformToCheck.GetRotation(), channel, Params);

		// Loop through each overlap to check if its a valid overlap.
		for (int32 OverlapIdx = 0; OverlapIdx < Overlaps.Num(); ++OverlapIdx)
		{
			FOverlapResult const& O = Overlaps[OverlapIdx];
			if (O.Component.IsValid())
			{
				if (UPrimitiveComponent* currOverlappingComp = O.Component.Get())
				{
					// Add found component if block only and is defiantly blocking collision, OR if block only is disabled add all overlaps to the array.
					if (!blockOnly || (currOverlappingComp->GetCollisionResponseToChannel(channel) == ECR_Block && currOverlappingComp->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics))
					{
						overlappingComponents.Add(currOverlappingComp);
					}
				}
			}
		}
	}

	// Return true or false if any overlaps was found for the comp in the specified collision channel.
	return (overlappingComponents.Num() > 0);
}