#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAutoLootOresModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
};
