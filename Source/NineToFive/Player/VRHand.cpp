// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/VRHand.h"
#include "Player/VRPawn.h"
#include "Player/HandsAnimInstance.h"
#include "XRMotionControllerBase.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "VR/VRFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include <Sound/SoundBase.h>

DEFINE_LOG_CATEGORY(LogHand);

AVRHand::AVRHand()
{
	// Tick for this function is ran in the pawn class.
	PrimaryActorTick.bCanEverTick = false; 

	// Setup motion controller. Default setup.
	controller = CreateDefaultSubobject<UMotionControllerComponent>("Controller");
	controller->MotionSource = FXRMotionControllerBase::LeftHandSourceId;
	controller->SetupAttachment(scene);
	controller->bDisableLowLatencyUpdate = true;
	RootComponent = controller;

	// handRoot comp.
	handRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HandRoot"));
	handRoot->SetupAttachment(controller);

	// Skeletal mesh component for the hand model. Default setup.
	handSkel = CreateDefaultSubobject<USkeletalMeshComponent>("handSkel");
	handSkel->SetCollisionProfileName("HandSkel");
	handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	handSkel->SetupAttachment(handRoot);
	handSkel->SetRenderCustomDepth(true);
	handSkel->SetGenerateOverlapEvents(true);
	handSkel->SetCustomDepthStencilValue(1); // Custom stencil mask material for showing hands through objects.
	handSkel->SetRelativeTransform(FTransform(FRotator(-20.0f, 0.0f, 0.0f), FVector(-18.0f, 0.0f, 0.0f), FVector(0.27f, 0.27f, 0.27f)));

	// Setup movement direction component.
	movementTarget = CreateDefaultSubobject<USceneComponent>("MovementTarget");
	movementTarget->SetMobility(EComponentMobility::Movable);
	movementTarget->SetupAttachment(handSkel);

	// Initialise default variables.
	handEnum = EControllerHand::Left;
	grabbing = false;
	gripping = false;
	foundController = false;
	active = true;
	collisionEnabled = false;
	thumbstick = FVector2D(0.0f, 0.0f);
	distanceFrameCount = 0;
	
#if WITH_EDITOR
	debug = false;
	devModeEnabled = false;
#endif
}

void AVRHand::BeginPlay()
{
	Super::BeginPlay();

	//...
}

void AVRHand::SetupHand(AVRHand * oppositeHand, AVRPawn* playerRef, bool dev)
{
	// Initialise class variables.
	player = playerRef;
	otherHand = oppositeHand;
	owningController = player->GetWorld()->GetFirstPlayerController();

	// Use dev mode to disable areas of code when in developer mode.
#if WITH_EDITOR
	devModeEnabled = dev;
#endif

	// Save the original transform of the hand for calculating offsets.
	originalHandTransform = controller->GetComponentTransform();
}

void AVRHand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Calculate controller velocity as its not simulating physics.
	lastHandPosition = currentHandPosition;
	currentHandPosition = controller->GetComponentLocation();
	handVelocity = (currentHandPosition - lastHandPosition) / DeltaTime;
	
	// Calculate angular velocity for the controller for when its not simulating physics.
	lastHandRotation = currentHandRotation;
	currentHandRotation = controller->GetComponentQuat();
	FQuat deltaRot = lastHandRotation.Inverse() * currentHandRotation;
	FVector axis;
	float angle;
	deltaRot.ToAxisAndAngle(axis, angle);
	angle = FMath::RadiansToDegrees(angle);
	handAngularVelocity = currentHandRotation.RotateVector((axis * angle) / DeltaTime);

	// Update the animation instance variables for the handSkel.
	UpdateAnimationInstance();
}

void AVRHand::Grab()
{
	// Grab pressed.
	grabbing = true;

#if WITH_EDITOR
	// If in dev-mode ensure the trigger is 1.0f when grabbed.
	if (devModeEnabled) trigger = 1.0f;
#endif
}

void AVRHand::Drop()
{
#if WITH_EDITOR
	// If in dev-mode ensure trigger value is 0.0f when dropped.
	if (devModeEnabled) trigger = 0.0f;
#endif

	// Grab released.
	grabbing = false;
}

void AVRHand::Grip(bool pressed)
{
	// Currently gripping.
	gripping = pressed;
}

void AVRHand::TeleportHand()
{
	//...
}

void AVRHand::UpdateControllerTrackedState()
{
	// Only allow the collision on this hand to be enabled if the controller is being tracked.
	bool trackingController = controller->IsTracked();
	if (trackingController)
	{
		if (!foundController)
		{
			foundController = true;
#if WITH_EDITOR
			if (debug) UE_LOG(LogHand, Warning, TEXT("Found and tracking the controller owned by %s"), *GetName());
#endif
		}
	}
	else if (foundController)
	{
		foundController = false;
#if WITH_EDITOR
		if (debug) UE_LOG(LogHand, Warning, TEXT("Lost the controller tracking owned by %s"), *GetName());
#endif
	}
}

void AVRHand::UpdateAnimationInstance()
{
	// Get the hand animation class and update animation variables.
	UHandsAnimInstance* handAnim = Cast<UHandsAnimInstance>(handSkel->GetAnimInstance());	
	if (handAnim)
	{
		handAnim->pointing = gripping;
		handAnim->fingerClosingAmount = (1 - trigger);
		handAnim->handClosingAmount = trigger * 100.0f;
	}
}

bool AVRHand::PlayFeedback(UHapticFeedbackEffect_Base* feedback, float intensity, bool replace)
{
	if (owningController)
	{
		// Check if should replace feedback effect. If don't replace, still playing and the new intensity is weaker don't play haptic effect.
		bool shouldPlay = replace ? true : (IsPlayingFeedback() ? GetCurrentFeedbackIntensity() < intensity : true);

		// If should play then play the given feedback haptic effect on this hand classes controller.
		if (shouldPlay)
		{
			// Play the given haptic effect if not nullptr.
			if (feedback)
			{
				currentHapticIntesity = intensity;
				owningController->PlayHapticEffect(feedback, handEnum, intensity * player->hapticIntensity, false);
				return true;
			}
			else return false;
		}
		else return false;
	}	
	else
	{
	    UE_LOG(LogHand, Log, TEXT("PlayFeedback: The feedback could not be played as the refference to the owning controller has been lost in the hand class %s."), *GetName());
		return false;
	}
}

float AVRHand::GetCurrentFeedbackIntensity()
{
	if (IsPlayingFeedback()) return currentHapticIntesity;
	else return 0.0f;
}

bool AVRHand::IsPlayingFeedback()
{
	bool isPlayingHapticEffect = false;
	if (owningController)
	{
		if (handEnum == EControllerHand::Left && owningController->ActiveHapticEffect_Left.IsValid()) isPlayingHapticEffect = true;
		else if (owningController->ActiveHapticEffect_Right.IsValid()) isPlayingHapticEffect = true;
	}

	// Return if this hand classes controller is playing a haptic effect.
	return isPlayingHapticEffect;
}

void AVRHand::Disable(bool disable)
{
	bool toggle = !disable;

	// Deactivate hand components.
	handSkel->SetActive(toggle);

	// Hide hand in game.
	handSkel->SetVisibility(toggle);

	// Deactivate hand colliders.
	if (toggle) handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	else handSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Disable this classes tick.
	this->SetActorTickEnabled(toggle);
	active = toggle;
}
