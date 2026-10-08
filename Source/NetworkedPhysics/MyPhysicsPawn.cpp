#include "MyPhysicsPawn.h"
#include "Engine/Engine.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"

static TAutoConsoleVariable<float> CVarPhysicsForceMultiplier(
	TEXT("p.Pawn.ForceMultiplier"),
	1000.0f,
	TEXT("Global multiplier for pawn movement forces."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPhysicsJumpMultiplier(
	TEXT("p.Pawn.JumpMultiplier"),
	250.0f,
	TEXT("Global multiplier for pawn jump forces."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPhysicsSteerMultiplier(
	TEXT("p.Pawn.SteerMultiplier"),
	50.0f,
	TEXT("Global multiplier for tank steer."),
	ECVF_Default);

//----------------------------------------------------
//-------------- Async Networked Physics--------------
//----------------------------------------------------

void FPhysicsPawnAsync::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle InPhysicsObject)
{
	if (PhysicsObject == InPhysicsObject)
	{
		PhysicsObject = nullptr;
	}
}

// Input to State application
/** Called on Autonomous-Proxy Client or Server if the server is controlling the pawn */
void FPhysicsPawnAsync::BuildInput_Internal(FNetInputPhysicsPawn& Input) const
{
	Input.MovementInput = MovementInput_Internal;
	Input.SteeringInput = SteeringInput_Internal;
	Input.BounceInput = BounceInput_Internal;
}
/** Validate incoming input data on the server, clamp to valid values */
void FPhysicsPawnAsync::ValidateInput_Internal(FNetInputPhysicsPawn& Input) const
{
	Input.MovementInput = FMath::Clamp(Input.MovementInput, -1.0f, 1.0f);
	Input.SteeringInput = FMath::Clamp(Input.SteeringInput, -1.0f, 1.0f);
	Input.BounceInput = FMath::Clamp(Input.BounceInput, 0.0f, 1.0f);
}
/** Called on Server and Sim-Proxy Clients each frame and on Clients during Resimulations */
void FPhysicsPawnAsync::ApplyInput_Internal(const FNetInputPhysicsPawn& Input)
{
	SetMovementInput_Internal(Input.MovementInput);
	SetSteeringInput_Internal(Input.SteeringInput);
	SetBounceInput_Internal(Input.BounceInput);
}

/** Called on Server */
void FPhysicsPawnAsync::BuildState_Internal(FNetStatePhysicsPawn& State) const
{
	
}

/** Called on Clients during resimulations */
void FPhysicsPawnAsync::ApplyState_Internal(const FNetStatePhysicsPawn& State)
{
	
}

void FPhysicsPawnAsync::OnPostInitialize_Internal()
{
	if (PhysicsObject)
	{
		Chaos::FWritePhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
		if (Chaos::FPBDRigidParticleHandle* ParticleHandle = Interface.GetRigidParticle(PhysicsObject))
		{
			ParticleHandle->SetSleepType(Chaos::ESleepType::NeverSleep);
		}
	}
}

void FPhysicsPawnAsync::ProcessInputs_Internal(int32 PhysicsSteps)
{
	const FAsyncInputPhysicsPawn* AsyncInput = GetConsumerInput_Internal();
	if (AsyncInput == nullptr)
	{
		return;
	}
	Chaos::FPhysicsSolverBase* BaseSolver = GetSolver();
	if (!BaseSolver || BaseSolver->IsResimming())
	{
		return;
	}
	
	
	// Get inputs
	SetMovementInput_Internal(AsyncInput->MovementInput);
	SetSteeringInput_Internal(AsyncInput->SteeringInput);
	SetBounceInput_Internal(AsyncInput->BounceInput);
	
};

void FPhysicsPawnAsync::OnPreSimulate_Internal()
{
	if (!PhysicsObject)
	{
		return;
	}
	
	Chaos::FWritePhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
	Chaos::FPBDRigidParticleHandle* ParticleHandle = Interface.GetRigidParticle(PhysicsObject);
	
	if (ParticleHandle == nullptr)
	{
		return;
	}
	
	if (const FAsyncInputPhysicsPawn* AsyncInput = GetConsumerInput_Internal())
	{
		SetMovementInput_Internal(AsyncInput->MovementInput);
		SetSteeringInput_Internal(AsyncInput->SteeringInput);
		SetBounceInput_Internal(AsyncInput->BounceInput);
	}
	
	// Get values from CVars
	const float ForceMultiplier = CVarPhysicsForceMultiplier.GetValueOnAnyThread();
	const float JumpMultiplier = CVarPhysicsJumpMultiplier.GetValueOnAnyThread();
	const float SteerMultiplier = CVarPhysicsSteerMultiplier.GetValueOnAnyThread();

	const float InputLinearMovementForce = MovementInput_Internal * ForceMultiplier;
	const float InputLinearSteeringForce = SteeringInput_Internal * ForceMultiplier;
	
	const float InputAngularMovementForce = MovementInput_Internal * ForceMultiplier;
	const float InputAngularSteeringForce = SteeringInput_Internal * ForceMultiplier;
	
	const float InputBounceForce = BounceInput_Internal * JumpMultiplier;
	
	// We use the particle's current rotation to find the "Forward" direction in world space
	const Chaos::FRotation3 WorldRotation = ParticleHandle->GetR();
	const Chaos::FVec3 ForwardVector = WorldRotation * Chaos::FVec3(1.0f, 0.0f, 0.0f);
	const Chaos::FVec3 WorldLinearMovement = ForwardVector * InputLinearMovementForce;
	
	// Calculate Steering Torque
	const Chaos::FVec3 LocalUp = Chaos::FVec3(0.0f, 0.0f, 1.0f);
	const Chaos::FVec3 WorldUp = WorldRotation * LocalUp;
	const Chaos::FVec3 WorldAngularMovement = WorldUp * InputLinearSteeringForce * SteerMultiplier;
	
	if (InputBounceForce > UE_SMALL_NUMBER)
	{
		// ONLY jump if we haven't already jumped for this specific button press
		if (!bHasProcessedJump)
		{
			Chaos::FVec3 CurrentV = ParticleHandle->GetV();

			// Get the last velocity and add to it
			if (CurrentV.Z < 0.0f)
			{
				CurrentV.Z = FMath::Abs(CurrentV.Z) * 0.8f; 
			}
			
			// Force Z to exactly the jump velocity but add to it so it's incremental
			CurrentV.Z += InputBounceForce; 

			// Apply the final calculated velocity back to the particle
			ParticleHandle->SetV(CurrentV);
			
			bHasProcessedJump = true; 
		}
	}
	else
	{
		bHasProcessedJump = false;
	}
	
	// Apply Linear Forces 
	if (WorldLinearMovement.SizeSquared() > UE_SMALL_NUMBER)
	{
		ParticleHandle->AddForce(WorldLinearMovement, true);
	}
	
	// Apply Angular Forces (Turning)
	if (WorldAngularMovement.SizeSquared() > UE_SMALL_NUMBER)
	{
		ParticleHandle->AddTorque(WorldAngularMovement, true);
	}
}


//----------------------------------------------------
//--------------------- Pawn -------------------------
//----------------------------------------------------

// Sets default values
AMyPhysicsPawn::AMyPhysicsPawn()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	// Only create a network physics component if physics prediction is enabled
	if (UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction)
	{
		static const FName NetworkPhysicsComponentName(TEXT("NetworkPhysicsComponent"));
		
		// Add network physics component as a subobject
		NetworkPhysicsComponent = CreateDefaultSubobject<UNetworkPhysicsComponent>(NetworkPhysicsComponentName);
		NetworkPhysicsComponent->SetNetAddressable(); // Make subobject component net addressable
		NetworkPhysicsComponent->SetIsReplicated(true);
	}
}

// Called when the game starts or when spawned
void AMyPhysicsPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMyPhysicsPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void AMyPhysicsPawn::Tick(float DeltaTime)
{
Super::Tick(DeltaTime);
 
	if (PhysicsPawnAsync)
	{
		if (IsLocallyControlled())
		{
			if (FAsyncInputPhysicsPawn* AsyncInput = PhysicsPawnAsync->GetProducerInputData_External())
			{
				AsyncInput->MovementInput = ForwardInput_External - BackwardInput_External;
				AsyncInput->SteeringInput = SteeringInput_External;
				AsyncInput->BounceInput = BounceInput_External;
				
				// Bounce input is one off go like an impulse so we immediately revert input.
				BounceInput_External = 0.0f;
			}
		}
	}
}

void AMyPhysicsPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
 
	if (UPrimitiveComponent* RootSimulatedComponent = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					// Create async callback object to run on Physics Thread
					PhysicsPawnAsync = Solver->CreateAndRegisterSimCallbackObject_External<FPhysicsPawnAsync>();
					if (ensure(PhysicsPawnAsync))
					{
						PhysicsPawnAsync->PhysicsObject = RootSimulatedComponent->GetPhysicsObjectByName(NAME_None);
 
						if (NetworkPhysicsComponent)
						{
							// Register the input and state structs along with PT interface in the networked physics component
							NetworkPhysicsComponent->CreateDataHistory<FNetInputPhysicsPawn, FNetStatePhysicsPawn>(PhysicsPawnAsync);
						}
					}
				}
			}
		}
	}
}

// Called to bind functionality to input
void AMyPhysicsPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

/** Interpolate input between two inputs */
void FNetInputPhysicsPawn::InterpolateData(const FNetworkPhysicsPayload& MinData, const FNetworkPhysicsPayload& MaxData, float LerpAlpha)
{
	const FNetInputPhysicsPawn& MinInput = static_cast<const FNetInputPhysicsPawn&>(MinData);
	const FNetInputPhysicsPawn& MaxInput = static_cast<const FNetInputPhysicsPawn&>(MaxData);
	MovementInput = FMath::Lerp(MinInput.MovementInput, MaxInput.MovementInput, LerpAlpha);
	SteeringInput = FMath::Lerp(MinInput.SteeringInput, MaxInput.SteeringInput, LerpAlpha);
	BounceInput = FMath::Lerp(MinInput.BounceInput, MaxInput.BounceInput, LerpAlpha);
}

/** Merge input into this input to take both inputs into account */
void FNetInputPhysicsPawn::MergeData(const FNetworkPhysicsPayload& FromData)
{
	const FNetInputPhysicsPawn& FromInput = static_cast<const FNetInputPhysicsPawn&>(FromData);
	MovementInput = FMath::Max(MovementInput, FromInput.MovementInput);
	SteeringInput = FMath::Max(SteeringInput, FromInput.SteeringInput);
	BounceInput = FMath::Max(BounceInput, FromInput.BounceInput);
}

/** Decay input during resimulation if input is predicted */
void FNetInputPhysicsPawn::DecayData(float DecayAmount)
{
	MovementInput = MovementInput * (1.0f - DecayAmount);
	SteeringInput = SteeringInput * (1.0f - DecayAmount);
	BounceInput = BounceInput * (1.0f - DecayAmount);
}

/** Compare predicted input with correct input from server and trigger resim if they differ */
bool FNetInputPhysicsPawn::CompareData(const FNetworkPhysicsPayload& PredictedData) const
{
	const FNetInputPhysicsPawn& PredictedInput = static_cast<const FNetInputPhysicsPawn&>(PredictedData);
	bool bHasDiff = false;
	bHasDiff |= MovementInput != PredictedInput.MovementInput;
	bHasDiff |= SteeringInput != PredictedInput.SteeringInput;
	bHasDiff |= BounceInput != PredictedInput.BounceInput;
	return (bHasDiff == false);
}