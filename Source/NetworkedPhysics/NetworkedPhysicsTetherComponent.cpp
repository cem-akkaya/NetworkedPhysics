// Fill out your copyright notice in the Description page of Project Settings.

#include "NetworkedPhysicsTetherComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"


// Sets default values for this component's properties
UNetworkedPhysicsTetherComponent::UNetworkedPhysicsTetherComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UNetworkedPhysicsTetherComponent::BeginPlay()
{
	Super::BeginPlay();

	TryAttachActors();
	
}

void UNetworkedPhysicsTetherComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PhysicsTetherAsync)
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					FPhysicsTetherPhysics* Async = PhysicsTetherAsync;
					Solver->EnqueueCommandImmediate([Async]() { Async->DestroyJoint_Internal(); });
					Solver->UnregisterAndFreeSimCallbackObject_External(PhysicsTetherAsync);
				}
			}
		}
		PhysicsTetherAsync = nullptr;
	}
 
	Super::EndPlay(EndPlayReason);
}

void UNetworkedPhysicsTetherComponent::TryAttachActors()
{
	if (!FirstActorTag.IsNone() &&  !SecondActorTag.IsNone())
	{
		TArray<AActor*> AttachedActorsA;
		TArray<AActor*>  AttachedActorsB;
		
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FirstActorTag, AttachedActorsA);
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), SecondActorTag, AttachedActorsB);
		if (AttachedActorsA.Num() != 1 || AttachedActorsB.Num() != 1 || AttachedActorsA[0] == AttachedActorsB[0])
		{
			return;
		}
		
		AttachedActors.Append(AttachedActorsA);
		AttachedActors.Append(AttachedActorsB);
		
		if (AttachedActors.Num() == 2)
		{
			TArray<Chaos::FConstPhysicsObjectHandle> PhysicsBodies;
			 for (AActor* CurrentActor : AttachedActors)
			{
				if (UPrimitiveComponent* CurrentPrim = CurrentActor->GetComponentByClass<UPrimitiveComponent>())
				{
					if (auto CurrentBody = CurrentPrim->GetBodyInstance())
					{
						if (CurrentBody->IsValidBodyInstance())
						{
							CurrentBody->SetUpdateKinematicFromSimulation(true);
							
							if (auto PhysicsBody = CurrentPrim->GetPhysicsObjectByName(NAME_None))
							{
								PhysicsBodies.Add(PhysicsBody);
							}
						}
						

					}

				};
			}

			if (PhysicsBodies.Num() == 2 && PhysicsBodies[0] != PhysicsBodies[1])
			{
				if (UWorld* World = GetWorld())
				{
					if (FPhysScene* Scene = World->GetPhysicsScene())
					{
						if (auto Solver = Scene->GetSolver())
						{
							PhysicsTetherAsync = Solver->CreateAndRegisterSimCallbackObject_External<FPhysicsTetherPhysics>();
							if (PhysicsTetherAsync)
							{
								PhysicsTetherAsync->CurrentPhysicsObjectHandles = PhysicsBodies;
								PhysicsTetherAsync->TetherMaxForce = TetherMaxForce;
								PhysicsTetherAsync->TetherRestLength = TetherRestLength;
							}
						}
					}
				}
			}
		}
	}
}

void FPhysicsTetherPhysics::OnPreSimulate_Internal()
{
	if (CurrentPhysicsObjectHandles.IsEmpty()) 
	{
		return;
	}
	
	if (TetherConstraintHandle)
	{
		return;
	}

	// access the rigid particle, we need to get the write interface, then only we can access the particle
	Chaos::FWritePhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
	TArray<Chaos::FPBDRigidParticleHandle*> RigidHandles;

	for (auto CurrentRigidHandle : CurrentPhysicsObjectHandles)
	{
		Chaos::FPBDRigidParticleHandle* RigidHandle = Interface.GetRigidParticle(CurrentRigidHandle);
		
		if (!RigidHandle)
		{
			return;
		}
		RigidHandles.Add(RigidHandle);
		
		if (RigidHandles.Num() == 2)
		{
			Chaos::FParticlePair CurrentTetherPairs{RigidHandles[0], RigidHandles[1]};
			TetherPair = CurrentTetherPairs;
		}
		
	}
	
	Chaos::FPBDRigidsSolver* CurrentSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
	if (!CurrentSolver)
	{
		return;
	}
	if (TetherPair.Num() == 2)
	{
		Chaos::FPBDJointSettings JointSettings;
		JointSettings.LinearMotionTypes = {Chaos::EJointMotionType::Free, Chaos::EJointMotionType::Free, Chaos::EJointMotionType::Free};
		JointSettings.bLinearPositionDriveEnabled = {true, true, true};
		JointSettings.LinearDrivePositionTarget = TetherPair[1]->GetR().Inverse().RotateVector((TetherPair[0]->GetX() - TetherPair[1]->GetX()).GetSafeNormal() * TetherRestLength);
		JointSettings.LinearDriveStiffness = Chaos::FVec3(2000.0);
		JointSettings.LinearDriveDamping = Chaos::FVec3(20.0);
		JointSettings.LinearDriveMaxForce = Chaos::FVec3(10000000.0);
		
		TetherConstraintHandle = CurrentSolver->GetEvolution()->CreateJointConstraint(TetherPair, JointSettings);
	}
	
	//const int32 ServerFrame = CurrentSolver->GetCurrentFrame() + UE::NetworkPhysicsUtils::GetNetworkPhysicsTickOffset_Internal(GetSolver());
}

void FPhysicsTetherPhysics::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject)
{
	if (CurrentPhysicsObjectHandles.Contains(PhysicsObject))
	{
		DestroyJoint_Internal();
		CurrentPhysicsObjectHandles.Reset();
	}
}

void FPhysicsTetherPhysics::DestroyJoint_Internal()
{
	if (TetherConstraintHandle)
	{
		Chaos::FPBDRigidsSolver* CurrentSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
		if (CurrentSolver)
		{
			CurrentSolver->GetEvolution()->DestroyJointConstraint(TetherConstraintHandle);
			TetherConstraintHandle = nullptr;
		}
	}
}

