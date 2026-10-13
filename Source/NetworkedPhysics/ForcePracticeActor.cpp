// Fill out your copyright notice in the Description page of Project Settings.


#include "ForcePracticeActor.h"
#include "PBDRigidsSolver.h"
#include "Chaos/ClusterUnionManager.h"
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
			ForcePracticeAsync->BoostFraction = BoostFraction;
			ForcePracticeAsync->BoostDuration = BoostDuration;
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
	
	Chaos::FClusterUnionManager& ClusterUnionManager = Evolution->GetRigidClustering().GetClusterUnionManager();

	const Chaos::FReal DeltaTime = GetDeltaTime_Internal();
	if (DeltaTime <= 0.0 || BoostFraction <= 0.0 || BoostDuration <= 0.0)
	{
		return;
	}

	const Chaos::FAABB3 QueryBounds(Center - BoxExtent, Center + BoxExtent);
	TSet<Chaos::FPBDRigidClusteredParticleHandle*> ActiveRoots;
	TSet<Chaos::FPBDRigidClusteredParticleHandle*> InsideRoots;

	// Filter the particles that are inside the query bounds
	for (Chaos::FTransientPBDRigidParticleHandle& Particle : Evolution->GetParticles().GetNonDisabledDynamicView())
	{
		if (Chaos::FPBDRigidParticleHandle* ParticleHandle = Particle.Handle()->CastToRigidParticle())
		{
			if (Chaos::FClusterUnion* Union = ClusterUnionManager.FindClusterUnionFromParticle(ParticleHandle))
			{
				if (Chaos::FPBDRigidClusteredParticleHandle* Root = Union->InternalCluster)
				{
					ActiveRoots.Add(Root);
					if (QueryBounds.Contains(Particle.GetX()))
					{
						InsideRoots.Add(Root);
					}
				}
			}
		}
	}

	// for the filtered particles that are inside the query bounds, apply a boost for a limited duration
	for (Chaos::FPBDRigidClusteredParticleHandle* Root : ActiveRoots)
	{
		if (InsideRoots.Contains(Root) && !PreviousInsideRoots.Contains(Root))
		{
			const Chaos::FVec3 Velocity = Root->GetV();
			const Chaos::FVec3 HorizontalVelocity(Velocity.X, Velocity.Y, 0.0);
			if (HorizontalVelocity.SizeSquared() > UE_SMALL_NUMBER && Root->M() > 0.0)
			{
				FBoostState& Boost = ActiveBoosts.FindOrAdd(Root);
				Boost.EntryVelocity = HorizontalVelocity;
				Boost.RemainingTime = BoostDuration;
			}
		}

		if (FBoostState* Boost = ActiveBoosts.Find(Root))
		{
			const Chaos::FReal AppliedTime = FMath::Min(Boost->RemainingTime, DeltaTime);
			const Chaos::FVec3 DashForce = Boost->EntryVelocity * (Root->M() * BoostFraction / BoostDuration) * (AppliedTime / DeltaTime);
			Root->AddForce(DashForce, true);
			Boost->RemainingTime -= AppliedTime;
			if (Boost->RemainingTime <= UE_SMALL_NUMBER)
			{
				ActiveBoosts.Remove(Root);
			}
		}
	}

	// remove boost states for particles that are no longer inside the query bounds, getappaaaaaa
	for (auto Boost = ActiveBoosts.CreateIterator(); Boost; ++Boost)
	{
		if (!ActiveRoots.Contains(Boost.Key()))
		{
			Boost.RemoveCurrent();
		}
	}

	PreviousInsideRoots = MoveTemp(InsideRoots);
}
