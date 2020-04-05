// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "IIdentifiableXRDevice.h"
#include "Globals.h"
#include "VRPawn.generated.h"

/* Declare log type for the player pawn class. */
DECLARE_LOG_CATEGORY_EXTERN(LogVRPawn, Log, All);

/* Declare classes used. */
class UFloatingPawnMovement;
class UCapsuleComponent;
class USceneComponent;
class UCameraComponent;
class USphereComponent;
class AVRMovement;
class UStaticMeshComponent;
class AVRHand;
class UInputComponent;
class UMaterial;

/* Post update ticking function integration. 
 * NOTE: Important for checking the tracking state of the HMD and hands. */
USTRUCT()
struct FPostUpdateTick : public FActorTickFunction
{
	GENERATED_BODY()

	/* Target actor. */
	class AVRPawn* Target;

	/* Declaration of the new ticking function for this class. */
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

template <>
struct TStructOpsTypeTraits<FPostUpdateTick> : public TStructOpsTypeTraitsBase2<FPostUpdateTick>
{
	enum { WithCopy = false };
};


/* VR Pawn system that connected the VRMovement component and VRHands.
 * NOTE: If anything in the player pawn or hands with physics collision is enabled CCD and set world location will cause
 *       game objects to be forced in the direction that the hand or head collider passes through so don't enable this option...  */
UCLASS()
class NINETOFIVE_API AVRPawn : public APawn
{
	GENERATED_BODY()

public:

	/* Movement component for the developer mode and certain types of vr movement. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UFloatingPawnMovement* floatingMovement;

	/* Movement component for the developer mode and certain types of vr movement. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UCapsuleComponent* movementCapsule;

	/* Location of the floor relative to the headset. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	USceneComponent* scene;

	/* Player camera to map HMD location, rotation etc. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UCameraComponent* camera;

	/* Head colliders. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	USphereComponent* headCollider;
	
	/* Vignette for movement peripheral vision damping. Helps with motion sickness, enable this in the movement component VR. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UStaticMeshComponent* vignette;

	/* Left hand class pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	AVRHand* leftHand;

	/* Right hand class pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	AVRHand* rightHand;

	/* Pointer to the current moving hand, NOTE: nullptr = no movement. */
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	AVRHand* movingHand;

	/* Blueprint template class to spawn the movement component from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pawn")
	TSubclassOf<AVRMovement> vrMovementTemplate;

	/* The template class/BP for the left hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pawn")
	TSubclassOf<AVRHand> leftHandTemplate;

	/* The template class/BP for the right hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pawn")
	TSubclassOf<AVRHand> rightHandTemplate;

	/* Intensity of the haptic effects for this pawns hand classes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pawn")
	float hapticIntensity;

	/* Enable any debug messages for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pawn")
	bool debug;

	/* Component that holds all the movement functionality. Created and spawned from template (vrMovementTemplate) in begin play. */
	UPROPERTY(BlueprintReadWrite, Category = "Pawn")
	AVRMovement* vrMovement;

	FPostUpdateTick postTick; /* Post ticking declaration. */
	TArray<TEnumAsByte<EObjectTypeQuery>> physicsColliders; /* Collision array for any physics objects. */
	TArray<AActor*> actorsToIgnore; /* Ignored actors for the physics colliders mainly... */
	bool foundHMD; /* Found and tracking the HMD. */
	bool tracked; /* Has the headset ever been tracked. Used to move the player to the start location on begin play and first tracked event for the HMD. */
	bool devModeActive; /* Local bool to check if dev mode is enabled. */

private:

	FXRDeviceId hmdDevice; /* Device ID for the current HMD device that is being used. */

protected:

	/* Ran before begin play. */
	virtual void PostInitializeComponents() override;

	/* Level start. */
	virtual void BeginPlay() override;

	/* Setup pawn input. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/* Disable/Enable collisions on the whole pawn including hands individually from each other based on current tracking status....
	 * NOTE: When this is not enabled, when the device loses tracking and repositions itself when found again the sweep will cause physic
	 *		 actors in the scene to be affected by the force of movement... 
	 * NOTE: Could also use SetWorldLocation/Rotation no physics functions but I have no control over the object being spawned and positioned
	 *	     on begin play. */
	void UpdateHardwareTrackingState();

public:

	/* Constructor. */
	AVRPawn();

	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/* Late Frame. */
	void PostUpdateTick(float DeltaTime);

	/* Teleported function to handle any events on teleport. */
	void Teleported();

	/* Move/Set new world location and rotation of where the player is stood and facing.
	 * @Param newLocation, The new location in world-space to teleport the player to.
	 * @Param newRotation, The new rotation in world-space to teleport the player facing. */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void MovePlayerWithRotation(FVector newLocation, FRotator newFacingRotation);

	/* Move/Set new world location of where the player is stood.
	 * @Param newLocation, The new location in world-space to teleport the player to. */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void MovePlayer(FVector newLocation);

	/////////////////////////////////////////////////////
	/*					Input events.                  */
	/////////////////////////////////////////////////////

	template<bool val> void GrabLeft() { GrabLeft(val); }
	UFUNCTION(Category = "Pawn|Input")
	void GrabLeft(bool pressed);

	template<bool val> void GrabRight() { GrabRight(val); }
	UFUNCTION(Category = "Pawn|Input")
	void GrabRight(bool pressed);

	template<bool val> void GripLeft() { GripLeft(val); }
	UFUNCTION(Category = "Pawn|Input")
	void GripLeft(bool pressed);

	template<bool val> void GripRight() { GripRight(val); }
	UFUNCTION(Category = "Pawn|Input")
	void GripRight(bool pressed);

	template<bool val> void ThumbLeft() { ThumbLeft(val); }
	UFUNCTION(Category = "Pawn|Input")
	void ThumbLeft(bool pressed);

	template<bool val> void ThumbRight() { ThumbRight(val); }
	UFUNCTION(Category = "Pawn|Input")
	void ThumbRight(bool pressed);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickLeftX(float val);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickLeftY(float val);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickRightX(float val);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickRightY(float val);

	UFUNCTION(Category = "Pawn|Input")
	void TriggerLeft(float val);

	UFUNCTION(Category = "Pawn|Input")
	void TriggerRight(float val);
};