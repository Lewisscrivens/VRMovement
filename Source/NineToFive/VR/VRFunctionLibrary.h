// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "MotionControllerComponent.h"
#include "Globals.h"
#include "VRFunctionLibrary.generated.h"

/* Declare classes used. */
class UPrimitiveComponent;
class UPhysicsConstraintComponent;

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogVRFunctionLibrary, Log, All);

/* This class stores any re-usable code or static functions to call to protected events. */
UCLASS()
class UVRFunctionLibrary : public UObject
{
	GENERATED_UCLASS_BODY()

protected:

	FTimerHandle colDelay;/* Timer handle for the collision delay function. */

public:

	/* Get the relative rotation from the world rotation and parent transform.
	 * @Param currentWorldRotation, The components current world rotation to get the relative rotation from.
	 * @Param parentTransform, The components parents transform to calculate relative rotation from. */
	UFUNCTION(BlueprintCallable, Category = Rotation)
		static FRotator GetRelativeRotationFromWorld(FRotator currentWorldRotation, FTransform parentTransform);

	/* Get the world rotation from the relative rotation and parent transform.
	 * @Param currentRelativeRotation, The current relative rotation of the component to get the world rotation from.
	 * @Param parentTransform, The components parent transform to calculate world rotation from. */
	UFUNCTION(BlueprintCallable, Category = Rotation)
		static FRotator GetWorldRotationFromRelative(FRotator currentRelativeRotation, FTransform parentTransform);

	/* Get the Yaw look at angle from a given vector.
	 * @Param vector, The vector to get the yaw angle rotation from. */
	UFUNCTION(BlueprintCallable, Category = Rotation)
		static float GetYawAngle(FVector vector);

	/* Get an angle between 0 and 360 from a cumulative angle.
	 * @Param angle, The angle to convert into 0 to 360 format. */
	UFUNCTION(BlueprintCallable, Category = Rotation)
		static float GetAngleFromCumulativeAngle(float angle);

	/* Rotates a vector location around a given axis and world location (pivotLocation).
	 * @Param toRotate, The vector to be rotated around the axis.
	 * @Param amountToRotate, The angle to rotate around the axis at the pivot location.
	 * @Param axis, The axis to rotate around.
	 * @Param pivotLocation, The world location of the pivot to be rotated around. */
	UFUNCTION(BlueprintCallable, Category = Rotation)
		static FVector RotateAround(FVector toRotate, float amountToRotate, FVector axis, FVector pivotLocation = FVector::ZeroVector);

	/* Returns the local extent of an actor where current rotation is taken into account.
	 * @Param actor, The actor to get the local extent of. */
	UFUNCTION(BlueprintCallable, Category = Vector)
		static FVector CalculateActorLocalExtent(AActor* actor);

	/* Fill a EObjectTypeQuery array.
	 * @Param array, array to fill. */
	UFUNCTION(BlueprintCallable, Category = Collision)
		static void FillObjectArray(TArray<TEnumAsByte<EObjectTypeQuery>>& array);

	/* Reset the current level.
	 * @Param worldContext, object in level that should be reset. */
	UFUNCTION(BlueprintCallable, Category = Level)
		static void ResetCurrentLevel(UObject* worldContext);

	/* Single function for setting constraint drive angular parameters.
	 * @Param constraint, constraint to change parameters on.
	 * @Param swing1Limit, swing1Limit to use.
	 * @Param swing1Mode, Type of motion to have in the swing1 axis.
	 * @Param swing2Limit, swing2Limit to use.
	 * @Param swing2Mode, Type of motion to have in the swing2 axis.
	 * @Param twistLimit, twistLimit to use.
	 * @Param twistMode, Type of motion to have in the twist axis. */
	UFUNCTION(BlueprintCallable, Category = Physics)
		static void SetAngularConstraintOptions(UPhysicsConstraintComponent* constraint, float swing1Limit,
			EAngularConstraintMotion swing1Mode, float swing2Limit, EAngularConstraintMotion swing2Mode, float twistLimit,
			EAngularConstraintMotion twistMode);

	/* Single function for setting constraint drive linear parameters.
	 * @Param constraint, constraint to change parameters on.
	 * @Param xLimit, x limit to use.
	 * @Param xMode, Type of motion to have in the x axis.
	 * @Param yLimit, y limit to use.
	 * @Param yMode, Type of motion to have in the y axis.
	 * @Param zLimit, z limit to use.
	 * @Param zMode, Type of motion to have in the z axis. */
	UFUNCTION(BlueprintCallable, Category = Physics)
		static void SetLinearConstraintOptions(UPhysicsConstraintComponent* constraint, float xLimit, ELinearConstraintMotion xMode,
			float yLimit, ELinearConstraintMotion yMode, float zLimit, ELinearConstraintMotion zMode);

	/* Function to lerp a transform for cleaner look in code.
	 * @Param startTransform, The start transform to lerp from.
	 * @Param endTransform, The end transform to lerp the startTransform to.
	 * @Param alpha,
	 * @Return FTransform, The current lerped progress transform. */
	UFUNCTION(BlueprintCallable, Category = Math)
		static FTransform LerpT(FTransform startTransform, FTransform endTransform, float alpha);

	/* Function to lerp a transform at a given speed.
	 * @Param startTransform, The start transform to lerp from.
	 * @Param endTransform, The end transform to lerp the startTransform to.
	 * @Param speed, The speed to lerp between start and end.
	 * @Param deltaTime, The worlds delta seconds value.
	 * @Return FTransform, The current lerped progress transform. */
	UFUNCTION(BlueprintCallable, Category = Math)
		static FTransform LerpTSpeed(FTransform startTransform, FTransform endTransform, float speed, float deltaTime);

	/* Copied from UKismetSystemLibrary as it needed to only return things that are trying block a given channel.
	 * NOTE: Used to check new location of component against blocking collision... */
	UFUNCTION(BlueprintCallable, Category = Collision)
		static bool ComponentOverlapComponentsByObject(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, ECollisionChannel blockingChannel, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents);

	/* Function to check if a given component at a transform overlaps blocking collision in a specified trace channel.
	 * @Param comp, The component to check overlaps for.
	 * @Param transformToCheck, The transform location/rotation to check the comp overlap at.
	 * @Param channel, The collision channel to check against.
	 * @Param ignoredActors, The actors to ignore.
	 * @Param @Output overlappingComponents, The output array for each overlapped component.
	 * @Param blockOnly, Should the overlap check only take blocking collisions into account.
	 * @Return true if overlaps was detected, false otherwise. */
	UFUNCTION(BlueprintCallable, Category = Collision)
		static bool ComponentOverlapComponentsByChannel(UPrimitiveComponent* comp, const FTransform& transformToCheck, ECollisionChannel channel,
			const TArray<AActor*>& ignoredActors, TArray<UPrimitiveComponent*>& overlappingComponents, bool blockOnly = true);
};
