// Fill out your copyright notice in the Description page of Project Settings.

#include "VRMovement.h"
#include "GameFramework/PlayerController.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Player/VRPawn.h"
#include "Player/VRHand.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SplineMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "VR/VRFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h" 
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationQueryFilter.h"

DEFINE_LOG_CATEGORY(LogVRMovement);

AVRMovement::AVRMovement()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root component.
	scene = CreateDefaultSubobject<USceneComponent>(TEXT("MovementScene"));
	scene->SetMobility(EComponentMobility::Movable);
	RootComponent = scene;

	// Spline for teleport beam.
	teleportSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TeleportSpline"));
	teleportSpline->Duration = 1.0f;
	teleportSpline->SetClosedLoop(false);
	teleportSpline->SetDefaultUpVector(FVector(0.0f, 0.0f, 1.0f), ESplineCoordinateSpace::World);
	teleportSpline->SetupAttachment(scene);

	// Setup teleporting meshes and spline.
	teleportRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportRing"));
	teleportRing->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	teleportRing->SetCollisionObjectType(ECollisionChannel::ECC_GameTraceChannel4);
	teleportRing->SetVisibility(false);
	teleportRing->SetupAttachment(scene);

	teleportArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportArrow"));
	teleportArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	teleportArrow->SetVisibility(false);
	teleportArrow->SetupAttachment(teleportRing);
	teleportArrow->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	teleportSplineEndMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportEndSplinePoint"));
	teleportSplineEndMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	teleportSplineEndMesh->SetVisibility(false);
	teleportSplineEndMesh->SetWorldScale3D(FVector(0.03f, 0.03f, 0.03f));
	teleportSplineEndMesh->SetupAttachment(scene);

	// Setup default values.
	invalidTeleportColor = FLinearColor::Red;
	validTeleportColor = FLinearColor::Green;
	firstMove = true;
	canMove = true;
	lastTeleportValid = false;
	teleportFade = true;
	teleportFadeColor = FLinearColor::Black;
	teleporting = false;
	cameraFadeTimeToLast = 0.1f;
	teleportHeight = 100.0f;
	teleportDeadzone = 0.4f;
	teleportDistance = 1000.0f;
	teleportGravity = -1600.0f;
	teleportSearchDistance = 80.0f;
	currentMovementMode = EVRMovementMode::Teleport;
	currentDirectionMode = EVRDirectionMode::Camera;
	teleportableTypes.Add(EObjectTypeQuery::ObjectTypeQuery9);
	cameraMoveDirection = true;
	vignetteDuringMovement = true;
	canApplyVignette = true;
	lastVignetteOpacity = 1.0f;
	minVignetteSpeed = 0.2f;
	vignetteTransitionSpeed = 5.0f;
	devHandOffset = FVector(70.0f, 25.0f, 8.0f);
	handMovementSpeed = 100.0f;
	walkingSpeed = 150.0f;
	minMovementOffsetRadius = 5.0f;
	maxMovementOffsetRadius = 40.0f;
	swingingArmsSpeed = 8.0f;
	inAir = false;
	requiresNavMesh = true;

#if WITH_EDITOR
	leftFrozen = rightFrozen = false;
	InputRebindAttemptTimer = 0.001f;
#endif
}

void AVRMovement::Tick(float DeltaTime)
{
	//  Check if the capsule is currently in the air and if it is enable physics, otherwise disable physics.
	if (player)
	{
		switch (currentMovementMode)
		{
#if WITH_EDITOR
			case EVRMovementMode::Developer:
				// Update the keyboard and mouse movement.
				UpdateDeveloperMovement(GetWorld()->GetDeltaSeconds());
#endif
			break;
			case EVRMovementMode::SpeedRamp: 
			case EVRMovementMode::Joystick:  
			case EVRMovementMode::SwingingArms:
			{
				FHitResult floorCheck;
				FCollisionQueryParams floorTraceParams;
				floorTraceParams.AddIgnoredActor(this);
				floorTraceParams.AddIgnoredActor(player);
				FVector feetLocation = player->scene->GetComponentLocation();
				GetWorld()->LineTraceSingleByProfile(floorCheck, feetLocation, feetLocation - FVector(0.0f, 0.0f, 1.0f), "PlayerCapsule", floorTraceParams);
				// If the floor was not found enable physics.
				if (floorCheck.bBlockingHit)
				{
					if (player->movementCapsule->IsSimulatingPhysics()) EnableCapsule(false);
				}
				else if (!player->movementCapsule->IsSimulatingPhysics()) EnableCapsule(true);
			}
			break;
		}
	}
}

void AVRMovement::SetupMovement(AVRPawn* playerPawn, bool dev)
{
	// Get and store a reference to the players controller.
	if (playerPawn)
	{
		player = playerPawn;
		playerController = Cast<APlayerController>(player->Controller);
	}

	// Ensure this player is set to the navAgent player setup in the project settings...
	UNavigationSystemV1* navSystem = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem());
	TArray<FNavDataConfig> navProps = navSystem->GetSupportedAgents();

	// agentID is the id of the nav agent setup in project settings. The index of what the players nav agent settings are...
	if (navProps.Num() > agentID && navProps[agentID].IsValid()) player->floatingMovement->NavAgentProps = navProps[agentID];
	else UE_LOG(LogVRMovement, Warning, TEXT("The agentID is out of bounds, navmesh may not support all agents..."));

	// Reset this in case the setup movement is being ran for a second time during runtime.
	canApplyVignette = true;
	player->vignette->SetActive(false);
	player->vignette->SetVisibility(false);

	// Movement needs to be setup twice in the case of developer mode.
	EVRMovementMode toSetUp = currentMovementMode;
	if (dev) toSetUp = EVRMovementMode::Teleport;
	
	// Update the current movement variables.
	switch (toSetUp)
	{
#if WITH_EDITOR
		case EVRMovementMode::Developer:
		{
			// Setup player movement controls.
			SetupDeveloperMovement();

			// Setup the teleport for developer movement.
			SetupMovement(nullptr, true);

			// Ensure capsule is disabled.
			player->movementCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		}
		break;
#endif
		case EVRMovementMode::Teleport:
		{
			// Initialise teleport width as the ring mesh width. Box extent is half the size of the box that fits the component.
			teleportWidth = teleportRing->Bounds.BoxExtent.X;

			// Disable capsule collision if in teleport mode.
			player->movementCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}	
		break;
		case EVRMovementMode::Lean:
		{
			// Only used in leaning movement.
			canApplyVignette = false;
		}
		// Still call speed ramp, joystick and swinging arms code as its still needed so no break.
		case EVRMovementMode::SpeedRamp: case EVRMovementMode::Joystick:  case EVRMovementMode::SwingingArms:
		{
			// Setup the capsule.
			player->movementCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			player->movementCapsule->SetCollisionProfileName("PlayerCapsule");

			// Set speed of floating movement component.
			player->floatingMovement->MaxSpeed = walkingSpeed;

			// Setup material for vignette so the opacity can be adjusted.
			if (vignetteDuringMovement)
			{
				if (vingetteMATInstance)
				{
					player->vignette->SetActive(true);
					player->vignette->SetVisibility(true);
					vignetteMAT = player->vignette->CreateDynamicMaterialInstance(0, vingetteMATInstance);
					vignetteMAT->SetScalarParameterValue("opacity", 1.0f);
				}
				else UE_LOG(LogVRMovement, Warning, TEXT("Null refference for the vignette material instance in the vr movement class..."));
			}
		}	
		break;
	}
}

void AVRMovement::EnableCapsule(bool enable)
{
	// Enable the capsules physics properties.
	if (enable)
	{
		FBodyInstance* capsuleBody = player->movementCapsule->GetBodyInstance();
		player->movementCapsule->SetSimulatePhysics(true);
		capsuleBody->bLockXRotation = true;
		capsuleBody->bLockYRotation = true;
		capsuleBody->bLockZRotation = true;
		capsuleBody->SetDOFLock(EDOFMode::Default);
		inAir = true;
	}
	// Disable the capsules physics properties.
	else
	{
		player->movementCapsule->SetSimulatePhysics(false);
		inAir = false;
	}
}

void AVRMovement::UpdateMovement(AVRHand* movementHand, bool released)
{
	// Ensure player is valid.
	if (player)
	{
		if (movementHand)
		{
			// Initialise the current moving hand before updating any movement properties.
			if (!released) currentMovingHand = movementHand;

			// Update the current movement mode.
			switch (currentMovementMode)
			{
#if WITH_EDITOR
				case EVRMovementMode::Developer:

					// Update the teleport while key is held down and teleport when released.
					if (released)
					{
						if (lastTeleportValid) TeleportPlayer();
						else DestroyTeleportSpline();
					}
					else UpdateTeleport(movementHand);

				break;
#endif
				case EVRMovementMode::Teleport:

					// check for camera fade to prevent accidental presses.
					if (!teleporting)
					{
						// Update the teleport while key is held down and teleport when released.
						if (released)
						{
							if (lastTeleportValid)
							{
								if (teleportFade) TeleportCameraFade();
								else TeleportPlayer();
							}
							else DestroyTeleportSpline();
						}
						else UpdateTeleport(movementHand);
					}				

				break;
				// Do not break lean case as it needs to run the speed ramp and joystick code also.
				case EVRMovementMode::Lean:

					// Disable the teleport ring.
					teleportRing->SetVisibility(false, true);

				case EVRMovementMode::SpeedRamp:
				case EVRMovementMode::Joystick:
				case EVRMovementMode::SwingingArms:

					// Update the controller movement mode. If released and vignette is enabled ramp the opacity back down to invisible at the specified speed.
					if (released && vignetteDuringMovement && vignetteMAT) GetWorld()->GetTimerManager().SetTimer(vignetteTimer, this, &AVRMovement::ResetVignette, 0.01f, true);
					else UpdateControllerMovement(movementHand);
				
				break;
			}

			// Adjust first move variable.
			if (released)
			{
				firstMove = true;
				currentMovingHand = nullptr;
			}
			else if (firstMove) firstMove = false;
		}	
	}
	else UE_LOG(LogVRMovement, Warning, TEXT("Null refference to player in the vr movement component. Cannot update movement..."));
}

#if WITH_EDITOR

void AVRMovement::SetupDeveloperMovement()
{
	// Setup the development mode for each hand.
	AttatchHand(player->leftHand);
	AttatchHand(player->rightHand);

	// Deactivate the head Collider and its collision in the VRPawn class.
	player->headCollider->SetActive(false);
	player->headCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Setup developer movement controls.
	UInputComponent* playerInput = player->InputComponent;

	//Fix for crash when trying to possess after starting the game in simulate mode ( doesn't always setup input still)
	if (!playerInput)
		EnableInput(UGameplayStatics::GetPlayerController(this, 0));

	//If we still cant bind, set a timer to try next frame
	if (!playerInput)
	{
		FTimerHandle timerHandle;
		FTimerDelegate timerDelegate;
		timerDelegate.BindUFunction(this, TEXT("SetupDeveloperMovement"));
		GetWorld()->GetTimerManager().SetTimer(timerHandle, timerDelegate, InputRebindAttemptTimer, false);

		UE_LOG(LogVRMovement, Error, TEXT(
			"SetupDeveloperMovement: Failed to get player input.Will attempt to reload in %f seconds, "
			"\n ignore the warning spam for this frame until the next attempt.\n"
			" (This can happen when you possess the player after simulating the game, soz it was either this or you crash lolol - sonny"),
			InputRebindAttemptTimer);

		return;
	}

	playerInput->BindAction("ResetHandPositions", IE_Released, this, &AVRMovement::DeveloperInput<EVRInput::ResetHands>);
	playerInput->BindAction("ToggleLeftHand", IE_Released, this, &AVRMovement::DeveloperInput<EVRInput::HideLeft>);
	playerInput->BindAction("ToggleRightHand", IE_Released, this, &AVRMovement::DeveloperInput<EVRInput::HideRight>);
	playerInput->BindAction("Reset", IE_Released, this, &AVRMovement::ResetLevel);
	playerInput->BindAxis("Forward");
	playerInput->BindAxis("Back");
	playerInput->BindAxis("Right");
	playerInput->BindAxis("Left");
	playerInput->BindAxis("Up");
	playerInput->BindAxis("Down");
	playerInput->BindAxis("MouseX");
	playerInput->BindAxis("MouseY");
	playerInput->BindAxis("TranslateHands");
}

void AVRMovement::DeveloperInput(EVRInput input)
{
	// Handle given input.
	switch (input)
	{
	case EVRInput::ResetHands:
	{
		// Get the hand classes.
		AVRHand* left = player->leftHand;
		AVRHand* right = player->rightHand;

		// Reset left hand.
		left->Disable(false);
		AttatchHand(left);
		leftFrozen = false;

		// Reset right hand.
		right->Disable(false);
		AttatchHand(right);
		rightFrozen = false;
	}
	break;
	case EVRInput::HideLeft:
	{
		// Toggle visible the left hand.
		AVRHand* left = player->leftHand;
		// If the hand is not yet frozen freeze the hand.
		if (!leftFrozen)
		{
			left->DetachFromActor(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
			leftFrozen = true;
		}
		// Otherwise if its frozen already but still active hide the hand.
		else if (left->active) left->Disable(true);
		// Otherwise if its both hidden and frozen reset the hand.
		else
		{
			left->Disable(false);
			AttatchHand(left);
			leftFrozen = false;
		}
	}
	break;
	case EVRInput::HideRight:
	{
		// Toggle visible the right hand.
		AVRHand* right = player->rightHand;
		// If the hand is not yet frozen freeze the hand.
		if (!rightFrozen)
		{
			right->DetachFromActor(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
			rightFrozen = true;
		}
		// Otherwise if its frozen already but still active hide the hand.
		else if (right->active) right->Disable(true);
		// Otherwise if its both hidden and frozen reset the hand.
		else
		{
			right->Disable(false);
			AttatchHand(right);
			rightFrozen = false;
		}
	}
	break;
	}
}


void AVRMovement::AttatchHand(AVRHand* handToAttach)
{
	// Attachment rules.
	FAttachmentTransformRules handAttatchRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
	
	// Change motion source of controller to prevent controllers changing position while in dev mode.
	handToAttach->controller->MotionSource = FName("Special_9");

	// Attach/Position the hand to the camera so it follows the developer char.
	handToAttach->AttachToComponent(player->camera, handAttatchRules);
	if (handToAttach->handEnum == EControllerHand::Left)
	{
		FVector leftOffset = devHandOffset;
		leftOffset.Y *= -1;
		handToAttach->SetActorRelativeLocation(leftOffset);
	}
	else handToAttach->SetActorRelativeLocation(devHandOffset);
	
	// Adjust rotation to ensure the hands are facing in the correct direction.
	handToAttach->SetActorRelativeRotation(FRotator(20.0f, 0.0f, 0.0f));
}

void AVRMovement::ResetLevel()
{
	UVRFunctionLibrary::ResetCurrentLevel(this);
}

bool AVRMovement::IsKeyDown(FName key)
{
	return (player->InputComponent->GetAxisValue(key) == 1);
}

void AVRMovement::UpdateDeveloperMovement(float deltaTime)
{
	// Body Movement
	FVector forward;
	if (cameraMoveDirection) forward = player->camera->GetForwardVector();
	else forward = player->GetActorForwardVector();
	FVector right = player->GetActorRightVector();
	FVector up = player->GetActorUpVector();
	if (IsKeyDown("Forward")) player->AddMovementInput(forward, 1.0f, true);
	if (IsKeyDown("Back")) player->AddMovementInput(forward, -1.0f, true);
	if (IsKeyDown("Left")) player->AddMovementInput(right, -1.0f, true);
	if (IsKeyDown("Right")) player->AddMovementInput(right, 1.0f, true);
	if (IsKeyDown("Up")) player->AddMovementInput(up, 1.0f, true);
	if (IsKeyDown("Down")) player->AddMovementInput(up, -1.0f, true);

	// Head Movement 
	float mouseX = player->InputComponent->GetAxisValue("MouseX");
	float mouseY = player->InputComponent->GetAxisValue("MouseY");
	FRotator newYaw = player->GetActorRotation();
	newYaw.Yaw += mouseX;
	player->SetActorRotation(newYaw);
	FRotator newPitch = player->camera->GetComponentRotation();
	newPitch.Pitch = FMath::Clamp(newPitch.Pitch + mouseY, -80.0f, 80.0f);
	player->camera->SetWorldRotation(newPitch);

	// Hand Movement
	float mouseWheel = player->InputComponent->GetAxisValue("TranslateHands");
	player->leftHand->AddActorWorldOffset(player->camera->GetForwardVector() * (mouseWheel * deltaTime * handMovementSpeed));
	player->rightHand->AddActorWorldOffset(player->camera->GetForwardVector() * (mouseWheel * deltaTime * handMovementSpeed));
}
#endif

void AVRMovement::UpdateControllerMovement(AVRHand* movementHand)
{
	// Lerp opacity to visible. visible opacity = 0.0f
	if (vignetteDuringMovement && canApplyVignette && vignetteMAT && lastVignetteOpacity > 0.0f)
	{
		// stop vignette reset function is playing on the world timer.
		GetWorld()->GetTimerManager().ClearTimer(vignetteTimer);
		// Lerp to location.
		LerpVignette(0.0f);
	}

	// Update the capsule if the player is not inside of it.
	FVector capsuleOffset = player->movementCapsule->GetComponentLocation() - player->camera->GetComponentLocation();
	capsuleOffset.Z = 0;
	float maxOffsetSizeBeforeReset = player->movementCapsule->GetUnscaledCapsuleRadius();

	// If the capsule is not close enough to the player reset its position and reposition the player inside.
	if (capsuleOffset.Size() > maxOffsetSizeBeforeReset && currentMovementMode != EVRMovementMode::Lean)
	{
		// NOTE: Could add some sort of validation here for checking the nav-mesh for closest available point.
		// Move capsule to current player location.
		FVector cameraLocation = player->camera->GetComponentLocation();
		player->movementCapsule->SetWorldLocation(FVector(cameraLocation.X, cameraLocation.Y, player->scene->GetComponentLocation().Z + player->movementCapsule->GetUnscaledCapsuleHalfHeight()), true);
		
		// Move scene component the relative offset between the capsule and camera position on the x and y axis.
		FVector cameraCapsuleOffset = player->movementCapsule->GetComponentTransform().InverseTransformPosition(player->camera->GetComponentLocation());
		cameraCapsuleOffset.Z = 0.0f;
		FVector newScenePosition = player->scene->GetComponentTransform().TransformPosition(-cameraCapsuleOffset);
		player->scene->SetWorldLocation(newScenePosition);
	}

	// Get the desired movement direction for the current movement mode.
	FVector controllerDirectionNoZ;
	float speedScale = 1.0f;
	if (currentMovementMode == EVRMovementMode::SpeedRamp)
	{
		// Move in the direction of the camera.
		if (currentDirectionMode == EVRDirectionMode::Camera) controllerDirectionNoZ = player->camera->GetForwardVector();
		// Move in the direction of controller.
		else if (currentDirectionMode == EVRDirectionMode::Controller) controllerDirectionNoZ = movementHand->controller->GetForwardVector();

		// Get speed ramp scale.
		speedScale = FMath::Clamp(movementHand->thumbstick.Y, 0.0f, 1.0f);
	}
	else if (currentMovementMode == EVRMovementMode::Joystick)
	{
		FRotator directionRotation = FRotator::ZeroRotator;
		// Move in the direction of the camera.
		if (currentDirectionMode == EVRDirectionMode::Camera) directionRotation = player->camera->GetForwardVector().Rotation();
		// Move in the direction of controller.
		else if (currentDirectionMode == EVRDirectionMode::Controller) directionRotation = movementHand->controller->GetForwardVector().Rotation();

		// Direction.
		FVector dir = FVector(movementHand->thumbstick.X, -movementHand->thumbstick.Y, 0);

		// Rotate the thumbstick movement relative to the current direction.
		controllerDirectionNoZ = dir.RotateAngleAxis(directionRotation.Yaw + 90.0f, FVector::UpVector);
		controllerDirectionNoZ.Normalize();
	}
	else if (currentMovementMode == EVRMovementMode::Lean)
	{
		// Move the ring to the correct location before calculations (at the players feet).
		teleportRing->SetVisibility(true, true);
		FVector currentLocation = player->movementCapsule->GetComponentLocation();
		FVector currentTPRingLocation = currentLocation;
		currentTPRingLocation.Z = player->scene->GetComponentLocation().Z;
		teleportRing->SetWorldLocation(currentTPRingLocation);

		// Flatten out the camera and ring locations to find the current direction of movement and size offset.
		currentLocation.Z = 0;
		FVector currentCameraLocation = player->camera->GetComponentLocation();
		currentCameraLocation.Z = 0;

		// After flattening the vectors onto a 2D pane calculate the offset direction.
		FVector currentOffset = currentCameraLocation - currentLocation;

		// If the current offset of the head is greater than the minimum amount required to start moving continue.
		if (currentOffset.Size() > minMovementOffsetRadius)
		{
			// Get direction based off the offset of the HMD to the capsule.
			controllerDirectionNoZ = currentOffset;
			controllerDirectionNoZ.Normalize();

			// Get the speed space by getting the normalized float distance between the min and max location. scale = current / max
			speedScale = (currentOffset.Size() - minMovementOffsetRadius) / (maxMovementOffsetRadius - minMovementOffsetRadius);
		}
		// Reset vignette if no longer moving.
		else
		{
			if (vignetteDuringMovement && canApplyVignette)
			{
				if (vignetteMAT) GetWorld()->GetTimerManager().SetTimer(vignetteTimer, this, &AVRMovement::ResetVignette, 0.01f, true);
				canApplyVignette = false;
			}
			speedScale = 0.0f;
		}

		// Point the arrow in the correct direction that is currently the relative forward vector for movement. (Cameras look direction)
		teleportArrow->SetWorldRotation(FRotator(0.0f, player->camera->GetComponentRotation().Yaw + 90.0f, 0.0f));
	}
	else if (currentMovementMode == EVRMovementMode::SwingingArms)
	{
		// If its first move use last as current.
		if (firstMove)
		{
			originalMovementLocation = movementHand->controller->GetComponentLocation();
			lastMovementLocation = originalMovementLocation;
		}

		// Get the current movement location of the controller in the world.
		FVector currentMovementLocation = movementHand->controller->GetComponentLocation();
		FVector movementDirection = lastMovementLocation - currentMovementLocation;

		// Get movement direction and speed scale.
		controllerDirectionNoZ = movementDirection.GetSafeNormal();
		speedScale = FMath::Clamp(movementDirection.Size(), 0.0f, 20.0f) / 20.0f;
		speedScale *= swingingArmsSpeed;

		// Save this frames current as last movement so it can be used next frame.
		lastMovementLocation = currentMovementLocation;
	}

	// Only show vignette at speeds above min vignette speed.
	if (vignetteDuringMovement)
	{
		// If the speed is greater than the min speed required to show the vignette, show it. Otherwise reset it.
		if (speedScale > minVignetteSpeed) canApplyVignette = true;
		else if (canApplyVignette)
		{
			GetWorld()->GetTimerManager().SetTimer(vignetteTimer, this, &AVRMovement::ResetVignette, 0.01f, true);
			canApplyVignette = false;
		}
	}

	// Don't allow any z direction.
	controllerDirectionNoZ.Z = 0;

	// Apply player movement.
	player->AddMovementInput(controllerDirectionNoZ, (speedScale / 2) * (walkingSpeed / 100.0f));
}

void AVRMovement::ResetVignette()
{
	// End the reset once the opacity has been reset.
	if (lastVignetteOpacity < 1.0f) LerpVignette(1.0f);
	else GetWorld()->GetTimerManager().ClearTimer(vignetteTimer);
}

void AVRMovement::LerpVignette(float target)
{
	float newOpacity = UKismetMathLibrary::FInterpTo(lastVignetteOpacity, target, GetWorld()->GetDeltaSeconds(), vignetteTransitionSpeed);
	lastVignetteOpacity = newOpacity;
	vignetteMAT->SetScalarParameterValue("opacity", newOpacity);
}

void AVRMovement::UpdateTeleport(AVRHand* movementHand)
{
	// Get rid of last frames spline and initially hide the teleport meshes.
	DestroyTeleportSpline();

	// Create the teleport spline.
	FVector splineEndLocation;
	FTransform splineStartTrasform = movementHand->movementTarget->GetComponentTransform();
	bool validLocation = CreateTeleportSpline(splineStartTrasform, splineEndLocation);

	if (validLocation)
	{
		// Continue to check if the location is valid if the end location is touching the ground.
		if (requiresNavMesh) lastTeleportValid = ValidateTeleportLocation(splineEndLocation);
		else  lastTeleportValid = true;

		// If the last valid location is a valid location update the ring and arrow to that location and show them in game.
		if (lastTeleportValid)
		{
			// Update the last valid teleport location.
			lastValidTeleportLocation = splineEndLocation;

			// Update the rotation direction depending on the thumb offset dead zone.
			FVector thumbOffset = FVector(movementHand->thumbstick.X * -1, movementHand->thumbstick.Y, 0.0f);
			if (thumbOffset.Size() > FMath::Clamp(teleportDeadzone, 0.0f, 1.0f))
			{
				FRotator thumbRotation = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, thumbOffset);
				teleportRotation = FRotator(thumbRotation.Pitch, player->camera->GetComponentRotation().Yaw + thumbRotation.Yaw - 90.0f, thumbRotation.Roll);
				if (!teleportArrow->IsVisible()) teleportArrow->SetVisibility(true);
			}
			else
			{
				teleportRotation = FRotator::ZeroRotator;
				if (teleportArrow->IsVisible()) teleportArrow->SetVisibility(false);
			}

			// Show the teleport meshes at the found valid location/rotation if its enabled.
			teleportRing->SetWorldLocationAndRotation(lastValidTeleportLocation, teleportRotation, false, nullptr, ETeleportType::TeleportPhysics);
			teleportRing->SetVisibility(true);
			
			// Update the teleporting components materials to valid.
			UpdateTeleportMaterials(true);
		}
		// Otherwise update the teleport material to invalid and disable lastTeleportValid.
		else UpdateTeleportMaterials(false);
	}
}

bool AVRMovement::CreateTeleportSpline(FTransform startTransform, FVector& outLocation)
{
	teleportSpline->ClearSplinePoints();
	teleportSpline->SetWorldLocationAndRotation(startTransform.GetLocation(), startTransform.GetRotation());

	// Return false if the hand is too close to the world up vector.
	if (FMath::IsNearlyEqual(startTransform.GetRotation().GetForwardVector().Z, 1.0f, 0.3f))
	{
		// First draw cancel VFX.
		FVector startPoint = startTransform.GetLocation();
		FVector endPoint = startPoint + (startTransform.GetRotation().GetForwardVector() * 30.0f);

		// Make spline mesh to go from start point to end point.
		FName splineMeshName = MakeUniqueObjectName(this, USplineMeshComponent::StaticClass(), FName("SplineMesh"));
		USplineMeshComponent* newMesh = NewObject<USplineMeshComponent>(this, splineMeshName);
		newMesh->SetMobility(EComponentMobility::Movable);
		newMesh->RegisterComponent();
		newMesh->SetStaticMesh(teleportSplineMesh);
		newMesh->SetStartAndEnd(startPoint, FVector(0.0f), endPoint, FVector(0.0f));
		splineMeshes.Add(newMesh);

		// Set location and show the end mesh at the endPoint.
		teleportSplineEndMesh->SetWorldLocation(endPoint, false, nullptr, ETeleportType::TeleportPhysics);
		teleportSplineEndMesh->SetVisibility(true);

		// Make teleport materials to be red. 
		UpdateTeleportMaterials(false);

		// Return false for nothing found.
		return false;
	}

	// Projectile trace used to create the spline arc and find a teleport location.
	FHitResult hit;
	TArray<FVector> outPathPositions;
	FVector outLastTraceDestination;

	// Ignore self and ignore the player and anything thats currently held in the hand.
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.Add(player);
	actorsToIgnore.Add(this);
	actorsToIgnore.Add(currentMovingHand);
	actorsToIgnore.Add(currentMovingHand->otherHand);

	// Projectile trace the spline hit location and use each stage of the trace to create a spline from the shape.
	UGameplayStatics::Blueprint_PredictProjectilePath_ByTraceChannel(GetWorld(), hit, outPathPositions, outLastTraceDestination, teleportSpline->GetComponentLocation(), teleportSpline->GetForwardVector() * teleportDistance, true, 0.0f, ECC_Visibility, false, actorsToIgnore, EDrawDebugTrace::None, 0.0f, 30.0f, 2.0f, teleportGravity);

	// Set the spline up from the projectile trace.
	for (FVector splinePoint : outPathPositions)
	{
		teleportSpline->AddSplinePoint(splinePoint, ESplineCoordinateSpace::World);
	}
	teleportSpline->SetSplinePointType(outPathPositions.Num() - 1, ESplinePointType::CurveClamped);

	// DO A RESOLUTION SCALE FOR SCALING DOWN HOW MANY SPLINE MESHES ARE BEING CREATED.
	// For each spline point of the current hands spline create a spline mesh between them and add them to an array.
	for (int i = 0; i < teleportSpline->GetNumberOfSplinePoints() - 1; i++)
	{
		FName splineMeshName = MakeUniqueObjectName(this, USplineMeshComponent::StaticClass(), FName("SplineMesh"));
		USplineMeshComponent* newMesh = NewObject<USplineMeshComponent>(this, splineMeshName);
		newMesh->SetMobility(EComponentMobility::Movable);
		newMesh->RegisterComponent();
		newMesh->SetStaticMesh(teleportSplineMesh);
		newMesh->SetStartAndEnd(teleportSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), teleportSpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World), teleportSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World), teleportSpline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::World));
		splineMeshes.Add(newMesh);
	}

	// Set location and show the end mesh of the spline.
	teleportSplineEndMesh->SetWorldLocation(teleportSpline->GetLocationAtSplinePoint(teleportSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World), false, nullptr, ETeleportType::TeleportPhysics);
	teleportSplineEndMesh->SetVisibility(true);

	// Check if the hit location is a valid navigatable point on the nav mesh and return valid hit as bool.
	if (hit.bBlockingHit)
	{
		outLocation = hit.Location + hit.Normal;
		return true;
	}
	else // Didn't hit anything so, show the invalid material.
	{
		lastTeleportValid = false;
		UpdateTeleportMaterials(false);
		return false;
	}
}

void AVRMovement::DestroyTeleportSpline()
{
	// Clear the spline mesh array of any components.
	if (splineMeshes.Num() != 0)
	{
		for (USplineMeshComponent* splineMesh : splineMeshes)
		{
			splineMesh->DestroyComponent();
		}
		splineMeshes.Empty();
	}

	// Hide any of the visuals such as the end of the spline mesh, ring and arrow.
	teleportSplineEndMesh->SetVisibility(false);
	teleportRing->SetVisibility(false, true);
}

bool AVRMovement::ValidateTeleportLocation(FVector& location)
{
	UNavigationSystemV1* navSystem = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem());
	if (navSystem)
	{		
		// Get the nav properties from the players movement component to determine player width, height etc.
		FVector foundLocation;
		FVector searchingExtent = FVector(teleportSearchDistance, teleportSearchDistance, teleportSearchDistance);
		FNavAgentProperties props = player->floatingMovement->GetNavAgentPropertiesRef();
		ANavigationData* navData = navSystem->GetNavDataForProps(props);
		bool locationOnNav = navSystem->K2_ProjectPointToNavigation(GetWorld(), location, foundLocation, navData, nullptr, searchingExtent);
		// If the location is on the nav-mesh return true and set location to said found location.
		if (locationOnNav)
		{
			// Do a final line trace to adjust the Z of the nav-mesh as it sometimes is not set up to be flush with the surface.
			FHitResult navMeshHeightError;
			FCollisionQueryParams floorTraceParams;
			floorTraceParams.AddIgnoredActor(this);
			floorTraceParams.AddIgnoredActor(player);
			GetWorld()->LineTraceSingleByObjectType(navMeshHeightError, foundLocation, foundLocation - FVector(0.0f, 0.0f, 10.0f), teleportableTypes, floorTraceParams);
			if (navMeshHeightError.bBlockingHit) location = navMeshHeightError.Location;
			// Otherwise if nothing is hit just use the nav meshes assumed location as it the next best option.
			else location = foundLocation;
			return true;
		}
		else return false;
	}
	else return false;
}

void AVRMovement::UpdateTeleportMaterials(bool valid)
{
	FLinearColor newColor = validTeleportColor;
	if (!valid) newColor = invalidTeleportColor;

	// Adjust the materials of the spline and spline end mesh to the valid tp material.
	teleportSplineEndMesh->SetVectorParameterValueOnMaterials("Color", FVector(newColor.R, newColor.G, newColor.B));
	teleportRing->SetVectorParameterValueOnMaterials("Color", FVector(newColor.R, newColor.G, newColor.B));
	teleportArrow->SetVectorParameterValueOnMaterials("Color", FVector(newColor.R, newColor.G, newColor.B));
	for (USplineMeshComponent* splineMesh : splineMeshes)
	{
		splineMesh->SetVectorParameterValueOnMaterials("Color", FVector(newColor.R, newColor.G, newColor.B));
	}
}

void AVRMovement::TeleportCameraFade()
{
	if (lastTeleportValid)
	{
		// If the teleport spline is still visible destroy it.
		if (splineMeshes.Num() > 0) DestroyTeleportSpline();

		// Fade the camera.
		if (playerController) playerController->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, cameraFadeTimeToLast, teleportFadeColor, false, true);

		// Delay the teleport the amount of time it took to fade the camera.
		FTimerHandle teleportTimer;
		FTimerDelegate teleportTimerDel;
		teleportTimerDel.BindUFunction(this, FName("TeleportPlayer"));
		GetWorld()->GetTimerManager().SetTimer(teleportTimer, teleportTimerDel, cameraFadeTimeToLast, false);
		teleporting = true;
	}	
}

void AVRMovement::TeleportPlayer()
{
	// If the teleport spline is still visible destroy it.
	if (splineMeshes.Num() > 0) DestroyTeleportSpline();

	// If in developer mode teleport capsule and raise from floor and teleport.
	if (currentMovementMode == EVRMovementMode::Developer)
	{
		// Teleport the player, add the capsule offset as the origin is at the center of the capsule. Also move scene back to capsule floor.
		float capHeight = player->movementCapsule->GetUnscaledCapsuleHalfHeight();
		player->movementCapsule->SetWorldLocation(lastValidTeleportLocation + FVector(0.0f, 0.0f, 150.0f), false, nullptr, ETeleportType::TeleportPhysics);
		player->scene->SetRelativeLocation(FVector::ZeroVector);
		player->Teleported();
	}
	// Otherwise find out the correct location for the capsule and the room relative to said capsule.
	else 
	{
		// Move the players location to the locationToTeleport facing in the rotationToFace.
		if (teleportRotation == FRotator::ZeroRotator) player->MovePlayer(lastValidTeleportLocation);
		else player->MovePlayerWithRotation(lastValidTeleportLocation, teleportRotation);

		// Un-fade the camera after teleport.
		if (teleportFade && playerController) playerController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, cameraFadeTimeToLast, teleportFadeColor, false, true);
		teleporting = false;
	}

	// Disable the teleport.
	lastTeleportValid = false;

	// Play teleport sound if it is not null.
	if (teleportSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), teleportSound, player->camera->GetComponentLocation());
}