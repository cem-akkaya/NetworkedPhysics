// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"
#include "LoaderSimComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NETWORKEDPHYSICS_API ULoaderSimComponent : public UModularVehicleBaseComponent
{
	GENERATED_BODY()

public:
	ULoaderSimComponent(const FObjectInitializer& ObjectInitializer);

	// Overridden to register our custom Arm module
	virtual void OnCreatePhysicsState() override;

protected:
	// Removed BeginPlay and TickComponent declarations since they aren't needed 
	// and were causing linker errors.
};
