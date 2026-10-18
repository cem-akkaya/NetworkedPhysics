// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"
#include "MovingPlaformPracticeActor.h"


// Sets default values
AMovingPlaformPracticeActor::AMovingPlaformPracticeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	MovingPlatformMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovingPlatformMesh"));

	SetRootComponent(MovingPlatformMeshComponent);

}

void AMovingPlaformPracticeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (MovingPlatformMesh)
	{
		MovingPlatformMeshComponent->SetStaticMesh(MovingPlatformMesh);
		MovingPlatformMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

// Called when the game starts or when spawned
void AMovingPlaformPracticeActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMovingPlaformPracticeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MovingPlatformAsync)
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(MovingPlatformAsync);
				}
			}
		}
		MovingPlatformAsync = nullptr;
	}
 
	Super::EndPlay(EndPlayReason);
}

void AMovingPlaformPracticeActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	if (auto CurrentBody =  Cast<UPrimitiveComponent>(GetRootComponent())->GetBodyInstance())
	{
		CurrentBody->SetUpdateKinematicFromSimulation(true);
		if (auto PhysicsBody = MovingPlatformMeshComponent->GetPhysicsObjectByName(NAME_None))
		{
			if (UWorld* World = GetWorld())
			{
				if (FPhysScene* Scene = World->GetPhysicsScene())
				{
					if (auto Solver = Scene->GetSolver())
					{
						MovingPlatformAsync = Solver->CreateAndRegisterSimCallbackObject_External<FMovingPlatformPhysics>();
						
						// Now finally we can give handle
						if (MovingPlatformAsync)
						{
							MovingPlatformAsync->CurrentPhysicsObjectHandle = PhysicsBody;
							MovingPlatformAsync->MovingPlatformMoveSpeed = MovingPlatformMoveSpeed;
							MovingPlatformAsync->MovingPlatformDeltaDistance = MovingPlatformDeltaDistance;
							MovingPlatformAsync->MovingPlatformMoveDirection = MovingPlatformMoveDirection.GetSafeNormal();
						}
	
					}
				}
			}
		}
	}
}

void FMovingPlatformPhysics::OnPreSimulate_Internal()
{
	if (!CurrentPhysicsObjectHandle) // this is my object
	{
		return;
	}

	// access the rigid particle, we need to get the write interface, then only we can access the particle. We get the interface, then we get our particle handle, what a confusing process
	Chaos::FWritePhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetWrite(); // This is the write interface
	Chaos::FPBDRigidParticleHandle* RigidHandle = Interface.GetRigidParticle(CurrentPhysicsObjectHandle); // Find the body particle from the handle object we have
	if (!RigidHandle)
	{
		return;
	}



	if (MovingPlatformDeltaDistance <= UE_SMALL_NUMBER || MovingPlatformMoveSpeed <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float CurrentStep = MovingPlatformMoveSpeed * GetDeltaTime_Internal();
	const float PreviousDistanceTraveled = DistanceTraveled;
	DistanceTraveled = FMath::Min(DistanceTraveled + CurrentStep, MovingPlatformDeltaDistance);
	const float EasedStep = 0.5f * MovingPlatformDeltaDistance * (FMath::Cos(PI * PreviousDistanceTraveled / MovingPlatformDeltaDistance) - FMath::Cos(PI * DistanceTraveled / MovingPlatformDeltaDistance));

	const Chaos::FVec3 TargetPosition = RigidHandle->GetX() + MovingPlatformMoveDirection * EasedStep * OscillationDirection;
	const Chaos::FKinematicTarget DestinationTarget = Chaos::FKinematicTarget::MakePositionTarget(TargetPosition, RigidHandle->GetR());
	
	Chaos::FPBDRigidsSolver* CurrentSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
	CurrentSolver->GetEvolution()->SetParticleKinematicTarget(RigidHandle, DestinationTarget);
	
	if (DistanceTraveled >= MovingPlatformDeltaDistance)
	{
		DistanceTraveled = 0.0f;
		OscillationDirection *= -1;
	}
	
}

void FMovingPlatformPhysics::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject)
{
	
}
