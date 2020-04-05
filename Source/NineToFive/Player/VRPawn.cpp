// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/VRPawn.h"
#include "Player/VRHand.h"
#include "TimerManager.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h"
#include "IXRTrackingSystem.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IHeadMountedDisplay.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "Camera/CameraComponent.h"
#include "ConstructorHelpers.h"
#include "Player/VRMovement.h"
#include "VR/VRFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/WidgetInteractionComponent.h"

DEFINE_LOG_CATEGORY(LogVRPawn);

AVRPawn::AVRPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PrePhysics;

	// Setup the movement component used for agent properties for navigation and directional movement.
	floatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	floatingMovement->NavAgentProps.AgentRadius = 30.0f;

	// Setup capsule used for floor movement and gravity. By default this is disabled as its setup in the movement component.
	movementCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	movementCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	movementCapsule->SetCapsuleHalfHeight(80.0f);
	movementCapsule->SetCapsuleRadius(32.0f);
	RootComponent = movementCapsule;

	// Setup floor location.
	scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
	scene->SetupAttachment(movementCapsule);
	scene->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));

	// Setup HMD with head Collider. Disable any collisions until the HMD is defiantly being tracked to prevent physics actors being effected.
	camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	camera->SetupAttachment(scene);
	headCollider = CreateDefaultSubobject<USphereComponent>(TEXT("HeadCollider"));
	headCollider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	headCollider->SetCollisionProfileName("HandSkel");
	headCollider->InitSphereRadius(20.0f);
	headCollider->SetupAttachment(camera);

	// Setup vignette.
	vignette = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Vignette"));
	vignette->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	vignette->SetupAttachment(camera);
	vignette->SetActive(false);
	vignette->SetVisibility(false);

	// Add post update ticking function to this actor.
	postTick.bCanEverTick = false;
	postTick.TickGroup = TG_PostUpdateWork;

	// Initialise default variables.
	BaseEyeHeight = 0.0f;
	hapticIntensity = 1.0f;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	devModeActive = false;
	movingHand = nullptr;
	tracked = false;

	// Only use debug when development is enabled.
#if WITH_EDITOR
	debug = false;
#endif
}

void AVRPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Spawn VRMovementComponent class.
	if (!vrMovement)
	{
		// Setup VRMovement spawn parameters.
		FActorSpawnParameters movementParam;
		movementParam.Owner = this;
		movementParam.Instigator = this;
		movementParam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Spawn in the VRMovement from the blueprint created template.
		vrMovement = GetWorld()->SpawnActor<AVRMovement>(vrMovementTemplate, FVector::ZeroVector, FRotator::ZeroRotator, movementParam);
		FAttachmentTransformRules movementAttatchRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
		vrMovement->AttachToComponent(scene, movementAttatchRules);
		vrMovement->SetOwner(this);
	}
	
#if WITH_EDITOR
	// Enable developer mode if the HMD headset is enabled.
	if (!UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		vrMovement->currentMovementMode = EVRMovementMode::Developer;
		devModeActive = true;
	}
#endif
	}

void AVRPawn::BeginPlay()
{
	Super::BeginPlay();
	
	// Register the secondary post update tick function in the world on level start.
	postTick.Target = this;
	postTick.bCanEverTick = true;
	postTick.RegisterTickFunction(GetWorld()->PersistentLevel);

	// Setup the physicalCollidable objects array and the actors to ignore when doing collision checks.
	physicsColliders.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	// Setup the device ID for the current HMD used...
	hmdDevice.SystemName = UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName();
	hmdDevice.DeviceId = 0; // HMD..

	// Spawn the left and the right hand in on begin play as they are in there own class. (Issues were occurring when doing this in the constructor I assume due to it not recompiling the hand code).
	FActorSpawnParameters spawnHandParams;
	FAttachmentTransformRules handAttatchRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
	spawnHandParams.Owner = this;
	spawnHandParams.Instigator = this;
	spawnHandParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// Spawn the hands using the template classes made in BP.
	leftHand = GetWorld()->SpawnActor<AVRHand>(leftHandTemplate, FVector::ZeroVector, FRotator::ZeroRotator, spawnHandParams);
	leftHand->AttachToComponent(scene, handAttatchRules);
	leftHand->SetOwner(this);
	rightHand = GetWorld()->SpawnActor<AVRHand>(rightHandTemplate, FVector::ZeroVector, FRotator::ZeroRotator, spawnHandParams);
	rightHand->AttachToComponent(scene, handAttatchRules);
	rightHand->SetOwner(this);

	// Setup hands and movement and any pointers they need and developer adjustments.
	vrMovement->SetupMovement(this);
	leftHand->SetupHand(rightHand, this, devModeActive);
	rightHand->SetupHand(leftHand, this, devModeActive);

	// Create array for the actors to be ignored from certain collision traces in the hand and pawn classes.
	actorsToIgnore.Add(this);
	actorsToIgnore.Add(leftHand);
	actorsToIgnore.Add(rightHand);

	// Set the tracking origin for the HMD to be the floor. To support PSVR check if its that headset and set tracking origin to eye level and add the default player height. Also add way to rotate.
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Floor);
}

void AVRPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update the hands tick function from this class. PRE PHYSICS...
	if (leftHand && leftHand->active) leftHand->Tick(DeltaTime);
	if (rightHand && rightHand->active) rightHand->Tick(DeltaTime);
}

void AVRPawn::PostUpdateTick(float DeltaTime)
{
	// If there is a hand moving, update the vr movement component.
	if (movingHand) vrMovement->UpdateMovement(movingHand);
	// End the movement if its still set in the movement class.
	else if (vrMovement->currentMovingHand) vrMovement->UpdateMovement(vrMovement->currentMovingHand, true);
	
	// Update the current collision properties based from the tracking of the HMD and then each hand to prevent physics actors being affected by repositioning these components.
	if (!devModeActive) UpdateHardwareTrackingState();
}

void AVRPawn::Teleported()
{
	if (leftHand) leftHand->TeleportHand();
	if (rightHand) rightHand->TeleportHand();
}

void AVRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Player pawn input bindings.
	PlayerInputComponent->BindAction("GrabL", IE_Pressed, this, &AVRPawn::GrabLeft<true>);
	PlayerInputComponent->BindAction("GrabL", IE_Released, this, &AVRPawn::GrabLeft<false>);
	PlayerInputComponent->BindAction("GrabR", IE_Pressed, this, &AVRPawn::GrabRight<true>);
	PlayerInputComponent->BindAction("GrabR", IE_Released, this, &AVRPawn::GrabRight<false>);
	PlayerInputComponent->BindAction("GripL", IE_Pressed, this, &AVRPawn::GripLeft<true>);
	PlayerInputComponent->BindAction("GripL", IE_Released, this, &AVRPawn::GripLeft<false>);
	PlayerInputComponent->BindAction("GripR", IE_Pressed, this, &AVRPawn::GripRight<true>);
	PlayerInputComponent->BindAction("GripR", IE_Released, this, &AVRPawn::GripRight<false>);
	PlayerInputComponent->BindAction("ThumbMiddleL", IE_Pressed, this, &AVRPawn::ThumbLeft<true>);
	PlayerInputComponent->BindAction("ThumbMiddleL", IE_Released, this, &AVRPawn::ThumbLeft<false>);
	PlayerInputComponent->BindAction("ThumbMiddleR", IE_Pressed, this, &AVRPawn::ThumbRight<true>);
	PlayerInputComponent->BindAction("ThumbMiddleR", IE_Released, this, &AVRPawn::ThumbRight<false>);
	PlayerInputComponent->BindAxis("TriggerL", this, &AVRPawn::TriggerLeft);
	PlayerInputComponent->BindAxis("TriggerR", this, &AVRPawn::TriggerRight);
	PlayerInputComponent->BindAxis("ThumbstickLeft_X", this, &AVRPawn::ThumbstickLeftX);
	PlayerInputComponent->BindAxis("ThumbstickLeft_Y", this, &AVRPawn::ThumbstickLeftY);
	PlayerInputComponent->BindAxis("ThumbstickRight_X", this, &AVRPawn::ThumbstickRightX);
	PlayerInputComponent->BindAxis("ThumbstickRight_Y", this, &AVRPawn::ThumbstickRightY);
}

void AVRPawn::GrabLeft(bool pressed)
{
	if (leftHand && leftHand->active)
	{
		if (pressed) leftHand->Grab();
		else leftHand->Drop();
	}
}

void AVRPawn::GrabRight(bool pressed)
{
	if (rightHand && rightHand->active)
	{
		if (pressed) rightHand->Grab();
		else rightHand->Drop();
	}
}

void AVRPawn::GripLeft(bool pressed)
{
	if (leftHand && leftHand->active)
	{
		leftHand->Grip(pressed);
	}
}

void AVRPawn::GripRight(bool pressed) 
{
	if (rightHand && rightHand->active)
	{
		rightHand->Grip(pressed);
	}
}

void AVRPawn::ThumbLeft(bool pressed)
{
	if (leftHand)
	{
		// If gripping cancel movement activation.
		if (leftHand->gripping)
		{
			return;
		}
 		// Otherwise activate current movement mode.
 		else if (vrMovement->canMove && leftHand->active)
 		{
			bool moveEnabled = false;
			switch (vrMovement->currentMovementMode)
			{
			case EVRMovementMode::Developer:
			case EVRMovementMode::Teleport:
			case EVRMovementMode::SwingingArms:
			case EVRMovementMode::Lean:
				moveEnabled = true;
				break;
			}

			// Move if enabled.
			if (moveEnabled)
			{
				if (pressed) movingHand = leftHand;
				else if (movingHand && movingHand == leftHand) movingHand = nullptr;
			}
 		}
	}
}

void AVRPawn::ThumbRight(bool pressed)
{
	if (rightHand)
	{
		// If gripping cancel movement activation.
		if (rightHand->gripping)
		{
			return;
		}
 		// Otherwise activate current movement mode.
 		else if (vrMovement->canMove && rightHand->active)
 		{
			bool moveEnabled = false;
			switch (vrMovement->currentMovementMode)
			{
			case EVRMovementMode::Developer:
			case EVRMovementMode::Teleport:
			case EVRMovementMode::SwingingArms:
			case EVRMovementMode::Lean:
				moveEnabled = true;
				break;
			}
 			
			// Move if enabled.
			if (moveEnabled)
			{
				if (pressed) movingHand = rightHand;
				else if (movingHand && movingHand == rightHand) movingHand = nullptr;
			}
 		}
	}
}

void AVRPawn::ThumbstickLeftX(float val)
{
	if (leftHand && leftHand->active)
	{
		// Update the value.
		leftHand->thumbstick.X = val;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (vrMovement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (vrMovement && vrMovement->canMove)
			{
				if (val != 0.0f)
				{
					if (!movingHand) movingHand = leftHand;
				}
				else if (movingHand == leftHand && FMath::Abs(leftHand->thumbstick.Y) == 0.0f) movingHand = nullptr;
			}
		}
	}
}

void AVRPawn::ThumbstickLeftY(float val)
{
	if (leftHand && leftHand->active)
	{
		// Update the value.
		leftHand->thumbstick.Y = val;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (vrMovement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (vrMovement && vrMovement->canMove)
			{
				if (val != 0.0f)
				{
					if (!movingHand) movingHand = leftHand;
				}
				else if (movingHand == leftHand && FMath::Abs(leftHand->thumbstick.X) == 0.0f) movingHand = nullptr;
			}
		}	
	}
}

void AVRPawn::ThumbstickRightX(float val)
{
	if (rightHand && rightHand->active)
	{
		// Update the value.
		rightHand->thumbstick.X = val;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (vrMovement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (vrMovement && vrMovement->canMove)
			{
				if (val != 0.0f)
				{
					if (!movingHand) movingHand = rightHand;
				}
				else if (movingHand == rightHand && FMath::Abs(rightHand->thumbstick.Y) == 0.0f) movingHand = nullptr;
			}
		}	
	}	
}

void AVRPawn::ThumbstickRightY(float val)
{
	if (rightHand && rightHand->active)
	{
		// Update the value.
		rightHand->thumbstick.Y = val;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (vrMovement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (vrMovement && vrMovement->canMove)
			{
				if (val != 0.0f)
				{
					if (!movingHand) movingHand = rightHand;
				}
				else if (movingHand == rightHand && FMath::Abs(rightHand->thumbstick.X) == 0.0f) movingHand = nullptr;
			}
		}	
	}
}

void AVRPawn::TriggerLeft(float val)
{
	if (!devModeActive) leftHand->trigger = val;
}

void AVRPawn::TriggerRight(float val)
{
	if (!devModeActive) rightHand->trigger = val;
}

void AVRPawn::UpdateHardwareTrackingState()
{
	// Only allow the collision to be enabled on the player while the headset is being tracked.
	bool trackingHMD = UHeadMountedDisplayFunctionLibrary::IsDeviceTracking(hmdDevice);
	if (trackingHMD)
	{
		if (!foundHMD)
		{
			// HMD tracked...
			foundHMD = true;

			// On first tracked event, move the player to the scenes location so they are centered on the player start component.
			if (!tracked)
			{
				MovePlayerWithRotation(scene->GetComponentLocation(), scene->GetComponentRotation());
				tracked = true;
			}

			// Print debug...
#if WITH_EDITOR
			if (debug) UE_LOG(LogVRPawn, Warning, TEXT("Found and tracking the HMD owned by %s"), *GetName());
#endif
		}
	}
	else if (foundHMD)
	{
		// HMD lost...
		foundHMD = false;

#if WITH_EDITOR
		if (debug) UE_LOG(LogVRPawn, Warning, TEXT("Lost the HMD tracking owned by %s"), *GetName());
#endif
	}

	// Update the tracked state of the controllers also.
	leftHand->UpdateControllerTrackedState();
	rightHand->UpdateControllerTrackedState();
}

void AVRPawn::MovePlayerWithRotation(FVector newLocation, FRotator newFacingRotation)
{
	// Make sure that the newFacingRotation is applied before moving the player...
	float newCameraRotation = newFacingRotation.Yaw - (camera->RelativeRotation.Yaw - 180.0f);
	FRotator newRotation = FRotator(0.0f, newCameraRotation - 180.0f, 0.0f);
	SetActorRotation(newRotation);

	// Move the player to the new location.
	MovePlayer(newLocation);
}

void AVRPawn::MovePlayer(FVector newLocation)
{
	// Move the capsule to the specified newLocation.
	FVector newCapsuleLocation = FVector(newLocation.X, newLocation.Y, newLocation.Z + movementCapsule->GetUnscaledCapsuleHalfHeight());
	movementCapsule->SetWorldLocation(newCapsuleLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Get the relative offset of the current camera/player location to the capsule.
	FVector camaraToCapsuleOffset = movementCapsule->GetComponentTransform().InverseTransformPosition(camera->GetComponentLocation());
	camaraToCapsuleOffset.Z = 0.0f;

	// Offset the VR Scene location by the relative offset of the camera to the capsule to place the player within the movement capsule at the newLocation.
	FVector newRoomLocation = scene->GetComponentTransform().TransformPosition(-camaraToCapsuleOffset);
	scene->SetWorldLocation(newRoomLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Run the teleported function to run event inside interactables that are grabbed to teleport them to the new location also.
	Teleported();
}

void FPostUpdateTick::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	// Call the AVRPawn's second tick function.
	if (Target)
	{
		Target->PostUpdateTick(DeltaTime);
	}
}
