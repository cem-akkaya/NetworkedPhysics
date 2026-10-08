// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosModularVehicle/VehicleSimBaseComponent.h"
#include "SimModule/SimulationModuleBase.h"
#include "VehicleSimSpringJointComponent.generated.h"

namespace Chaos
{
	/** Settings for the Spring Joint */
	struct FSpringJointSettings
	{
		// Linear spring settings
		float LinearStiffness = 50000.0f;
		float LinearDamping = 5000.0f;
		FVector LinearMaxOffset = FVector(50.0f, 50.0f, 50.0f);
		
		// Mass approximation for internal simulation
		float SimulatedMass = 100.0f;

		bool bReverseX = false;
		bool bReverseY = false;
		bool bReverseZ = false;
	};

	/** Module that handles a simplified location-only lag effect between parent and children */
	class FSpringJointSimModule : public ISimulationModuleBase, public TSimModuleSettings<FSpringJointSettings>
	{
	public:
		FSpringJointSimModule(const FSpringJointSettings& InSettings) 
			: ISimulationModuleBase()
			, TSimModuleSettings<FSpringJointSettings>(InSettings)
			, CurrentLinearOffset(FVector::ZeroVector)
			, CurrentLinearVel(FVector::ZeroVector)
			, bInitialized(false)
		{}

		virtual void Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual void Animate() override;

		virtual const FString GetDebugName() const override { return TEXT("SpringJointSimModule"); }
		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType == eSimModuleTypeFlags::Velocity); }
		virtual TSharedPtr<FModuleNetData> GenerateNetData(const int32 NodeArrayIndex) const override { return nullptr; }

		DEFINE_CHAOSSIMTYPENAME(FSpringJointSimModule);

	private:
		FVector CurrentLinearOffset;
		FVector CurrentLinearVel;

		bool bInitialized;
	};

} // namespace Chaos

/** Component for the Spring Joint (e.g. Chariot connection) */
UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent))
class NETWORKEDPHYSICS_API UVehicleSimSpringJointComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:
	UVehicleSimSpringJointComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring")
	float LinearStiffness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring")
	float LinearDamping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring")
	FVector LinearMaxOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring")
	float SimulatedMass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring")
	bool bReverseX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring")
	bool bReverseY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring")
	bool bReverseZ;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Suspension; }
	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;
};
