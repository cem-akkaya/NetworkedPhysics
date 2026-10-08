#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "MyPhysicsPawn.generated.h"


//----------------------------------------------------
//--------------------- Structs ----------------------
//----------------------------------------------------

// GameThread to PhysicsThread input data struct
struct FAsyncInputPhysicsPawn : public Chaos::FSimCallbackInput
{
	float MovementInput;
	float SteeringInput;
	float BounceInput;
	
	void Reset()
	{
		MovementInput = 0.0f;
		SteeringInput = 0.0f;
		BounceInput = 0.0f;
	}
};

// Networked input data struct
USTRUCT()
struct FNetInputPhysicsPawn : public FNetworkPhysicsPayload
{
	GENERATED_BODY()
 
	FNetInputPhysicsPawn()
		: MovementInput(0.0f), SteeringInput(0.0f), BounceInput(0)
	{
	}

	UPROPERTY()
	float MovementInput;
 
	UPROPERTY()
	float SteeringInput;
	
	UPROPERTY()
	float BounceInput;
 
	void InterpolateData(const FNetworkPhysicsPayload& MinData, const FNetworkPhysicsPayload& MaxData, float LerpAlpha) override;
	void MergeData(const FNetworkPhysicsPayload& FromData) override;
	void DecayData(float DecayAmount) override;
	bool CompareData(const FNetworkPhysicsPayload& PredictedData) const override;
 
	const FString DebugData() const override { return FString::Printf(TEXT("MovementInput: %f - SteeringInput :%f"), MovementInput, SteeringInput); }
};

// PhysicsThread to GameThread output data struct
struct FAsyncOutputPhysicsPawn : public Chaos::FSimCallbackOutput
{
	void Reset() {}
};

//----------------------------------------------------
//-------------- Async Networked Physics--------------
//----------------------------------------------------

USTRUCT()
struct FNetStatePhysicsPawn : public FNetworkPhysicsPayload
{
	GENERATED_BODY()
	FNetStatePhysicsPawn() {}
	void InterpolateData(const FNetworkPhysicsPayload& MinData, const FNetworkPhysicsPayload& MaxData, float LerpAlpha) override { }
	bool CompareData(const FNetworkPhysicsPayload& PredictedData) const override { return true; }
};

class FPhysicsPawnAsync : public Chaos::TSimCallbackObject<FAsyncInputPhysicsPawn, FAsyncOutputPhysicsPawn,
	(Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PhysicsObjectUnregister)>
	, TNetworkPhysicsInputState_Internal<FNetInputPhysicsPawn, FNetStatePhysicsPawn>
{
	friend class AMyPhysicsPawn;
	~FPhysicsPawnAsync() {}
	
	// TSimCallbackObject callbacks
	virtual void OnPostInitialize_Internal() override;
	virtual void ProcessInputs_Internal(int32 PhysicsStep) override;
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle InPhysicsObject) override;

	// TNetworkPhysicsInputState_Internal callbacks
	virtual void BuildInput_Internal(FNetInputPhysicsPawn& Input) const override;
	virtual void ValidateInput_Internal(FNetInputPhysicsPawn& Input) const override;
	virtual void ApplyInput_Internal(const FNetInputPhysicsPawn& Input) override;
	virtual void BuildState_Internal(FNetStatePhysicsPawn& State) const override;
	virtual void ApplyState_Internal(const FNetStatePhysicsPawn& State) override;
	
public:
	Chaos::FConstPhysicsObjectHandle PhysicsObject = nullptr;
	
	
	// Move Networking
	void SetMovementInput_Internal(float InMovementInput) { MovementInput_Internal = InMovementInput; }
	const float GetMovementInput_Internal() const { return MovementInput_Internal; }
	
	// Steer Networking
	void SetSteeringInput_Internal(float InSteeringInput) { SteeringInput_Internal = InSteeringInput; }
	const float GetSteeringInput_Internal() const { return SteeringInput_Internal; }
	
	// Bounce Networking
	void SetBounceInput_Internal(float InBounceInput) { BounceInput_Internal = InBounceInput; }
	const float GetBounceInput_Internal() const { return BounceInput_Internal; }
	
private:
	float MovementInput_Internal;
	float SteeringInput_Internal;
	float BounceInput_Internal;
	
	// Helpers
	
	// For consuming single fire action consistently in async thread
	bool bHasProcessedJump = false;
};


//----------------------------------------------------
//--------------------- Pawn -------------------------
//----------------------------------------------------


UCLASS()
class NETWORKEDPHYSICS_API AMyPhysicsPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AMyPhysicsPawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void PostInitializeComponents() override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// Inputs
	UFUNCTION(BlueprintCallable, Category = "Game|PhysicsPawn")
	void SetForwardInput(const float InForwardInput)
	{
		ForwardInput_External = FMath::Clamp(InForwardInput, 0.0f, 1.0f);
	}
 
	UFUNCTION(BlueprintCallable, Category = "Game|PhysicsPawn")
	void SetBackwardInput(const float InBackwardInput)
	{
		BackwardInput_External = FMath::Clamp(InBackwardInput, 0.0f, 1.0f);
	}
 
	UFUNCTION(BlueprintCallable, Category = "Game|PhysicsPawn")
	void SetSteeringInput(const float InSteeringInput)
	{
		SteeringInput_External = FMath::Clamp(InSteeringInput, -1.0f, 1.0f);
	}
	
	UFUNCTION(BlueprintCallable, Category = "Game|PhysicsPawn")
	void SetBounceInput(const float InBounceInput)
	{
		BounceInput_External = FMath::Clamp(InBounceInput, 0.f, 1.0f);
	}


private:
	FPhysicsPawnAsync* PhysicsPawnAsync = nullptr;
	
	UPROPERTY()
	TObjectPtr<UNetworkPhysicsComponent> NetworkPhysicsComponent = nullptr;
	
	float ForwardInput_External = 0.0f;
	float BackwardInput_External = 0.0f;
 
	float SteeringInput_External = 0.0f;
	float BounceInput_External = 0.0f;
};

