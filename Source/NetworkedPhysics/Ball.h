// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Ball.generated.h"

UCLASS()
class NETWORKEDPHYSICS_API ABall : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABall();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION()
	void OnBallHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
				   UPrimitiveComponent* OtherComp, FVector NormalImpulse,
				   const FHitResult& Hit);

	UPROPERTY(EditAnywhere)
	float InitialSpeed = 800.f;

	UPROPERTY(EditAnywhere)
	float Acceleration = 200.f;
	
	UPROPERTY(EditAnywhere)
	FVector LaunchDirection = FVector(1.f, 0.f, 0.1f);

private:
	float CurrentSpeed;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* CollisionSphere;
};
