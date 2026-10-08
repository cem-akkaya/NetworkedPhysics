// Fill out your copyright notice in the Description page of Project Settings.


#include "LoaderPawn.h"
#include "LoaderSimComponent.h"
#include "WheeledVehiclePawn.h"
#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"

// Update constructor to call the correct initializer signature
// Use the exact FName the base class uses to ensure the override registers correctly
ALoaderPawn::ALoaderPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<ULoaderSimComponent>(TEXT("VehicleSimComponent0")))
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALoaderPawn::BeginPlay()
{
	Super::BeginPlay();
}

void ALoaderPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ALoaderPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

