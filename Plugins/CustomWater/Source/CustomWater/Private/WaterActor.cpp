#include "WaterActor.h"
#include "WaterUtils.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#endif

AWaterActor::AWaterActor()
{
	PrimaryActorTick.bCanEverTick = true;

	WaterMesh = CreateDefaultSubobject<UWaterMeshComponent>(TEXT("WaterMesh"));
	SetRootComponent(WaterMesh);

	FFTCalculator.Initialize();

#if WITH_EDITOR
	if (GIsEditor && !IsTemplate())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.OnActorSelectionChanged().AddUObject(this, &AWaterActor::OnActorSelectionChanged);
	}
#endif
}

void AWaterActor::BeginPlay()
{
	Super::BeginPlay();
}

void AWaterActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	FFTCalculator.Calculate(GetWorld());
	FFTCalculator.ShowDebugDisplacementPoints(GetWorld(), GetActorLocation());
}

UMaterialInstanceDynamic* AWaterActor::GetWaterMaterialInstance()
{
	CreateOrUpdateWaterMID();
	return WaterMID;
}

void AWaterActor::CreateOrUpdateWaterMID()
{
	if (GetWorld())
	{
		WaterMID = FWaterUtils::GetOrCreateTransientMID(WaterMID, TEXT("WaterMID"), WaterMaterial, GetTransientMIDFlags());
	}
}

EObjectFlags AWaterActor::GetTransientMIDFlags() const
{
	return RF_Transient | RF_NonPIEDuplicateTransient | RF_TextExportTransient;
}

void AWaterActor::Update()
{
	if (WaterMesh)
	{
		WaterMesh->Update();
	}
}

#if WITH_EDITOR
void AWaterActor::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	for (UObject* SelectedObject : NewSelection)
	{
		AActor* SelectedActor = Cast<AActor>(SelectedObject);

		if (SelectedActor)
		{
			WaterMesh->MarkWaterMeshGridDirty();
		}
	}
}

void AWaterActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	WaterMesh->MarkWaterMeshGridDirty();
}

void AWaterActor::PostEditUndo()
{
	Super::PostEditUndo();

	WaterMesh->MarkWaterMeshGridDirty();
}

void AWaterActor::PostEditImport()
{
	Super::PostEditImport();

	WaterMesh->MarkWaterMeshGridDirty();
}

void AWaterActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	WaterMesh->MarkWaterMeshGridDirty();
}
#endif