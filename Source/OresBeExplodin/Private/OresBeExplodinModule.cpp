#include "OresBeExplodinModule.h"

#include "OresBeExplodin.h"

void FOresBeExplodinModule::StartupModule()
{
	FOresBeExplodin::RegisterHooks();
}

IMPLEMENT_MODULE(FOresBeExplodinModule, OresBeExplodin)
