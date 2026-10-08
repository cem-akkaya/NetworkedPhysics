// Fill out your copyright notice in the Description page of Project Settings.

#include "LoaderSimComponent.h"

ULoaderSimComponent::ULoaderSimComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULoaderSimComponent::OnCreatePhysicsState()
{
	// Custom Arm modules are now added automatically by the UVehicleSimArmComponent added in BP
	Super::OnCreatePhysicsState();
}
