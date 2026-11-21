#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Subsystem/ModSubsystem.h"
#include "AutoLootOresSubsystem.generated.h"

/** Counts the deposits mined out by hand in this save. Written into the save file. Server only. */
UCLASS()
class AAutoLootOresSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AAutoLootOresSubsystem();

	/** The subsystem for this world, or null (main menu, clients, not registered yet). */
	static AAutoLootOresSubsystem* Get(const UObject* WorldContext);
	/** Registers the subsystem, adopting the instance loaded from the save if there is one. Server only. */
	static AAutoLootOresSubsystem* GetOrRegister(UWorld* World);

	int32 GetManuallyMined() const { return ManuallyMined; }
	void AddManuallyMined() { ++ManuallyMined; }

	//~ Begin IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void GatherDependencies_Implementation(TArray<UObject*>& OutDependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }
	//~ End IFGSaveInterface

private:
	UPROPERTY(SaveGame)
	int32 ManuallyMined = 0;
};
