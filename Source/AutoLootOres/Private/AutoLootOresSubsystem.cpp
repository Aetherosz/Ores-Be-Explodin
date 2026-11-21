#include "AutoLootOresSubsystem.h"

#include "Engine/World.h"
#include "Subsystem/SubsystemActorManager.h"

AAutoLootOresSubsystem::AAutoLootOresSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

AAutoLootOresSubsystem* AAutoLootOresSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<AAutoLootOresSubsystem>() : nullptr;
}

AAutoLootOresSubsystem* AAutoLootOresSubsystem::GetOrRegister(UWorld* World)
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
	if (AAutoLootOresSubsystem* Existing = Manager->GetSubsystemActor<AAutoLootOresSubsystem>())
	{
		return Existing;
	}
	// Finds the actor loaded from the save by name, or spawns a new one.
	Manager->RegisterSubsystemActor(StaticClass());
	return Manager->GetSubsystemActor<AAutoLootOresSubsystem>();
}
