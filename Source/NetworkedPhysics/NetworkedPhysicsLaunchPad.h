// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NetworkedPhysicsLaunchPad.generated.h"

class FLaunchpadPhysics;

UCLASS()
class NETWORKEDPHYSICS_API ANetworkedPhysicsLaunchPad : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANetworkedPhysicsLaunchPad();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networked Physics")
	FVector LaunchPadVelocity = FVector(0.0f,0.0f,10000.0f);

	virtual void Destroyed() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	void ResolvePhysicsBody();

	UPROPERTY()
	UStaticMeshComponent* LaunchPadMeshComponent;
	
	FLaunchpadPhysics* LaunchpadPhysicsAsync = nullptr;
};

struct FLaunchpadPhysicsInput : public Chaos::FSimCallbackInput
{
	void Reset() {}
};

struct FLaunchpadPhysicsOutput : public Chaos::FSimCallbackOutput
{
	void Reset() {}
};

class FLaunchpadPhysics : public Chaos::TSimCallbackObject<FLaunchpadPhysicsInput, FLaunchpadPhysicsOutput,(Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PhysicsObjectUnregister | Chaos::ESimCallbackOptions::ContactModification)>
{
	friend class ANetworkedPhysicsLaunchPad;
	
	Chaos::FConstPhysicsObjectHandle CurrentPhysicsObjectHandle;
	
	FVector LaunchPadVelocity = FVector(0.0f,0.0f,0.0f);
	
	virtual void OnPreSimulate_Internal() override;
	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override;
	virtual void OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject) override;
};