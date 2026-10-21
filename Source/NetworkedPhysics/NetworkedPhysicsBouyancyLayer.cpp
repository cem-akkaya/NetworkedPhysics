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
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"

int32 FBouyancyLayerPhysics::CountSubmergedProbes(const Chaos::FPBDRigidParticleHandle* LayerBody, const Chaos::FPBDRigidParticleHandle* OtherBody) const
{
	constexpr int32 GridDimension = 9;
	constexpr Chaos::FReal GridSize = 100.0;
	const Chaos::FReal CellSize = GridSize / GridDimension;
	const Chaos::FImplicitObject* LayerShape = LayerBody->GetGeometry();
	const Chaos::FImplicitObject* OtherShape = OtherBody->GetGeometry();
	if (!LayerShape || !OtherShape)
	{
		return 0;
	}

	const Chaos::FRigidTransform3 LayerTransform = LayerBody->GetTransformPQ();
	const Chaos::FRigidTransform3 OtherTransform = OtherBody->GetTransformPQ();
	int32 InsideCount = 0;

	for (int32 X = 0; X < GridDimension; ++X)
	{
		for (int32 Y = 0; Y < GridDimension; ++Y)
		{
			for (int32 Z = 0; Z < GridDimension; ++Z)
			{
				const Chaos::FVec3 WorldPoint = OtherBody->GetP() + Chaos::FVec3(
					(X + 0.5) * CellSize - GridSize * 0.5,
					(Y + 0.5) * CellSize - GridSize * 0.5,
					(Z + 0.5) * CellSize - GridSize * 0.5);
				if (WorldPoint.Z <= WaterSurfaceZ &&
					LayerShape->Overlap(LayerTransform.InverseTransformPosition(WorldPoint), 0.0) &&
					OtherShape->Overlap(OtherTransform.InverseTransformPosition(WorldPoint), 0.0))
				{
					++InsideCount;
				}
			}
		}
	}

	return InsideCount;
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
	TSet<Chaos::FPBDRigidParticleHandle*> UpdatedBodies;
	Chaos::FReal ForcePerProbe = PerProbeForce;

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
					//Then we can do something, but we do some dynamic force on object now rather than launching it
					if (OtherBody->IsDynamic())
					{
						// we convert it to probe, cause probe will not apply any force however collision still happenning so we can do something with it
						CurrentContactCandidate.ConvertToProbe();
						if (UpdatedBodies.Contains(OtherBody))
						{
							continue;
						}
						UpdatedBodies.Add(OtherBody);

						const int32 ProbeCount = CountSubmergedProbes(RigidHandle, OtherBody);
						if (ProbeCount > 0)
						{
							// Each submerged probe contributes
							const Chaos::FReal UpwardForce = ProbeCount * ForcePerProbe;
							const Chaos::FReal VelocityChange = UpwardForce * OtherBody->InvM() * GetDeltaTime_Internal();
							const Chaos::FVec3 NewVelocity = OtherBody->GetV() + Chaos::FVec3(0.0, 0.0, VelocityChange);
							const auto Pair = CurrentContactCandidate.GetParticlePair();
							int32 OtherBodyIndex = 0;
							if (Pair[0] != OtherBody)
							{
								OtherBodyIndex = 1;
							}
							CurrentContactCandidate.ModifyParticleVelocity(NewVelocity, OtherBodyIndex);
						}
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
	}
}


void FBouyancyLayerPhysics::OnPreSimulate_Internal()
{
	
}	


