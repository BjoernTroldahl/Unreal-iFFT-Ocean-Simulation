#include "WaterSubsystem.h"
#include "WaterActor.h"

void UWaterSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (AWaterActor* WaterActor : TActorRange<AWaterActor>(GetWorld()))
	{
		if (WaterActor)
		{
			WaterActor->Update();
		}
	}
}

TStatId UWaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWaterSubsystem, STATGROUP_Tickables);
}