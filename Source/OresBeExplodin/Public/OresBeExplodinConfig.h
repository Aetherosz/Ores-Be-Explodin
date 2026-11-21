#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "OresBeExplodinConfig.generated.h"

/**
 * What to do with the deposits in range. The menu shows these names with spaces added
 * ("Loot All Except Uranium"); the stored value is the index.
 */
UENUM()
enum class EOresBeExplodinMode : uint8
{
	/** Auto Loot All! (Warning: Uranium) */
	LootAll,
	/** Auto Destroy All! (doesn't loot, just destroys) */
	DestroyAll,
	/** Auto Loot All - Except Uranium (leaves Uranium alone) */
	LootAllExceptUranium,
	/** Auto Loot All - Destroy Uranium */
	LootAllDestroyUranium,
};

/**
 * Shows up in the game's Mods -> Mod Configuration menu (main menu and pause menu).
 * SML saves it to FactoryGame/Configs/OresBeExplodin.cfg.
 */
UCLASS()
class UOresBeExplodinConfig : public UModConfiguration
{
	GENERATED_BODY()

public:
	UOresBeExplodinConfig();

	/** Read-only line: whether auto-looting is active, and the hand-mined count. */
	static const FString StatusKey;
	static const FString EnabledKey;
	/** EOresBeExplodinMode as an integer. */
	static const FString ModeKey;
	/** Range in meters. */
	static const FString RangeKey;
	/** Deposits to mine out by hand (per save) before auto-looting starts. 0 = right away. */
	static const FString ManualFirstKey;

	static constexpr int32 DefaultMode = static_cast<int32>(EOresBeExplodinMode::LootAllExceptUranium);
	static constexpr int32 MinRange = 1;
	static constexpr int32 MaxRange = 100;
	static constexpr int32 DefaultRange = 10;
	static constexpr int32 MaxManualFirst = 100;

	static FConfigId GetId();

	/**
	 * SML's C++ property classes have no menu widget (only its Blueprint subclasses do), so a
	 * config built in C++ shows up empty. Swaps the default object's properties for SML's
	 * BP_ConfigProperty* classes. Call before registering, once SML's content is loaded.
	 */
	static void UseMenuWidgets();
};
