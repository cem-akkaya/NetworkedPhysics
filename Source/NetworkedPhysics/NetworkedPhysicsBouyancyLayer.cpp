// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkedPhysicsBouyancyLayer.h"
#include "NetworkedPhysicsWaterGridData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/ContactModification.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/DebugDrawQueue.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#endif

void FBouyancyLayerPhysics::ApplySubmergedProbeForces(const Chaos::FPBDRigidParticleHandle* LayerBody, Chaos::FPBDRigidParticleHandle* OtherBody) const
{
	if (LayerGridDimension <= 0 || WaterGridCells.Num() != LayerGridDimension * LayerGridDimension || LayerDimensions.X <= UE_SMALL_NUMBER || LayerDimensions.Y <= UE_SMALL_NUMBER)
	{
		return;
	}

	const Chaos::FAABB3& Bounds = OtherBody->WorldSpaceInflatedBounds();
	const Chaos::FReal CurrentGridSize = Bounds.Extents().GetMax();
	
	const Chaos::FReal CellSize = CurrentGridSize / ForceGridDimension;
	const Chaos::FImplicitObject* LayerShape = LayerBody->GetGeometry();
	const Chaos::FImplicitObject* OtherShape = OtherBody->GetGeometry();
	if (!LayerShape || !OtherShape)
	{
		return;
	}

	const Chaos::FRigidTransform3 LayerTransform = LayerBody->GetTransformXR();
	const Chaos::FRigidTransform3 OtherTransform = OtherBody->GetTransformXR();
	const Chaos::FReal WaterCellWidth = LayerDimensions.X * 2.0 / LayerGridDimension;
	const Chaos::FReal WaterCellLength = LayerDimensions.Y * 2.0 / LayerGridDimension;
	const Chaos::FVec3 CenterOfMass = OtherBody->XCom();
	Chaos::FVec3 TotalForce = Chaos::FVec3::ZeroVector;
	Chaos::FVec3 TotalTorque = Chaos::FVec3::ZeroVector;
	#if CHAOS_DEBUG_DRAW
	const bool bDrawProbes = Chaos::FDebugDrawQueue::IsDebugDrawingEnabled();
	#endif

	for (int32 X = 0; X < ForceGridDimension; ++X)
	{
		for (int32 Y = 0; Y < ForceGridDimension; ++Y)
		{
			for (int32 Z = 0; Z < ForceGridDimension; ++Z)
			{
				const Chaos::FVec3 WorldPoint = Bounds.Center() + Chaos::FVec3(
					(X + 0.5) * CellSize - CurrentGridSize * 0.5,
					(Y + 0.5) * CellSize - CurrentGridSize * 0.5,
					(Z + 0.5) * CellSize - CurrentGridSize * 0.5);
				const Chaos::FVec3 LayerPoint = LayerTransform.InverseTransformPosition(WorldPoint);
				const int32 WaterX = FMath::FloorToInt((LayerPoint.X + LayerDimensions.X) / WaterCellWidth);
				const int32 WaterY = FMath::FloorToInt((LayerPoint.Y + LayerDimensions.Y) / WaterCellLength);
				Chaos::FReal SubmergedDepth = 0.0;
				Chaos::FVec3 ProbeNormal = Chaos::FVec3::ZeroVector;
				if (WaterX >= 0 && WaterX < LayerGridDimension && WaterY >= 0 && WaterY < LayerGridDimension)
				{
					const FBouyancyWaterGridCell& WaterCell = WaterGridCells[WaterX * LayerGridDimension + WaterY];
					const Chaos::FReal WaterTop = WaterCell.LocalCenter.Z + WaterCell.LocalExtent.Z;
					const Chaos::FReal WaterBottom = WaterCell.LocalCenter.Z - WaterCell.LocalExtent.Z;
					if (WaterCell.bValid && WaterCell.LocalExtent.Z > UE_SMALL_NUMBER && LayerPoint.Z >= WaterBottom && LayerPoint.Z <= WaterTop)
					{
						// Deeper probes receive more of this cell's buoyancy force.
						SubmergedDepth = FMath::Clamp((WaterTop - LayerPoint.Z) / (WaterTop - WaterBottom), 0.0, 1.0);
						ProbeNormal = LayerTransform.GetRotation().RotateVector(WaterCell.LocalSurfaceNormal);
					}
				}
				const bool bSubmerged = SubmergedDepth > 0.0 &&
					LayerShape->Overlap(LayerPoint, 0.0) &&
					OtherShape->Overlap(OtherTransform.InverseTransformPosition(WorldPoint), 0.0);
				#if CHAOS_DEBUG_DRAW
				if (bDrawProbes)
				{
					FColor ProbeColor(80, 0, 0);
					if (bSubmerged)
					{
						ProbeColor = FColor::Green;
					}
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(WorldPoint, ProbeColor, false, 0.05f, 1, 8.0f);
				}
				#endif
				if (bSubmerged)
				{
					const Chaos::FVec3 CurrentProbeForce = ProbeNormal * PerProbeForce * SubmergedDepth;
					TotalForce += CurrentProbeForce;
					TotalTorque += Chaos::FVec3::CrossProduct(WorldPoint - CenterOfMass, CurrentProbeForce);
				}
			}
		}
	}

	const Chaos::FReal ForceMagnitude = TotalForce.Size();
	if (ForceMagnitude > UE_SMALL_NUMBER)
	{
		const Chaos::FReal ForceScale = FMath::Min(1.0, MaxForcePerObject / ForceMagnitude);
		OtherBody->AddForce(TotalForce * ForceScale, true);
		OtherBody->AddTorque(TotalTorque * ForceScale, true);
	}

}

void ANetworkedPhysicsBouyancyLayer::BakeWaterGrid()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::Editor)
	{
		return;
	}

	UNetworkedPhysicsWaterGridData* GridData = WaterGridData;
	if (!GridData)
	{
		const FString AssetName = FString::Printf(TEXT("DA_WaterGrid_%s_%s"), *GetWorld()->GetMapName(), *GetName());
		const FString PackageName = TEXT("/Game/BakedWaterGrids/") + AssetName;
		const FString AssetPath = PackageName + TEXT(".") + AssetName;
		GridData = LoadObject<UNetworkedPhysicsWaterGridData>(nullptr, *AssetPath);
		if (!GridData)
		{
			UPackage* Package = CreatePackage(*PackageName);
			GridData = NewObject<UNetworkedPhysicsWaterGridData>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(GridData);
		}

		Modify();
		WaterGridData = GridData;
		MarkPackageDirty();
	}

	GridData->Modify();
	const int32 CellsPerAxis = FMath::Max(1, LayerGridDimension);
	const FVector CellSize = LayerDimensions * 2.0f / CellsPerAxis;
	GridData->LayerDimensions = LayerDimensions;
	GridData->CellsPerAxis = CellsPerAxis;
	GridData->Cells.Reset();
	GridData->Cells.SetNum(CellsPerAxis * CellsPerAxis);
	const FTransform LayerTransform = BouyancyLayerCollisionComponent->GetComponentTransform();
	const float TopZ = LayerDimensions.Z;
	const FVector BoxUp = LayerTransform.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	const FQuat BoxTilt = FQuat::FindBetweenNormals(FVector::UpVector, BoxUp);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	for (int32 X = 0; X < CellsPerAxis; ++X)
	{
		for (int32 Y = 0; Y < CellsPerAxis; ++Y)
		{
			const float LocalX = -LayerDimensions.X + (X + 0.5f) * CellSize.X;
			const float LocalY = -LayerDimensions.Y + (Y + 0.5f) * CellSize.Y;
			const FVector TraceStart = LayerTransform.TransformPosition(FVector(LocalX, LocalY, LayerDimensions.Z));
			const FVector TraceEnd = LayerTransform.TransformPosition(FVector(LocalX, LocalY, -LayerDimensions.Z));
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, QueryParams))
			{
				const float BottomZ = LayerTransform.InverseTransformPosition(Hit.ImpactPoint).Z;
				const float WaterHeight = TopZ - BottomZ;
				if (WaterHeight > UE_SMALL_NUMBER && BottomZ >= -LayerDimensions.Z)
				{
					FBouyancyWaterGridCell& Cell = GridData->Cells[X * CellsPerAxis + Y];
					Cell.LocalCenter = FVector(LocalX, LocalY, BottomZ + WaterHeight * 0.5f);
					Cell.LocalExtent = FVector(CellSize.X * 0.5f, CellSize.Y * 0.5f, WaterHeight * 0.5f);
					const FVector WaterNormal = BoxTilt.RotateVector(Hit.ImpactNormal).GetSafeNormal();
					Cell.LocalSurfaceNormal = LayerTransform.InverseTransformVectorNoScale(WaterNormal).GetSafeNormal();
					Cell.bValid = true;
				}
			}
		}
	}

	UPackage* Package = GridData->GetOutermost();
	Package->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	if (!UPackage::SavePackage(Package, GridData, *Filename, FSavePackageArgs()))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save water grid asset: %s"), *Filename);
	}

	const FVector LayerScale = LayerTransform.GetScale3D();
	for (const FBouyancyWaterGridCell& Cell : GridData->Cells)
	{
		if (!Cell.bValid)
		{
			continue;
		}

		const float WorldHeight = Cell.LocalExtent.Z * 2.0f * FMath::Abs(LayerScale.Z);
		const FVector ArrowStart = LayerTransform.TransformPosition(Cell.LocalCenter - FVector(0.0f, 0.0f, Cell.LocalExtent.Z));
		const FVector WorldNormal = LayerTransform.TransformVectorNoScale(Cell.LocalSurfaceNormal).GetSafeNormal();
		DrawDebugDirectionalArrow(GetWorld(), ArrowStart, ArrowStart + WorldNormal * WorldHeight, WorldHeight * 0.2f, FColor::Cyan, false, 5.0f, 0, 1.0f);
	}
#endif
}

// Sets default values
ANetworkedPhysicsBouyancyLayer::ANetworkedPhysicsBouyancyLayer()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	BouyancyLayerCollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BouyancyLayer"));
	SetRootComponent(BouyancyLayerCollisionComponent);
	BouyancyLayerCollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BouyancyLayerCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BouyancyLayerCollisionComponent->SetSimulatePhysics(false);
	BouyancyLayerCollisionComponent->SetBoxExtent(LayerDimensions);
	BouyancyLayerCollisionComponent->SetMaxDepenetrationVelocity(NAME_None, 0.0f);

	BouyancyLayerVisualComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BouyancyLayerVisual"));
	BouyancyLayerVisualComponent->SetupAttachment(BouyancyLayerCollisionComponent);
	BouyancyLayerVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BouyancyLayerVisualComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	BouyancyLayerVisualComponent->SetRelativeScale3D(LayerDimensions / 50.0f);
	BouyancyLayerVisualComponent->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Engine/ArtTools/RenderToTexture/Materials/M_RadiusDebugTranslucent.M_RadiusDebugTransLucent")));
}

// Called when the game starts or when spawned
void ANetworkedPhysicsBouyancyLayer::BeginPlay()
{
	Super::BeginPlay();

	if (WaterGridData && !WaterGridData->Cells.IsEmpty())
	{
		ResolvePhysicsBody();
	}
}

void ANetworkedPhysicsBouyancyLayer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	BouyancyLayerCollisionComponent->SetBoxExtent(LayerDimensions);
	BouyancyLayerVisualComponent->SetRelativeScale3D(LayerDimensions / 50.0f);
}

void ANetworkedPhysicsBouyancyLayer::Destroyed()
{
	Super::Destroyed();

	if (BouyancyLayerPhysicsAsync)
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(BouyancyLayerPhysicsAsync);
				}
			}
		}
		BouyancyLayerPhysicsAsync = nullptr;
	}
}

void ANetworkedPhysicsBouyancyLayer::ResolvePhysicsBody()
{
	if (auto CurrentBody = BouyancyLayerCollisionComponent->GetBodyInstance())
	{
		if (CurrentBody->IsValidBodyInstance())
		{
			CurrentBody->SetUpdateKinematicFromSimulation(true);
							
			if (Chaos::FPhysicsObject* PhysicsBody = BouyancyLayerCollisionComponent->GetPhysicsObjectByName(NAME_None))
			{
				if (UWorld* World = GetWorld())
				{
					if (FPhysScene* Scene = World->GetPhysicsScene())
					{
						if (auto Solver = Scene->GetSolver())
						{
							BouyancyLayerPhysicsAsync = Solver->CreateAndRegisterSimCallbackObject_External<FBouyancyLayerPhysics>();
							if (BouyancyLayerPhysicsAsync)
							{
								auto GridSize = CurrentBody->GetBodyBounds().GetExtent().GetMax();
								BouyancyLayerPhysicsAsync->CurrentPhysicsObjectHandle = PhysicsBody;
								BouyancyLayerPhysicsAsync->PerProbeForce = PerProbeForce;
								BouyancyLayerPhysicsAsync->MaxForcePerObject = FMath::Max(0.0f, MaxForcePerObject);
								BouyancyLayerPhysicsAsync->GridSize = FMath::Max(1.0f, GridSize);
								BouyancyLayerPhysicsAsync->ForceGridDimension = FMath::Max(1, ForceGridDimension);
								BouyancyLayerPhysicsAsync->LayerDimensions = WaterGridData->LayerDimensions;
								BouyancyLayerPhysicsAsync->LayerGridDimension = WaterGridData->CellsPerAxis;
								BouyancyLayerPhysicsAsync->WaterGridCells = WaterGridData->Cells;
							}
						}
					}
				}
			}
		}
	}
}



// we have to register the callback object to the solver Chaos::ESimCallbackOptions: :ContactModification this works only if the callback object is registered to the solver
void FBouyancyLayerPhysics::OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier)
{
	//For a physics collision manual detection we have to collide first, then when a collision happens we have to check if the collision is with the bouyancy layer
	if (!CurrentPhysicsObjectHandle) 
	{
		return;
	}

	Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite(); 
	Chaos::FPBDRigidParticleHandle* RigidHandle = WriteInterface.GetRigidParticle(CurrentPhysicsObjectHandle); 
	
	if (!RigidHandle)
	{
		return;
	}
	
	Chaos::FContactPairModifierParticleRange CurrentContactCandidates = Modifier.GetContacts(RigidHandle);

	for (auto CurrentContactCandidate : CurrentContactCandidates)
	{
		auto ContactCount =  CurrentContactCandidate.GetNumContacts();
		if (ContactCount > 0)
		{
			Chaos::FGeometryParticleHandle* CurrentCandidate = CurrentContactCandidate.GetOtherParticle(RigidHandle);
			if (CurrentCandidate)
			{
				if (auto OtherBody = CurrentCandidate->CastToRigidParticle())
				{
					if (OtherBody->IsDynamic())
					{
						// we convert it to probe, cause probe will not apply any force however collision still happenning so we can do something with it
						CurrentContactCandidate.ConvertToProbe();
						BodiesToBuoy.Add(OtherBody);
					}
				}
			
			}
		}
	}

	

}

void FBouyancyLayerPhysics::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle PhysicsObject)
{
	if (CurrentPhysicsObjectHandle == PhysicsObject)
	{
		CurrentPhysicsObjectHandle = nullptr;
		BodiesToBuoy.Reset();
		return;
	}

	Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
	BodiesToBuoy.Remove(WriteInterface.GetRigidParticle(PhysicsObject));
}

void FBouyancyLayerPhysics::FirstPreResimStep_Internal(int32 PhysicsStep)
{
	BodiesToBuoy.Reset();
}


void FBouyancyLayerPhysics::OnPreSimulate_Internal()
{
	if (!CurrentPhysicsObjectHandle)
	{
		BodiesToBuoy.Reset();
		return;
	}

	Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
	Chaos::FPBDRigidParticleHandle* LayerBody = WriteInterface.GetRigidParticle(CurrentPhysicsObjectHandle);
	if (!LayerBody)
	{
		BodiesToBuoy.Reset();
		return;
	}

	for (Chaos::FPBDRigidParticleHandle* OtherBody : BodiesToBuoy)
	{
		if (OtherBody && OtherBody->IsDynamic())
		{
			ApplySubmergedProbeForces(LayerBody, OtherBody);
		}
	}
	BodiesToBuoy.Reset();

	#if CHAOS_DEBUG_DRAW
	if (Chaos::FDebugDrawQueue::IsDebugDrawingEnabled())
	{
		const Chaos::FVec3 SurfaceCenter = LayerBody->GetTransformXR().TransformPosition(Chaos::FVec3(0.0, 0.0, LayerDimensions.Z));
		Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(SurfaceCenter, FColor::Cyan, false, 0.05f, 1, 12.0f);
	}
	#endif
}
