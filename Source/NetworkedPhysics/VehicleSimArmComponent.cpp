#include "VehicleSimArmComponent.h"
#include "SimModule/SimulationModuleBase.h"
#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/ClusterUnionManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VehicleSimArmComponent)

namespace Chaos
{
	// -------------------------------------------------------------------
	// FOLLOWER SIM MODULE
	// -------------------------------------------------------------------

	void FFollowerSimModule::Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		if (Setup().MoveSpeed > 0.01f && !Setup().InputName.IsNone())
		{
			const float InputValue = static_cast<float>(Inputs.GetControls().GetFloat(Setup().InputName));
			if (FMath::Abs(InputValue) > 0.01f)
			{
				CurrentAngle = FMath::Clamp(CurrentAngle + InputValue * Setup().MoveSpeed * DeltaTime, Setup().MinAngle, Setup().MaxAngle);
			}
		}
	}

	// -------------------------------------------------------------------
	// ARM SIM MODULE
	// -------------------------------------------------------------------

	void FArmSimModule::Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		// 1. Setup startup state
		if (!bInitialized)
		{
			TargetAngle = 0.0f;
			CurrentAngle = 0.0f;
			CurrentAngularVel = 0.0f;
			bInitialized = true;
		}

		// 2. Handle User Input
		CollisionSyncAccumulator += DeltaTime;
		const float InputValue = static_cast<float>(Inputs.GetControls().GetFloat(Setup().InputName));
		if (FMath::Abs(InputValue) > 0.01f)
		{
			TargetAngle = FMath::Clamp(TargetAngle + InputValue * Setup().MoveSpeed * DeltaTime, Setup().MinAngle, Setup().MaxAngle);
		}

		// 3. Simple Physics Math (PD Controller)
		const float AngleError = TargetAngle - CurrentAngle;
		float TorqueMagnitude = (AngleError * Setup().Stiffness) - (CurrentAngularVel * Setup().Damping);
		
		const float MaxTorque = 1.0e8f;
		TorqueMagnitude = FMath::Clamp(TorqueMagnitude, -MaxTorque, MaxTorque);

		const float ArmInertia = 1000.0f; 
		CurrentAngularVel += (TorqueMagnitude / ArmInertia) * DeltaTime;
		CurrentAngle += CurrentAngularVel * DeltaTime;

		if (CurrentAngle > Setup().MaxAngle || CurrentAngle < Setup().MinAngle)
		{
			CurrentAngle = FMath::Clamp(CurrentAngle, Setup().MinAngle, Setup().MaxAngle);
			CurrentAngularVel = 0.0f;
		}
		
		// 4. Apply Force to Chassis (This is needed, its a tasteless solution but works since its physics)
			if (ParticleHandle)
			{
				if (ISimulationModuleBase* ChassisModule = VehicleModuleSystem.AccessSimModule(0))
				{
					FQuat ParticleRotation = ParticleHandle->GetR();
					ParticleRotation.Normalize();

					FQuat ParentInitialRotation = GetInitialParticleTransform().GetRotation();
					ParentInitialRotation.Normalize();

					FVector NormalizedAxis = Setup().Axis;
					if (!NormalizedAxis.IsNormalized())
					{
						NormalizedAxis.Normalize();
					}

					const FVector ClusterHingeAxis = ParentInitialRotation.RotateVector(NormalizedAxis);
					const FVector WorldHingeAxis = ParticleRotation.RotateVector(ClusterHingeAxis);
					ChassisModule->AddLocalTorque(WorldHingeAxis * -TorqueMagnitude, true, false);
				}
			}

		// 5. Move Attached Children (Find Them )
		if (Proxy && Proxy->GetType() == EPhysicsProxyType::ClusterUnionProxy)
		{
			FClusterUnionPhysicsProxy* CUProxy = static_cast<FClusterUnionPhysicsProxy*>(Proxy);
			
			// Find all children
			TArray<int32> ChildrenToProcess;
			for (int32 ChildIdx : VehicleModuleSystem.GetChildren(SimTreeIndex)) ChildrenToProcess.Add(ChildIdx);

			// Adoption fallback for orphaned modules
			if (ChildrenToProcess.Num() == 0)
			{
				for (int32 i = 0; i < 32; ++i)
				{
					if (i == SimTreeIndex) continue;
					if (ISimulationModuleBase* Module = VehicleModuleSystem.AccessSimModule(i))
					{
						if (Module->GetDebugName() == TEXT("FollowerSimModule")) ChildrenToProcess.Add(i);
					}
				}
			}
			
			const FTransform& ParentInitial = GetInitialParticleTransform();
			FQuat ParentInitialRotation = ParentInitial.GetRotation();
			ParentInitialRotation.Normalize();

			FVector NormalizedAxis = Setup().Axis;
			if (!NormalizedAxis.IsNormalized())
			{
				NormalizedAxis.Normalize();
			}

			const FVector ClusterHingeAxis = ParentInitialRotation.RotateVector(NormalizedAxis);
			const FQuat ArmRotationInClusterSpace = FQuat(ClusterHingeAxis, FMath::DegreesToRadians(CurrentAngle));
			
			for (int32 ChildIdx : ChildrenToProcess)
			{
				if (ISimulationModuleBase* ChildModule = VehicleModuleSystem.AccessSimModule(ChildIdx))
				{
					// For Child ID get ChildParticle , Update an Push
					if (FPBDRigidClusteredParticleHandle* ChildParticle = ChildModule->GetClusterParticle(CUProxy))
					{
						const FTransform& ChildInitial = ChildModule->GetInitialParticleTransform();
						FQuat ChildTilt = FQuat::Identity;

						if (FFollowerSimModule* Follower = static_cast<FFollowerSimModule*>(ChildModule))
						{
							ChildTilt = Follower->GetLocalRotation();
						}

						// Calculate the new location (Arc movement so its both rotation and translation)
						const FVector RelativeInitialPos = ChildInitial.GetLocation() - ParentInitial.GetLocation();
						const FVector RotatedPos = ArmRotationInClusterSpace.RotateVector(RelativeInitialPos);
						const FVector NewWorldPos = ParentInitial.GetLocation() + RotatedPos;
						
						// Calculate the new orientation
						FQuat ChildInitialRotation = ChildInitial.GetRotation();
						ChildInitialRotation.Normalize();

						FQuat FinalRotation = ArmRotationInClusterSpace * ChildInitialRotation * ChildTilt;
						FinalRotation.Normalize();

						FTransform FinalTransform = ChildInitial;
						FinalTransform.SetLocation(NewWorldPos);
						FinalTransform.SetRotation(FinalRotation);
						
						// Push to child module for smooth Game Thread interpolation
						if (FFollowerSimModule* Follower = static_cast<FFollowerSimModule*>(ChildModule))
						{
							Follower->SetTargetAnimationTransform(FinalTransform);
						}

						// Use official API to update child transform and trigger geometry refresh
						if (FPBDRigidsSolver* Solver = CUProxy->GetSolver<FPBDRigidsSolver>())
						{
							if (FPBDRigidsEvolutionGBF* Evolution = static_cast<FPBDRigidsEvolutionGBF*>(Solver->GetEvolution()))
							{
								FClusterUnionManager& ClusterUnionManager = Evolution->GetRigidClustering().GetClusterUnionManager();
								
								// This also syncs the world transform and queues a geometry refresh
								TArray<FPBDRigidParticleHandle*> Particles = { ChildParticle };
								TArray<FTransform> Transforms = { FinalTransform };
								ClusterUnionManager.UpdateClusterUnionParticlesChildToParent(CUProxy->GetClusterUnionIndex(), Particles, Transforms, false);
								
								// Force the queued update to apply NOW
								ClusterUnionManager.HandleUpdateChildToParentOperation(CUProxy->GetClusterUnionIndex(), Particles);
							}
						}
					}
				}
			}

			// 6. Request lightweight geometry update (throttled) instead of forcing a full flush every tick
			if (ChildrenToProcess.Num() > 0)
			{
				const bool bAngleChanged = FMath::Abs(CurrentAngle - LastSyncedAngle) > CollisionAngleEpsDeg;
				if (bAngleChanged && CollisionSyncAccumulator >= CollisionSyncInterval)
				{
					if (FPBDRigidsSolver* Solver = CUProxy->GetSolver<FPBDRigidsSolver>())
					{
						if (FPBDRigidsEvolutionGBF* Evolution = static_cast<FPBDRigidsEvolutionGBF*>(Solver->GetEvolution()))
						{
							FClusterUnionManager& ClusterUnionManager = Evolution->GetRigidClustering().GetClusterUnionManager();
							// Ensure the union processes incremental geometry regeneration for this cluster soon
							ClusterUnionManager.RequestDeferredClusterPropertiesUpdate(CUProxy->GetClusterUnionIndex(), EUpdateClusterUnionPropertiesFlags::IncrementalGenerateGeometry);
							LastSyncedAngle = CurrentAngle;
							CollisionSyncAccumulator = 0.0f;
							static int32 LogCounter = 0;
							if ((LogCounter++ % 60) == 0)
							{
								UE_LOG(LogTemp, Verbose, TEXT("Component: ARM SIM | Deferred incremental geometry update requested"));
							}
						}
					}
				}
			}
		}
	}
}

// -------------------------------------------------------------------
// COMPONENT IMPLEMENTATIONS
// -------------------------------------------------------------------

UVehicleSimArmComponent::UVehicleSimArmComponent()
{
	Stiffness = 500000.0f;
	Damping = 10000.0f;
	MaxAngle = 90.0f;
	MinAngle = -10.0f;
	MoveSpeed = 45.0f;
	ArmInputName = TEXT("RaiseArm");
	RotationAxis = FVector(0.f, 1.f, 0.f); 
	bAnimationEnabled = true;
}

Chaos::ISimulationModuleBase* UVehicleSimArmComponent::CreateNewCoreModule() const
{
	Chaos::FArmSettings Settings;
	Settings.Stiffness = Stiffness;
	Settings.Damping = Damping;
	Settings.MaxAngle = MaxAngle;
	Settings.MinAngle = MinAngle;
	Settings.MoveSpeed = MoveSpeed;
	Settings.InputName = ArmInputName;
	Settings.Axis = RotationAxis;

	Chaos::FArmSimModule* NewModule = new Chaos::FArmSimModule(Settings);
	NewModule->SetAnimationEnabled(bAnimationEnabled);
	return NewModule;
}

UVehicleSimFollowerComponent::UVehicleSimFollowerComponent()
{
	MoveSpeed = 0.0f;
	FollowerInputName = NAME_None;
	RotationAxis = FVector(0.f, 1.f, 0.f);
	FollowAxis = FVector(0.f, 1.f, 0.f);
	MaxAngle = 45.0f;
	MinAngle = -45.0f;
	bInvertRotation = false;
}

Chaos::ISimulationModuleBase* UVehicleSimFollowerComponent::CreateNewCoreModule() const
{
	Chaos::FFollowerSettings Settings;
	Settings.MoveSpeed = MoveSpeed;
	Settings.InputName = FollowerInputName;
	Settings.Axis = RotationAxis;
	Settings.FollowAxis = FollowAxis;
	Settings.MaxAngle = MaxAngle;
	Settings.MinAngle = MinAngle;
	Settings.bInvert = bInvertRotation;

	return new Chaos::FFollowerSimModule(Settings);
}
