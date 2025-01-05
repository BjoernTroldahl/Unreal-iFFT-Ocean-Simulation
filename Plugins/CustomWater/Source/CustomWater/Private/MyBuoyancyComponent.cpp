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

	ActorTransform.SetLocation(FVector(ActorTransform.GetLocation().X, ActorTransform.GetLocation().Y, WaterHeight));
	if (MyStaticMeshComponent->IsValidLowLevelFast())
	{
		RelativeStaticMeshLocation = MyStaticMeshComponent->GetRelativeLocation();
		RelativeStaticMeshRotation = MyStaticMeshComponent->GetRelativeRotation();
	}
	ParentActor->SetActorLocation(FVector(WorldActorLocation.X, WorldActorLocation.Y, WaterHeight));
	FFTCalculator = InitializeWaterZoneReference();
}

// Called every frame
void UMyBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bWaterZoneValid)
	{
		if (ActorTransform.GetLocation() != ParentActor->GetActorLocation())
		{
			ActorTransform = ParentActor->GetActorTransform();
			WorldActorLocation = ParentActor->GetActorLocation();
			WorldActorRotation = ParentActor->GetActorRotation();
			ParentActor->SetActorLocation(FVector(WorldActorLocation.X, WorldActorLocation.Y, WaterHeight));
		}

		if (PontoonsLocations.Num() > 2)
		{
			const TArray<FVector> BuoyancyArray = GetBuoyancyArray(PontoonsLocations);
			const FQuat ActorQuat = CalculateBuoyancyRotation(BuoyancyArray);
			FVector BuoyancyLocation = GetMultiBuoyancyLocation(PontoonsLocations);
			BuoyancyLocation = FVector(BuoyancyLocation.X, BuoyancyLocation.Y, BuoyancyLocation.Z + WaterHeight);
			const FRotator BuoyancyRotation = ActorQuat.Rotator() * RotationStrength + WorldActorRotation;

			if (MyStaticMeshComponent->IsValidLowLevelFast())
			{
				MyStaticMeshComponent->SetWorldLocation(BuoyancyLocation);
				MyStaticMeshComponent->SetWorldRotation(BuoyancyRotation);
			}

			TArray<FVector> DebugArray = {};
			for (FVector Location : BuoyancyArray)
			{
				Location = FVector(Location.X, Location.Y, Location.Z + WaterHeight);
				DebugArray.Add(Location);
			}
			if (DebugPoints) { DrawBuoyancyArrayDebugPoints(DebugArray); }
		}
		else
		{
			if (MyStaticMeshComponent->IsValidLowLevelFast())
			{
				const FVector BuoyancyLocation = GetBuoyancyLocation(PontoonsLocations[0]);
				MyStaticMeshComponent->SetWorldLocation(BuoyancyLocation);
			}
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