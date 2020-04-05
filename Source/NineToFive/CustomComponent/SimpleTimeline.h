#pragma once
#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "Components/ActorComponent.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "SimpleTimeline.generated.h"

/* A time line helper class for creating easier to use timers. */
UCLASS()
class NINETOFIVE_API USimpleTimeline : public UActorComponent
{
	GENERATED_BODY()

public:

    /* Constructor. */
	USimpleTimeline();

	/* Time line used to morph the player */
	UTimelineComponent* timelineComponent;

	/* Initializes a new time line.
	 * @Param timelineCurve, The curve that the time line will use
	 * @Param timelineName, The name of the time line
	 * @Param propertySetObject, The object that the time line callbacks will be called to
	 * @Param callbackFunction, The function name to call for each tick of the time line
	 * @Param finishFunction, The function name to call at the end of the time line
	 * @Param owningActor, The actor that this time line will be attached to
	 * @Param timelineVariableName, The name of the variable located in the property set object that the time line will change if none = NAME_None
	 * @Param looping, Should the time line loop
	 * @Param timelineLength, The length of the time line can be the total length of the curve of between the key points. */
	UFUNCTION(BlueprintCallable, Category = "Objects", meta = (DeterminesOutputType = "ObjClass"))
		static USimpleTimeline* CreateNewTimeline(UCurveFloat* timelineCurve, FName timelineName, UObject* propertySetObject,
			FName callbackFunction, FName finishFunction, AActor* owningActor, FName timelineVariableName = NAME_None, bool looping = false,
			ETimelineLengthMode timelineLength = ETimelineLengthMode::TL_LastKeyFrame, TEnumAsByte<ETimelineDirection::Type> timelineDirection = ETimelineDirection::Forward);	

	/* Is the time line currently playing */
	bool IsPlaying() const;

	/* Is the timer playing in reverse? */
	bool IsReversing() const;

	/* Stop the time line and reset the playback location */
	void Stop();

	/* Stop the time line but do not reset the playback location */
	void Pause();

	/* Play the time line from the start */
	void PlayFromStart();

	/* Continue playing the time line */
	void PlayFromCurrentLocation();

	/* Play the time line in reverse */
	void Reverse();

	/* Sets the position of the time line */
	void SetPosition(int position, bool fireEvents = false, bool fireUpdateEvent = false);
};