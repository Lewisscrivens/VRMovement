// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavigationData.h"
#include "NavQueryFilter.h"
#include "Globals.h"
#include "VRMovement.generated.h"

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogVRMovement, Log, All);

/* Declare classes used. */
class USceneComponent;
class USplineComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AVRPawn;
class AVRHand;
class APlayerController;
class USoundBase;

/* Different movement modes. */
UENUM(BlueprintType)
enum class EVRMovementMode : uint8
{
	Developer UMETA(Hidden),
	Teleport UMETA(DisplayName = "Teleport", ToolTip = "Use teleportation as current movement mode."),
	SpeedRamp UMETA(DisplayName = "SpeedRamp", ToolTip = "Point direction mode component in any given direction while placing thumb from bottom of the pad to the top where bottom is 0 speed and top is 100% speed."),
	Joystick UMETA(DisplayName = "Joystick", ToolTip = "Press thumb down on movement button and place on the area of the pad you want to move. Movement is relative to the current relative component selected in direction mode."),
	Lean UMETA(DisplayName = "Lean", ToolTip = "When the button is pressed your relative location is set and a ring will apear at your feet. Lean in any given direction for movement, this is relative to the cameras look direction."),
	SwingingArms UMETA(DisplayName = "SwingingArms", ToolTip = "Swinging the arms forward, then pressing the movement button and swinging the arms back move you in the direction you are dragging. Works well with swivel chair."),
};

/* Relative components to calculate current direction from. NOTE: Only supported on Speed ramp and joystick. */
UENUM(BlueprintType)
enum class EVRDirectionMode : uint8
{
	Camera UMETA(DisplayName = "Camera", ToolTip = "For any directional movement modes use the camera's look at direction to calculate relative directions."),
	Controller UMETA(DisplayName = "Controller", ToolTip = "For any directional movement modes use the current moving controllers look at direction to calculate relative directions."),
};

/* Developer input events. */
UENUM(BlueprintType)
enum class EVRInput : uint8
{
	Mouse,
	Scroll,
	ResetHands,
	HideLeft,
	HideRight,
};

/* The VRPawns Movement component class containing all virtual reality movement functionality.
 * NOTE: If movement mode is changed during runtime the SetupMovement function must be ran afterwards for the class to work correctly... */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType, hidecategories = (Rendering, Replication, Input, Actor, LOD, Cooking))
class NINETOFIVE_API AVRMovement : public AActor
{
	GENERATED_BODY()

public:	
	
	/* Scene component to act as root to any components used for movement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USceneComponent* scene;

	/* Spline used to place the procedural mesh along usually a cylinder with no top or bottom polygons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USplineComponent* teleportSpline;

	/* Mesh used to show location to teleport to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UStaticMeshComponent* teleportRing;

	/* Mesh used to indicate the direction inside of the teleport ring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UStaticMeshComponent* teleportArrow;

	/* Mesh to be placed at the end of the teleport spline. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UStaticMeshComponent* teleportSplineEndMesh;

	/* Mesh to be procedurally placed along the teleport spline. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UStaticMesh* teleportSplineMesh;

	/* Current type of movement mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	EVRMovementMode currentMovementMode;

	/* Relative components to calculate current direction from. NOTE: Only supported on Speed ramp and joystick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	EVRDirectionMode currentDirectionMode;

	/* Used to prevent the players movement input functions from working. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool canMove;

	/* Sound to play when teleporting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport")
	USoundBase* teleportSound;

	/* Array for the floor types that can be teleported onto . */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Teleport")
	TArray<TEnumAsByte<EObjectTypeQuery>> teleportableTypes;

	/* Array destination of the current agent properties from navigation system settings in the project settings... */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Teleport")
	int agentID = 1;

	/* Material color for invalid teleport location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Teleport")
	FLinearColor invalidTeleportColor;

	/* Material color for valid teleport location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Teleport")
	FLinearColor validTeleportColor;

	/* Camera fade on teleport to avoid motion sickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport")
	bool teleportFade;

	/* Teleport Fade color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport")
	FLinearColor teleportFadeColor;

	/* Amount of time for the camera fade to last when fading in and out during teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float cameraFadeTimeToLast;

	/* How far down from the end spline point to trace, to try and find the walkable object type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float teleportHeight;

	/* Size of the dead zone for the teleport rotation arrow. NOTE: Clamped between 0 and 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport", meta = (ClampMin = "0.0", UIMin = "0.6", ClampMax = "0.0", UIMax = "0.6"))
	float teleportDeadzone;

	/* Teleport ball velocity to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport", meta = (ClampMin = "0.0", UIMin = "5000.0", ClampMax = "0.0", UIMax = "5000.0"))
	float teleportDistance;

	/* Teleport ball amount effected by gravity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport", meta = (ClampMin = "-5000.0", UIMin = "0.0", ClampMax = "-5000.0", UIMax = "0.0"))
	float teleportGravity;

	/* How far in any given direction will the end location of the teleport arc try to find a valid nav-mesh location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport", meta = (ClampMin = "0.0", UIMin = "100.0", ClampMax = "0.0", UIMax = "100.0"))
	float teleportSearchDistance;

	/* Move direction is relative to the camera where as if this is false move direction will be relative to world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WalkingMovement")
	bool cameraMoveDirection;

	/* Offset of the hands from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DeveloperMovement")
	FVector devHandOffset;

	/* Amount forward and backward the hands can be translated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DeveloperMovement")
	float handMovementSpeed;

	/* Amount forward and backward the hands can be translated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WalkingMovement", meta = (ClampMin = "100.0", UIMin = "300.0", ClampMax = "100.0", UIMax = "300.0"))
	float walkingSpeed;

	/* Will the peripherals be darkened during walking movement to decrease motion sickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WalkingMovement")
	bool vignetteDuringMovement;

	/* Vignette transition speed between 1 and 0 on its material instances opacity scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WalkingMovement")
	float vignetteTransitionSpeed;

	/* Speed scale in which the vignette activates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WalkingMovement", meta = (ClampMin = "0.0", UIMin = "1.0", ClampMax = "0.0", UIMax = "1.0"))
	float minVignetteSpeed;

	/* Will the peripherals be darkened during walking movement to decrease motion sickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WalkingMovement")
	UMaterialInterface* vingetteMATInstance;

	/* Minimum offset from the center required to start movement, this is then checked against the max offset that determines speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|LeaningMovement", meta = (ClampMin = "0.0", ClampMax = "20.0", UIMin = "0.0", UIMax = "20.0"))
	float minMovementOffsetRadius;

	/* Max offset for leaning movement where the capsule and ring will be placed back at the players camera x/y location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|LeaningMovement", meta = (ClampMin = "30.0", ClampMax = "50.0", UIMin = "30.0", UIMax = "50.0"))
	float maxMovementOffsetRadius;

	/* Swinging arms speed scalar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|SwingingArms", meta = (ClampMin = "0.0", ClampMax = "30.0", UIMin = "0.0", UIMax = "30.0"))
	float swingingArmsSpeed;

	/* Swinging arms speed scalar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Teleport")
	bool requiresNavMesh;

	/* Sets how long to wait before trying to rebind inputs if we failed to bind. NOTE: Bug fix for simulate mode. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Developer")
	float InputRebindAttemptTimer;

	/* Reff to player controller. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	APlayerController* playerController;

	AVRPawn* player; /* Used across the class for accessing different variables for example the player capsule. */
	AVRHand* currentMovingHand; /* The current hand class that is activating movement. */

private:

	bool firstMove;// Used to determine the first frame of movement.
	bool inAir;// Is the player currently in the air.
	FVector originalMovementLocation;
	FVector lastMovementLocation;

	/////////////////////////////////////////////////
	//			    Teleporting Vars.			   //
	/////////////////////////////////////////////////

	bool lastTeleportValid;
	bool teleporting;
	float teleportWidth;
	FVector lastValidTeleportLocation;
	FRotator teleportRotation;
	TArray<class USplineMeshComponent*> splineMeshes;

	/////////////////////////////////////////////////
	//			     Vignette Vars.			       //
	/////////////////////////////////////////////////

	UMaterialInstanceDynamic* vignetteMAT;
	float lastVignetteOpacity;
	FTimerHandle vignetteTimer;
	bool canApplyVignette;

	/////////////////////////////////////////////////
	//			   Development Vars.			   //
	/////////////////////////////////////////////////

#if WITH_EDITOR
	/* Which hands are currently frozen in place. */
	bool leftFrozen, rightFrozen; 
#endif

public:	

	/* Constructor. */
	AVRMovement();

	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/////////////////////////////////////////////////
	//		  Movement or Other Functions.		   //
	/////////////////////////////////////////////////

	/* Movement begin play called from pawns begin play. */
	UFUNCTION(BlueprintCallable)
	void SetupMovement(AVRPawn* playerPawn, bool dev = false);

	/* Function to enable/disable the capsule collisions physics for gravity. */
	void EnableCapsule(bool enable = true);

	/* Function to update the different types of vr movement depending on current mode selected, also ran on release execute code on release. */
	void UpdateMovement(AVRHand* movementHand, bool released = false);

	/* Function to update while the teleport button is down. */
	void UpdateControllerMovement(AVRHand* movementHand);

	/* Function to interpolate vignette opacity back to 1.0 (invisible). */
	UFUNCTION(BlueprintCallable, Category = "WalkingMovement")
	void ResetVignette();

	/* Interpolates the vignettes opacity value stored in the vignetteMAT variable to a specified target. */
	void LerpVignette(float target);

	/////////////////////////////////////////////////
	//			Teleporting Functions.			   //
	/////////////////////////////////////////////////

	/* Function to update while the teleport button is down. */
	void UpdateTeleport(AVRHand* movementHand);

	/* Returns weather or not the teleport spline has hit anything, also updated outLocation. */
	bool CreateTeleportSpline(FTransform startTransform, FVector& outLocation);

	/* Removes all spline meshes and hides any teleport components. */
	void DestroyTeleportSpline();

	/* Check if area is a valid teleport location. */
	bool ValidateTeleportLocation(FVector& location);

	/* Change the materials of all the teleport meshes. */
	void UpdateTeleportMaterials(bool valid);

	/* Fade the camera out -> teleport -> fade back in. */
	void TeleportCameraFade();

	/* Function ran if the last teleport location is valid and the teleport button has been released. Is binded so needs UFUNCTION. */
	UFUNCTION(Category = "Teleport")
	void TeleportPlayer();

	/////////////////////////////////////////////////
	//			Engine Only Functions.			   //
	/////////////////////////////////////////////////

#if WITH_EDITOR
	/* Setup the controls for developer movement. */
	UFUNCTION()
	void SetupDeveloperMovement();

	/* Template function for developer input. */
	template<EVRInput val> void DeveloperInput() { DeveloperInput(val); }
	UFUNCTION(Category = Developer)
	void DeveloperInput(EVRInput input);

	/* Attach the hands to the camera of the pawn so the hands follow the Developer movement. Also used to re-connect hands after them being frozen. */
	void AttatchHand(AVRHand* handToAttach);

	/* Reset level function for developer mode. */
	void ResetLevel();

	/* Returns true or false for a key being down. NOTE: Only works on axis bindings. */
	bool IsKeyDown(FName key);

	/* Input function for any development movement. */
	void UpdateDeveloperMovement(float deltaTime);	
#endif
};

