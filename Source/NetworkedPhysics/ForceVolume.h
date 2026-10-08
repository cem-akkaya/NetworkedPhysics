// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/SimCallbackObject.h"

#include "ForceVolume.generated.h"

class FForceVolumeAsync;

/**
*	A volume applying a per-frame force to any dynamic body inside it, on the Physics Thread.
*
*	Setup: a shape as the root component (a Box is natural) plus a Force vector. The volume is read from that
*	component's world bounds, so resizing the box is all that is needed.
*/
UCLASS()
class NETWORKEDPHYSICS_API AForceVolume : public AActor
{
	GENERATED_BODY()

public:
	AForceVolume();

protected:
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Force (cm/s^2 acceleration vector) applied to every dynamic body inside the volume each frame. */
	UPROPERTY(EditAnywhere, Category = "ForceVolume")
	FVector Force = FVector(0.0f, 0.0f, 200000.0f);

	FForceVolumeAsync* VolumeAsync = nullptr;
};

// --------------------------------------------------------------------------------------------------
// Physics Thread
// --------------------------------------------------------------------------------------------------

// Nothing is marshaled between GT and PT, leave AsyncInput empty
struct FAsyncInputForceVolume : public Chaos::FSimCallbackInput
{
	void Reset() {}
};

// Nothing is marshaled between GT and PT, leave AsyncOutput empty
struct FAsyncOutputForceVolume : public Chaos::FSimCallbackOutput
{
	void Reset() {}
};

// Physics Thread class listening to the PreSimulate callback
class FForceVolumeAsync : public Chaos::TSimCallbackObject<
	FAsyncInputForceVolume,
	FAsyncOutputForceVolume,
	(Chaos::ESimCallbackOptions::Presimulate)>
{
	friend AForceVolume;

	~FForceVolumeAsync() {}

	virtual void OnPreSimulate_Internal() override;

	FVector Center = FVector::ZeroVector;
	FVector BoxExtent = FVector::ZeroVector;
	FVector Force = FVector::ZeroVector;
	bool bHasData = false;
};
