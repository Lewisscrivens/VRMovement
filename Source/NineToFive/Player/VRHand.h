// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "MotionControllerComponent.h"
#include "GameFramework/Actor.h"
#include "Globals.h"
#include "VRHand.generated.h"

/* Declare log type for the hand class. */
DECLARE_LOG_CATEGORY_EXTERN(LogHand, Log, All);

/* Declare classes used in the H file. */
class AVRPawn;
class USoundBase;
class APlayerController;
class USceneComponent;
class UBoxComponent;
class UMotionControllerComponent;
class UVRPhysicsHandleComponent;
class USkeletalMeshComponent;
class UHapticFeedbackEffect_Base;
class UEffectsContainer;
class UAudioComponent;

/* NOTE: Just flipping a mesh on an axis to create a left and right hand from the said mesh will break its physics asset in version UE4.21.2...
 * NOTE: HandSkel collision used for interacting with grabbable etc. Constrained components must use physicsCollider to prevent constraint breakage. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NINETOFIVE_API AVRHand : public AActor
{
	GENERATED_BODY()

public:

	/* Scene component to hold the controller. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneComponent* scene;

	/* Motion controller. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UMotionControllerComponent* controller;

	/* Scene component to hold the hand skel and colliders. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneComponent* handRoot;

	/* Pointer to the hand skeletal mesh component from the player controller pawn. Set in BP. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	USkeletalMeshComponent* handSkel;

	/* Movement controller direction. (Point forward on X-axis in direction of hand and position to spawn teleporting spline etc.) */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneComponent* movementTarget;

	/* Pointer to the main player class. Initialized in the player class. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Hand")
	AVRPawn* player;

	/* Pointer to the other hand for grabbing objects from hands. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Hand")
	AVRHand* otherHand;

	/* Enum for what this current hand is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	EControllerHand handEnum;

	/* The name of this controller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	FName controllerName;

	/* Is the player grabbing? */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	bool grabbing;

	/* Is the player gripping? */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	bool gripping;

	/* Current velocity of the hand calculated in the tick function. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	FVector handVelocity;

	/* Current angular velocity of the hand calculated in the tick function. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	FVector handAngularVelocity;

	/* Current trigger value used for animating the hands etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	float trigger;

	/* Current thumb stick values for this hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	FVector2D thumbstick;
	
	/* Is the hand active. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	bool active;

	/* Is the controller currently being tracked. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand")
	bool foundController; 

	/* Enable any debug messages for this class.
	 * NOTE: Only used when DEVELOPMENT = 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand")
	bool debug;

protected:

	/* Level start. */
	virtual void BeginPlay() override;

private:

	APlayerController* owningController; /* The owning player controller of this hand class. */
	FVector lastHandPosition, currentHandPosition;/* Last and current hand positions used for calculating force/velocity etc. */
	FQuat lastHandRotation, currentHandRotation;
	FTransform originalHandTransform;/* Saved original hand transform at the end of initialization. */	

	int distanceFrameCount; /* How many frames has the hand been too far away from the grabbed object. */
	float currentHapticIntesity; /* The current playing haptic effects intensity for this hand classes controller. */
	bool collisionEnabled; /* Collision is enabled or disabled for this hand, disabled on begin play until the controller is tracked. */
	bool lastFrameOverlap; /* Did we overlap something in the last frame. */

#if WITH_EDITOR
	bool devModeEnabled; /* Local bool to check if dev mode is enabled. */
#endif

private:

	/* Update the hand animation variables. */
	void UpdateAnimationInstance();

public:

	/* Constructor */
	AVRHand();

	/* Frame */
	virtual void Tick(float DeltaTime) override;

	/* Grab pressed. */
	void Grab();

	/* Grab released. */
	void Drop();

	/* Initialise variables given from the AVRPawn, Also acts as this classes begin play.
	 * @Param oppositeHand, Pointer to the other hand.
	 * @Param playerRef, Pointer to the VRPawn class. 
	 * @Param dev, Is developer mode activated. */
	void SetupHand(AVRHand * oppositeHand, AVRPawn* playerRef, bool dev);

	/* Update the tracked state and collisions of this controller. */
	void UpdateControllerTrackedState();

	/* Grip is pressed/released. Currently only used for animation in BP. */
	void Grip(bool pressed);

	/* Function to run the teleport event after teleportation in the VRMovement class. */
	void TeleportHand();

	/* Play the given feedback for the pawn.
	 * @Param feedback, the feedback effect to use, if left null this function will use the defaultFeedback in the pawn class.
	 * @Param intensity, the intensity of the effect to play.
	 * @Param replace, Should replace the current haptic effect playing? If there is one... 
	 * @NOTE  If replace is false it will only replace a haptic feedback effect if the new intensity is greater than the current playing one. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	bool PlayFeedback(UHapticFeedbackEffect_Base* feedback = nullptr, float intensity = 1.0f, bool replace = false);

	/* Get the current haptic intensity if a haptic effect is playing.
	 * @Return 0 if no haptic effect is playing, otherwise return the current haptic effects intensity. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	float GetCurrentFeedbackIntensity();

	/* @Return true if this hand classes controller is currently playing a haptic effect. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	bool IsPlayingFeedback();

	/* Disables all hand functionality for the current hand, used in developer mode mainly for disabling hands temporarily.
	 * @Param disable, disable = disable the hand and !disable = enable the hand. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void Disable(bool disable);
};