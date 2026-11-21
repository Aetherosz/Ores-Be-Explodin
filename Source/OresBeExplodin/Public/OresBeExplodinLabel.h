#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "OresBeExplodinLabel.generated.h"

/**
 * A read-only line of text in the mod config menu. SML has no label property, so this is an
 * empty section (saved as {} in the .cfg) that draws its DisplayName as plain text, without
 * the collapse arrow and divider SML's own section widget adds.
 */
UCLASS()
class UOresBeExplodinLabel : public UConfigPropertySection
{
	GENERATED_BODY()

public:
	virtual UUserWidget* CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const override;
};

/** Shows a label's DisplayName. Redrawn every frame, so text changes appear while the menu is open. */
UCLASS()
class UOresBeExplodinLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<const UConfigProperty> Label;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
