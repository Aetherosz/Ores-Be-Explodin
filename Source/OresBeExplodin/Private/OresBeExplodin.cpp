#include "OresBeExplodin.h"

#include "OresBeExplodinSubsystem.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogOresBeExplodin, Log, All);

TWeakObjectPtr<UConfigManager> FOresBeExplodin::ConfigManager;
TWeakObjectPtr<UWorld> FOresBeExplodin::ScannedWorld;
bool FOresBeExplodin::bAutoMining = false;

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

void FOresBeExplodin::RegisterHooks()
{
	// Register the mod config as soon as the game instance exists, so it is
	// listed in the main menu's Mods -> Mod Configuration before any save loads.
	FWorldDelegates::OnStartGameInstance.AddStatic(&FOresBeExplodin::RegisterConfig);
	FWorldDelegates::OnWorldCleanup.AddStatic(&FOresBeExplodin::OnWorldCleanup);

#if !WITH_EDITOR
	// Write config changes right away. SML otherwise saves on a timer that doesn't run while the
	// game is paused, and its flush at shutdown crashes, so "change a value in the pause menu,
	// then Exit to Desktop" would crash the game.
	SUBSCRIBE_METHOD_AFTER(UConfigManager::MarkConfigurationDirty,
		[](UConfigManager* Self, const FConfigId& Id)
		{
			if (Id == UOresBeExplodinConfig::GetId())
			{
				Self->FlushPendingSaves();
				// The scan timer is paused with the game, so refresh the status line here too.
				if (UWorld* World = ScannedWorld.Get())
				{
					UpdateStatus(EMenuMode::InSave, World);
				}
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
				AOresBeExplodinSubsystem::GetOrRegister(World);
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

void FOresBeExplodin::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void FOresBeExplodin::Scan(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	UpdateStatus(EMenuMode::InSave, World);

	if (!GetConfigBool(UOresBeExplodinConfig::EnabledKey, true))
	{
		return;
	}
	const AOresBeExplodinSubsystem* Subsystem = AOresBeExplodinSubsystem::Get(World);
	const int32 ManualFirst = FMath::Clamp(GetConfigInt(UOresBeExplodinConfig::ManualFirstKey, 0), 0, UOresBeExplodinConfig::MaxManualFirst);
	if (ManualFirst > 0 && (!Subsystem || Subsystem->GetManuallyMined() < ManualFirst))
	{
		return;
	}

	const int32 ModeValue = GetConfigInt(UOresBeExplodinConfig::ModeKey, UOresBeExplodinConfig::DefaultMode);
	const EOresBeExplodinMode Mode = ModeValue >= 0 && ModeValue <= static_cast<int32>(EOresBeExplodinMode::LootAllDestroyUranium)
		? static_cast<EOresBeExplodinMode>(ModeValue)
		: static_cast<EOresBeExplodinMode>(UOresBeExplodinConfig::DefaultMode);
	const float RangeCm = 100.f * FMath::Clamp(GetConfigInt(UOresBeExplodinConfig::RangeKey, UOresBeExplodinConfig::DefaultRange),
		UOresBeExplodinConfig::MinRange, UOresBeExplodinConfig::MaxRange);
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

FOresBeExplodin::EDepositAction FOresBeExplodin::GetAction(EOresBeExplodinMode Mode, const AFGResourceDeposit* Deposit)
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
	case EOresBeExplodinMode::LootAll:
		return EDepositAction::Loot;
	case EOresBeExplodinMode::DestroyAll:
		return EDepositAction::Destroy;
	case EOresBeExplodinMode::LootAllExceptUranium:
		return bRadioactive ? EDepositAction::Skip : EDepositAction::Loot;
	case EOresBeExplodinMode::LootAllDestroyUranium:
		return bRadioactive ? EDepositAction::Destroy : EDepositAction::Loot;
	}
	return EDepositAction::Skip;
}

// ---------------------------------------------------------------------------
// Mining
// ---------------------------------------------------------------------------

int32 FOresBeExplodin::GetResourcesLeft(const AFGResourceDeposit* Deposit)
{
	// Protected in the game's header; read through reflection instead of an access transformer.
	static const FIntProperty* Property = FindFProperty<FIntProperty>(AFGResourceNode::StaticClass(), TEXT("mResourcesLeft"));
	return Property ? Property->GetPropertyValue_InContainer(Deposit) : 0;
}

bool FOresBeExplodin::IsMinedOut(AFGResourceDeposit* Deposit)
{
	return Deposit->IsDepositEmpty() || !Deposit->HasAnyResources();
}

bool FOresBeExplodin::MineOut(AFGResourceDeposit* Deposit, AFGCharacterPlayer* Player)
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

void FOresBeExplodin::DestroyDeposit(AFGResourceDeposit* Deposit, AFGCharacterPlayer* Player)
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

void FOresBeExplodin::ForceDeplete(AFGResourceDeposit* Deposit)
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
		UE_LOG(LogOresBeExplodin, Warning, TEXT("Couldn't empty %s"), *Deposit->GetName());
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
	UE_LOG(LogOresBeExplodin, Verbose, TEXT("Emptied %s directly"), *Deposit->GetName());
}

void FOresBeExplodin::OnManuallyMined(UWorld* World)
{
	if (AOresBeExplodinSubsystem* Subsystem = AOresBeExplodinSubsystem::GetOrRegister(World))
	{
		Subsystem->AddManuallyMined();
		UpdateStatus(EMenuMode::InSave, World);
	}
}

// ---------------------------------------------------------------------------
// Mod config
// ---------------------------------------------------------------------------

void FOresBeExplodin::RegisterConfig(UGameInstance* GameInstance)
{
	if (!GameInstance || ConfigManager.IsValid())
	{
		return;
	}
	UConfigManager* Manager = GameInstance->GetSubsystem<UConfigManager>();
	if (!Manager)
	{
		UE_LOG(LogOresBeExplodin, Error, TEXT("SML config manager not found; using default settings"));
		return;
	}
	UOresBeExplodinConfig::UseMenuWidgets();
	Manager->RegisterModConfiguration(UOresBeExplodinConfig::StaticClass());
	ConfigManager = Manager;
	UpdateStatus(EMenuMode::MainMenu);
}

UConfigProperty* FOresBeExplodin::GetConfigProperty(const FString& Key)
{
	UConfigManager* Manager = ConfigManager.Get();
	UConfigPropertySection* Root = Manager ? Manager->GetConfigurationRootSection(UOresBeExplodinConfig::GetId()) : nullptr;
	const TObjectPtr<UConfigProperty>* Property = Root ? Root->SectionProperties.Find(Key) : nullptr;
	return Property ? Property->Get() : nullptr;
}

int32 FOresBeExplodin::GetConfigInt(const FString& Key, int32 Fallback)
{
	const UConfigPropertyInteger* Property = Cast<UConfigPropertyInteger>(GetConfigProperty(Key));
	return Property ? Property->Value : Fallback;
}

bool FOresBeExplodin::GetConfigBool(const FString& Key, bool bFallback)
{
	const UConfigPropertyBool* Property = Cast<UConfigPropertyBool>(GetConfigProperty(Key));
	return Property ? Property->Value : bFallback;
}

void FOresBeExplodin::SetStatusText(const FString& Text)
{
	// The label (UOresBeExplodinLabel) redraws its DisplayName every frame.
	if (UConfigProperty* Status = GetConfigProperty(UOresBeExplodinConfig::StatusKey))
	{
		Status->DisplayName = FText::FromString(Text);
	}
}

void FOresBeExplodin::UpdateStatus(EMenuMode Mode, UWorld* World)
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

	const AOresBeExplodinSubsystem* Subsystem = AOresBeExplodinSubsystem::Get(World);
	const int32 Mined = Subsystem ? Subsystem->GetManuallyMined() : 0;
	const int32 ManualFirst = FMath::Clamp(GetConfigInt(UOresBeExplodinConfig::ManualFirstKey, 0), 0, UOresBeExplodinConfig::MaxManualFirst);
	if (!GetConfigBool(UOresBeExplodinConfig::EnabledKey, true))
	{
		SetStatusText(FString::Printf(TEXT("Off. Deposits mined by hand in this save: %d."), Mined));
	}
	else if (Mined < ManualFirst)
	{
		const int32 Left = ManualFirst - Mined;
		SetStatusText(FString::Printf(TEXT("Waiting: Mine %d more %s by hand to start auto-looting (%d of %d done)."),
			Left, Left == 1 ? TEXT("deposit") : TEXT("deposits"), Mined, ManualFirst));
	}
	else
	{
		SetStatusText(FString::Printf(TEXT("Active. Deposits mined by hand in this save: %d."), Mined));
	}
}
