#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WEVORASoloWidget.generated.h"

class AWEVORARunDirector;
class UTextBlock;
class UButton;

/** Modal cards and results, supporting both mouse and keyboard without persisted widget assets. */
UCLASS()
class WEVORA_API UWEVORASoloWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Show(AWEVORARunDirector* Director);
protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	void BuildControls();
	UPROPERTY(Transient) TObjectPtr<AWEVORARunDirector> Run;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Labels;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> Buttons;
	UFUNCTION() void ChooseOne();
	UFUNCTION() void ChooseTwo();
	UFUNCTION() void ChooseThree();
};
