#include "VehicleAerodynamicsLibrary.h"

#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"
#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "ChaosModularVehicle/VehicleSimAerofoilComponent.h"
#include "ChaosModularVehicle/VehicleSimChassisComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "SimModule/AerofoilModule.h"
#include "SimModule/ChassisModule.h"
#include "SimModule/SimModuleTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VehicleAerodynamicsLibrary)

namespace
{
	template <typename TComponent, typename TModule, typename TUpdate>
	void UpdateSimModule(TComponent* Component, TUpdate Update)
	{
		AActor* Owner = Component->GetOwner();
		if (!Owner)
		{
			return;
		}

		TInlineComponentArray<UModularVehicleBaseComponent*> Vehicles;
		Owner->GetComponents(Vehicles);
		for (UModularVehicleBaseComponent* Vehicle : Vehicles)
		{
			if (!Vehicle || !Vehicle->VehicleSimulationPT)
			{
				continue;
			}

			const FVehicleComponentData* ComponentData = Vehicle->ComponentToPhysicsObjects.Find(Component);
			if (!ComponentData || ComponentData->Guid == INDEX_NONE)
			{
				continue;
			}

			UWorld* World = Vehicle->GetWorld();
			FPhysScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
			Chaos::FPhysicsSolver* Solver = PhysicsScene ? PhysicsScene->GetSolver() : nullptr;
			if (!Solver)
			{
				continue;
			}

			FModularVehicleSimulation* Simulation = Vehicle->VehicleSimulationPT.Get();
			const int32 ModuleGuid = ComponentData->Guid;
			Solver->EnqueueCommandImmediate([Simulation, ModuleGuid, Update]()
			{
				TUniquePtr<Chaos::FSimModuleTree>& Tree = Simulation->AccessSimComponentTree();
				if (!Tree)
				{
					return;
				}

				for (int32 Index = 0; Index < Tree->GetNumNodes(); ++Index)
				{
					Chaos::ISimulationModuleBase* Module = Tree->AccessSimModule(Index);
					if (Module && Module->GetGuid() == ModuleGuid)
					{
						if (TModule* TypedModule = Module->Cast<TModule>())
						{
							Update(TypedModule->AccessSetup());
						}
						return;
					}
				}
			});
			return;
		}
	}
}

void UVehicleAerodynamicsLibrary::SetChassisDragAreaRuntime(UVehicleSimChassisComponent* ChassisComponent, float AreaMetresSquared)
{
	if (!IsValid(ChassisComponent) || !FMath::IsFinite(AreaMetresSquared) || AreaMetresSquared < 0.0f)
	{
		return;
	}

	ChassisComponent->AreaMetresSquared = AreaMetresSquared;
	UpdateSimModule<UVehicleSimChassisComponent, Chaos::FChassisSimModule>(
		ChassisComponent,
		[AreaMetresSquared](Chaos::FChassisSettings& Settings)
		{
			Settings.AreaMetresSquared = AreaMetresSquared;
		});
}

void UVehicleAerodynamicsLibrary::SetAerofoilDragAreaRuntime(UVehicleSimAerofoilComponent* AerofoilComponent, float Area, float DragMultiplier, float LiftMultiplier)
{
	if (!IsValid(AerofoilComponent) || !FMath::IsFinite(Area) || Area < 0.0f || !FMath::IsFinite(DragMultiplier) || !FMath::IsFinite(LiftMultiplier))
	{
		return;
	}

	AerofoilComponent->Area = Area;
	AerofoilComponent->DragMultiplier = DragMultiplier;
	AerofoilComponent->LiftMultiplier = LiftMultiplier;
	UpdateSimModule<UVehicleSimAerofoilComponent, Chaos::FAerofoilSimModule>(
		AerofoilComponent,
		[Area, DragMultiplier, LiftMultiplier](Chaos::FAerofoilSettings& Settings)
		{
			Settings.Area = Area;
			Settings.DragMultiplier = DragMultiplier;
			Settings.LiftMultiplier = LiftMultiplier;
		});
}
