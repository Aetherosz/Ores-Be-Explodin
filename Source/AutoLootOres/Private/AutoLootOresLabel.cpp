#include "AutoLootOresLabel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

UUserWidget* UAutoLootOresLabel::CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const
{
	if (!ParentWidget)
	{
		return nullptr;
	}
	UAutoLootOresLabelWidget* Widget = CreateWidget<UAutoLootOresLabelWidget>(ParentWidget, UAutoLootOresLabelWidget::StaticClass());
	if (Widget)
	{
		Widget->Label = this;
	}
	return Widget;
}

TSharedRef<SWidget> UAutoLootOresLabelWidget::RebuildWidget()
{
	const TWeakObjectPtr<const UConfigProperty> WeakLabel = Label;
	return SNew(SBox)
		.Padding(FMargin(12.f, 6.f))
		[
			SNew(STextBlock)
			.Text_Lambda([WeakLabel]()
			{
				const UConfigProperty* Property = WeakLabel.Get();
				return Property ? Property->DisplayName : FText::GetEmpty();
			})
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
			.ColorAndOpacity(FLinearColor::White)
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
		];
}
