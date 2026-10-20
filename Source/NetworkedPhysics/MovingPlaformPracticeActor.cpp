// Fill out your copyright notice in the Description page of Project Settings.
#include "MovingPlaformPracticeActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"


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
							MovingPlatformAsync->StartLocation = GetActorLocation();
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

	Chaos::FPBDRigidsSolver* CurrentSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
	if (!CurrentSolver)
	{
		return;
	}

	const int32 ServerFrame = CurrentSolver->GetCurrentFrame() + UE::NetworkPhysicsUtils::GetNetworkPhysicsTickOffset_Internal(GetSolver());
	const float CurrentStep = MovingPlatformMoveSpeed * GetSolver()->GetAsyncDeltaTime();
	DistanceTraveled = FMath::Fmod(ServerFrame * CurrentStep, 2.0f * MovingPlatformDeltaDistance);
	OscillationDirection = 1;
	if (DistanceTraveled >= MovingPlatformDeltaDistance)
	{
		DistanceTraveled -= MovingPlatformDeltaDistance;
		OscillationDirection *= -1;
	}

	float DistanceFromStart = DistanceTraveled;
	if (OscillationDirection < 0)
	{
		DistanceFromStart = MovingPlatformDeltaDistance - DistanceTraveled;
	}

	const Chaos::FVec3 TargetPosition = StartLocation + MovingPlatformMoveDirection * DistanceFromStart;
	const Chaos::FKinematicTarget DestinationTarget = Chaos::FKinematicTarget::MakePositionTarget(TargetPosition, RigidHandle->GetR());
	CurrentSolver->GetEvolution()->SetParticleKinematicTarget(RigidHandle, DestinationTarget);
}

void FMovingPlatformPhysics::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject)
{
	
}
