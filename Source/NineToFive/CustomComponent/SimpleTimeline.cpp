// Fill out your copyright notice in the Description page of Project Settings.

#include "CustomComponent/SimpleTimeline.h"
#include "GameFramework/Actor.h"


USimpleTimeline::USimpleTimeline()
{
}

USimpleTimeline* USimpleTimeline::MAKE(UCurveFloat * timelineCurve, FName timelineName, UObject * propertySetObject, FName callbackFunction, FName finishFunction, AActor * owningActor, FName timelineVariableName, bool looping, ETimelineLengthMode timelineLength, TEnumAsByte<ETimelineDirection::Type> timelineDirection)
{
	if (timelineCurve)
	{
		FString name = timelineName.ToString().Append("_TimelineAnimatior");

		USimpleTimeline* timeline = NewObject<USimpleTimeline>(propertySetObject, timelineName);
		timeline->timelineComponent = NewObject<UTimelineComponent>(propertySetObject, FName(*name));

		//Time line 
		FOnTimelineFloat onTimelineCallback;
		FOnTimelineEventStatic onTimelingFinishCallback;

		//Get the created time line
		UTimelineComponent* timelineComponent = timeline->timelineComponent;
		timelineComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;

		//Add the owning actor to the time line
		owningActor->BlueprintCreatedComponents.Add(timelineComponent);

		//Set Time line Components
		timelineComponent->SetPropertySetObject(propertySetObject);
		timelineComponent->SetLooping(looping);
		timelineComponent->SetTimelineLengthMode(timelineLength);
		timelineComponent->SetPlaybackPosition(0.0f, false, false);

		onTimelineCallback.BindUFunction(propertySetObject, callbackFunction);
		onTimelingFinishCallback.BindUFunction(propertySetObject, finishFunction);

		timelineComponent->AddInterpFloat(timelineCurve, onTimelineCallback, timelineVariableName);
		timelineComponent->SetTimelineFinishedFunc(onTimelingFinishCallback);

		timelineComponent->RegisterComponent();

		return timeline;
	}
	return nullptr;
}


bool USimpleTimeline::IsPlaying() const
{
	return timelineComponent->IsPlaying();
}

bool USimpleTimeline::IsReversing() const
{
	return timelineComponent->IsReversing();
}

void USimpleTimeline::Stop()
{
	timelineComponent->Stop();
	timelineComponent->SetPlaybackPosition(0.0f, false, false);
}

void USimpleTimeline::Pause()
{
	timelineComponent->Stop();
}

void USimpleTimeline::PlayFromStart()
{
	timelineComponent->PlayFromStart();
}

void USimpleTimeline::PlayFromCurrentLocation()
{
	timelineComponent->Play();
}

void USimpleTimeline::Reverse()
{
	timelineComponent->Reverse();
}

void USimpleTimeline::SetPosition(int position, bool fireEvents, bool fireUpdateEvent)
{
	timelineComponent->SetPlaybackPosition(position, fireEvents, fireUpdateEvent);
}