// Copyright Amir Ben-Kiki and unDAW org.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"






class TKMaximizerModule : public IModuleInterface
{
public:


	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
