#include "VehicleSimSpringJointComponent.h"
#include "SimModule/SimulationModuleBase.h"
#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/ClusterUnionManager.h"
#include "SimModule/SimModuleTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VehicleSimSpringJointComponent)

namespace Chaos
{
	void FSpringJointSimModule::Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		if (!bInitialized)
		{
			CurrentLinearOffset = FVector::ZeroVector;
			CurrentLinearVel = FVector::ZeroVector;
			bInitialized = true;
		}

		// Get local velocity of this module.
		// In a Modular Vehicle, this is usually the velocity of the parent rigid body in the module's local space.
		const FVector LocalVel = GetLocalLinearVelocity();

		// pushing the spring back.
		FVector DisplacementForce = -LocalVel * Setup().SimulatedMass;

		// restore
		FVector RestoringForce = -Setup().LinearStiffness * CurrentLinearOffset;

		// DampingForce
		FVector DampingForce = -Setup().LinearDamping * CurrentLinearVel;

		FVector TotalLinearForce = DisplacementForce + RestoringForce + DampingForce;

		float Mass = FMath::Max(Setup().SimulatedMass, 1.0f);
		CurrentLinearVel += (TotalLinearForce / Mass) * DeltaTime;
		CurrentLinearOffset += CurrentLinearVel * DeltaTime;

		// Clamp Offset to prevent extreme values but maybe we can interpolate for a natural move?
		CurrentLinearOffset.X = FMath::Clamp(CurrentLinearOffset.X, -Setup().LinearMaxOffset.X, Setup().LinearMaxOffset.X);
		CurrentLinearOffset.Y = FMath::Clamp(CurrentLinearOffset.Y, -Setup().LinearMaxOffset.Y, Setup().LinearMaxOffset.Y);
		CurrentLinearOffset.Z = FMath::Clamp(CurrentLinearOffset.Z, -Setup().LinearMaxOffset.Z, Setup().LinearMaxOffset.Z);
	}

	void FSpringJointSimModule::Animate()
	{
		// pass the displacement to the Game Thread
		AnimationData.AnimFlags = Chaos::EAnimationFlags::AnimatePosition;

		FVector AnimatedOffset = CurrentLinearOffset;
		if (Setup().bReverseX) AnimatedOffset.X *= -1.0f;
		if (Setup().bReverseY) AnimatedOffset.Y *= -1.0f;
		if (Setup().bReverseZ) AnimatedOffset.Z *= -1.0f;

		AnimationData.AnimationLocOffset = AnimatedOffset;
	}
}

UVehicleSimSpringJointComponent::UVehicleSimSpringJointComponent()
{
	LinearStiffness = 50000.0f;
	LinearDamping = 5000.0f;
	LinearMaxOffset = FVector(50.0f, 50.0f, 50.0f);
	SimulatedMass = 100.0f;
	bReverseX = false;
	bReverseY = false;
	bReverseZ = false;
	bAnimationEnabled = true;
}

Chaos::ISimulationModuleBase* UVehicleSimSpringJointComponent::CreateNewCoreModule() const
{
	Chaos::FSpringJointSettings Settings;
	Settings.LinearStiffness = LinearStiffness;
	Settings.LinearDamping = LinearDamping;
	Settings.LinearMaxOffset = LinearMaxOffset;
	Settings.SimulatedMass = SimulatedMass;
	Settings.bReverseX = bReverseX;
	Settings.bReverseY = bReverseY;
	Settings.bReverseZ = bReverseZ;

	Chaos::FSpringJointSimModule* NewModule = new Chaos::FSpringJointSimModule(Settings);
	NewModule->SetAnimationEnabled(bAnimationEnabled);
	return NewModule;
}
