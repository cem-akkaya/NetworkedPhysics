// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/ParticleHandleFwd.h"
#include "NetworkedPhysicsBouyancyLayer.generated.h"

class FBouyancyLayerPhysics;

UCLASS()
class NETWORKEDPHYSICS_API ANetworkedPhysicsBouyancyLayer : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANetworkedPhysicsBouyancyLayer();

	virtual void Destroyed() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	void ResolvePhysicsBody();

	UPROPERTY()
	UStaticMeshComponent* BouyancyLayerMeshComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouyancy Layer")
	float PerProbeForce = 100.0f;
	
	FBouyancyLayerPhysics* BouyancyLayerPhysicsAsync = nullptr;
};

struct FBouyancyLayerPhysicsInput : public Chaos::FSimCallbackInput
{
	void Reset() {}
};

struct FBouyancyLayerPhysicsOutput : public Chaos::FSimCallbackOutput
{
	void Reset() {}
};

class FBouyancyLayerPhysics : public Chaos::TSimCallbackObject<FBouyancyLayerPhysicsInput, FBouyancyLayerPhysicsOutput,(Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PhysicsObjectUnregister | Chaos::ESimCallbackOptions::ContactModification)>
{
	friend class ANetworkedPhysicsBouyancyLayer;
	
	Chaos::FConstPhysicsObjectHandle CurrentPhysicsObjectHandle = nullptr;
	float WaterSurfaceZ = 0.0f;
	float PerProbeForce = 0.0f;
	
	int32 CountSubmergedProbes(const Chaos::FPBDRigidParticleHandle* LayerBody, const Chaos::FPBDRigidParticleHandle* OtherBody) const;

	virtual void OnPreSimulate_Internal() override;
	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override;
	virtual void OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject) override;
};
