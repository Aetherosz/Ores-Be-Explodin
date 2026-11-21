#include "AutoLootOresModule.h"

#include "AutoLootOres.h"

void FAutoLootOresModule::StartupModule()
{
	FAutoLootOres::RegisterHooks();
}

IMPLEMENT_MODULE(FAutoLootOresModule, AutoLootOres)
