// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosModularVehicle/VehicleSimBaseComponent.h"
#include "SimModule/SimulationModuleBase.h"
#include "VehicleSimArmComponent.generated.h"

// -------------------------------------------------------------------
// PHYSICS SETTINGS (For the Editor)
// -------------------------------------------------------------------

namespace Chaos
{
	/** Settings for the Arm joint */
	struct FArmSettings
	{
		float Stiffness = 500000.0f;
		float Damping = 10000.0f;
		float MaxAngle = 90.0f;
		float MinAngle = -10.0f;
		float MoveSpeed = 45.0f;
		FName InputName = TEXT("RaiseArm");
		FVector Axis = FVector(0.f, 1.f, 0.f);
	};

	/** Settings for things that follow the Arm (like a bucket) */
	struct FFollowerSettings
	{
		float MoveSpeed = 0.0f;
		FName InputName = NAME_None;
		FVector Axis = FVector(0.f, 1.f, 0.f);
		FVector FollowAxis = FVector(0.f, 1.f, 0.f);
		float MaxAngle = 45.0f;
		float MinAngle = -45.0f;
		bool bInvert = false;
	};
}

// -------------------------------------------------------------------
// SIMULATION MODULES (The Physics Brain)
// -------------------------------------------------------------------

namespace Chaos
{
	/** Module that handles the Arm movement */
	class FArmSimModule : public ISimulationModuleBase, public TSimModuleSettings<FArmSettings>
	{
	public:
		FArmSimModule(const FArmSettings& InSettings) 
			: ISimulationModuleBase()
			, TSimModuleSettings<FArmSettings>(InSettings)
			, TargetAngle(0.0f)
			, CurrentAngle(0.0f)
			, CurrentAngularVel(0.0f)
			, bInitialized(false)
			, LastSyncedAngle(0.0f)
			, CollisionSyncAccumulator(0.0f)
		{}

		virtual void Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual void Animate() override
		{
			AnimationData.AnimFlags = Chaos::EAnimationFlags::AnimateRotation;
			AnimationData.CombinedRotation = FQuat(Setup().Axis, FMath::DegreesToRadians(CurrentAngle));
		}

		virtual const FString GetDebugName() const override { return TEXT("ArmSimModule"); }
		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType == eSimModuleTypeFlags::TorqueBased); }
		virtual TSharedPtr<FModuleNetData> GenerateNetData(const int32 NodeArrayIndex) const override { return nullptr; }

		DEFINE_CHAOSSIMTYPENAME(FArmSimModule);

	private:
		float TargetAngle;
		float CurrentAngle;
		float CurrentAngularVel;
		bool bInitialized;
		// Throttle expensive collision/geometry syncs to avoid stalls
		float LastSyncedAngle;
		float CollisionSyncAccumulator; // seconds
		static constexpr float CollisionSyncInterval = 0.05f; // 20 Hz max
		static constexpr float CollisionAngleEpsDeg = 0.25f; // only if changed visibly
	};

	/** Module that handles things attached to the Arm */
	class FFollowerSimModule : public ISimulationModuleBase, public TSimModuleSettings<FFollowerSettings>
	{
	public:
		FFollowerSimModule(const FFollowerSettings& InSettings) 
			: ISimulationModuleBase()
			, TSimModuleSettings<FFollowerSettings>(InSettings)
			, CurrentAngle(0.0f)
			, TargetAnimationTransform(FTransform::Identity)
		{}

		virtual void Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual void Animate() override 
		{
			// We handle animation manually in the Arm's Simulate() to fix engine hierarchy bugs.
			// However, we set BOTH flags here to bypass the PT engine's "==" check 
			// (which skips if flags != Rotation or Position exactly) but still trigger GT interpolation.
			const FTransform& ChildInitial = GetInitialParticleTransform();
			
			AnimationData.AnimFlags = Chaos::EAnimationFlags::AnimateRotation | Chaos::EAnimationFlags::AnimatePosition;
			
			// Calculate delta rotation and position for the Game Thread to interpolate smoothly
			AnimationData.AnimationLocOffset = GetComponentTransform().InverseTransformVector(TargetAnimationTransform.GetLocation() - ChildInitial.GetLocation());
			AnimationData.CombinedRotation = ChildInitial.GetRotation().Inverse() * TargetAnimationTransform.GetRotation();
		}

		void SetTargetAnimationTransform(const FTransform& InTransform) { TargetAnimationTransform = InTransform; }
		FQuat GetLocalRotation() const { return FQuat(Setup().Axis, FMath::DegreesToRadians(CurrentAngle)); }

		virtual const FString GetDebugName() const override { return TEXT("FollowerSimModule"); }
		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return false; }
		virtual TSharedPtr<FModuleNetData> GenerateNetData(const int32 NodeArrayIndex) const override { return nullptr; }
		
		DEFINE_CHAOSSIMTYPENAME(FFollowerSimModule);

	private:
		float CurrentAngle;
		FTransform TargetAnimationTransform;
	};
}

// -------------------------------------------------------------------
// COMPONENTS (What you see in the Unreal Editor)
// -------------------------------------------------------------------

/** Component for the Arm */
UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent), hidecategories = (Object, Replication, Cooking, Activation, LOD, Physics, Collision, AssetUserData, Event))
class NETWORKEDPHYSICS_API UVehicleSimArmComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:
	UVehicleSimArmComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float Stiffness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float Damping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MaxAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MinAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MoveSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FName ArmInputName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FVector RotationAxis;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Rudder; }
	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;
};

/** Component for the Bucket / Follower */
UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent))
class NETWORKEDPHYSICS_API UVehicleSimFollowerComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:
	UVehicleSimFollowerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MoveSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FName FollowerInputName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FVector RotationAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FVector FollowAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MaxAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MinAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool bInvertRotation;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Thruster; }
	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;
};
