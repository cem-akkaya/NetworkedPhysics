// Copyright Epic Games, Inc. All Rights Reserved.

#include "ForceVolume.h"

#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

AForceVolume::AForceVolume()
{
	// No need to tick on the game thread
	PrimaryActorTick.bCanEverTick = false;
	
	// No networking needed
	bReplicates = false;
}

void AForceVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(GetRootComponent());
	if (!Root)
	{
		return;
	}

	// Bounds is the component's world AABB, encapsulating the component.
	const FBoxSphereBounds RootBounds = Root->Bounds;

	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				// Create physics thread class 
				VolumeAsync = Solver->CreateAndRegisterSimCallbackObject_External<FForceVolumeAsync>();

				if (ensure(VolumeAsync))
				{
					// Hand over the properties to the physics thread, safe to do here inline after creation of the PT class
					// Unsafe if done elsewhere, in that case pass the properties via AsyncInput / AsyncOutput
					VolumeAsync->Center = RootBounds.Origin;
					VolumeAsync->BoxExtent = RootBounds.BoxExtent;
					VolumeAsync->Force = Force;
					VolumeAsync->bHasData = true;
				}
			}
		}
	}
}

void AForceVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (VolumeAsync)
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(VolumeAsync);
				}
			}
		}
		VolumeAsync = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// --------------------------------------------------------------------------------------------------
// FForceVolumeAsync (Physics Thread)
// --------------------------------------------------------------------------------------------------

void FForceVolumeAsync::OnPreSimulate_Internal()
{
	if (!bHasData)
	{
		return;
	}
 
	Chaos::FPBDRigidsSolver* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
	if (!RigidsSolver)
	{
		return;
	}
 
	Chaos::FPBDRigidsEvolutionGBF* Evolution = static_cast<Chaos::FPBDRigidsEvolutionGBF*>(RigidsSolver->GetEvolution());
	if (!Evolution)
	{
		return;
	}
 
	// Create an AABB to check if other particles are inside of if
	const Chaos::FAABB3 QueryBounds(Center - BoxExtent, Center + BoxExtent);
	const Chaos::FVec3 LocalForce = Force;
 
	// Iterate over all non-disabled dynamic physics particles (objects) in parallel
	Evolution->GetParticles().GetNonDisabledDynamicView().ParallelFor([&QueryBounds, &LocalForce](auto& Particle, int32 Index)
	{
		if (QueryBounds.Contains(Particle.GetX()))
		{
			// If within the AABB bounds, apply a force to the particle
			Particle.AddForce(LocalForce, true);
		}
	});
}
