#include "VehicleSimRotatorComponent.h"
#include "SimModule/SimulationModuleBase.h"
#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"
#include "SimModule/SimModuleTree.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/ClusterUnionManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VehicleSimRotatorComponent)

namespace Chaos
{
	void FRotatorSimModule::Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		// 1. Handle User Input (Directly from Controls like the Arm Module)
		bool bHasInput = false;
		if (!Setup().InputName.IsNone())
		{
			const float InputValue = static_cast<float>(Inputs.GetControls().GetFloat(Setup().InputName));
			if (FMath::Abs(InputValue) > 0.01f)
			{
				TargetAngle += FMath::DegreesToRadians(InputValue * Setup().MoveSpeed * DeltaTime);
				bHasInput = true;
			}
		}

		// Return to zero logic
		if (!bHasInput && Setup().bReturnToZero)
		{
			float TargetRad = 0.0f;
			float Step = FMath::DegreesToRadians(Setup().MoveSpeed * DeltaTime);
			
			if (TargetAngle > TargetRad)
			{
				TargetAngle = FMath::Max(TargetRad, TargetAngle - Step);
			}
			else if (TargetAngle < TargetRad)
			{
				TargetAngle = FMath::Min(TargetRad, TargetAngle + Step);
			}
		}
		// Fallback for SetRotation from Blueprints (only if no active return-to-zero or input)
		else if (!bHasInput && Inputs.StateInputs)
		{
			TargetAngle = FMath::DegreesToRadians(static_cast<float>(Inputs.GetState().GetFloat(TEXT("RotatorTargetAngle"))));
		}

		// Clamp and Update Current
		float MinRad = FMath::DegreesToRadians(Setup().MinAngle);
		float MaxRad = FMath::DegreesToRadians(Setup().MaxAngle);
		TargetAngle = FMath::Clamp(TargetAngle, MinRad, MaxRad);
		CurrentAngle = TargetAngle;

		// 2. Physically move attached children (Exactly mirroring FArmSimModule pattern)
		if (Proxy && Proxy->GetType() == EPhysicsProxyType::ClusterUnionProxy)
		{
			FClusterUnionPhysicsProxy* CUProxy = static_cast<FClusterUnionPhysicsProxy*>(Proxy);
			
			// Find all children
			TArray<int32> ChildrenToProcess;
			for (int32 ChildIdx : VehicleModuleSystem.GetChildren(SimTreeIndex)) 
			{
				ChildrenToProcess.Add(ChildIdx);
			}

			if (ChildrenToProcess.Num() > 0)
			{
				const FTransform& ParentInitial = GetInitialParticleTransform();
				FQuat ParentInitialRotation = ParentInitial.GetRotation();
				ParentInitialRotation.Normalize();

				FVector NormalizedRotationAxis = Setup().RotationAxis;
				if (!NormalizedRotationAxis.IsNormalized())
				{
					NormalizedRotationAxis.Normalize();
				}

				const FVector ClusterHingeAxis = ParentInitialRotation.RotateVector(NormalizedRotationAxis);
				const FQuat RotatorRotation = FQuat(ClusterHingeAxis, CurrentAngle);
				
				// Force a collision update for the cluster if children moved
				bool bNeedsCollisionUpdate = false;

				for (int32 ChildIdx : ChildrenToProcess)
				{
					if (ISimulationModuleBase* ChildModule = VehicleModuleSystem.AccessSimModule(ChildIdx))
					{
						// For Child ID get ChildParticle , Update and Push
						if (FPBDRigidClusteredParticleHandle* ChildParticle = ChildModule->GetClusterParticle(CUProxy))
						{
							const FTransform& ChildInitial = ChildModule->GetInitialParticleTransform();
							
							// Calculate the new location (Arc movement)
							// We use the InitialParticleTransform which should be the pivot location in Cluster space.
							// For Geometry Collections, the particle's X is its CoM, but the ClusterUnionManager::UpdateClusterUnionParticlesChildToParent
							// expects the transform of the particle's origin (pivot) relative to the Cluster Union origin.
							const FVector RelativeInitialPos = ChildInitial.GetLocation() - ParentInitial.GetLocation();
							const FVector RotatedPos = RotatorRotation.RotateVector(RelativeInitialPos);
							const FVector NewWorldPos = ParentInitial.GetLocation() + RotatedPos;

							// Calculate the new orientation
							FQuat ChildInitialRotation = ChildInitial.GetRotation();
							ChildInitialRotation.Normalize();

							FQuat FinalRotation = RotatorRotation * ChildInitialRotation;
							FinalRotation.Normalize();

							FTransform FinalTransform = ChildInitial;
							FinalTransform.SetLocation(NewWorldPos);
							FinalTransform.SetRotation(FinalRotation);
							
							if (FPBDRigidsSolver* Solver = CUProxy->GetSolver<FPBDRigidsSolver>())
							{
								if (FPBDRigidsEvolutionGBF* Evolution = static_cast<FPBDRigidsEvolutionGBF*>(Solver->GetEvolution()))
								{
									FClusterUnionManager& ClusterUnionManager = Evolution->GetRigidClustering().GetClusterUnionManager();
									
									TArray<FPBDRigidParticleHandle*> Particles = { ChildParticle };
									TArray<FTransform> Transforms = { FinalTransform };
									ClusterUnionManager.UpdateClusterUnionParticlesChildToParent(CUProxy->GetClusterUnionIndex(), Particles, Transforms, false);
									
									// Explicitly trigger the update to ensure it reflects in the next physics step
									ClusterUnionManager.HandleUpdateChildToParentOperation(CUProxy->GetClusterUnionIndex(), Particles);
									bNeedsCollisionUpdate = true;
								}
							}
						}
					}
				}

				if (bNeedsCollisionUpdate)
				{
					if (FPBDRigidsSolver* Solver = CUProxy->GetSolver<FPBDRigidsSolver>())
					{
						if (FPBDRigidsEvolutionGBF* Evolution = static_cast<FPBDRigidsEvolutionGBF*>(Solver->GetEvolution()))
						{
							Evolution->GetRigidClustering().GetClusterUnionManager().HandleDeferredClusterUnionUpdateProperties();
						}
					}
				}
			}
		}
	}

	void FRotatorSimModule::Animate()
	{
		AnimationData.AnimFlags = Chaos::EAnimationFlags::AnimateRotation;
		
		FVector NormalizedRotationAxis = Setup().RotationAxis;
		if (!NormalizedRotationAxis.IsNormalized())
		{
			NormalizedRotationAxis.Normalize();
		}

		AnimationData.CombinedRotation = FQuat(NormalizedRotationAxis, CurrentAngle);
		AnimationData.CombinedRotation.Normalize();
	}
}

UVehicleSimRotatorComponent::UVehicleSimRotatorComponent()
{
	MaxAngle = 10.0f;
	MinAngle = -10.0f;
	MoveSpeed = 45.0f;
	InputName = NAME_None;
	bReturnToZero = false;
	RotationAxis = FVector(0.0f, 0.0f, 1.0f);
	bAnimationEnabled = true;
	TargetAngle = 0.0f;
}

void UVehicleSimRotatorComponent::SetRotation(float AngleDegrees)
{
	TargetAngle = FMath::Clamp(AngleDegrees, MinAngle, MaxAngle);

	if (UModularVehicleBaseComponent* Vehicle = Cast<UModularVehicleBaseComponent>(GetAttachParent()))
	{
		Vehicle->SetState(TEXT("RotatorTargetAngle"), (double)TargetAngle);
	}
}

TArray<FModuleInputSetup> UVehicleSimRotatorComponent::GetInputConfig() const
{
	TArray<FModuleInputSetup> Config = Super::GetInputConfig();
	
	// Add both for flexibility
	Config.Add(FModuleInputSetup(TEXT("RotatorTargetAngle"), EModuleInputValueType::MAxis1D));
	
	if (!InputName.IsNone())
	{
		Config.Add(FModuleInputSetup(InputName, EModuleInputValueType::MAxis1D));
	}
	
	return Config;
}

Chaos::ISimulationModuleBase* UVehicleSimRotatorComponent::CreateNewCoreModule() const
{
	Chaos::FRotatorSettings Settings;
	Settings.MaxAngle = MaxAngle;
	Settings.MinAngle = MinAngle;
	Settings.MoveSpeed = MoveSpeed;
	Settings.InputName = InputName;
	Settings.bReturnToZero = bReturnToZero;
	Settings.RotationAxis = RotationAxis;

	Chaos::FRotatorSimModule* NewModule = new Chaos::FRotatorSimModule(Settings);
	NewModule->SetTargetAngle(FMath::DegreesToRadians(TargetAngle));
	NewModule->SetAnimationEnabled(bAnimationEnabled);
	NewModule->SetTreeIndex(GetTreeIndex());
	
	NewModule->SetAnimationData(GetBoneName(), GetAnimationOffset(), GetAnimationSetupIndex());

	return NewModule;
}
