#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VehicleAerodynamicsLibrary.generated.h"

class UVehicleSimAerofoilComponent;
class UVehicleSimChassisComponent;

UCLASS()
class NETWORKEDPHYSICS_API UVehicleAerodynamicsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Modular Vehicle|Aerodynamics", meta = (DisplayName = "Set Chassis Drag Area Runtime"))
	static void SetChassisDragAreaRuntime(UVehicleSimChassisComponent* ChassisComponent, float AreaMetresSquared);

	UFUNCTION(BlueprintCallable, Category = "Modular Vehicle|Aerodynamics", meta = (DisplayName = "Set Aerofoil Drag Area Runtime"))
	static void SetAerofoilDragAreaRuntime(UVehicleSimAerofoilComponent* AerofoilComponent, float Area, float DragMultiplier, float LiftMultiplier);
};
