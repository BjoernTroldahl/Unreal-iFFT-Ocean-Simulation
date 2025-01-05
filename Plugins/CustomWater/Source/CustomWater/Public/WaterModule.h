// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

class IWaterEditorServices;

//////////////////////////////////////////////////////////////////////////
// IWaterModuleInterface

CUSTOMWATER_API DECLARE_LOG_CATEGORY_EXTERN(LogWater, Log, All);


class IWaterModuleInterface : public IModuleInterface
{
public:
#if WITH_EDITOR
	virtual CUSTOMWATER_API void SetWaterEditorServices(IWaterEditorServices* InWaterEditorServices) = 0;
	virtual CUSTOMWATER_API IWaterEditorServices* GetWaterEditorServices() const = 0;
#endif // WITH_EDITOR
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#endif
