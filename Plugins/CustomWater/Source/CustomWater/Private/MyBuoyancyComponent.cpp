// Fill out your copyright notice in the Description page of Project Settings.
#include "MyBuoyancyComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"


// Sets default values for this component's properties
UMyBuoyancyComponent::UMyBuoyancyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	ParentActor = GetOwner();
}


// Called when the game starts
void UMyBuoyancyComponent::BeginPlay()
{
	Super::BeginPlay();
	ActorTransform = ParentActor->GetActorTransform();
	WorldActorLocation = ParentActor->GetActorLocation();
	WorldActorRotation = ParentActor->GetActorRotation();
	LastPosition = WorldActorLocation;
	TargetLocation = WorldActorLocation;

	ActorTransform.SetLocation(FVector(ActorTransform.GetLocation().X, ActorTransform.GetLocation().Y, WaterHeight));
	if (MyStaticMeshComponent->IsValidLowLevelFast())
	{
		RelativeStaticMeshLocation = MyStaticMeshComponent->GetRelativeLocation();
		RelativeStaticMeshRotation = MyStaticMeshComponent->GetRelativeRotation();
	}
	ParentActor->SetActorLocation(FVector(WorldActorLocation.X, WorldActorLocation.Y, WaterHeight));
	FFTCalculator = InitializeWaterZoneReference();

	UE_LOG(LogTemp, Warning, TEXT("Initial Position: X=%f, Y=%f, Z=%f"),
	WorldActorLocation.X, WorldActorLocation.Y, WorldActorLocation.Z);
}

// Called every frame
void UMyBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bWaterZoneValid || !MyStaticMeshComponent || !MyStaticMeshComponent->IsValidLowLevelFast())
	{
		return;
	}

	// Update movement first
	UpdateMovement(DeltaTime);

	// Debug log velocity
	UE_LOG(LogTemp, Warning, TEXT("Current Velocity: Y=%f"), CurrentVelocity.Y);

	// Get current position
	WorldActorLocation = ParentActor->GetActorLocation();
	WorldActorRotation = ParentActor->GetActorRotation();
	ActorTransform = ParentActor->GetActorTransform();

	if (PontoonsLocations.Num() > 2)
	{
		const TArray<FVector> BuoyancyArray = GetBuoyancyArray(PontoonsLocations);
		const FQuat ActorQuat = CalculateBuoyancyRotation(BuoyancyArray);
		FVector BuoyancyLocation = GetMultiBuoyancyLocation(PontoonsLocations);

		// Get the forward vector based on current rotation - make sure the boat model is imported correctly, otherwise it will use another axis as the forward vector
		FVector ForwardVector = MyStaticMeshComponent->GetForwardVector();

		// Calculate movement in the direction the boat is facing
		FVector MovementDelta = ForwardVector * CurrentVelocity.Y * DeltaTime;

		// Keep the current world location and add our movement to it
		FVector CurrentLocation = MyStaticMeshComponent->GetComponentLocation();

		// Calculate the target position including both buoyancy and movement
		TargetLocation = FVector(
			CurrentLocation.X + MovementDelta.X,
			CurrentLocation.Y + MovementDelta.Y,
			BuoyancyLocation.Z + WaterHeight
		);

	    // Smoothly interpolate to the target location
            FVector NewLocation = FMath::VInterpTo(
            CurrentLocation,
            TargetLocation,
            DeltaTime,
            1.0f / MovementSmoothing
        );

		FRotator BuoyancyRotation = ActorQuat.Rotator() * RotationStrength + WorldActorRotation;
		FRotator NewRotation = FRotator(
			BuoyancyRotation.Pitch,
			TargetRotation.Yaw, // Use our controlled Yaw
			BuoyancyRotation.Roll  
		);

		// Apply the new transform
		MyStaticMeshComponent->SetWorldLocation(NewLocation);
		MyStaticMeshComponent->SetWorldRotation(NewRotation);

		// Debug log
		UE_LOG(LogTemp, Warning, TEXT("Movement - Velocity: %f, Location: X=%f, Y=%f"),
			CurrentVelocity.Y, NewLocation.X, NewLocation.Y);

		//If the static mesh is a BUOY, it shouldn't move.
		if (MyStaticMeshComponent && MyStaticMeshComponent->ComponentHasTag(FName("Buoy")))
		{
			BuoyancyLocation = FVector(BuoyancyLocation.X, BuoyancyLocation.Y, BuoyancyLocation.Z + WaterHeight);
			MyStaticMeshComponent->SetWorldLocation(BuoyancyLocation);
			MyStaticMeshComponent->SetWorldRotation(BuoyancyRotation);
		}

		// Debug visualization
		if (DebugPoints)
		{
			TArray<FVector> DebugArray;
			for (FVector Location : BuoyancyArray)
			{
				Location = FVector(
					Location.X + MovementDelta.X,
					Location.Y + MovementDelta.Y,
					Location.Z + WaterHeight
				);
				DebugArray.Add(Location);
			}
			DrawBuoyancyArrayDebugPoints(DebugArray);
		}
	}
}



FVector UMyBuoyancyComponent::GetBuoyancyLocation(FVector RelativeLocation)
{
	FVector BuoyancyLocation = FVector::ZeroVector;
	FVector WorldLocation = ActorTransform.TransformPosition(RelativeLocation);
	if (FFTCalculator == nullptr) { return BuoyancyLocation; }
	else {

		FVector GridPointLocation = FVector(WorldLocation.X, WorldLocation.Y, -RelativeLocation.Z) / FFTCalculator->Scale * FFTCalculator->MultiplyScale;
		FVector Displacement = FFTCalculator->GetDisplacementAtPoint(GridPointLocation);

		BuoyancyLocation = GridPointLocation * FFTCalculator->Scale / FFTCalculator->MultiplyScale + Displacement / FFTCalculator->Scale / FFTCalculator->OverlapScale;

		return BuoyancyLocation;
	}
}

FVector UMyBuoyancyComponent::GetMultiBuoyancyLocation(TArray<FVector> PontoonsArray)
{
	FVector BuoyancyLocation = FVector::ZeroVector;
	FVector AveragePontoons = FindAverageLocation(PontoonsArray);

	BuoyancyLocation = GetBuoyancyLocation(AveragePontoons);
	return BuoyancyLocation;
}



FOceanFFTCalculator* UMyBuoyancyComponent::InitializeWaterZoneReference()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaterActor::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		for (AActor* FoundActor : FoundActors)
		{
			WaterMeshActor = Cast<AWaterActor>(FoundActor);
			if (WaterMeshActor)
			{
				return &WaterMeshActor->FFTCalculator;
			}
		}
	}
	bWaterZoneValid = false;
	UE_LOG(LogTemp, Warning, TEXT("WaterZone isn't valid"));
	return nullptr;
}

FVector UMyBuoyancyComponent::FindAverageLocation(TArray<FVector> Locations)
{
	FVector AverageLocation = FVector::ZeroVector;
	for (FVector& Location : Locations)
	{
		AverageLocation = AverageLocation + Location;
	}
	AverageLocation = AverageLocation / Locations.Num();
	return AverageLocation;
}

TArray<FVector> UMyBuoyancyComponent::GetBuoyancyArray(TArray<FVector> Points)
{
	TArray<FVector> PointArray = {};
	for (FVector& Point : Points)
	{
		PointArray.Add(GetBuoyancyLocation(Point));
	}
	return PointArray;
}

FQuat UMyBuoyancyComponent::CalculateBuoyancyRotation(const TArray<FVector> Points)
{
	FQuat AverageRotation = FQuat::Identity;

	for (const FVector& WavePoint : Points)
	{
		float DistanceToCenter = FVector::Dist(ParentActor->GetActorLocation(), WavePoint);
		float Weight = 1.0f / (DistanceToCenter + SMALL_NUMBER);

		FQuat WaveRotation = CalculateWaveRotation(WavePoint);

		// Accumulate the weighted rotation
		AverageRotation = FQuat::Slerp(AverageRotation, WaveRotation, Weight);
	}
	return AverageRotation;
}

FQuat UMyBuoyancyComponent::CalculateWaveRotation(const FVector& WavePoint)
{
	FVector TargetVector(0.0f, 0.0f, 1.0f);
	FVector WaveDirection = (WavePoint - ParentActor->GetActorLocation()).GetSafeNormal();
	FQuat WaveRotation = FQuat::FindBetween(WaveDirection, TargetVector);

	return WaveRotation;
}

void UMyBuoyancyComponent::DrawBuoyancyArrayDebugPoints(const TArray<FVector>& BuoyancyArray)
{
	for (const FVector& Point : BuoyancyArray)
	{
		DrawDebugPoint(GetWorld(), Point, 50.f, FColor(255.f, 0.f, 0.f, 255.f), false, 0.f, 0);
	}
}

//Boat movement
void UMyBuoyancyComponent::UpdateMovement(float DeltaTime)
{
	const float MinSpeedThreshold = 1.0f;
	const float MinRotationThreshold = 0.1f;

	if (bIsMovingForward)
	{

		// Accelerate towards max velocity
		CurrentVelocity.Y = FMath::FInterpTo(
			CurrentVelocity.Y,
			MaxVelocity,
			DeltaTime,
			MovementSpeed
		);
	}
	else
	{
		
	// Decelerate to zero
        CurrentVelocity.Y = FMath::FInterpTo(
            CurrentVelocity.Y,
            0.0f,
            DeltaTime,
            MovementSpeed
        );

		// Stop completely if very slow
		if (FMath::Abs(CurrentVelocity.Y) < MinSpeedThreshold)
		{
			CurrentVelocity.Y = 0.0f;
		}
	}

	// Handle rotation
	float TargetRotationRate = bIsRotating ? MaxRotationRate : 0.0f;

	// Smoothly interpolate rotation rate
	CurrentRotationRate = FMath::FInterpTo(
		CurrentRotationRate,
		TargetRotationRate,
		DeltaTime,
		RotationSpeed
	);

	// Stop rotation if rate is very small
	if (FMath::Abs(CurrentRotationRate) < MinRotationThreshold)
	{
		CurrentRotationRate = 0.0f;
	}

	// Update target rotation (now using Yaw instead of Roll)
	if (MyStaticMeshComponent && MyStaticMeshComponent->IsValidLowLevelFast())
	{
		FRotator CurrentRotation = MyStaticMeshComponent->GetComponentRotation();
		TargetRotation = CurrentRotation + FRotator(0.0f, CurrentRotationRate * DeltaTime, 0.0f);
	}

	UE_LOG(LogTemp, Warning, TEXT("Velocity: Y=%f, RotationRate: Z=%f"), CurrentVelocity.Y, CurrentRotationRate);

}
