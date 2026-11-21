#include "OresBeExplodinConfig.h"

#include "OresBeExplodinLabel.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/WidgetExtension/CP_Integer.h"
#include "Configuration/Properties/WidgetExtension/CP_Section.h"

#define LOCTEXT_NAMESPACE "OresBeExplodin"

const FString UOresBeExplodinConfig::StatusKey = TEXT("Status");
const FString UOresBeExplodinConfig::EnabledKey = TEXT("Enabled");
const FString UOresBeExplodinConfig::ModeKey = TEXT("Mode");
const FString UOresBeExplodinConfig::RangeKey = TEXT("Range");
const FString UOresBeExplodinConfig::ManualFirstKey = TEXT("ManualFirst");

FConfigId UOresBeExplodinConfig::GetId()
{
	FConfigId Id;
	Id.ModReference = TEXT("OresBeExplodin");
	Id.ConfigCategory = TEXT("");
	return Id;
}

UOresBeExplodinConfig::UOresBeExplodinConfig()
{
	ConfigId = GetId();
	DisplayName = LOCTEXT("ConfigName", "Ores Be Explodin'");
	Description = LOCTEXT("ConfigDescription",
		"Automatically mines the loose ore deposits around you. Changes apply right away. In multiplayer the host's settings count.");

	RootSection = CreateDefaultSubobject<UConfigPropertySection>(TEXT("RootSection"));

	UOresBeExplodinLabel* Status = CreateDefaultSubobject<UOresBeExplodinLabel>(TEXT("Status"));
	Status->DisplayName = LOCTEXT("StatusMenu", "Load a save to see the status.");
	Status->bAllowUserReset = false;
	RootSection->SectionProperties.Add(StatusKey, Status);

	UConfigPropertyBool* Enabled = CreateDefaultSubobject<UConfigPropertyBool>(TEXT("Enabled"));
	Enabled->DisplayName = LOCTEXT("EnabledName", "Enabled");
	Enabled->Tooltip = LOCTEXT("EnabledTooltip", "Turn auto-looting on or off without removing the mod.");
	Enabled->DefaultValue = true;
	Enabled->Value = true;
	Enabled->bRequiresWorldReload = false;
	RootSection->SectionProperties.Add(EnabledKey, Enabled);

	UConfigPropertyInteger* Mode = CreateDefaultSubobject<UConfigPropertyInteger>(TEXT("Mode"));
	Mode->DisplayName = LOCTEXT("ModeName", "Mode");
	Mode->Tooltip = LOCTEXT("ModeTooltip",
		"Loot All: mines every deposit in range into your inventory. Warning: that includes Uranium!\n"
		"Destroy All: removes every deposit in range. You get nothing.\n"
		"Loot All Except Uranium: mines everything, leaves Uranium deposits alone.\n"
		"Loot All Destroy Uranium: mines everything, removes Uranium deposits without giving you the Uranium.\n"
		"\"Uranium\" means any radioactive resource, so radioactive ores from other mods count too.\n"
		"When your inventory is full, looting stops and the rest of the deposit stays.");
	Mode->DefaultValue = DefaultMode;
	Mode->Value = DefaultMode;
	Mode->bRequiresWorldReload = false;
	RootSection->SectionProperties.Add(ModeKey, Mode);

	UConfigPropertyInteger* Range = CreateDefaultSubobject<UConfigPropertyInteger>(TEXT("Range"));
	Range->DisplayName = LOCTEXT("RangeName", "Range (meters)");
	Range->Tooltip = FText::Format(
		LOCTEXT("RangeTooltip", "Deposits within this distance of a player are mined ({0} - {1} m). A foundation is 8 m wide."),
		FText::AsNumber(MinRange), FText::AsNumber(MaxRange));
	Range->DefaultValue = DefaultRange;
	Range->Value = DefaultRange;
	Range->bRequiresWorldReload = false;
	RootSection->SectionProperties.Add(RangeKey, Range);

	UConfigPropertyInteger* ManualFirst = CreateDefaultSubobject<UConfigPropertyInteger>(TEXT("ManualFirst"));
	ManualFirst->DisplayName = LOCTEXT("ManualFirstName", "Deposits to mine by hand first");
	ManualFirst->Tooltip = FText::Format(
		LOCTEXT("ManualFirstTooltip",
			"Auto-looting starts once this many deposits have been mined out by hand in the save (0 - {0}).\n"
			"0 = it works right away. The count is kept per save."),
		FText::AsNumber(MaxManualFirst));
	ManualFirst->DefaultValue = 0;
	ManualFirst->Value = 0;
	ManualFirst->bRequiresWorldReload = false;
	RootSection->SectionProperties.Add(ManualFirstKey, ManualFirst);
}

namespace
{
	/** Copy of Native as SML's Blueprint version of its class, or Native itself if there is none. */
	UConfigProperty* ToMenuProperty(UConfigProperty* Native, UObject* Outer, const FString& Key)
	{
		const UClass* NativeClass = Native->GetClass();
		const TCHAR* BlueprintName =
			NativeClass == UConfigPropertySection::StaticClass() ? TEXT("BP_ConfigPropertySection") :
			NativeClass == UConfigPropertyInteger::StaticClass() ? TEXT("BP_ConfigPropertyInteger") :
			NativeClass == UConfigPropertyBool::StaticClass() ? TEXT("BP_ConfigPropertyBool") : nullptr;
		if (!BlueprintName)
		{
			// Has its own widget (our label) or is already a Blueprint class: copy it as is.
			return DuplicateObject<UConfigProperty>(Native, Outer,
				MakeUniqueObjectName(Outer, NativeClass, FName(*(Native->GetName() + TEXT("_Menu")))));
		}
		UClass* BlueprintClass = LoadClass<UConfigProperty>(nullptr,
			*FString::Printf(TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/%s.%s_C"), BlueprintName, BlueprintName));
		if (!BlueprintClass)
		{
			UE_LOG(LogTemp, Error, TEXT("OresBeExplodin: %s not found, the config menu will be empty"), BlueprintName);
			return Native;
		}

		// Unique name: the native property may live in the same outer under its name (e.g. RootSection),
		// and UE refuses to replace an object with one of a different class. The name isn't used anyway;
		// sections look properties up by their map key.
		UConfigProperty* Menu = NewObject<UConfigProperty>(Outer, BlueprintClass,
			MakeUniqueObjectName(Outer, BlueprintClass, FName(*(Native->GetName() + TEXT("_Menu")))));
		Menu->DisplayName = Native->DisplayName;
		Menu->Tooltip = Native->Tooltip;
		Menu->bRequiresWorldReload = Native->bRequiresWorldReload;
		Menu->bHidden = Native->bHidden;
		Menu->bAllowUserReset = Native->bAllowUserReset;

		if (const UConfigPropertyInteger* Int = Cast<UConfigPropertyInteger>(Native))
		{
			UConfigPropertyInteger* MenuInt = CastChecked<UConfigPropertyInteger>(Menu);
			MenuInt->DefaultValue = Int->DefaultValue;
			MenuInt->Value = Int->Value;
			if (UCP_Integer* Widget = Cast<UCP_Integer>(MenuInt))
			{
				// SML's default is a text box with min = max = 0.
				if (Key == UOresBeExplodinConfig::ModeKey)
				{
					Widget->WidgetType = ECP_IntegerWidgetType::CPI_Enum;
					Widget->EnumClass = StaticEnum<EOresBeExplodinMode>();
					Widget->MinValue = 0;
					Widget->MaxValue = static_cast<int32>(EOresBeExplodinMode::LootAllDestroyUranium);
				}
				else
				{
					Widget->WidgetType = ECP_IntegerWidgetType::CPI_Spinbox;
					const bool bRange = Key == UOresBeExplodinConfig::RangeKey;
					Widget->MinValue = bRange ? UOresBeExplodinConfig::MinRange : 0;
					Widget->MaxValue = bRange ? UOresBeExplodinConfig::MaxRange : UOresBeExplodinConfig::MaxManualFirst;
				}
			}
		}
		else if (const UConfigPropertyBool* Bool = Cast<UConfigPropertyBool>(Native))
		{
			UConfigPropertyBool* MenuBool = CastChecked<UConfigPropertyBool>(Menu);
			MenuBool->DefaultValue = Bool->DefaultValue;
			MenuBool->Value = Bool->Value;
		}
		else if (const UConfigPropertySection* Section = Cast<UConfigPropertySection>(Native))
		{
			UConfigPropertySection* MenuSection = CastChecked<UConfigPropertySection>(Menu);
			if (UCP_Section* Widget = Cast<UCP_Section>(MenuSection))
			{
				// SML's default is one horizontal row with a scroll bar.
				Widget->WidgetType = ECP_SectionWidgetType::CPS_Vertical;
			}
			for (const TPair<FString, TObjectPtr<UConfigProperty>>& Child : Section->SectionProperties)
			{
				MenuSection->SectionProperties.Add(Child.Key, Child.Value ? ToMenuProperty(Child.Value, MenuSection, Child.Key) : nullptr);
			}
		}
		return Menu;
	}
}

namespace
{
	/**
	 * Labels for the Mode dropdown. Shipping builds have no UMETA display names, so without
	 * this the menu shows the raw enumerator names ("LootAllExceptUranium").
	 */
	FText GetModeDisplayName(int32 Index)
	{
		switch (static_cast<EOresBeExplodinMode>(Index))
		{
		case EOresBeExplodinMode::LootAll:
			return LOCTEXT("ModeLootAll", "Auto Loot All - Warning: Uranium! Pack enough Iodine-Infused Filters");
		case EOresBeExplodinMode::DestroyAll:
			return LOCTEXT("ModeDestroyAll", "Auto Destroy All - Did you know you can use Explosive Rebar?");
		case EOresBeExplodinMode::LootAllExceptUranium:
			return LOCTEXT("ModeExceptUranium", "Auto Loot All - Except Uranium");
		case EOresBeExplodinMode::LootAllDestroyUranium:
			return LOCTEXT("ModeDestroyUranium", "Auto Loot All - Destroy Uranium");
		}
		return FText::GetEmpty();
	}
}

void UOresBeExplodinConfig::UseMenuWidgets()
{
	StaticEnum<EOresBeExplodinMode>()->SetEnumDisplayNameFn(&GetModeDisplayName);

	UOresBeExplodinConfig* Default = GetMutableDefault<UOresBeExplodinConfig>();
	if (Default->RootSection)
	{
		Default->RootSection = CastChecked<UConfigPropertySection>(ToMenuProperty(Default->RootSection, Default, FString()));
	}
}

#undef LOCTEXT_NAMESPACE
