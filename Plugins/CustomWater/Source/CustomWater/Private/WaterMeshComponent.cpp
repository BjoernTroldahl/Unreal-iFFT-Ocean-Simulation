// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterMeshComponent.h"
#include "EngineUtils.h"
#include "MaterialDomain.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "WaterMeshSceneProxy.h"
#include "WaterModule.h"
#include "WaterUtils.h"
#include "WaterActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterMeshComponent)

/** Scalability CVars*/
static TAutoConsoleVariable<int32> CVarWaterMeshLODCountBias(
	TEXT("r.Water.WaterMesh.LODCountBias"), 0,
	TEXT("This value is added to the LOD Count of each Water Mesh Component. Negative values will lower the quality(fewer and larger water tiles at the bottom level of the water quadtree), higher values will increase quality (more and smaller water tiles at the bottom level of the water quadtree)"),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarWaterMeshTessFactorBias(
	TEXT("r.Water.WaterMesh.TessFactorBias"), 0,
	TEXT("This value is added to the tessellation factor of each Mesh Component. Negative values will lower the overall density/resolution or the vertex grid, higher values will increase the density/resolution "),
	ECVF_Scalability);

static TAutoConsoleVariable<float> CVarWaterMeshLODScaleBias(
	TEXT("r.Water.WaterMesh.LODScaleBias"), 0.0f,
	TEXT("This value is added to the LOD Scale of each Mesh Component. Negative values will lower the overall density/resolution or the vertex grid and make the LODs smaller, higher values will increase the density/resolution and make the LODs larger. Smallest value is -0.5. That will make the inner LOD as tight and optimized as possible"),
	ECVF_Scalability);

/** Debug CVars */ 
static TAutoConsoleVariable<int32> CVarWaterMeshShowTileGenerationGeometry(
	TEXT("r.Water.WaterMesh.ShowTileGenerationGeometry"),
	0,
	TEXT("This debug option will display the geometry used for intersecting the water grid and generating tiles"),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarWaterMeshForceRebuildMeshPerFrame(
	TEXT("r.Water.WaterMesh.ForceRebuildMeshPerFrame"),
	0,
	TEXT("Force rebuilding the entire mesh each frame"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarWaterMeshEnabled(
	TEXT("r.Water.WaterMesh.Enabled"),
	1,
	TEXT("If the water mesh is enabled or disabled. This affects both rendering and the water tile generation"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarWaterEnabled(
	TEXT("r.Water.Water.Enabled"),
	1,
	TEXT("Water"),
	ECVF_RenderThreadSafe
);

extern TAutoConsoleVariable<float> CVarWaterSplineResampleMaxDistance;


// ----------------------------------------------------------------------------------

UWaterMeshComponent::UWaterMeshComponent()
{
	bAutoActivate = true;
	bHasPerInstanceHitProxies = true;

	SetMobility(EComponentMobility::Static);
}

void UWaterMeshComponent::PostLoad()
{
	Super::PostLoad();
}

void UWaterMeshComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FComponentPSOPrecacheParamsList& OutParams)
{
	const FVertexFactoryType* WaterVertexFactoryType = GetWaterVertexFactoryType(/*bWithWaterSelectionSupport = */ false);
	if (FarDistanceMaterial)
	{
		FComponentPSOPrecacheParams& ComponentParams = OutParams[OutParams.AddDefaulted()];
		ComponentParams.Priority = EPSOPrecachePriority::High;
		ComponentParams.MaterialInterface = FarDistanceMaterial;
		ComponentParams.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(WaterVertexFactoryType));
		ComponentParams.PSOPrecacheParams = BasePrecachePSOParams;
	}
	for (UMaterialInterface* MaterialInterface : UsedMaterials)
	{
		if (MaterialInterface)
		{
			FComponentPSOPrecacheParams& ComponentParams = OutParams[OutParams.AddDefaulted()];
			ComponentParams.Priority = EPSOPrecachePriority::High;
			ComponentParams.MaterialInterface = MaterialInterface;
			ComponentParams.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(WaterVertexFactoryType));
			ComponentParams.PSOPrecacheParams = BasePrecachePSOParams;
		}
	}
}

void UWaterMeshComponent::PostInitProperties()
{
	Super::PostInitProperties();

	UpdateBounds();
	MarkRenderTransformDirty();
}

FPrimitiveSceneProxy* UWaterMeshComponent::CreateSceneProxy()
{
	// Early out
	if (!bIsEnabled)
	{
		return nullptr;
	}

	return new FWaterMeshSceneProxy(this);
}

void UWaterMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	for (UMaterialInterface* Mat : UsedMaterials)
	{
		if (Mat)
		{
			OutMaterials.Add(Mat);
		}
	}
}

void UWaterMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	UE_LOG(LogWater, Warning, TEXT("SetMaterial is not compatible with UWaterMeshComponent since all materials on this component are auto-populated from the Water Bodies contained within it."));
}

#if WITH_EDITOR

bool UWaterMeshComponent::ShouldRenderSelected() const
{
	return true;
}

#endif // WITH_EDITOR

FMaterialRelevance UWaterMeshComponent::GetWaterMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Combine the material relevance for all materials.
	FMaterialRelevance Result;
	for (UMaterialInterface* Mat : UsedMaterials)
	{
		Result |= Mat->GetRelevance_Concurrent(InFeatureLevel);
	}

	return Result;
}

void UWaterMeshComponent::PushTessellatedWaterMeshBoundsToPoxy(const FBox2D& TessellatedWaterMeshBounds)
{
	if (SceneProxy)
	{
		static_cast<FWaterMeshSceneProxy*>(SceneProxy)->OnTessellatedWaterMeshBoundsChanged_GameThread(TessellatedWaterMeshBounds);
	}
}

void UWaterMeshComponent::SetExtentInTiles(FIntPoint NewExtentInTiles)
{
	ExtentInTiles = NewExtentInTiles;
	MarkWaterMeshGridDirty();
	MarkRenderStateDirty();
}

void UWaterMeshComponent::SetTileSize(float NewTileSize)
{
	TileSize = NewTileSize;
	MarkWaterMeshGridDirty();
	MarkRenderStateDirty();
}

FBoxSphereBounds UWaterMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Always return valid bounds (tree is initialized with invalid bounds and if nothing is inserted, the tree bounds will stay invalid)
	FBox NewBounds = WaterQuadTree.GetBounds();

	if (NewBounds.Min.Z >= NewBounds.Max.Z)
	{
		NewBounds.Min.Z = 0.0f;
		NewBounds.Max.Z = 100.0f;
	}
	// Add the far distance to the bounds if it's valid
	if (FarDistanceMaterial)
	{
		NewBounds = NewBounds.ExpandBy(FVector(FarDistanceMeshExtent, FarDistanceMeshExtent, 0.0f));
	}
	return NewBounds;
}

static bool IsMaterialUsedWithWater(const UMaterialInterface* InMaterial)
{
	return (InMaterial && InMaterial->CheckMaterialUsage_Concurrent(EMaterialUsage::MATUSAGE_Water));
}

void UWaterMeshComponent::RebuildWaterMesh(float InTileSize, const FIntPoint& InExtentInTiles)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RebuildWaterMesh);

	AActor* Actor = GetOwner <AActor>();
	AWaterActor* WaterActor = Cast<AWaterActor>(Actor);
	FVector ActorLocation = WaterActor->GetActorLocation();
	FVector2D ActorPosition = FVector2D(ActorLocation.X, ActorLocation.Y);

	// Position snapped to the grid
	const FVector2D GridPosition = FVector2D(FMath::GridSnap<FVector::FReal>(GetComponentLocation().X, InTileSize), FMath::GridSnap<FVector::FReal>(GetComponentLocation().Y, InTileSize));
	const FVector2D WorldExtent = FVector2D(InTileSize * InExtentInTiles.X, InTileSize * InExtentInTiles.Y);

	const FBox2D WaterWorldBox = FBox2D(-WorldExtent + GridPosition + ActorPosition, WorldExtent + GridPosition + ActorPosition);
	
	// This resets the tree to an initial state, ready for node insertion
	WaterQuadTree.InitTree(WaterWorldBox, InTileSize, InExtentInTiles);

	UsedMaterials.Empty();

	float FarMeshHeight = GetComponentLocation().Z;

	FWaterBodyRenderData RenderData;

	const EWaterBodyType WaterBodyType = EWaterBodyType::Ocean;

	//Assign material instance
	UMaterialInstanceDynamic* WaterMaterial = WaterActor->GetWaterMaterialInstance();
	RenderData.Material = UMaterial::GetDefaultMaterial(MD_Surface);

	if (WaterMaterial != nullptr)
	{
		RenderData.Material = WaterMaterial;
		if (!IsMaterialUsedWithWater(RenderData.Material))
		{
			RenderData.Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		UsedMaterials.Add(RenderData.Material);
	}
	
	//Min and max user defined priority range.
	const int32 MinPriority = -8192;
	const int32 MaxPriority = 8191;
	RenderData.Priority = 1;
	RenderData.WaterBodyIndex = 1;
	RenderData.SurfaceBaseHeight = Actor->GetActorLocation().Z;
	RenderData.WaterBodyType = static_cast<int8>(WaterBodyType);
#if WITH_WATER_SELECTION_SUPPORT
	RenderData.HitProxy = new HActor(Actor, nullptr);
	RenderData.bWaterBodySelected = Actor->IsSelected();
#endif 

	uint32 WaterBodyRenderDataIndex = WaterQuadTree.AddWaterBodyRenderData(RenderData);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Ocean);

		TArray<FVector2D> Polygon;
		TArray<FVector> PolyLineVertices;
		FBox OceanBounds;

		float PlaneScale = TileSize * 2 / 100;
		// Define the vertices of a square
		FVector Vertex1(-PlaneScale, -PlaneScale, 0.f);
		FVector Vertex2(PlaneScale, -PlaneScale, 0.f);
		FVector Vertex3(-PlaneScale, PlaneScale, 0.f);
		FVector Vertex4(PlaneScale, PlaneScale, 0.f);

		Vertex1 = ActorLocation + Vertex1;
		Vertex2 = ActorLocation + Vertex2;
		Vertex3 = ActorLocation + Vertex3;
		Vertex4 = ActorLocation + Vertex4;

		PolyLineVertices.Add(Vertex1);
		PolyLineVertices.Add(Vertex2);
		PolyLineVertices.Add(Vertex3);
		PolyLineVertices.Add(Vertex4);

		for (const FVector& Vertex : PolyLineVertices)
		{
			FVector2D Vertex2D(Vertex.X, Vertex.Y);
			Polygon.Add(Vertex2D);
		}

		//Calculate bounding box;
		FVector MinExtent = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		FVector MaxExtent = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (const FVector& Vertex : PolyLineVertices)
		{
			MinExtent.X = FMath::Min(MinExtent.X, Vertex.X);
			MinExtent.Y = FMath::Min(MinExtent.Y, Vertex.Y);
			MinExtent.Z = FMath::Min(MinExtent.Z, Vertex.Z);

			MaxExtent.X = FMath::Max(MaxExtent.X, Vertex.X);
			MaxExtent.Y = FMath::Max(MaxExtent.Y, Vertex.Y);
			MaxExtent.Z = FMath::Max(MaxExtent.Z, Vertex.Z);
		}

		OceanBounds = FBox(MinExtent, MaxExtent);

		WaterQuadTree.AddWaterTilesInsideBounds(OceanBounds, WaterBodyRenderDataIndex);
		//far mesh height
		float MaxWaveHeight = 10.f;
		FarMeshHeight = RenderData.SurfaceBaseHeight - MaxWaveHeight;
	}

	// Build the far distance mesh instances if needed
	if ((bUseFarMesh) && (IsMaterialUsedWithWater(FarDistanceMaterial) && FarDistanceMeshExtent > 0.0f))
	{
		UsedMaterials.Add(FarDistanceMaterial);

		WaterQuadTree.AddFarMesh(FarDistanceMaterial, FarDistanceMeshExtent, FarMeshHeight, CenterMeshExtent, ActorPosition);
	}

	WaterQuadTree.Unlock(true);

	MarkRenderStateDirty();
}

void UWaterMeshComponent::Update()
{
	bIsEnabled = FWaterUtils::IsWaterMeshEnabled(/*bIsRenderThread = */false);

	// Early out
	if (!bIsEnabled)
	{
		return;
	}

	const int32 NewLODCountBias = CVarWaterMeshLODCountBias.GetValueOnGameThread();
	const int32 NewTessFactorBias = CVarWaterMeshTessFactorBias.GetValueOnGameThread();
	const float NewLODScaleBias = CVarWaterMeshLODScaleBias.GetValueOnGameThread();
	if (bNeedsRebuild
		|| !!CVarWaterMeshShowTileGenerationGeometry.GetValueOnGameThread()
		|| !!CVarWaterMeshForceRebuildMeshPerFrame.GetValueOnGameThread()
		|| (NewLODCountBias != LODCountBiasScalability)
		|| (NewTessFactorBias != TessFactorBiasScalability)
		|| (NewLODScaleBias != LODScaleBiasScalability))
	{
		LODCountBiasScalability = NewLODCountBias;
		TessFactorBiasScalability = NewTessFactorBias;
		LODScaleBiasScalability = NewLODScaleBias;
		const float LODCountBiasFactor = FMath::Pow(2.0f, (float)LODCountBiasScalability);
		RebuildWaterMesh(TileSize / LODCountBiasFactor, FIntPoint(FMath::CeilToInt(ExtentInTiles.X * LODCountBiasFactor), FMath::CeilToInt(ExtentInTiles.Y * LODCountBiasFactor)));
		PrecachePSOs();
		bNeedsRebuild = false;
	}
}


#if WITH_EDITOR
void UWaterMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// Properties that needs the scene proxy to be rebuilt
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, LODScale)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, TessellationFactor)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, TileSize)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, ExtentInTiles)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, ForceCollapseDensityLevel)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, FarDistanceMaterial)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, FarDistanceMeshExtent)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, CenterMeshExtent)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, bUseFarMesh))
		{
			MarkWaterMeshGridDirty();
			MarkRenderStateDirty();
		}
	}
}
#endif

