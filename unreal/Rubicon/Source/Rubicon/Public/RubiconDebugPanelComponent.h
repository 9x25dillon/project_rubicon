// Debug HUD: a world-space panel attached to the pawn's body frame (readable in the headset), plus
// ARubiconDebugHUD for the desktop mirror. Both print the live FRubiconFrameOutputs.
// Toggle: rubicon.DebugHUD 0|1, F1, or the right thumbstick click.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/HUD.h"
#include "RubiconDebugPanelComponent.generated.h"

class UTextBlock;

UCLASS()
class RUBICON_API URubiconDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLines(const TArray<FString>& Lines);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Text;
};

UCLASS(ClassGroup = (Rubicon), meta = (BlueprintSpawnableComponent))
class RUBICON_API URubiconDebugPanelComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	URubiconDebugPanelComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static bool IsDebugVisible();
	static void ToggleDebugVisible();
};

UCLASS()
class RUBICON_API ARubiconDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
