// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosModularVehicle/ModularVehicleClusterPawn.h"
#include "LoaderPawn.generated.h"

UCLASS()
class NETWORKEDPHYSICS_API ALoaderPawn : public AModularVehicleClusterPawn
{
	GENERATED_BODY()

public:
	ALoaderPawn(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
