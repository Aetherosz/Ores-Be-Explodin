#include "OresBeExplodingSubsystem.h"

#include "Engine/World.h"
#include "Subsystem/SubsystemActorManager.h"

AOresBeExplodingSubsystem::AOresBeExplodingSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

AOresBeExplodingSubsystem* AOresBeExplodingSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<AOresBeExplodingSubsystem>() : nullptr;
}

AOresBeExplodingSubsystem* AOresBeExplodingSubsystem::GetOrRegister(UWorld* World)
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
	if (AOresBeExplodingSubsystem* Existing = Manager->GetSubsystemActor<AOresBeExplodingSubsystem>())
	{
		return Existing;
	}
	// Finds the actor loaded from the save by name, or spawns a new one.
	Manager->RegisterSubsystemActor(StaticClass());
	return Manager->GetSubsystemActor<AOresBeExplodingSubsystem>();
}
