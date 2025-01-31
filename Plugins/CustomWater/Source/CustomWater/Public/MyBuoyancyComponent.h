// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WaterActor.h"

#include "MyBuoyancyComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CUSTOMWATER_API UMyBuoyancyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMyBuoyancyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy Data")
	TArray<FVector> PontoonsLocations = { FVector::ZeroVector };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy Data")
	float RotationStrength = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy Data")
	bool DebugPoints = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy Data")
	UStaticMeshComponent* MyStaticMeshComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy Data")
	float WaterHeight = 0;

	UFUNCTION(BlueprintCallable, Category = "Buoyancy")
	FVector GetBuoyancyLocation(FVector RelativeLocation);
	UFUNCTION(BlueprintCallable, Category = "Buoyancy")
	FVector GetMultiBuoyancyLocation(TArray<FVector> PontoonsArray);
	UFUNCTION(BlueprintCallable, Category = "Buoyancy")
	TArray<FVector> GetBuoyancyArray(TArray<FVector> Points);

//Moving the boat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MovementSpeed = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MaxVelocity = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	//boolean for controlling forward movement speed
	bool bIsMovingForward = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MovementSmoothing = 0.1f;  // Lower = smoother but less responsive

	//Rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RotationSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MaxRotationRate = 180.0f;  // Maximum degrees per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsRotating = false;

private:
	AActor* ParentActor = nullptr;
	AWaterActor* WaterMeshActor;
	FTransform ActorTransform;
	FVector WorldActorLocation;
	FRotator WorldActorRotation;
	FVector RelativeStaticMeshLocation;
	FRotator RelativeStaticMeshRotation;
	FOceanFFTCalculator* FFTCalculator;
	bool bWaterZoneValid = true;
	FVector CurrentVelocity = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	void UpdateMovement(float DeltaTime);
	FVector LastPosition = FVector::ZeroVector;
	float CurrentRotationRate = 0.0f;
	FRotator TargetRotation = FRotator::ZeroRotator;

	FOceanFFTCalculator* InitializeWaterZoneReference();
	FVector FindAverageLocation(TArray<FVector> Locations);
	FQuat CalculateBuoyancyRotation(const TArray<FVector> Points);
	FQuat CalculateWaveRotation(const FVector& WavePoint);
	void DrawBuoyancyArrayDebugPoints(const TArray<FVector>& BuoyancyArray);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
