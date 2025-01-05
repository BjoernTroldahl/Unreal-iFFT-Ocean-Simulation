#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterMeshComponent.h"
#include "OceanFFTCalculator.h"
#include "WaterActor.generated.h"

UCLASS()
class CUSTOMWATER_API AWaterActor : public AActor
{
	GENERATED_BODY()

public:
	AWaterActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, Category = Water, BlueprintReadOnly)
	UWaterMeshComponent* WaterMesh;

	UPROPERTY(EditAnywhere, Category = Rendering)
	TObjectPtr<UMaterialInterface> WaterMaterial;

	UPROPERTY(Category = Debug, VisibleInstanceOnly, Transient, NonPIEDuplicateTransient, TextExportTransient, meta = (DisplayAfter = "WaterMaterial"))
	TObjectPtr<UMaterialInstanceDynamic> WaterMID;

	FOceanFFTCalculator FFTCalculator;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = Rendering)
	UMaterialInstanceDynamic* GetWaterMaterialInstance();

	UFUNCTION(BlueprintCallable, Category = Rendering)
	void CreateOrUpdateWaterMID();

	EObjectFlags GetTransientMIDFlags() const;

	void Update();

#if WITH_EDITOR
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};