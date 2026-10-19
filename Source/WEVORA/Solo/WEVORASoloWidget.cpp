#include "Solo/WEVORASoloWidget.h"
#include "Solo/WEVORARunDirector.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Reply.h"
#include "Styling/CoreStyle.h"

void UWEVORASoloWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

void UWEVORASoloWidget::BuildControls()
{
	if (Heading) { return; }
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.015f,0.025f,0.05f,0.97f));
	Backdrop->SetPadding(FMargin(70,50));
	WidgetTree->RootWidget = Backdrop;
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(); Backdrop->SetContent(Column);
	Heading = WidgetTree->ConstructWidget<UTextBlock>();
	Heading->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold",24)));
	Heading->SetAutoWrapText(true); Column->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0,0,0,35));
	for (int32 Index=0; Index<3; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular",18));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White)); Label->SetAutoWrapText(true);
		Button->SetBackgroundColor(FLinearColor(0.12f,0.22f,0.32f)); Button->AddChild(Label);
		Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,0,0,22));
		Buttons.Add(Button); Labels.Add(Label);
	}
	Buttons[0]->OnClicked.AddDynamic(this,&ThisClass::ChooseOne);
	Buttons[1]->OnClicked.AddDynamic(this,&ThisClass::ChooseTwo);
	Buttons[2]->OnClicked.AddDynamic(this,&ThisClass::ChooseThree);
}

void UWEVORASoloWidget::Show(AWEVORARunDirector* Director)
{
	BuildControls();
	Run = Director;
	const bool bChoice = Run->State == EWEVORARunState::UpgradeChoice;
	Heading->SetText(FText::FromString(bChoice
		? TEXT("WAVE CLEAR - Choose an upgrade\n1 / 2 / 3 or click. All upgrades last for this run.")
		: (Run->State == EWEVORARunState::Victory ? TEXT("TRIAL CLEARED\n") : TEXT("RUN ENDED\n")) + Run->ResultReason));
	for (int32 Index=0; Index<3; ++Index)
	{
		const bool bRetrySave = !bChoice && Index == 1 && Run->Progression->bSavePending;
		const bool bVisible = bChoice ? Run->Choices.IsValidIndex(Index) : Index == 0 || bRetrySave;
		Buttons[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bVisible)
		{
			Labels[Index]->SetText(FText::FromString(bChoice
				? FString::Printf(TEXT("[%d]  %s\n%s\n"), Index+1,*Run->Choices[Index].Title.ToString(),*Run->Choices[Index].Description.ToString())
				: bRetrySave ? TEXT("Save again [2]\n") : TEXT("Retry  [R / Enter]\nStart a new run with temporary upgrades cleared.\n")));
		}
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UWEVORASoloWidget::ChooseOne()
{
	if (!Run) { return; }
	if (Run->State == EWEVORARunState::UpgradeChoice) { Run->SelectUpgrade(0); }
	else { Run->Retry(); }
}
void UWEVORASoloWidget::ChooseTwo()
{
	if (!Run) { return; }
	if (Run->State == EWEVORARunState::UpgradeChoice) { Run->SelectUpgrade(1); } else { Run->RetrySave(); }
}
void UWEVORASoloWidget::ChooseThree() { if (Run) { Run->SelectUpgrade(2); } }

FReply UWEVORASoloWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::R && Run) { Run->Retry(); return FReply::Handled(); }
	if (Run && Run->State == EWEVORARunState::UpgradeChoice)
	{
		if (Key == EKeys::One || Key == EKeys::NumPadOne) { ChooseOne(); return FReply::Handled(); }
		if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { ChooseTwo(); return FReply::Handled(); }
		if (Key == EKeys::Three || Key == EKeys::NumPadThree) { ChooseThree(); return FReply::Handled(); }
	}
	else if (Key == EKeys::R || Key == EKeys::Enter) { ChooseOne(); return FReply::Handled(); }
	else if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { ChooseTwo(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry,Event);
}
