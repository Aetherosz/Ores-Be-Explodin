#pragma once

#include "CoreMinimal.h"
#include "AutoLootOresConfig.h"

class AFGCharacterPlayer;
class AFGResourceDeposit;
class UConfigManager;
class UConfigProperty;
class UGameInstance;
class UWorld;

/**
 * Mines the hand-mineable resource deposits near players. Runs on the server (single player,
 * host or dedicated server), so in multiplayer only the host needs it and its settings count.
 */
class FAutoLootOres
{
public:
	static void RegisterHooks();

private:
	enum class EMenuMode { MainMenu, Client, InSave };
	enum class EDepositAction { Skip, Loot, Destroy };

	/** How often deposits are looked for, and how many are handled per pass (keeps frames smooth). */
	static constexpr float ScanInterval = 0.5f;
	static constexpr int32 DepositsPerScan = 3;
	/** Mining cycles per deposit per pass. A bigger deposit is finished on the next pass. */
	static constexpr int32 MaxCyclesPerScan = 200;

	static void RegisterConfig(UGameInstance* GameInstance);
	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	/** Timer on the server: handles the deposits in range of each player. */
	static void Scan(UWorld* World);
	static EDepositAction GetAction(EAutoLootOresMode Mode, const AFGResourceDeposit* Deposit);

	/**
	 * Mines the deposit into the player's inventory through the game's own mining function, until
	 * it is empty or nothing more fits. Returns true if anything was mined.
	 */
	static bool MineOut(AFGResourceDeposit* Deposit, AFGCharacterPlayer* Player);
	/** Empties the deposit without leaving the player any items. */
	static void DestroyDeposit(AFGResourceDeposit* Deposit, AFGCharacterPlayer* Player);
	/** Empties the deposit directly: used when it can't be mined through the player (no room). */
	static void ForceDeplete(AFGResourceDeposit* Deposit);

	static int32 GetResourcesLeft(const AFGResourceDeposit* Deposit);
	static bool IsMinedOut(AFGResourceDeposit* Deposit);

	/** A deposit was mined out by hand (not by this mod). */
	static void OnManuallyMined(UWorld* World);

	static UConfigProperty* GetConfigProperty(const FString& Key);
	static int32 GetConfigInt(const FString& Key, int32 Fallback);
	static bool GetConfigBool(const FString& Key, bool bFallback);
	static void SetStatusText(const FString& Text);
	static void UpdateStatus(EMenuMode Mode, UWorld* World = nullptr);

	static TWeakObjectPtr<UConfigManager> ConfigManager;
	/** The world the scan timer runs in. */
	static TWeakObjectPtr<UWorld> ScannedWorld;
	/** True while this mod is mining, so its own mining isn't counted as mining by hand. */
	static bool bAutoMining;
};
