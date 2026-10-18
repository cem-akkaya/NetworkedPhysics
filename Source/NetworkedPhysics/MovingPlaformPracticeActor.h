// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "MovingPlaformPracticeActor.generated.h"

class FMovingPlatformPhysics;

UCLASS()
class NETWORKEDPHYSICS_API AMovingPlaformPracticeActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMovingPlaformPracticeActor();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform")
	float  MovingPlatformDeltaDistance  = 500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform")
	FVector MovingPlatformMoveDirection = FVector(0.0f,0.0f,1.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform")
	float MovingPlatformMoveSpeed = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform")
	UStaticMesh* MovingPlatformMesh;
	
	virtual void PostInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UStaticMeshComponent* MovingPlatformMeshComponent;
	
	FMovingPlatformPhysics* MovingPlatformAsync = nullptr;
};

struct FMovingPlatformInputPhysics : public Chaos::FSimCallbackInput
{
	void Reset() {}
};
 
struct FMovingPlatformOutputPhysics : public Chaos::FSimCallbackOutput
{
	void Reset() {}
};
 

class FMovingPlatformPhysics : public Chaos::TSimCallbackObject<FMovingPlatformInputPhysics, FMovingPlatformOutputPhysics,(Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PhysicsObjectUnregister)>
{
	friend AMovingPlaformPracticeActor;
	
	float DistanceTraveled = 0.0f;
	int OscillationDirection = 1;
	float MovingPlatformMoveSpeed = 0.0f;
	float  MovingPlatformDeltaDistance  = 0.0f;
	FVector MovingPlatformMoveDirection = FVector::ZeroVector;
	FVector StartLocation = FVector::ZeroVector;

	
	Chaos::FConstPhysicsObjectHandle CurrentPhysicsObjectHandle  = nullptr;
	
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject) override;
};

