// Fill out your copyright notice in the Description page of Project Settings.


#include "Ball.h"

#include "Components/SphereComponent.h"


// Sets default values
ABall::ABall()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	CollisionSphere = CreateDefaultSubobject<USphereComponent>("CollisionSphere");
	CollisionSphere->InitSphereRadius(50.0f);
	CollisionSphere->SetCollisionProfileName("BlockAll");
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetHiddenInGame(false);
	CollisionSphere->SetLineThickness(2.f);
	CollisionSphere->SetSimulatePhysics(false);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
}

// Called when the game starts or when spawned
void ABall::BeginPlay()
{
	Super::BeginPlay();
	if (GetInstigator())
	{
		LaunchDirection = GetInstigator()->GetActorForwardVector();
	}
	CurrentSpeed = InitialSpeed;
    CollisionSphere->OnComponentHit.AddDynamic(this, &ABall::OnBallHit);
}

// Called every frame
void ABall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (CurrentSpeed > 0.0f)
	{
		CurrentSpeed += Acceleration * DeltaTime;
		AddActorWorldOffset(LaunchDirection * CurrentSpeed * DeltaTime, true, nullptr, ETeleportType::None );
	}
}

void ABall::OnBallHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	
	// Just set direction to normal
	FVector BallLocation = GetActorLocation();
	LaunchDirection = Hit.Normal;
}

