// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ConstraintHandle.h"
#include "Components/ActorComponent.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PBDJointConstraintTypes.h"
#include "NetworkedPhysicsTetherComponent.generated.h"

class FPhysicsTetherPhysics;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NETWORKEDPHYSICS_API UNetworkedPhysicsTetherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UNetworkedPhysicsTetherComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type EndPlayReason);

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networked Physics")
	FName FirstActorTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networked Physics")
	FName SecondActorTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networked Physics")
	float TetherRestLength = 500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networked Physics")
	float TetherMaxForce = 100.0f;

private:
	
	UPROPERTY()
	TArray<AActor*> AttachedActors;
	
	UFUNCTION()
	void TryAttachActors();

	FPhysicsTetherPhysics* PhysicsTetherAsync = nullptr;
};

struct FPhysicsTetherInputPhysics : public Chaos::FSimCallbackInput
{
	void Reset() {}
};
 
struct FPhysicsTetherOutputPhysics : public Chaos::FSimCallbackOutput
{
	void Reset() {}
};
 

class FPhysicsTetherPhysics : public Chaos::TSimCallbackObject<FPhysicsTetherInputPhysics, FPhysicsTetherOutputPhysics,(Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PhysicsObjectUnregister)>
{
	friend UNetworkedPhysicsTetherComponent;
	
	float TetherRestLength = 0.0f;
	float TetherMaxForce = 0.0f;
	float TetherCurrentLength = 0.0f;
	
	TArray<Chaos::FConstPhysicsObjectHandle> CurrentPhysicsObjectHandles;
	Chaos::FParticlePair TetherPair;
	
	Chaos::FPBDJointConstraintHandle* TetherConstraintHandle = nullptr;
	
	void DestroyJoint_Internal();
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject) override;
};
