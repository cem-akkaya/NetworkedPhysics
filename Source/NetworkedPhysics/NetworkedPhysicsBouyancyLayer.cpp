// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkedPhysicsBouyancyLayer.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/ContactModification.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/DebugDrawQueue.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"

void FBouyancyLayerPhysics::ApplySubmergedProbeForces(const Chaos::FPBDRigidParticleHandle* LayerBody, Chaos::FPBDRigidParticleHandle* OtherBody) const
{
	const Chaos::FReal CellSize = GridSize / GridDimension;
	const Chaos::FImplicitObject* LayerShape = LayerBody->GetGeometry();
	const Chaos::FImplicitObject* OtherShape = OtherBody->GetGeometry();
	if (!LayerShape || !OtherShape)
	{
		return;
	}

	const Chaos::FRigidTransform3 LayerTransform = LayerBody->GetTransformXR();
	const Chaos::FRigidTransform3 OtherTransform = OtherBody->GetTransformXR();
	const Chaos::FVec3 ProbeForce(0.0, 0.0, PerProbeForce);
	const Chaos::FVec3 CenterOfMass = OtherBody->XCom();
	#if CHAOS_DEBUG_DRAW
	const bool bDrawProbes = Chaos::FDebugDrawQueue::IsDebugDrawingEnabled();
	#endif

	for (int32 X = 0; X < GridDimension; ++X)
	{
		for (int32 Y = 0; Y < GridDimension; ++Y)
		{
			for (int32 Z = 0; Z < GridDimension; ++Z)
			{
				const Chaos::FVec3 WorldPoint = OtherBody->GetX() + Chaos::FVec3(
					(X + 0.5) * CellSize - GridSize * 0.5,
					(Y + 0.5) * CellSize - GridSize * 0.5,
					(Z + 0.5) * CellSize - GridSize * 0.5);
				const bool bSubmerged = WorldPoint.Z <= WaterSurfaceZ &&
					LayerShape->Overlap(LayerTransform.InverseTransformPosition(WorldPoint), 0.0) &&
					OtherShape->Overlap(OtherTransform.InverseTransformPosition(WorldPoint), 0.0);
				#if CHAOS_DEBUG_DRAW
				if (bDrawProbes)
				{
					FColor ProbeColor = FColor::Red;
					if (bSubmerged)
					{
						ProbeColor = FColor::Green;
					}
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(WorldPoint, ProbeColor, false, 0.05f, 1, 2.0f);
				}
				#endif
				if (bSubmerged)
				{
					// add force
					OtherBody->AddForce(ProbeForce, true);
					OtherBody->AddTorque(Chaos::FVec3::CrossProduct(WorldPoint - CenterOfMass, ProbeForce), true);
				}
			}
		}
	}

}

// Sets default values
ANetworkedPhysicsBouyancyLayer::ANetworkedPhysicsBouyancyLayer()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	BouyancyLayerMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BouyancyLayerMeshComponent"));
	SetRootComponent(BouyancyLayerMeshComponent);
	BouyancyLayerMeshComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BouyancyLayerMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BouyancyLayerMeshComponent->SetSimulatePhysics(false);
	BouyancyLayerMeshComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	BouyancyLayerMeshComponent->SetWorldScale3D(FVector(50.0f,50.0f,25.0f));
	BouyancyLayerMeshComponent->SetMaxDepenetrationVelocity(NAME_None, 0.0f);
	BouyancyLayerMeshComponent->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Engine/ArtTools/RenderToTexture/Materials/M_RadiusDebugTranslucent.M_RadiusDebugTransLucent")));
}

// Called when the game starts or when spawned
void ANetworkedPhysicsBouyancyLayer::BeginPlay()
{
	Super::BeginPlay();
	
	ResolvePhysicsBody();
}

void ANetworkedPhysicsBouyancyLayer::Destroyed()
{
	Super::Destroyed();

	if (BouyancyLayerPhysicsAsync)
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(BouyancyLayerPhysicsAsync);
				}
			}
		}
		BouyancyLayerPhysicsAsync = nullptr;
	}
}

void ANetworkedPhysicsBouyancyLayer::ResolvePhysicsBody()
{
	if (auto CurrentBody = BouyancyLayerMeshComponent->GetBodyInstance())
	{
		if (CurrentBody->IsValidBodyInstance())
		{
			CurrentBody->SetUpdateKinematicFromSimulation(true);
							
			if (Chaos::FPhysicsObject* PhysicsBody = BouyancyLayerMeshComponent->GetPhysicsObjectByName(NAME_None))
			{
				if (UWorld* World = GetWorld())
				{
					if (FPhysScene* Scene = World->GetPhysicsScene())
					{
						if (auto Solver = Scene->GetSolver())
						{
							BouyancyLayerPhysicsAsync = Solver->CreateAndRegisterSimCallbackObject_External<FBouyancyLayerPhysics>();
							if (BouyancyLayerPhysicsAsync)
							{
								BouyancyLayerPhysicsAsync->CurrentPhysicsObjectHandle = PhysicsBody;
								BouyancyLayerPhysicsAsync->WaterSurfaceZ = BouyancyLayerMeshComponent->Bounds.GetBox().Max.Z;
								BouyancyLayerPhysicsAsync->PerProbeForce = PerProbeForce;
								BouyancyLayerPhysicsAsync->GridSize = FMath::Max(1.0f, GridSize);
								BouyancyLayerPhysicsAsync->GridDimension = FMath::Max(1, GridDimension);
							}
						}
					}
				}
			}
		}
	}
}

// we have to register the callback object to the solver Chaos::ESimCallbackOptions::ContactModification this works only if the callback object is registered to the solver
void FBouyancyLayerPhysics::OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier)
{
	//For a physics collision manual detection we have to collide first, then when a collision happens we have to check if the collision is with the bouyancy layer
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
					if (OtherBody->IsDynamic())
					{
						// we convert it to probe, cause probe will not apply any force however collision still happenning so we can do something with it
						CurrentContactCandidate.ConvertToProbe();
						BodiesToBuoy.Add(OtherBody);
					}
				}
			
			}
		}
	}

	

}

void FBouyancyLayerPhysics::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject)
{
	if (CurrentPhysicsObjectHandle == PhysicsObject)
	{
		CurrentPhysicsObjectHandle = nullptr;
		BodiesToBuoy.Reset();
		return;
	}

	Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
	BodiesToBuoy.Remove(WriteInterface.GetRigidParticle(PhysicsObject));
}

void FBouyancyLayerPhysics::FirstPreResimStep_Internal(int32 PhysicsStep)
{
	BodiesToBuoy.Reset();
}


void FBouyancyLayerPhysics::OnPreSimulate_Internal()
{
	if (!CurrentPhysicsObjectHandle)
	{
		BodiesToBuoy.Reset();
		return;
	}

	Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
	Chaos::FPBDRigidParticleHandle* LayerBody = WriteInterface.GetRigidParticle(CurrentPhysicsObjectHandle);
	if (!LayerBody)
	{
		BodiesToBuoy.Reset();
		return;
	}

	for (Chaos::FPBDRigidParticleHandle* OtherBody : BodiesToBuoy)
	{
		if (OtherBody && OtherBody->IsDynamic())
		{
			ApplySubmergedProbeForces(LayerBody, OtherBody);
		}
	}
	BodiesToBuoy.Reset();

	#if CHAOS_DEBUG_DRAW
	if (Chaos::FDebugDrawQueue::IsDebugDrawingEnabled())
	{
		const Chaos::FVec3 LayerPosition = LayerBody->GetX();
		const Chaos::FVec3 SurfaceCenter(LayerPosition.X, LayerPosition.Y, WaterSurfaceZ);
		Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(SurfaceCenter, FColor::Cyan, false, 0.05f, 1, 12.0f);
	}
	#endif
}


