// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/Declares.h"
#include "Components/BoxComponent.h"
#include "Chaos/SimCallbackObject.h"
#include "ForcePracticeActor.generated.h"


// Game to Physics inputs. Empty for now.
struct FForcePracticeInput : public Chaos::FSimCallbackInput
{
	void Reset() {}
};

// Physics to Game outputs. Nothing to reset yet.
struct FForcePracticeOutput : public Chaos::FSimCallbackOutput
{
	void Reset() {}
};

// Name the callback here so the actor can hold its pointer.
class FForcePracticeAsync;

// Game Thread actor with the box and force settings.
UCLASS()
class NETWORKEDPHYSICS_API AForcePracticeActor : public AActor
{
	GENERATED_BODY()

public:
	// Set up the box and turn off Game Thread ticking.
	AForcePracticeActor();

	// Box marks the area where we apply force.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Force")
	UBoxComponent* ForceCollisionBox;

	// Force we can change in the actor's Details panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Force")
	FVector ApplicationForce = FVector(0.0, 0.0, 1000000);

protected:
	// Set up the actor when play starts.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	Chaos::FPhysicsSolver* TryGetPhysicsSolver() const;
private:
	// Keep the callback pointer so we can unregister it later.
	FForcePracticeAsync* ForcePracticeAsync = nullptr;

};


// Physics Thread class. Chaos runs it before each physics step once registered.
class FForcePracticeAsync : public Chaos::TSimCallbackObject<FForcePracticeInput, FForcePracticeOutput,(Chaos::ESimCallbackOptions::Presimulate)>
{
	// Called before a physics step.
	void  OnPreSimulate_Internal() override;
	~FForcePracticeAsync() {}
	
	//  himmm thats cool
	friend AForcePracticeActor;
	
	// Declare the properties we want to pass to the physics thread.
	FVector Center = FVector::ZeroVector;
	FVector BoxExtent = FVector::ZeroVector;
	FVector Force = FVector::ZeroVector;
	bool bHasData = false;
};
