#include "AutoLootOres.h"

#include "AutoLootOresSubsystem.h"

#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Patching/NativeHookManager.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGResourceDeposit.h"
#include "Resources/FGResourceDescriptor.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutoLootOres, Log, All);

TWeakObjectPtr<UConfigManager> FAutoLootOres::ConfigManager;
TWeakObjectPtr<UWorld> FAutoLootOres::ScannedWorld;
bool FAutoLootOres::bAutoMining = false;

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

void FAutoLootOres::RegisterHooks()
{
	// Register the mod config as soon as the game instance exists, so it is
	// listed in the main menu's Mods -> Mod Configuration before any save loads.
	FWorldDelegates::OnStartGameInstance.AddStatic(&FAutoLootOres::RegisterConfig);
	FWorldDelegates::OnWorldCleanup.AddStatic(&FAutoLootOres::OnWorldCleanup);

#if !WITH_EDITOR
	// Write config changes right away. SML otherwise saves on a timer that doesn't run while the
	// game is paused, and its flush at shutdown crashes, so "change a value in the pause menu,
	// then Exit to Desktop" would crash the game.
	SUBSCRIBE_METHOD_AFTER(UConfigManager::MarkConfigurationDirty,
		[](UConfigManager* Self, const FConfigId& Id)
		{
			if (Id == UAutoLootOresConfig::GetId())
			{
				Self->FlushPendingSaves();
			}
		});

	// On the first player spawn in a world: set up the per-save counter and start scanning.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGCharacterPlayer::BeginPlay, GetMutableDefault<AFGCharacterPlayer>(),
		[](AFGCharacterPlayer* Self)
		{
			UWorld* World = Self->GetWorld();
			if (!World)
			{
				return;
			}
			if (!ConfigManager.IsValid())
			{
				RegisterConfig(Self->GetGameInstance()); // fallback if OnStartGameInstance fired before we loaded
			}
			if (World->GetNetMode() == NM_Client)
			{
				UpdateStatus(EMenuMode::Client);
				return;
			}
			if (ScannedWorld.Get() != World)
			{
				ScannedWorld = World;
				AAutoLootOresSubsystem::GetOrRegister(World);
				// World timers stop while single player is paused, so nothing is mined behind a menu.
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle,
					FTimerDelegate::CreateWeakLambda(World, [World]() { Scan(World); }),
					ScanInterval, true);
				UpdateStatus(EMenuMode::InSave, World);
			}
		});

	// Counts deposits mined out by hand, for the "mine X by hand first" setting.
	SUBSCRIBE_METHOD_VIRTUAL(AFGResourceDeposit::ExtractResourceAndGiveToPlayer, GetMutableDefault<AFGResourceDeposit>(),
		[](auto& Scope, AFGResourceDeposit* Self, AFGCharacterPlayer* Player, int32 Amount)
		{
			if (bAutoMining || !Self->HasAuthority())
			{
				return; // the game's function runs as usual
			}
			const bool bHadResources = !IsMinedOut(Self);
			Scope(Self, Player, Amount);
			if (bHadResources && IsMinedOut(Self))
			{
				OnManuallyMined(Self->GetWorld());
			}
		});
#endif
}

void FAutoLootOres::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (ScannedWorld.Get() == World)
	{
		ScannedWorld.Reset();
	}
	UpdateStatus(EMenuMode::MainMenu);
}

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------

void FAutoLootOres::Scan(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	UpdateStatus(EMenuMode::InSave, World);

	if (!GetConfigBool(UAutoLootOresConfig::EnabledKey, true))
	{
		return;
	}
	const AAutoLootOresSubsystem* Subsystem = AAutoLootOresSubsystem::Get(World);
	const int32 ManualFirst = FMath::Clamp(GetConfigInt(UAutoLootOresConfig::ManualFirstKey, 0), 0, UAutoLootOresConfig::MaxManualFirst);
	if (ManualFirst > 0 && (!Subsystem || Subsystem->GetManuallyMined() < ManualFirst))
	{
		return;
	}

	const int32 ModeValue = GetConfigInt(UAutoLootOresConfig::ModeKey, UAutoLootOresConfig::DefaultMode);
	const EAutoLootOresMode Mode = ModeValue >= 0 && ModeValue <= static_cast<int32>(EAutoLootOresMode::LootAllDestroyUranium)
		? static_cast<EAutoLootOresMode>(ModeValue)
		: static_cast<EAutoLootOresMode>(UAutoLootOresConfig::DefaultMode);
	const float RangeCm = 100.f * FMath::Clamp(GetConfigInt(UAutoLootOresConfig::RangeKey, UAutoLootOresConfig::DefaultRange),
		UAutoLootOresConfig::MinRange, UAutoLootOresConfig::MaxRange);
	const double RangeSquared = FMath::Square(static_cast<double>(RangeCm));

	// Players who are in the game right now. The bodies of offline players stay in the world
	// and are skipped; a player driving a vehicle has no controller on the body but counts.
	TArray<AFGCharacterPlayer*, TInlineAllocator<8>> Players;
	for (TActorIterator<AFGCharacterPlayer> It(World); It; ++It)
	{
		AFGCharacterPlayer* Player = *It;
		if (IsValid(Player) && Player->HasAuthority() && Player->IsAliveAndWell() && Player->GetInventory()
			&& (Player->GetController() || Player->IsDrivingVehicle()))
		{
			Players.Add(Player);
		}
	}
	if (Players.Num() == 0)
	{
		return;
	}

	int32 Handled = 0;
	for (TActorIterator<AFGResourceDeposit> It(World); It && Handled < DepositsPerScan; ++It)
	{
		AFGResourceDeposit* Deposit = *It;
		if (!IsValid(Deposit) || IsMinedOut(Deposit))
		{
			continue;
		}
		const EDepositAction Action = GetAction(Mode, Deposit);
		if (Action == EDepositAction::Skip)
		{
			continue;
		}
		const FVector Location = Deposit->GetActorLocation();
		for (AFGCharacterPlayer* Player : Players)
		{
			if (FVector::DistSquared(Location, Player->GetActorLocation()) > RangeSquared)
			{
				continue;
			}
			if (Action == EDepositAction::Destroy)
			{
				DestroyDeposit(Deposit, Player);
				++Handled;
			}
			else if (MineOut(Deposit, Player))
			{
				++Handled;
			}
			if (IsMinedOut(Deposit))
			{
				break; // otherwise (inventory full) the next player in range gets a turn
			}
		}
	}
}

FAutoLootOres::EDepositAction FAutoLootOres::GetAction(EAutoLootOresMode Mode, const AFGResourceDeposit* Deposit)
{
	const TSubclassOf<UFGResourceDescriptor> Resource = Deposit->GetResourceClass();
	if (!Resource)
	{
		return EDepositAction::Skip; // not set up yet
	}
	// "Uranium" = anything radioactive, which also covers radioactive ores added by other mods.
	const bool bRadioactive = UFGItemDescriptor::GetRadioactiveDecay(Resource) > 0.f;
	switch (Mode)
	{
	case EAutoLootOresMode::LootAll:
		return EDepositAction::Loot;
	case EAutoLootOresMode::DestroyAll:
		return EDepositAction::Destroy;
	case EAutoLootOresMode::LootAllExceptUranium:
		return bRadioactive ? EDepositAction::Skip : EDepositAction::Loot;
	case EAutoLootOresMode::LootAllDestroyUranium:
		return bRadioactive ? EDepositAction::Destroy : EDepositAction::Loot;
	}
	return EDepositAction::Skip;
}

// ---------------------------------------------------------------------------
// Mining
// ---------------------------------------------------------------------------

int32 FAutoLootOres::GetResourcesLeft(const AFGResourceDeposit* Deposit)
{
	// Protected in the game's header; read through reflection instead of an access transformer.
	static const FIntProperty* Property = FindFProperty<FIntProperty>(AFGResourceNode::StaticClass(), TEXT("mResourcesLeft"));
	return Property ? Property->GetPropertyValue_InContainer(Deposit) : 0;
}

bool FAutoLootOres::IsMinedOut(AFGResourceDeposit* Deposit)
{
	return Deposit->IsDepositEmpty() || !Deposit->HasAnyResources();
}

bool FAutoLootOres::MineOut(AFGResourceDeposit* Deposit, AFGCharacterPlayer* Player)
{
	UFGInventoryComponent* Inventory = Player->GetInventory();
	const TSubclassOf<UFGResourceDescriptor> Resource = Deposit->GetResourceClass();
	if (!Inventory || !Resource)
	{
		return false;
	}

	TGuardValue<bool> Guard(bAutoMining, true);
	bool bMinedAnything = false;
	for (int32 Cycle = 0; Cycle < MaxCyclesPerScan && !IsMinedOut(Deposit); ++Cycle)
	{
		if (!Inventory->HasEnoughSpaceForItem(FInventoryItem(Resource)))
		{
			break; // inventory full: the rest of the deposit stays
		}
		const int32 Before = GetResourcesLeft(Deposit);
		// The same call hand mining makes each cycle, so emptying, effects and radiation follow the game's rules.
		Deposit->ExtractResourceAndGiveToPlayer(Player);
		if (GetResourcesLeft(Deposit) >= Before)
		{
			break; // the game refused (e.g. a full cycle doesn't fit)
		}
		bMinedAnything = true;
	}
	return bMinedAnything;
}

void FAutoLootOres::DestroyDeposit(AFGResourceDeposit* Deposit, AFGCharacterPlayer* Player)
{
	UFGInventoryComponent* Inventory = Player->GetInventory();
	const TSubclassOf<UFGResourceDescriptor> Resource = Deposit->GetResourceClass();
	if (Inventory && Resource)
	{
		// Mine it the normal way, then take the mined items away again. This keeps the game's own
		// "deposit emptied" handling instead of imitating it.
		const int32 Before = Inventory->GetNumItems(Resource);
		MineOut(Deposit, Player);
		const int32 Gained = Inventory->GetNumItems(Resource) - Before;
		if (Gained > 0)
		{
			Inventory->Remove(Resource, Gained);
		}
	}
	if (!IsMinedOut(Deposit) && (!Inventory || !Resource || !Inventory->HasEnoughSpaceForItem(FInventoryItem(Resource))))
	{
		ForceDeplete(Deposit); // no room to mine it through the inventory
	}
}

void FAutoLootOres::ForceDeplete(AFGResourceDeposit* Deposit)
{
	for (int32 Guard = 0; Guard < 16 && Deposit->HasAnyResources(); ++Guard)
	{
		const int32 Left = GetResourcesLeft(Deposit);
		if (Left <= 0 || Deposit->ExtractResource(Left) <= 0)
		{
			break;
		}
	}
	if (Deposit->HasAnyResources())
	{
		UE_LOG(LogAutoLootOres, Warning, TEXT("Couldn't empty %s"), *Deposit->GetName());
		return;
	}
	if (!Deposit->IsDepositEmpty())
	{
		// What the game does when the last bit is mined: flag it (saved and replicated) and play
		// the depleted effect, which also swaps the mesh.
		if (const FBoolProperty* Emptied = FindFProperty<FBoolProperty>(AFGResourceDeposit::StaticClass(), TEXT("mIsEmptied")))
		{
			Emptied->SetPropertyValue_InContainer(Deposit, true);
		}
		if (UFunction* Effect = Deposit->FindFunction(TEXT("Multicast_PlayDepletedEffect")))
		{
			Deposit->ProcessEvent(Effect, nullptr);
		}
		Deposit->FlushNetDormancy();
		Deposit->ForceNetUpdate();
	}
	UE_LOG(LogAutoLootOres, Verbose, TEXT("Emptied %s directly"), *Deposit->GetName());
}

void FAutoLootOres::OnManuallyMined(UWorld* World)
{
	if (AAutoLootOresSubsystem* Subsystem = AAutoLootOresSubsystem::GetOrRegister(World))
	{
		Subsystem->AddManuallyMined();
		UpdateStatus(EMenuMode::InSave, World);
	}
}

// ---------------------------------------------------------------------------
// Mod config
// ---------------------------------------------------------------------------

void FAutoLootOres::RegisterConfig(UGameInstance* GameInstance)
{
	if (!GameInstance || ConfigManager.IsValid())
	{
		return;
	}
	UConfigManager* Manager = GameInstance->GetSubsystem<UConfigManager>();
	if (!Manager)
	{
		UE_LOG(LogAutoLootOres, Error, TEXT("SML config manager not found; using default settings"));
		return;
	}
	UAutoLootOresConfig::UseMenuWidgets();
	Manager->RegisterModConfiguration(UAutoLootOresConfig::StaticClass());
	ConfigManager = Manager;
	UpdateStatus(EMenuMode::MainMenu);
}

UConfigProperty* FAutoLootOres::GetConfigProperty(const FString& Key)
{
	UConfigManager* Manager = ConfigManager.Get();
	UConfigPropertySection* Root = Manager ? Manager->GetConfigurationRootSection(UAutoLootOresConfig::GetId()) : nullptr;
	const TObjectPtr<UConfigProperty>* Property = Root ? Root->SectionProperties.Find(Key) : nullptr;
	return Property ? Property->Get() : nullptr;
}

int32 FAutoLootOres::GetConfigInt(const FString& Key, int32 Fallback)
{
	const UConfigPropertyInteger* Property = Cast<UConfigPropertyInteger>(GetConfigProperty(Key));
	return Property ? Property->Value : Fallback;
}

bool FAutoLootOres::GetConfigBool(const FString& Key, bool bFallback)
{
	const UConfigPropertyBool* Property = Cast<UConfigPropertyBool>(GetConfigProperty(Key));
	return Property ? Property->Value : bFallback;
}

void FAutoLootOres::SetStatusText(const FString& Text)
{
	// The label (UAutoLootOresLabel) redraws its DisplayName every frame.
	if (UConfigProperty* Status = GetConfigProperty(UAutoLootOresConfig::StatusKey))
	{
		Status->DisplayName = FText::FromString(Text);
	}
}

void FAutoLootOres::UpdateStatus(EMenuMode Mode, UWorld* World)
{
	switch (Mode)
	{
	case EMenuMode::MainMenu:
		SetStatusText(TEXT("Load a save to see the status."));
		return;
	case EMenuMode::Client:
		SetStatusText(TEXT("Multiplayer: the host's settings are used."));
		return;
	case EMenuMode::InSave:
		break;
	}

	const AAutoLootOresSubsystem* Subsystem = AAutoLootOresSubsystem::Get(World);
	const int32 Mined = Subsystem ? Subsystem->GetManuallyMined() : 0;
	const int32 ManualFirst = FMath::Clamp(GetConfigInt(UAutoLootOresConfig::ManualFirstKey, 0), 0, UAutoLootOresConfig::MaxManualFirst);
	if (!GetConfigBool(UAutoLootOresConfig::EnabledKey, true))
	{
		SetStatusText(FString::Printf(TEXT("Off. Deposits mined by hand in this save: %d."), Mined));
	}
	else if (Mined < ManualFirst)
	{
		SetStatusText(FString::Printf(TEXT("Waiting: mine %d more deposit(s) by hand to start auto-looting (%d of %d done)."),
			ManualFirst - Mined, Mined, ManualFirst));
	}
	else
	{
		SetStatusText(FString::Printf(TEXT("Active. Deposits mined by hand in this save: %d."), Mined));
	}
}
