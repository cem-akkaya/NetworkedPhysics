// Fill out your copyright notice in the Description page of Project Settings.


#include "ForcePracticeActor.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

// Sets default values
AForcePracticeActor::AForcePracticeActor()
{
	// Since we don't need to call game thread tick disable.
	PrimaryActorTick.bCanEverTick = false;
	
	
	ForceCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	RootComponent = ForceCollisionBox;
	
	// We read the box's bounds ourselves, so it needs no collision.
	ForceCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForceCollisionBox->SetBoxExtent(FVector(300, 300, 300));
	
	// Let's show it ingame 
	ForceCollisionBox->SetVisibility(true);
	ForceCollisionBox->SetHiddenInGame(false);
	ForceCollisionBox->SetLineThickness(2.0f);
}

// Called when the game starts or when spawned
void AForcePracticeActor::BeginPlay()
{
	Super::BeginPlay();
	
	const FBoxSphereBounds BoxBounds = ForceCollisionBox->Bounds;

	// We get solver and bind.
	if (TryGetPhysicsSolver())
	{
		ForcePracticeAsync = TryGetPhysicsSolver()->CreateAndRegisterSimCallbackObject_External<FForcePracticeAsync>();
		
		if (ensure(ForcePracticeAsync))
		{
			// Hand over the properties to the physics thread, safe to do here inline after creation of the PT class
			// Unsafe if done elsewhere, in that case pass the properties via AsyncInput / AsyncOutput
			// Todo: check how this can be done in output and input.
			ForcePracticeAsync->Center = BoxBounds.Origin;
			ForcePracticeAsync->BoxExtent = BoxBounds.BoxExtent;
			ForcePracticeAsync->Force = ApplicationForce;
			ForcePracticeAsync->bHasData = true;
		}
	}
	
}

void AForcePracticeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Before end we unregister.
	if (ForcePracticeAsync && TryGetPhysicsSolver() )
	{
		TryGetPhysicsSolver()->UnregisterAndFreeSimCallbackObject_External(ForcePracticeAsync);
		ForcePracticeAsync = nullptr;
	}
	
	Super::EndPlay(EndPlayReason);
	
}

Chaos::FPhysicsSolver* AForcePracticeActor::TryGetPhysicsSolver() const
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			// Not FPhysSolver cause not sure.
			if (Chaos::FPhysicsSolver* CurrentSolver = PhysScene->GetSolver())
			{
				return CurrentSolver;
			}
		}
	}
	return nullptr;
}


void FForcePracticeAsync::OnPreSimulate_Internal()
{
	// simply each call turn gate, if not return.
	if (!bHasData)
	{
		return;
	}
	
	// make sure of solver ? kinda but why we get solver if we already have it.
	Chaos::FPBDRigidsSolver* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
	if (!RigidsSolver)
	{
		return;
	}
	
	// get the solver's evolution so we can access its physics particles. This is a collection then we have to select.
	Chaos::FPBDRigidsEvolutionGBF* Evolution = static_cast<Chaos::FPBDRigidsEvolutionGBF*>(RigidsSolver->GetEvolution());
	if (!Evolution)
	{
		return;
	}
	
	//build
	const Chaos::FAABB3 QueryBounds(Center - BoxExtent, Center + BoxExtent);
	const Chaos::FVec3 LocalForce = Force;

	Evolution->GetParticles().GetNonDisabledDynamicView().ParallelFor([&QueryBounds, &LocalForce](auto& Particle, int32 Index)
	{
		if (QueryBounds.Contains(Particle.GetX()))
		{
			Particle.AddForce(LocalForce, true);
		}
	});
	
	// Evolution->ForEachPhysicsParticle([&](Chaos::FPhysicsParticle* Particle)
	// {
	// 	if (QueryBounds.Intersects(Particle->GetAABB()))
	// 	{
	// 		Particle->AddForceAtLocation(LocalForce, Particle->GetLocation());
	// 	}
	// });
}

