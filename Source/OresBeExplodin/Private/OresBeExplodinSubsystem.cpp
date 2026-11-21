#include "OresBeExplodinSubsystem.h"

#include "Engine/World.h"
#include "Subsystem/SubsystemActorManager.h"

AOresBeExplodinSubsystem::AOresBeExplodinSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

AOresBeExplodinSubsystem* AOresBeExplodinSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<AOresBeExplodinSubsystem>() : nullptr;
}

AOresBeExplodinSubsystem* AOresBeExplodinSubsystem::GetOrRegister(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>();
	if (!Manager)
	{
		return nullptr;
	}
	if (AOresBeExplodinSubsystem* Existing = Manager->GetSubsystemActor<AOresBeExplodinSubsystem>())
	{
		return Existing;
	}
	// Finds the actor loaded from the save by name, or spawns a new one.
	Manager->RegisterSubsystemActor(StaticClass());
	return Manager->GetSubsystemActor<AOresBeExplodinSubsystem>();
}
