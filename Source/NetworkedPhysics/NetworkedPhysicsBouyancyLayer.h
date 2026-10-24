// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/ParticleHandleFwd.h"
#include "NetworkedPhysicsWaterGridData.h"
#include "NetworkedPhysicsBouyancyLayer.generated.h"

class FBouyancyLayerPhysics;

UCLASS()
class NETWORKEDPHYSICS_API ANetworkedPhysicsBouyancyLayer : public AActor
{
	GENERATED_BODY()

public:
	ANetworkedPhysicsBouyancyLayer();

	UFUNCTION(CallInEditor, Category = "Bouyancy Layer|Fluid")
	void BakeWaterGrid();

	virtual void Destroyed() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = "Bouyancy Layer|Fluid")
	FVector LayerDimensions = FVector(500.0f, 500.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouyancy Layer|Fluid", meta = (ClampMin = "1", DisplayName = "Probes Per Axis"))
	int32 LayerGridDimension = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bouyancy Layer|Fluid")
	TObjectPtr<UNetworkedPhysicsWaterGridData> WaterGridData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouyancy Layer|Probes")
	float PerProbeForce = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouyancy Layer|Probes", meta = (ClampMin = "0.0"))
	float MaxForcePerObject = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouyancy Layer|Probes", meta = (ClampMin = "1", DisplayName = "Probes Per Axis"))
	int32 ForceGridDimension = 9;


protected:
	virtual void BeginPlay() override;
	
	void ResolvePhysicsBody();

	UPROPERTY()
	UBoxComponent* BouyancyLayerCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bouyancy Layer")
	UStaticMeshComponent* BouyancyLayerVisualComponent;

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

class FBouyancyLayerPhysics : public Chaos::TSimCallbackObject<FBouyancyLayerPhysicsInput, FBouyancyLayerPhysicsOutput,(Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PhysicsObjectUnregister | Chaos::ESimCallbackOptions::ContactModification | Chaos::ESimCallbackOptions::Rewind)>
{
	friend class ANetworkedPhysicsBouyancyLayer;
	
	Chaos::FConstPhysicsObjectHandle CurrentPhysicsObjectHandle = nullptr;
	float PerProbeForce = 0.0f;
	float MaxForcePerObject = 0.0f;
	float GridSize = 100.0f;
	int32 ForceGridDimension = 9;
	FVector LayerDimensions = FVector::ZeroVector;
	int32 LayerGridDimension = 0;
	TArray<FBouyancyWaterGridCell> WaterGridCells;
	
	// Contacts found after integration are applied as forces on the next physics step.
	TSet<Chaos::FPBDRigidParticleHandle*> BodiesToBuoy;
	
	void ApplySubmergedProbeForces(const Chaos::FPBDRigidParticleHandle* LayerBody, Chaos::FPBDRigidParticleHandle* OtherBody) const;

	virtual void OnPreSimulate_Internal() override;
	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override;
	virtual void OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject) override;
	virtual void FirstPreResimStep_Internal(int32 PhysicsStep) override;
};
