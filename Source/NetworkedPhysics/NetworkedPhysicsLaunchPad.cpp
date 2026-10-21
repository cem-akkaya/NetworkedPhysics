// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkedPhysicsLaunchPad.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"

// Sets default values
ANetworkedPhysicsLaunchPad::ANetworkedPhysicsLaunchPad()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	LaunchPadMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaunchPadMeshComponent"));
	SetRootComponent(LaunchPadMeshComponent);
	LaunchPadMeshComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	LaunchPadMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	LaunchPadMeshComponent->SetSimulatePhysics(false);
	LaunchPadMeshComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	LaunchPadMeshComponent->SetWorldScale3D(FVector(3.0f,3.0f,0.5f));
}

// Called when the game starts or when spawned
void ANetworkedPhysicsLaunchPad::BeginPlay()
{
	Super::BeginPlay();
	
	ResolvePhysicsBody();
}

void ANetworkedPhysicsLaunchPad::Destroyed()
{
	Super::Destroyed();

	if (LaunchpadPhysicsAsync)
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(LaunchpadPhysicsAsync);
				}
			}
		}
		LaunchpadPhysicsAsync = nullptr;
	}
}

void ANetworkedPhysicsLaunchPad::ResolvePhysicsBody()
{
	if (auto CurrentBody = LaunchPadMeshComponent->GetBodyInstance())
	{
		if (CurrentBody->IsValidBodyInstance())
		{
			CurrentBody->SetUpdateKinematicFromSimulation(true);
							
			if (Chaos::FPhysicsObject* PhysicsBody = LaunchPadMeshComponent->GetPhysicsObjectByName(NAME_None))
			{
				if (UWorld* World = GetWorld())
				{
					if (FPhysScene* Scene = World->GetPhysicsScene())
					{
						if (auto Solver = Scene->GetSolver())
						{
							LaunchpadPhysicsAsync = Solver->CreateAndRegisterSimCallbackObject_External<FLaunchpadPhysics>();
							if (LaunchpadPhysicsAsync)
							{
								LaunchpadPhysicsAsync->CurrentPhysicsObjectHandle = PhysicsBody;
								LaunchpadPhysicsAsync->LaunchPadVelocity = LaunchPadVelocity;
							}
						}
					}
				}
			}
		}
	}
}

// we have to register the callback object to the solver Chaos::ESimCallbackOptions::ContactModification this works only if the callback object is registered to the solver
void FLaunchpadPhysics::OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier)
{
	//For a physics collision manual detection we have to collide first, then when a collision happens we have to check if the collision is with the launch pad
	if (!CurrentPhysicsObjectHandle) 
	{
		return;
	}

	Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite(); 
	Chaos::FPBDRigidParticleHandle* RigidHandle = WriteInterface.GetRigidParticle(CurrentPhysicsObjectHandle); 
	
	if (!RigidHandle)
	{
		return;
	}
	
	Chaos::FContactPairModifierParticleRange CurrentContactCandidates = Modifier.GetContacts(RigidHandle);

	for (auto CurrentContactCandidate : CurrentContactCandidates)
	{
		auto ContactCount =  CurrentContactCandidate.GetNumContacts();
		if (ContactCount > 0)
		{
			Chaos::FGeometryParticleHandle* CurrentCandidate = CurrentContactCandidate.GetOtherParticle(RigidHandle);
			if (CurrentCandidate)
			{
				if (auto OtherBody = CurrentCandidate->CastToRigidParticle())
				{
					//Then we can launch it
					if (OtherBody->IsDynamic())
					{
						// for single impulse
						if (CurrentContactCandidate.GetIsInitialContact())
						{
							const auto ParticlePair = CurrentContactCandidate.GetParticlePair();
							int32 OtherIndex = 0;
							if (ParticlePair[0] != CurrentCandidate)
							{
								OtherIndex = 1;
							}
							CurrentContactCandidate.ModifyParticleVelocity(LaunchPadVelocity, OtherIndex);
						}
					}
				}
			
			}
		}
	}

	

}

void FLaunchpadPhysics::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject)
{
	if (CurrentPhysicsObjectHandle == PhysicsObject)
	{
		CurrentPhysicsObjectHandle = nullptr;
	}
}


void FLaunchpadPhysics::OnPreSimulate_Internal()
{
	
}	


