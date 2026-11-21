#include "OresBeExplodingModule.h"

#include "OresBeExploding.h"

void FOresBeExplodingModule::StartupModule()
{
	FOresBeExploding::RegisterHooks();
}

IMPLEMENT_MODULE(FOresBeExplodingModule, OresBeExploding)
