#include "RubiconDebugPanelComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "RubiconDebugFormat.h"
#include "RubiconNarrativeSubsystem.h"
#include "RubiconSettings.h"
#include "Styling/CoreStyle.h"

namespace RubiconDebugPanelDetail
{
	/** Frame lines, with the consent advisory first while the session waits at the Threshold. */
	TArray<FString> LinesWithAdvisory(const URubiconNarrativeSubsystem& N, int32 WrapColumns)
	{
		const FRubiconFrameOutputs& F = N.GetFrameOutputsRef();
		TArray<FString> Out;
		if (F.bValid && F.State == ERubiconTimelineState::Threshold)
		{
			FString Title, Body;
			N.GetAdvisory(Title, Body);
			Out.Add(Title.ToUpper());
			Out.Append(WrapColumns > 0 ? RubiconDebugFormat::Wrap(Body, WrapColumns) : TArray<FString>{ Body });
			Out.Add(TEXT("Press the trigger (Space) to begin."));
			Out.Add(FString());
		}
		Out.Append(RubiconDebugFormat::Lines(F));
		return Out;
	}
}

static TAutoConsoleVariable<int32> CVarRubiconDebugHUD(TEXT("rubicon.DebugHUD"), -1,
	TEXT("Rubicon debug HUD: 1 shown, 0 hidden, -1 project default (URubiconSettings::bStartInDebugHUD)."));

bool URubiconDebugPanelComponent::IsDebugVisible()
{
	const int32 V = CVarRubiconDebugHUD.GetValueOnGameThread();
	return V < 0 ? URubiconSettings::Get()->bStartInDebugHUD : V != 0;
}

void URubiconDebugPanelComponent::ToggleDebugVisible()
{
	CVarRubiconDebugHUD->Set(IsDebugVisible() ? 0 : 1, ECVF_SetByCode);
}

void URubiconDebugWidget::SetLines(const TArray<FString>& Lines)
{
	if (Text) Text->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

TSharedRef<SWidget> URubiconDebugWidget::RebuildWidget()
{
	// Built in code: no widget blueprint asset is needed.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Border->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.72f));
		Border->SetPadding(FMargin(16.f));
		Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Lines"));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 13));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.82f, 0.74f)));
		Text->SetAutoWrapText(true);
		Border->SetContent(Text);
		WidgetTree->RootWidget = Border;
	}
	return Super::RebuildWidget();
}

URubiconDebugPanelComponent::URubiconDebugPanelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetWidgetSpace(EWidgetSpace::World);
	SetWidgetClass(URubiconDebugWidget::StaticClass());
	SetDrawSize(FVector2D(1100, 760));
	SetRelativeScale3D(FVector(0.055));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetTwoSided(true);
	SetBlendMode(EWidgetBlendMode::Transparent);
}

void URubiconDebugPanelComponent::BeginPlay()
{
	Super::BeginPlay();
	SetVisibility(IsDebugVisible());
}

void URubiconDebugPanelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const bool bVisible = IsDebugVisible();
	if (IsVisible() != bVisible) SetVisibility(bVisible);
	if (!bVisible) return;
	const URubiconNarrativeSubsystem* N = GetWorld() ? GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>() : nullptr;
	if (URubiconDebugWidget* W = Cast<URubiconDebugWidget>(GetUserWidgetObject()))
	{
		W->SetLines(N ? RubiconDebugPanelDetail::LinesWithAdvisory(*N, 0) : TArray<FString>{ TEXT("RUBICON  narrative subsystem missing") });
	}
}

void ARubiconDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !URubiconDebugPanelComponent::IsDebugVisible()) return;
	const URubiconNarrativeSubsystem* N = GetWorld() ? GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>() : nullptr;
	if (!N) return;
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	double Y = 24.0;
	for (const FString& Line : RubiconDebugPanelDetail::LinesWithAdvisory(*N, 110))
	{
		Canvas->SetDrawColor(FColor(220, 210, 190));
		Canvas->DrawText(Font, Line, 24.f, static_cast<float>(Y));
		Y += 16.0;
	}
}
