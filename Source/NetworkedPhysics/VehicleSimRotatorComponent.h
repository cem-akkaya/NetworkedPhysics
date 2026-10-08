// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosModularVehicle/VehicleSimBaseComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "SimModule/SimulationModuleBase.h"
#include "VehicleSimRotatorComponent.generated.h"

namespace Chaos
{
	/** Settings for the Rotator Joint */
	struct FRotatorSettings
	{
		float MaxAngle = 10.0f; // Degrees
		float MinAngle = -10.0f; // Degrees
		float MoveSpeed = 45.0f; // Degrees per second
		FName InputName = NAME_None;
		bool bReturnToZero = false;
		
		// Rotation axis in local space
		FVector RotationAxis = FVector(0.f, 0.f, 1.f);
	};

	/** Module that handles an angular lag effect between parent and children */
	class FRotatorSimModule 
		: public ISimulationModuleBase
		, public TSimModuleSettings<FRotatorSettings>
		, public TSimulationModuleTypeable<FRotatorSimModule>
	{
	public:
		FRotatorSimModule(const FRotatorSettings& InSettings) 
			: ISimulationModuleBase()
			, TSimModuleSettings<FRotatorSettings>(InSettings)
			, CurrentAngle(0.0f)
			, TargetAngle(0.0f)
		{}

		virtual void Simulate(IPhysicsProxyBase* Proxy, Chaos::FPBDRigidParticleHandle* ParticleHandle, float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual void Animate() override;

		virtual const FString GetDebugName() const override { return TEXT("RotatorSimModule"); }
		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType == eSimModuleTypeFlags::Velocity); }
		virtual TSharedPtr<FModuleNetData> GenerateNetData(const int32 NodeArrayIndex) const override { return nullptr; }
		virtual Chaos::FSimOutputData* GenerateOutputData() const override { return nullptr; }

		void SetTargetAngle(float InAngle) { TargetAngle = InAngle; }

		DEFINE_CHAOSSIMTYPENAME(FRotatorSimModule);

	private:
		float CurrentAngle; // Radians
		float TargetAngle; // Radians
	};

} // namespace Chaos

/** Component for the Rotator Joint */
UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent))
class NETWORKEDPHYSICS_API UVehicleSimRotatorComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:
	UVehicleSimRotatorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotator")
	float MaxAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotator")
	float MinAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotator")
	float MoveSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotator")
	FName InputName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotator")
	bool bReturnToZero;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotator")
	FVector RotationAxis;

	UFUNCTION(BlueprintCallable, Category = "Rotator")
	void SetRotation(float AngleDegrees);

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Undefined; }
	virtual TArray<FModuleInputSetup> GetInputConfig() const override;
	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;


private:
	// The target angle for the joint
	float TargetAngle;
};
