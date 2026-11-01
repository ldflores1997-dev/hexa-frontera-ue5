#include "HFHUD.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameUserSettings.h"
#include "GlobalRenderResources.h"
#include "HFBoardActor.h"
#include "HFGameMode.h"
#include "HFMatch.h"
#include "HFPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
// Slate usa colores lineales: FLinearColor(FColor) convierte desde sRGB, que es como se ven en pantalla.
const FLinearColor PanelColor(FColor(0x15, 0x18, 0x1F, 224));
const FLinearColor PanelEdge(FColor(0xFF, 0xFF, 0xFF, 26));
const FLinearColor SurfaceColor(FColor(0x2E, 0x33, 0x3E));
const FLinearColor HighlightColor(FColor(0xFF, 0xFF, 0xFF, 18));
const FLinearColor TextColor(FColor(0xED, 0xEB, 0xE0));
const FLinearColor MutedColor(FColor(0x9A, 0x9A, 0x95));
const FLinearColor AccentColor(FColor(0xFF, 0xC7, 0x59));
const FLinearColor InkColor(FColor(0x1E, 0x18, 0x0C)); // texto sobre el dorado
const FLinearColor DangerColor(FColor(0xFF, 0x73, 0x59));

FSlateFontInfo Font(int32 Size, bool bBold = false, int32 LetterSpacing = 0)
{
	FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
	Info.LetterSpacing = LetterSpacing;
	return Info;
}

const FSlateBrush* PanelBrush()
{
	static const FSlateRoundedBoxBrush Brush(PanelColor, 12.0f, PanelEdge, 1.0f);
	return &Brush;
}

const FSlateBrush* RoundBrush(float Radius = 6.0f)
{
	// Blanco: el color lo pone quien lo usa (BorderBackgroundColor o ButtonColorAndOpacity).
	static const FSlateRoundedBoxBrush Six(FLinearColor::White, 6.0f);
	static const FSlateRoundedBoxBrush Three(FLinearColor::White, 3.0f);
	return Radius <= 3.0f ? &Three : &Six;
}

// Un solo estilo de botón, blanco y redondeado; cada uso lo tiñe (normal, principal o elegido).
const FButtonStyle* ButtonStyle()
{
	static FButtonStyle Style;
	static bool bReady = false;
	if (!bReady)
	{
		static const FSlateRoundedBoxBrush Normal(FLinearColor(0.82f, 0.82f, 0.82f), 7.0f);
		static const FSlateRoundedBoxBrush Hovered(FLinearColor::White, 7.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), 1.0f);
		static const FSlateRoundedBoxBrush Pressed(FLinearColor(0.65f, 0.65f, 0.65f), 7.0f);
		static const FSlateRoundedBoxBrush Disabled(FLinearColor(0.55f, 0.55f, 0.55f, 0.35f), 7.0f);
		Style.SetNormal(Normal).SetHovered(Hovered).SetPressed(Pressed).SetDisabled(Disabled)
			.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));
		bReady = true;
	}
	return &Style;
}

enum class EKind : uint8
{
	Normal,  // gris
	Primary, // dorado: la acción principal de la pantalla
	Quiet,   // casi transparente: volver, cancelar
};

FLinearColor KindFill(EKind Kind)
{
	switch (Kind)
	{
	case EKind::Primary: return AccentColor;
	case EKind::Quiet: return FLinearColor(1.0f, 1.0f, 1.0f, 0.08f);
	default: return SurfaceColor;
	}
}

TSharedRef<SWidget> Panel(TSharedRef<SWidget> Inner, FMargin Padding = FMargin(18.0f, 14.0f))
{
	return SNew(SBorder)
		.BorderImage(PanelBrush())
		.Padding(Padding)
		[
			Inner
		];
}

TSharedRef<STextBlock> Label(TAttribute<FText> Text, int32 Size = 13, bool bBold = false, TAttribute<FSlateColor> Color = FSlateColor(TextColor))
{
	return SNew(STextBlock)
		.Text(Text)
		.Font(Font(Size, bBold))
		.ColorAndOpacity(Color)
		.AutoWrapText(true);
}

// Título de sección: chico, en mayúsculas y espaciado.
TSharedRef<STextBlock> Section(const FString& Text)
{
	return SNew(STextBlock)
		.Text(FText::FromString(Text.ToUpper()))
		.Font(Font(10, true, 160))
		.ColorAndOpacity(MutedColor);
}

// Texto centrado que ocupa todo el ancho del panel (con HAlign_Center el ajuste de línea queda angosto).
TSharedRef<STextBlock> Centered(TSharedRef<STextBlock> Text)
{
	Text->SetJustification(ETextJustify::Center);
	return Text;
}

TSharedRef<SButton> Button(TAttribute<FText> Text, TFunction<void()> OnClick, TAttribute<bool> Enabled = true, EKind Kind = EKind::Normal, int32 Size = 12)
{
	const FLinearColor Ink = Kind == EKind::Primary ? InkColor : TextColor;
	return SNew(SButton)
		.ButtonStyle(ButtonStyle())
		.ButtonColorAndOpacity(KindFill(Kind))
		.IsEnabled(Enabled)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ContentPadding(FMargin(12.0f, 7.0f))
		.OnClicked_Lambda([OnClick]()
		{
			OnClick();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Text)
			.Font(Font(Size, true))
			.ColorAndOpacity_Lambda([Enabled, Ink]() { return FSlateColor(Enabled.Get() ? Ink : Ink.CopyWithNewOpacity(0.4f)); })
		];
}

// Opción tipo píldora: dorada cuando está elegida.
TSharedRef<SButton> Choice(TAttribute<FText> Text, TFunction<bool()> IsSelected, TFunction<void()> OnClick, TAttribute<bool> Enabled = true)
{
	return SNew(SButton)
		.ButtonStyle(ButtonStyle())
		.ButtonColorAndOpacity_Lambda([IsSelected]() { return IsSelected() ? AccentColor : SurfaceColor; })
		.IsEnabled(Enabled)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ContentPadding(FMargin(14.0f, 6.0f))
		.OnClicked_Lambda([OnClick]()
		{
			OnClick();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Text)
			.Font(Font(12, true))
			.ColorAndOpacity_Lambda([IsSelected, Enabled]()
			{
				const FLinearColor Ink = IsSelected() ? InkColor : TextColor;
				return FSlateColor(Enabled.Get() ? Ink : Ink.CopyWithNewOpacity(0.4f));
			})
		];
}

// Fila de píldoras pegadas, una por opción.
TSharedRef<SHorizontalBox> ChoiceRow(const TArray<TSharedRef<SButton>>& Choices)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (const TSharedRef<SButton>& Item : Choices)
	{
		Row->AddSlot().FillWidth(1.0f).Padding(0, 0, 6, 0)[Item];
	}
	return Row;
}

// Fila de opciones: etiqueta a la izquierda y control a la derecha.
TSharedRef<SWidget> SettingRow(const FString& Name, TSharedRef<SWidget> Control)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.38f).VAlign(VAlign_Center)[Label(FText::FromString(Name), 13, true)]
		+ SHorizontalBox::Slot().FillWidth(0.62f).VAlign(VAlign_Center)[Control];
}

FText Str(const FString& Value)
{
	return FText::FromString(Value);
}

FLinearColor PlayerColor(const AHFGameMode* Mode, int32 Player)
{
	return Mode && Mode->GetBoard() && Mode->GetBoard()->PlayerColors.IsValidIndex(Player) ? Mode->GetBoard()->PlayerColors[Player] : TextColor;
}

// Color del jugador aclarado para leerse sobre los paneles. Igual que en el panel de jugadores.
FSlateColor PlayerTextColor(const AHFGameMode* Mode, int32 Player)
{
	return FSlateColor(FLinearColor::LerpUsingHSV(PlayerColor(Mode, Player), FLinearColor::White, 0.25f));
}
}

AHFGameMode* AHFHUD::GetHexGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AHFGameMode>() : nullptr;
}

AHFPlayerController* AHFHUD::GetHexController() const
{
	return Cast<AHFPlayerController>(GetOwningPlayerController());
}

void AHFHUD::BeginPlay()
{
	Super::BeginPlay();
	for (const FString& Id : FHFMatch::ListScenarioIds())
	{
		hf::Scenario Scenario;
		FString Error;
		if (FHFMatch::LoadScenario(Id, Scenario, Error))
		{
			Scenarios.Add({Id, FHFMatch::ToFString(Scenario.name), FHFMatch::ToFString(Scenario.description)});
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Error);
		}
	}
	// Con -HFDemo la partida ya arrancó: se muestra el tablero de juego en vez del menú.
	const AHFGameMode* Mode = GetHexGameMode();
	if (Mode && Mode->IsInMatch())
	{
		ShowGame();
	}
	else
	{
		ShowMenu();
	}
}

void AHFHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Holder.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
	}
	Holder.Reset();
	Content.Reset();
	Super::EndPlay(EndPlayReason);
}

void AHFHUD::SetContent(TSharedRef<SWidget> NewContent)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (Holder.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
	}
	Content = NewContent;
	Holder = SNew(SWeakWidget).PossiblyNullContent(Content);
	GEngine->GameViewport->AddViewportWidgetContent(Holder.ToSharedRef(), 10);
}

// ---------------------------------------------------------------- Menú

void AHFHUD::ShowMenu()
{
	ResetTrade();
	SetContent(BuildMenu());
}

void AHFHUD::SetMenuStep(EMenuStep Step)
{
	MenuStep = Step;
	ShowMenu();
}

void AHFHUD::SelectScenario(int32 Index)
{
	if (Scenarios.IsEmpty())
	{
		return;
	}
	SelectedScenario = (Index % Scenarios.Num() + Scenarios.Num()) % Scenarios.Num();
	if (AHFGameMode* Mode = GetHexGameMode())
	{
		Mode->PreviewScenario(Scenarios[SelectedScenario].Id, PlayerCount);
	}
}

void AHFHUD::HandleEscape()
{
	const AHFGameMode* Mode = GetHexGameMode();
	if (Mode && Mode->IsInMatch())
	{
		if (bOptionsOpen)
		{
			bOptionsOpen = false;
		}
		else if (bTradeOpen && !bPaused)
		{
			ResetTrade();
		}
		else
		{
			SetPaused(!bPaused);
		}
		return;
	}
	switch (MenuStep)
	{
	case EMenuStep::Players: SetMenuStep(EMenuStep::World); break;
	case EMenuStep::World:
	case EMenuStep::Options: SetMenuStep(EMenuStep::Title); break;
	default: break;
	}
}

void AHFHUD::SetPaused(bool bPause)
{
	bPaused = bPause;
	bOptionsOpen = false;
	UGameplayStatics::SetGamePaused(this, bPause);
}

void AHFHUD::GoToMenu()
{
	SetPaused(false);
	if (AHFGameMode* Mode = GetHexGameMode())
	{
		Mode->ReturnToMenu();
	}
	MenuStep = EMenuStep::Title;
	ShowMenu();
}

void AHFHUD::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

TSharedRef<SWidget> AHFHUD::BuildMenu()
{
	switch (MenuStep)
	{
	case EMenuStep::World: return BuildWorldStep();
	case EMenuStep::Players: return BuildPlayersStep();
	case EMenuStep::Options:
		return SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.45f))]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[BuildOptions([this]() { SetMenuStep(EMenuStep::Title); })];
	default: return BuildTitle();
	}
}

// Encabezado de los pasos del menú: el nombre del juego chico y en qué paso se está.
static TSharedRef<SWidget> StepHeader(int32 Step, const FString& Name)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Str(TEXT("HEXA FRONTERA"))).Font(Font(22, true, 220)).ColorAndOpacity(AccentColor)
			.ShadowOffset(FVector2D(0, 2)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.6f))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(STextBlock).Text(Str(FString::Printf(TEXT("Paso %d de 2  ·  %s"), Step, *Name))).Font(Font(13, true)).ColorAndOpacity(TextColor)
			.ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.6f))
		];
}

TSharedRef<SWidget> AHFHUD::BuildTitle()
{
	auto MenuButton = [](const FString& Text, TFunction<void()> OnClick, EKind Kind)
	{
		return SNew(SBox).WidthOverride(300.0f).HeightOverride(48.0f)[Button(Str(Text), OnClick, true, Kind, 15)];
	};
	return SNew(SOverlay)
		// Velo oscuro para que el título se lea sobre el tablero que gira detrás.
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(Str(TEXT("HEXA FRONTERA"))).Font(Font(58, true, 420)).ColorAndOpacity(AccentColor)
				.ShadowOffset(FVector2D(0, 3)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 44)
			[
				SNew(STextBlock).Text(Str(TEXT("Recursos, comercio y mundos que cambian"))).Font(Font(16, false, 60)).ColorAndOpacity(TextColor)
				.ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
			[
				MenuButton(TEXT("Jugar"), [this]() { SetMenuStep(EMenuStep::World); }, EKind::Primary)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
			[
				MenuButton(TEXT("Opciones"), [this]() { SetMenuStep(EMenuStep::Options); }, EKind::Normal)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
			[
				MenuButton(TEXT("Salir"), [this]() { QuitGame(); }, EKind::Quiet)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, 28.0f))
		[
			SNew(SBorder)
			.BorderImage(RoundBrush())
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
			.Padding(FMargin(14.0f, 6.0f))
			[
				Label(Str(TEXT("Clic derecho + arrastrar: girar la cámara  ·  Rueda: zoom  ·  WASD: mover  ·  Q/E: rotar  ·  Espacio: dados o fin de turno  ·  Esc: pausa")),
					11, false, TextColor)
			]
		];
}

TSharedRef<SWidget> AHFHUD::BuildWorldStep()
{
	auto Arrow = [this](const FString& Glyph, int32 Delta)
	{
		return SNew(SBox).WidthOverride(52.0f).HeightOverride(52.0f)[Button(Str(Glyph), [this, Delta]() { SelectScenario(SelectedScenario + Delta); }, true, EKind::Normal, 18)];
	};
	// Un punto por mundo: el elegido en dorado.
	TSharedRef<SHorizontalBox> Dots = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Scenarios.Num(); ++Index)
	{
		Dots->AddSlot().AutoWidth().Padding(4, 0)
		[
			SNew(SBox).WidthOverride(10.0f).HeightOverride(10.0f)
			[
				SNew(SBorder).BorderImage(RoundBrush(3.0f))
				.BorderBackgroundColor_Lambda([this, Index]() { return SelectedScenario == Index ? AccentColor : FLinearColor(1, 1, 1, 0.25f); })
			]
		];
	}
	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(40.0f, 32.0f))[StepHeader(1, TEXT("Elegí un mundo"))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, 36.0f))
		[
			SNew(SBox).WidthOverride(680.0f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT("◀"), -1)]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(16, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								Centered(Label(TAttribute<FText>::CreateLambda([this]()
								{
									return Scenarios.IsValidIndex(SelectedScenario) ? Str(Scenarios[SelectedScenario].Name) : FText::GetEmpty();
								}), 26, true, AccentColor))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
							[
								Centered(Label(TAttribute<FText>::CreateLambda([this]()
								{
									return Scenarios.IsValidIndex(SelectedScenario) ? Str(Scenarios[SelectedScenario].Description) : FText::GetEmpty();
								}), 13, false, TextColor))
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12, 0, 0)[Dots]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT("▶"), 1)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(130.0f)[Button(Str(TEXT("Atrás")), [this]() { SetMenuStep(EMenuStep::Title); }, true, EKind::Quiet)]]
						+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
						[
							Button(Str(TEXT("Otro tablero")), [this]() { SelectScenario(SelectedScenario); }, true, EKind::Quiet)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(170.0f)
							[
								Button(Str(TEXT("Siguiente")), [this]() { SetMenuStep(EMenuStep::Players); },
									TAttribute<bool>::CreateLambda([this]() { return Scenarios.IsValidIndex(SelectedScenario); }), EKind::Primary, 14)
							]
						]
					],
					FMargin(22.0f, 18.0f))
			]
		];
}

TSharedRef<SWidget> AHFHUD::BuildPlayersStep()
{
	auto Preview = [this]()
	{
		if (AHFGameMode* Mode = GetHexGameMode(); Mode && Scenarios.IsValidIndex(SelectedScenario))
		{
			Mode->PreviewScenario(Scenarios[SelectedScenario].Id, PlayerCount);
		}
	};
	TArray<TSharedRef<SButton>> Counts;
	for (int32 Count = 2; Count <= 4; ++Count)
	{
		Counts.Add(Choice(FText::AsNumber(Count), [this, Count]() { return PlayerCount == Count; }, [this, Count, Preview]() { PlayerCount = Count; Preview(); }));
	}
	TArray<TSharedRef<SButton>> Timers;
	for (int32 Seconds : {60, 90, 120, 0})
	{
		Timers.Add(Choice(Str(Seconds > 0 ? FString::Printf(TEXT("%d s"), Seconds) : FString(TEXT("Sin límite"))),
			[this, Seconds]() { return TurnSeconds == Seconds; }, [this, Seconds]() { TurnSeconds = Seconds; }));
	}

	TSharedRef<SVerticalBox> Seats = SNew(SVerticalBox);
	for (int32 Seat = 0; Seat < 4; ++Seat)
	{
		Seats->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SBorder)
			.BorderImage(RoundBrush())
			.BorderBackgroundColor(HighlightColor)
			.Padding(FMargin(10.0f, 6.0f))
			.Visibility_Lambda([this, Seat]() { return Seat < PlayerCount ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SBox).WidthOverride(14.0f).HeightOverride(14.0f)
					[
						SNew(SBorder).BorderImage(RoundBrush(3.0f))
						.BorderBackgroundColor_Lambda([this, Seat]() { return PlayerColor(GetHexGameMode(), Seat); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Label(Str(FString::Printf(TEXT("Asiento %d"), Seat + 1)), 14, true)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SBox).WidthOverride(96.0f)[Choice(Str(TEXT("Humano")), [this, Seat]() { return !Bots[Seat]; }, [this, Seat]() { Bots[Seat] = false; })]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(96.0f)[Choice(Str(TEXT("Bot")), [this, Seat]() { return Bots[Seat]; }, [this, Seat]() { Bots[Seat] = true; })]
				]
			]
		];
	}

	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(40.0f, 32.0f))[StepHeader(2, TEXT("Jugadores y reloj"))]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(40.0f))
		[
			SNew(SBox).WidthOverride(460.0f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						Label(TAttribute<FText>::CreateLambda([this]()
						{
							return Scenarios.IsValidIndex(SelectedScenario) ? Str(Scenarios[SelectedScenario].Name) : FText::GetEmpty();
						}), 20, true, AccentColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 6)[Section(TEXT("Jugadores"))]
					+ SVerticalBox::Slot().AutoHeight()[ChoiceRow(Counts)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[Seats]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 6)[Section(TEXT("Reloj por turno"))]
					+ SVerticalBox::Slot().AutoHeight()[ChoiceRow(Timers)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 22, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
						[
							SNew(SBox).WidthOverride(120.0f)[Button(Str(TEXT("Atrás")), [this]() { SetMenuStep(EMenuStep::World); }, true, EKind::Quiet)]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SBox).HeightOverride(44.0f)
							[
								Button(Str(TEXT("Comenzar partida")), [this]() { StartMatch(); },
									TAttribute<bool>::CreateLambda([this]() { return Scenarios.IsValidIndex(SelectedScenario); }), EKind::Primary, 15)
							]
						]
					],
					FMargin(22.0f, 18.0f))
			]
		];
}

TSharedRef<SWidget> AHFHUD::BuildOptions(TFunction<void()> OnBack)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return Panel(Button(Str(TEXT("Volver")), OnBack, true, EKind::Primary));
	}
	auto Apply = [Settings]()
	{
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	};

	TArray<TSharedRef<SButton>> Quality;
	const TCHAR* QualityNames[] = {TEXT("Baja"), TEXT("Media"), TEXT("Alta"), TEXT("Épica")};
	for (int32 Level = 0; Level < 4; ++Level)
	{
		Quality.Add(Choice(Str(QualityNames[Level]), [Settings, Level]() { return Settings->GetOverallScalabilityLevel() == Level; },
			[Settings, Level, Apply]() { Settings->SetOverallScalabilityLevel(Level); Apply(); }));
	}

	TArray<TSharedRef<SButton>> Modes;
	const TPair<EWindowMode::Type, const TCHAR*> ModeNames[] = {{EWindowMode::Windowed, TEXT("Ventana")}, {EWindowMode::WindowedFullscreen, TEXT("Sin bordes")}, {EWindowMode::Fullscreen, TEXT("Completa")}};
	for (const TPair<EWindowMode::Type, const TCHAR*>& Mode : ModeNames)
	{
		const EWindowMode::Type Value = Mode.Key;
		Modes.Add(Choice(Str(Mode.Value), [Settings, Value]() { return Settings->GetFullscreenMode() == Value; }, [Settings, Value, Apply]()
		{
			Settings->SetFullscreenMode(Value);
			if (Value == EWindowMode::WindowedFullscreen)
			{
				Settings->SetScreenResolution(Settings->GetDesktopResolution()); // sin bordes siempre ocupa el escritorio
			}
			Apply();
		}));
	}

	// Resoluciones que admite el monitor (desde 1280 de ancho), sin repetir.
	Resolutions.Reset();
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	for (const FIntPoint& Size : Supported)
	{
		if (Size.X >= 1280)
		{
			Resolutions.AddUnique(Size);
		}
	}
	if (Resolutions.IsEmpty())
	{
		Resolutions = {FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080)};
	}
	auto StepResolution = [this, Settings, Apply](int32 Delta)
	{
		const int32 Current = Resolutions.IndexOfByKey(Settings->GetScreenResolution());
		const int32 Next = Current == INDEX_NONE ? Resolutions.Num() - 1 : FMath::Clamp(Current + Delta, 0, Resolutions.Num() - 1);
		Settings->SetScreenResolution(Resolutions[Next]);
		Apply();
	};
	TAttribute<bool> CanResize = TAttribute<bool>::CreateLambda([Settings]() { return Settings->GetFullscreenMode() != EWindowMode::WindowedFullscreen; });
	TSharedRef<SWidget> Resolution = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(40.0f)[Button(Str(TEXT("◀")), [StepResolution]() { StepResolution(-1); }, CanResize)]]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			Centered(Label(TAttribute<FText>::CreateLambda([Settings]()
			{
				const FIntPoint Size = Settings->GetScreenResolution();
				return Str(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y));
			}), 13, true))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[SNew(SBox).WidthOverride(40.0f)[Button(Str(TEXT("▶")), [StepResolution]() { StepResolution(1); }, CanResize)]];

	TArray<TSharedRef<SButton>> VSync = {
		Choice(Str(TEXT("Sí")), [Settings]() { return Settings->IsVSyncEnabled(); }, [Settings, Apply]() { Settings->SetVSyncEnabled(true); Apply(); }),
		Choice(Str(TEXT("No")), [Settings]() { return !Settings->IsVSyncEnabled(); }, [Settings, Apply]() { Settings->SetVSyncEnabled(false); Apply(); })};

	TArray<TSharedRef<SButton>> Limits;
	for (float Limit : {30.0f, 60.0f, 120.0f, 0.0f})
	{
		Limits.Add(Choice(Str(Limit > 0.0f ? FString::Printf(TEXT("%d"), FMath::RoundToInt(Limit)) : FString(TEXT("Libre"))),
			[Settings, Limit]() { return FMath::IsNearlyEqual(Settings->GetFrameRateLimit(), Limit); },
			[Settings, Limit, Apply]() { Settings->SetFrameRateLimit(Limit); Apply(); }));
	}

	return SNew(SBox).WidthOverride(560.0f)
	[
		Panel(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[Label(Str(TEXT("Opciones")), 24, true, AccentColor)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Section(TEXT("Gráficos"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[SettingRow(TEXT("Calidad"), ChoiceRow(Quality))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[SettingRow(TEXT("Límite de cuadros"), ChoiceRow(Limits))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[SettingRow(TEXT("Sincronía vertical"), ChoiceRow(VSync))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 6)[Section(TEXT("Pantalla"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[SettingRow(TEXT("Modo"), ChoiceRow(Modes))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[SettingRow(TEXT("Resolución"), Resolution)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
			[
				Label(Str(TEXT("Los cambios se aplican al instante y se guardan para la próxima vez.")), 11, false, MutedColor)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 16, 0, 0)
			[
				SNew(SBox).WidthOverride(160.0f)[Button(Str(TEXT("Volver")), OnBack, true, EKind::Primary, 14)]
			],
			FMargin(24.0f, 20.0f))
	];
}

void AHFHUD::StartMatch()
{
	AHFGameMode* Mode = GetHexGameMode();
	if (!Mode || !Scenarios.IsValidIndex(SelectedScenario))
	{
		return;
	}
	FHFMatchConfig Config;
	Config.ScenarioId = Scenarios[SelectedScenario].Id;
	Config.TurnSeconds = TurnSeconds;
	// Misma semilla que la vista previa: el tablero que se ve es el que se juega.
	Config.Seed = Mode->GetPreviewScenarioId() == Config.ScenarioId ? Mode->GetPreviewSeed() : FMath::RandRange(1, 1000000);
	int32 HumanNumber = 0;
	int32 BotNumber = 0;
	for (int32 Seat = 0; Seat < PlayerCount; ++Seat)
	{
		Config.Bots.Add(Bots[Seat]);
		Config.PlayerNames.Add(Bots[Seat] ? FString::Printf(TEXT("Bot %d"), ++BotNumber) : FString::Printf(TEXT("Jugador %d"), ++HumanNumber));
	}
	FString Error;
	if (Mode->BeginHexMatch(Config, Error))
	{
		ShowGame();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No se pudo empezar: %s"), *Error);
	}
}

// ---------------------------------------------------------------- Juego

void AHFHUD::ShowGame()
{
	ResetTrade();
	SetContent(BuildGame());
}

void AHFHUD::ResetTrade()
{
	bTradeOpen = false;
	bAwaitingConsent = false;
	TradePartner = -1;
	const AHFGameMode* Mode = GetHexGameMode();
	const int32 ResourceCount = Mode && Mode->GetMatch() ? Mode->GetMatch()->GetScenario().ResourceCount() : 0;
	TradeGive.Init(0, ResourceCount);
	TradeGet.Init(0, ResourceCount);
}

TSharedRef<SWidget> AHFHUD::BuildGame()
{
	auto Mode = [this]() { return GetHexGameMode(); };

	return SNew(SOverlay)
		// Jugadores (arriba a la izquierda)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(390.0f)[BuildPlayersPanel()]
		]
		// Turno, dados, reloj y ayuda (arriba al centro)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.0f, 20.0f))
		[
			SNew(SBox).WidthOverride(520.0f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
					[
						Centered(Label(TAttribute<FText>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							if (!M || !M->GetMatch()) return FText::GetEmpty();
							const hf::Game& Game = M->GetMatch()->GetGame();
							if (Game.GetPhase() == hf::Phase::Setup) return Str(FString::Printf(TEXT("Preparación · %s"), *M->GetMatch()->PlayerName(Game.CurrentPlayer())));
							return Str(FString::Printf(TEXT("Ronda %d · Turno de %s"), Game.Round(), *M->GetMatch()->PlayerName(Game.CurrentPlayer())));
						}), 18, true, TAttribute<FSlateColor>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							return M && M->GetMatch() ? PlayerTextColor(M, M->GetMatch()->GetGame().CurrentPlayer()) : FSlateColor(TextColor);
						})))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0, 4)
					[
						Centered(Label(TAttribute<FText>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							if (!M || !M->GetMatch()) return FText::GetEmpty();
							return Str(M->IsRollingDice() ? FString(TEXT("Dados: rodando…")) : TEXT("Dados: ") + M->GetMatch()->DescribeRoll());
						}), 14, true))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
					[
						BuildTimer()
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
					[
						Centered(Label(TAttribute<FText>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							if (!M || !M->GetMatch()) return FText::GetEmpty();
							const FHFMatch& Match = *M->GetMatch();
							const hf::Game& Game = Match.GetGame();
							if (Game.GetPhase() == hf::Phase::Finished) return Str(TEXT("Partida terminada"));
							if (M->IsRollingDice()) return Str(TEXT("Los dados están rodando…"));
							if (Match.IsBotTurn()) return Str(FString::Printf(TEXT("Juega %s…"), *Match.PlayerName(Game.CurrentPlayer())));
							switch (Game.GetPhase())
							{
							case hf::Phase::Setup:
								return Str(Game.SetupNeedsRoute()
									? FString::Printf(TEXT("Colocá una %s junto a tu %s"), *Match.PieceName(hf::PieceKind::Route), *Match.PieceName(hf::PieceKind::Outpost))
									: FString::Printf(TEXT("Elegí dónde va tu %s (puntos resaltados)"), *Match.PieceName(hf::PieceKind::Outpost)));
							case hf::Phase::AwaitRoll:
								return Str(TEXT("Tirá los dados de terreno (Espacio). Antes podés reforzar una casilla con Influencia."));
							case hf::Phase::ChooseWild:
								return Str(TEXT("¡Comodín! Elegí qué terreno produce"));
							default:
								return Str(TEXT("Construí, comerciá o terminá el turno (Espacio)"));
							}
						}), 12, false, MutedColor))
					]
				)
			]
		]
		// Acciones y mercado (derecha)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(370.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[BuildActionsPanel()]
				+ SScrollBox::Slot().Padding(0, 10, 0, 0)[BuildMarketPanel()]
			]
		]
		// Contratos (abajo a la derecha)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(370.0f)[BuildContractsPanel()]
		]
		// Registro (abajo a la izquierda)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(460.0f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Section(TEXT("Registro"))]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Label(TAttribute<FText>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							return M ? Str(FString::Join(M->GetMessages(), TEXT("\n"))) : FText::GetEmpty();
						}), 12, false, TextColor)
					])
			]
		]
		// Aviso de error
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				const AHFGameMode* M = GetHexGameMode();
				return M && GetWorld() && GetWorld()->GetTimeSeconds() - M->GetLastErrorTime() < 2.5f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				Panel(Label(TAttribute<FText>::CreateLambda([this]()
				{
					const AHFGameMode* M = GetHexGameMode();
					return M ? Str(M->GetLastError()) : FText::GetEmpty();
				}), 16, true, DangerColor))
			]
		]
		// Comodín: elegir qué terreno produce
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(420.0f)
			.Visibility_Lambda([Mode]()
			{
				const AHFGameMode* M = Mode();
				return M && M->IsInMatch() && !M->GetMatch()->IsBotTurn() && M->GetMatch()->GetGame().GetPhase() == hf::Phase::ChooseWild
					? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				BuildWildPanel()
			]
		]
		// Intercambio
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(460.0f)
			.Visibility_Lambda([this]() { return bTradeOpen ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildTradePanel()
			]
		]
		// Ganador
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility_Lambda([Mode]()
			{
				const AHFGameMode* M = Mode();
				return M && M->GetMatch() && M->GetMatch()->GetGame().GetPhase() == hf::Phase::Finished ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 6)[Section(TEXT("Fin de la partida"))]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						Label(TAttribute<FText>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							if (!M || !M->GetMatch()) return FText::GetEmpty();
							return Str(FString::Printf(TEXT("¡%s gana la partida!"), *M->GetMatch()->PlayerName(M->GetMatch()->GetGame().Winner())));
						}), 28, true, TAttribute<FSlateColor>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							return M && M->GetMatch() ? PlayerTextColor(M, M->GetMatch()->GetGame().Winner()) : FSlateColor(TextColor);
						}))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 16, 0, 0)
					[
						SNew(SBox).WidthOverride(220.0f)[Button(Str(TEXT("Volver al menú")), [this]() { GoToMenu(); }, true, EKind::Primary, 14)]
					],
					FMargin(36.0f, 26.0f))
			]
		]
		// Pausa (Esc): por encima de todo
		+ SOverlay::Slot()
		[
			BuildPause()
		];
}

TSharedRef<SWidget> AHFHUD::BuildPause()
{
	auto PauseButton = [](const FString& Text, TFunction<void()> OnClick, EKind Kind)
	{
		return SNew(SBox).HeightOverride(44.0f)[Button(Str(Text), OnClick, true, Kind, 14)];
	};
	return SNew(SOverlay)
		.Visibility_Lambda([this]() { return bPaused ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(320.0f)
			.Visibility_Lambda([this]() { return bOptionsOpen ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 4)[Label(Str(TEXT("Pausa")), 26, true, AccentColor)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)
					[
						Label(TAttribute<FText>::CreateLambda([this]()
						{
							const AHFGameMode* M = GetHexGameMode();
							return M && M->GetMatch() ? Str(FString::Printf(TEXT("Ronda %d · el reloj está detenido"), M->GetMatch()->GetGame().Round())) : FText::GetEmpty();
						}), 12, false, MutedColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[PauseButton(TEXT("Continuar"), [this]() { SetPaused(false); }, EKind::Primary)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[PauseButton(TEXT("Opciones"), [this]() { bOptionsOpen = true; }, EKind::Normal)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[PauseButton(TEXT("Volver al menú"), [this]() { GoToMenu(); }, EKind::Normal)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[PauseButton(TEXT("Salir del juego"), [this]() { QuitGame(); }, EKind::Quiet)],
					FMargin(24.0f, 22.0f))
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bPaused && bOptionsOpen ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildOptions([this]() { bOptionsOpen = false; })
			]
		];
}

TSharedRef<SWidget> AHFHUD::BuildPlayersPanel()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const AHFGameMode* Mode = GetHexGameMode();
	if (!Mode || !Mode->GetMatch())
	{
		return Panel(Box);
	}
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[Section(TEXT("Jugadores"))];
	const int32 Count = static_cast<int32>(Mode->GetMatch()->GetGame().Players().size());
	for (int32 Player = 0; Player < Count; ++Player)
	{
		auto IsCurrent = [this, Player]()
		{
			const AHFGameMode* M = GetHexGameMode();
			if (!M || !M->GetMatch()) return false;
			const hf::Game& Game = M->GetMatch()->GetGame();
			return Game.CurrentPlayer() == Player && Game.GetPhase() != hf::Phase::Finished;
		};
		const FLinearColor Color = PlayerColor(Mode, Player);
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SBorder)
			.BorderImage(RoundBrush())
			.BorderBackgroundColor_Lambda([IsCurrent]() { return IsCurrent() ? FLinearColor(1.0f, 1.0f, 1.0f, 0.1f) : FLinearColor::Transparent; })
			.Padding(FMargin(8.0f, 6.0f))
			[
				SNew(SHorizontalBox)
				// Franja del color del jugador; más ancha en su turno.
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
				[
					SNew(SBox).WidthOverride_Lambda([IsCurrent]() { return FOptionalSize(IsCurrent() ? 6.0f : 3.0f); })
					[
						SNew(SBorder).BorderImage(RoundBrush(3.0f)).BorderBackgroundColor(Color)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							Label(TAttribute<FText>::CreateLambda([this, Player]()
							{
								const AHFGameMode* M = GetHexGameMode();
								return M && M->GetMatch() ? Str(M->GetMatch()->PlayerName(Player)) : FText::GetEmpty();
							}), 14, true, PlayerTextColor(Mode, Player))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							Label(TAttribute<FText>::CreateLambda([this, Player]()
							{
								const AHFGameMode* M = GetHexGameMode();
								if (!M || !M->GetMatch()) return FText::GetEmpty();
								const hf::Game& Game = M->GetMatch()->GetGame();
								return Str(FString::Printf(TEXT("%d / %d"), Game.Prestige(Player), Game.PrestigeTarget()));
							}), 13, true)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 4)
					[
						SNew(SBox).HeightOverride(5.0f)
						[
							SNew(SProgressBar)
							.FillColorAndOpacity(Color)
							.Percent_Lambda([this, Player]()
							{
								const AHFGameMode* M = GetHexGameMode();
								if (!M || !M->GetMatch()) return TOptional<float>(0.0f);
								const hf::Game& Game = M->GetMatch()->GetGame();
								return TOptional<float>(Game.PrestigeTarget() > 0 ? FMath::Min(1.0f, static_cast<float>(Game.Prestige(Player)) / Game.PrestigeTarget()) : 0.0f);
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Label(TAttribute<FText>::CreateLambda([this, Player]()
						{
							const AHFGameMode* M = GetHexGameMode();
							if (!M || !M->GetMatch()) return FText::GetEmpty();
							const FHFMatch& Match = *M->GetMatch();
							const hf::Player& P = Match.GetGame().Players()[static_cast<size_t>(Player)];
							TArray<FString> Parts;
							for (int32 R = 0; R < static_cast<int32>(P.resources.size()); ++R)
							{
								Parts.Add(FString::Printf(TEXT("%s %d"), *Match.ResourceName(R), P.resources[static_cast<size_t>(R)]));
							}
							return Str(FString::Join(Parts, TEXT(" · ")) + FString::Printf(TEXT("\nMonedas %d · Influencia %d"), P.coins, P.influence));
						}), 11, false, MutedColor)
					]
				]
			]
		];
	}
	return Panel(Box);
}

TSharedRef<SWidget> AHFHUD::BuildActionsPanel()
{
	auto HumanPhase = [this](hf::Phase Phase)
	{
		const AHFGameMode* M = GetHexGameMode();
		return M && M->IsInMatch() && !M->GetMatch()->IsBotTurn() && !M->IsRollingDice() && M->GetMatch()->GetGame().GetPhase() == Phase;
	};
	auto CanBuild = [this, HumanPhase](hf::PieceKind Kind)
	{
		if (!HumanPhase(hf::Phase::Main)) return false;
		const hf::Game& Game = GetHexGameMode()->GetMatch()->GetGame();
		const int32 Player = Game.CurrentPlayer();
		if (!Game.CanBuild(Player, Kind)) return false;
		switch (Kind)
		{
		case hf::PieceKind::Outpost: return !Game.LegalOutpostVertices(Player).empty();
		case hf::PieceKind::Route: return !Game.LegalRouteEdges(Player).empty();
		case hf::PieceKind::Center: return !Game.LegalUpgradeVertices(Player).empty();
		default: return true;
		}
	};
	auto PieceLabel = [this](hf::PieceKind Kind)
	{
		return TAttribute<FText>::CreateLambda([this, Kind]()
		{
			const AHFGameMode* M = GetHexGameMode();
			if (!M || !M->GetMatch()) return FText::GetEmpty();
			const FHFMatch& Match = *M->GetMatch();
			return Str(FString::Printf(TEXT("%s  (%s)"), *Match.PieceName(Kind), *Match.CostText(Match.GetScenario().Piece(Kind).cost)));
		});
	};
	auto UseTool = [this](EHFTool Tool)
	{
		if (AHFPlayerController* Controller = GetHexController())
		{
			Controller->SetTool(Tool);
		}
	};

	return Panel(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Section(TEXT("Acciones"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(Str(TEXT("Tirar dados")), [this, UseTool]()
			{
				UseTool(EHFTool::None);
				if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::Roll());
			},
				TAttribute<bool>::CreateLambda([HumanPhase]() { return HumanPhase(hf::Phase::AwaitRoll); }), EKind::Primary)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(Str(TEXT("Reforzar casilla (1 Influencia)")), [UseTool]() { UseTool(EHFTool::Boost); },
				TAttribute<bool>::CreateLambda([this, HumanPhase]()
				{
					if (!HumanPhase(hf::Phase::AwaitRoll)) return false;
					const hf::Game& Game = GetHexGameMode()->GetMatch()->GetGame();
					return Game.BoostedTile() < 0 && Game.Players()[static_cast<size_t>(Game.CurrentPlayer())].influence > 0;
				}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(PieceLabel(hf::PieceKind::Outpost), [UseTool]() { UseTool(EHFTool::Outpost); },
				TAttribute<bool>::CreateLambda([CanBuild]() { return CanBuild(hf::PieceKind::Outpost); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(PieceLabel(hf::PieceKind::Route), [UseTool]() { UseTool(EHFTool::Route); },
				TAttribute<bool>::CreateLambda([CanBuild]() { return CanBuild(hf::PieceKind::Route); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(PieceLabel(hf::PieceKind::Center), [UseTool]() { UseTool(EHFTool::Center); },
				TAttribute<bool>::CreateLambda([CanBuild]() { return CanBuild(hf::PieceKind::Center); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(PieceLabel(hf::PieceKind::Work), [this]() { if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::Work()); },
				TAttribute<bool>::CreateLambda([CanBuild]() { return CanBuild(hf::PieceKind::Work); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(Str(TEXT("Intercambiar con otro jugador")), [this]() { ResetTrade(); bTradeOpen = true; },
				TAttribute<bool>::CreateLambda([HumanPhase]() { return HumanPhase(hf::Phase::Main); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)
		[
			Button(Str(TEXT("Terminar turno")), [this, UseTool]()
			{
				UseTool(EHFTool::None);
				if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::End());
			},
				TAttribute<bool>::CreateLambda([HumanPhase]() { return HumanPhase(hf::Phase::Main); }), EKind::Primary)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
		[
			Button(Str(TEXT("Pausa (Esc)")), [this]() { SetPaused(true); }, true, EKind::Quiet)
		]);
}

TSharedRef<SWidget> AHFHUD::BuildMarketPanel()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[Section(TEXT("Mercado"))];
	const AHFGameMode* Mode = GetHexGameMode();
	if (!Mode || !Mode->GetMatch())
	{
		return Panel(Box);
	}
	auto InMain = [this]()
	{
		const AHFGameMode* M = GetHexGameMode();
		return M && M->IsInMatch() && !M->GetMatch()->IsBotTurn() && M->GetMatch()->GetGame().GetPhase() == hf::Phase::Main;
	};
	const int32 ResourceCount = Mode->GetMatch()->GetScenario().ResourceCount();
	for (int32 Resource = 0; Resource < ResourceCount; ++Resource)
	{
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Label(TAttribute<FText>::CreateLambda([this, Resource]()
				{
					const AHFGameMode* M = GetHexGameMode();
					if (!M || !M->GetMatch()) return FText::GetEmpty();
					return Str(FString::Printf(TEXT("%s  (%d)"), *M->GetMatch()->ResourceName(Resource), M->GetMatch()->GetGame().MarketStock(Resource)));
				}), 12)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				Button(TAttribute<FText>::CreateLambda([this, Resource]()
				{
					const AHFGameMode* M = GetHexGameMode();
					return M && M->GetMatch() ? Str(FString::Printf(TEXT("Comprar %d"), M->GetMatch()->GetGame().MarketBuyPrice(Resource))) : FText::GetEmpty();
				}),
				[this, Resource]() { if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::Buy(Resource)); },
				TAttribute<bool>::CreateLambda([this, Resource, InMain]()
				{
					if (!InMain()) return false;
					const hf::Game& Game = GetHexGameMode()->GetMatch()->GetGame();
					return Game.MarketStock(Resource) > 0 && Game.Players()[static_cast<size_t>(Game.CurrentPlayer())].coins >= Game.MarketBuyPrice(Resource);
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(TAttribute<FText>::CreateLambda([this, Resource]()
				{
					const AHFGameMode* M = GetHexGameMode();
					return M && M->GetMatch() ? Str(FString::Printf(TEXT("Vender %d"), M->GetMatch()->GetGame().MarketSellPrice(Resource))) : FText::GetEmpty();
				}),
				[this, Resource]() { if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::Sell(Resource)); },
				TAttribute<bool>::CreateLambda([this, Resource, InMain]()
				{
					if (!InMain()) return false;
					const hf::Game& Game = GetHexGameMode()->GetMatch()->GetGame();
					return Game.Players()[static_cast<size_t>(Game.CurrentPlayer())].resources[static_cast<size_t>(Resource)] > 0;
				}))
			]
		];
	}
	return Panel(Box);
}

TSharedRef<SWidget> AHFHUD::BuildContractsPanel()
{
	return Panel(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Section(TEXT("Contratos"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Label(TAttribute<FText>::CreateLambda([this]()
			{
				const AHFGameMode* M = GetHexGameMode();
				if (!M || !M->GetMatch()) return FText::GetEmpty();
				const FHFMatch& Match = *M->GetMatch();
				const auto& Contracts = Match.GetScenario().contracts;
				TArray<FString> Lines;
				for (size_t C = 0; C < Contracts.size(); ++C)
				{
					const int32 Owner = Match.GetGame().ContractOwners()[C];
					Lines.Add(FString::Printf(TEXT("%s  +%d  %s"), *FHFMatch::ToFString(Contracts[C].name), Contracts[C].prestige,
						Owner >= 0 ? *(TEXT("» ") + Match.PlayerName(Owner)) : TEXT("")));
				}
				return Str(FString::Join(Lines, TEXT("\n")));
			}), 12, false, MutedColor)
		]);
}

TSharedRef<SWidget> AHFHUD::BuildWildPanel()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const AHFGameMode* Mode = GetHexGameMode();
	if (!Mode || !Mode->GetMatch())
	{
		return Panel(Box);
	}
	const FHFMatch& StartMatchRef = *Mode->GetMatch();
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[Label(Str(TEXT("Comodín")), 18, true, AccentColor)];
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[
		Label(TAttribute<FText>::CreateLambda([this]()
		{
			const AHFGameMode* M = GetHexGameMode();
			if (!M || !M->GetMatch()) return FText::GetEmpty();
			return Str(M->GetMatch()->GetGame().PendingWilds() >= 2
				? FString(TEXT("Salieron dos comodines: el terreno que elijas produce el doble."))
				: FString(TEXT("Elegí qué terreno produce junto con el otro dado.")));
		}), 12, false, MutedColor)
	];
	for (int32 Terrain : StartMatchRef.GetScenario().ProducingTerrains())
	{
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			Button(TAttribute<FText>::CreateLambda([this, Terrain]()
			{
				const AHFGameMode* M = GetHexGameMode();
				if (!M || !M->GetMatch()) return FText::GetEmpty();
				const FHFMatch& Match = *M->GetMatch();
				const hf::Game& Game = Match.GetGame();
				const int32 Resource = Match.GetScenario().terrains[static_cast<size_t>(Terrain)].resource;
				// Cuánto le da a este jugador, para decidir rápido.
				return Str(FString::Printf(TEXT("%s: %s (te da %d)"), *Match.TerrainName(Terrain), *Match.ResourceName(Resource),
					Game.TerrainYieldFor(Game.CurrentPlayer(), Terrain)));
			}),
			[this, Terrain]() { if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::Wild(Terrain)); })
		];
	}
	return Panel(Box);
}

TSharedRef<SWidget> AHFHUD::BuildTradePanel()
{
	const AHFGameMode* Mode = GetHexGameMode();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (!Mode || !Mode->GetMatch())
	{
		return Panel(Box);
	}
	const FHFMatch& StartMatchRef = *Mode->GetMatch();
	const int32 PlayerTotal = static_cast<int32>(StartMatchRef.GetGame().Players().size());
	const int32 ResourceCount = StartMatchRef.GetScenario().ResourceCount();

	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[Label(Str(TEXT("Intercambio")), 16, true, AccentColor)];

	TSharedRef<SHorizontalBox> Partners = SNew(SHorizontalBox);
	for (int32 Player = 0; Player < PlayerTotal; ++Player)
	{
		Partners->AddSlot().AutoWidth().Padding(0, 0, 6, 0)
		[
			SNew(SButton)
			.Visibility_Lambda([this, Player]()
			{
				const AHFGameMode* M = GetHexGameMode();
				return M && M->GetMatch() && M->GetMatch()->GetGame().CurrentPlayer() != Player ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.OnClicked_Lambda([this, Player]() { TradePartner = Player; return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text_Lambda([this, Player]()
				{
					const AHFGameMode* M = GetHexGameMode();
					return M && M->GetMatch() ? Str(M->GetMatch()->PlayerName(Player)) : FText::GetEmpty();
				})
				.Font(Font(12, true))
				.ColorAndOpacity_Lambda([this, Player]() { return FSlateColor(TradePartner == Player ? AccentColor : TextColor); })
			]
		];
	}
	Box->AddSlot().AutoHeight()[Label(Str(TEXT("Con quién")), 12, false, MutedColor)];
	Box->AddSlot().AutoHeight().Padding(0, 2, 0, 8)[Partners];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f)[Label(Str(TEXT("Recurso")), 12, true, MutedColor)]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[Label(Str(TEXT("Das")), 12, true, MutedColor)]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[Label(Str(TEXT("Recibís")), 12, true, MutedColor)]
	];

	auto Stepper = [](TArray<int32>* Amounts, int32 Resource)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Button(Str(TEXT("-")), [Amounts, Resource]() { (*Amounts)[Resource] = FMath::Max(0, (*Amounts)[Resource] - 1); })]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
			[
				Label(TAttribute<FText>::CreateLambda([Amounts, Resource]() { return Amounts->IsValidIndex(Resource) ? FText::AsNumber((*Amounts)[Resource]) : FText::GetEmpty(); }), 13, true)
			]
			+ SHorizontalBox::Slot().AutoWidth()[Button(Str(TEXT("+")), [Amounts, Resource]() { (*Amounts)[Resource] = FMath::Min(20, (*Amounts)[Resource] + 1); })];
	};
	for (int32 Resource = 0; Resource < ResourceCount; ++Resource)
	{
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Label(Str(StartMatchRef.ResourceName(Resource)), 12)]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[Stepper(&TradeGive, Resource)]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[Stepper(&TradeGet, Resource)]
		];
	}

	// Confirmación del otro jugador humano (se pasa el control).
	Box->AddSlot().AutoHeight().Padding(0, 10, 0, 0)
	[
		SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return bAwaitingConsent ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Label(TAttribute<FText>::CreateLambda([this]()
			{
				const AHFGameMode* M = GetHexGameMode();
				return M && M->GetMatch() ? Str(FString::Printf(TEXT("%s, ¿aceptás la oferta?"), *M->GetMatch()->PlayerName(TradePartner))) : FText::GetEmpty();
			}), 14, true, AccentColor)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
			[
				Button(Str(TEXT("Aceptar")), [this]()
				{
					if (AHFGameMode* M = GetHexGameMode())
					{
						M->ApplyHumanAction(hf::Action::Trade(TradePartner, std::vector<int>(TradeGive.GetData(), TradeGive.GetData() + TradeGive.Num()),
							std::vector<int>(TradeGet.GetData(), TradeGet.GetData() + TradeGet.Num())));
					}
					ResetTrade();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[Button(Str(TEXT("Rechazar")), [this]() { ResetTrade(); })]
		]
	];

	Box->AddSlot().AutoHeight().Padding(0, 10, 0, 0)
	[
		SNew(SHorizontalBox)
		.Visibility_Lambda([this]() { return bAwaitingConsent ? EVisibility::Collapsed : EVisibility::Visible; })
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
		[
			Button(Str(TEXT("Proponer")), [this]() { ProposeTrade(); }, TAttribute<bool>::CreateLambda([this]()
			{
				int32 Give = 0;
				int32 Get = 0;
				for (int32 Amount : TradeGive) Give += Amount;
				for (int32 Amount : TradeGet) Get += Amount;
				return TradePartner >= 0 && Give > 0 && Get > 0;
			}))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[Button(Str(TEXT("Cancelar")), [this]() { ResetTrade(); })]
	];
	return Panel(Box);
}

void AHFHUD::ProposeTrade()
{
	AHFGameMode* Mode = GetHexGameMode();
	if (!Mode || !Mode->IsInMatch() || TradePartner < 0)
	{
		return;
	}
	const FHFMatch& Match = *Mode->GetMatch();
	const std::vector<int> Give(TradeGive.GetData(), TradeGive.GetData() + TradeGive.Num());
	const std::vector<int> Get(TradeGet.GetData(), TradeGet.GetData() + TradeGet.Num());
	if (!Match.IsBot(TradePartner))
	{
		bAwaitingConsent = true;
		return;
	}
	// El bot da lo que el jugador pide y recibe lo que el jugador ofrece.
	if (Match.BotAcceptsTrade(TradePartner, Get, Give))
	{
		Mode->ApplyHumanAction(hf::Action::Trade(TradePartner, Give, Get));
	}
	else
	{
		Mode->ShowError(FString::Printf(TEXT("%s rechaza la oferta"), *Match.PlayerName(TradePartner)));
	}
	ResetTrade();
}

// ---------------------------------------------------------------- Reloj

TSharedRef<SWidget> AHFHUD::BuildTimer()
{
	return SNew(SHorizontalBox)
		.Visibility_Lambda([this]()
		{
			const AHFGameMode* M = GetHexGameMode();
			return M && M->GetTurnTimeLeft() >= 0.0f ? EVisibility::Visible : EVisibility::Collapsed;
		})
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0, 0, 10, 0)
		[
			SNew(SBox).HeightOverride(10.0f)
			[
				SNew(SProgressBar)
				.Percent_Lambda([this]()
				{
					const AHFGameMode* M = GetHexGameMode();
					return M && M->GetTurnSeconds() > 0 ? TOptional<float>(M->GetTurnTimeLeft() / M->GetTurnSeconds()) : TOptional<float>(0.0f);
				})
				.FillColorAndOpacity_Lambda([this]()
				{
					// Los últimos 10 segundos la barra se pone roja y titila.
					const AHFGameMode* M = GetHexGameMode();
					const float Left = M ? M->GetTurnTimeLeft() : 0.0f;
					if (Left > 10.0f) return FSlateColor(AccentColor);
					const float Blink = 0.6f + 0.4f * FMath::Abs(FMath::Sin(Now() * 6.0f));
					return FSlateColor(DangerColor * Blink);
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Label(TAttribute<FText>::CreateLambda([this]()
			{
				const AHFGameMode* M = GetHexGameMode();
				const int32 Left = M ? FMath::CeilToInt(M->GetTurnTimeLeft()) : 0;
				return Str(FString::Printf(TEXT("%d:%02d"), Left / 60, Left % 60));
			}), 14, true, TAttribute<FSlateColor>::CreateLambda([this]()
			{
				const AHFGameMode* M = GetHexGameMode();
				return FSlateColor(M && M->GetTurnTimeLeft() <= 10.0f ? DangerColor : TextColor);
			}))
		];
}

// ---------------------------------------------------------------- Mano de cartas

namespace
{
const float CardWidth = 96.0f;
const float CardHeight = 134.0f;
const float CardSpacing = 104.0f;
const float FlightSeconds = 0.8f;
const float LeaveSeconds = 0.6f;

FVector2D Rotate2D(const FVector2D& V, float Degrees)
{
	const float R = FMath::DegreesToRadians(Degrees);
	return FVector2D(V.X * FMath::Cos(R) - V.Y * FMath::Sin(R), V.X * FMath::Sin(R) + V.Y * FMath::Cos(R));
}

FLinearColor WithAlpha(FLinearColor Color, float Alpha)
{
	Color.A = Alpha;
	return Color;
}
}

float AHFHUD::Now() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
}

int32 AHFHUD::HandPlayer() const
{
	// Se muestra la mano del humano que juega; en el turno de un bot, la del último humano.
	const AHFGameMode* M = GetHexGameMode();
	if (!M || !M->IsInMatch())
	{
		return -1;
	}
	const FHFMatch& Match = *M->GetMatch();
	const int32 Current = Match.GetGame().CurrentPlayer();
	if (!Match.IsBot(Current))
	{
		return Current;
	}
	if (LastHumanPlayer >= 0)
	{
		return LastHumanPlayer;
	}
	for (int32 P = 0; P < static_cast<int32>(Match.GetGame().Players().size()); ++P)
	{
		if (!Match.IsBot(P))
		{
			return P;
		}
	}
	return Current; // todos bots: se ve la mano de quien juega
}

void AHFHUD::QueueCardFlight(int32 Player, int32 Resource, int32 Amount, const FVector& WorldLocation)
{
	APlayerController* Controller = GetOwningPlayerController();
	FVector2D Screen;
	if (!Controller || !Controller->ProjectWorldLocationToScreen(WorldLocation + FVector(0, 0, 80), Screen, false))
	{
		return;
	}
	// Hasta 4 cartas por casilla, una detrás de otra; la última lleva el resto.
	const int32 Cards = FMath::Clamp(Amount, 1, 4);
	float Start = Now();
	for (const FCardFlight& Existing : Flights)
	{
		Start = FMath::Max(Start, Existing.StartTime - 0.25f); // las tandas no salen todas juntas
	}
	for (int32 I = 0; I < Cards; ++I)
	{
		FCardFlight Flight;
		Flight.Player = Player;
		Flight.Resource = Resource;
		Flight.Amount = I == Cards - 1 ? Amount - (Cards - 1) : 1;
		Flight.Start = Screen;
		Flight.StartTime = Start + I * 0.12f;
		Flights.Add(Flight);
	}
}

void AHFHUD::DrawPolygon(const FVector2D& Center, float Radius, int32 Sides, float Angle, const FLinearColor& Color)
{
	for (int32 K = 0; K < Sides; ++K)
	{
		const float A = FMath::DegreesToRadians(Angle - 90.0f + 360.0f * K / Sides);
		const float B = FMath::DegreesToRadians(Angle - 90.0f + 360.0f * (K + 1) / Sides);
		FCanvasTriangleItem Triangle(Center, Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius,
			Center + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Radius, GWhiteTexture);
		Triangle.SetColor(Color);
		Triangle.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Triangle);
	}
}

UTexture* AHFHUD::ResourceArt(int32 Resource)
{
	const AHFGameMode* M = GetHexGameMode();
	AHFBoardActor* Board = M ? M->GetBoard() : nullptr;
	if (!Board || !M->GetMatch())
	{
		return nullptr;
	}
	const hf::Scenario& Scenario = M->GetMatch()->GetScenario();
	const FString ScenarioId = FHFMatch::ToFString(Scenario.id);
	if (ScenarioId != CardArtScenario)
	{
		CardArt.Reset();
		CardArtRetry.Reset();
		CardArtScenario = ScenarioId;
	}
	if (const TObjectPtr<UTexture>* Found = CardArt.Find(Resource); Found && *Found)
	{
		return Found->Get();
	}
	// Al arrancar el juego el registro de recursos puede no estar listo: si no aparece, se reintenta cada 2 s.
	float& Retry = CardArtRetry.FindOrAdd(Resource);
	if (Now() < Retry)
	{
		return nullptr;
	}
	Retry = Now() + 2.0f;
	UTexture* Art = nullptr;
	for (const hf::TerrainDef& Terrain : Scenario.terrains)
	{
		if (Terrain.resource == Resource)
		{
			if (UMaterialInterface* Surface = Board->LoadSurface(FHFMatch::ToFString(Terrain.id)))
			{
				Surface->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("ColorTex")), Art, true);
			}
			break;
		}
	}
	if (UTexture2D* Texture = Cast<UTexture2D>(Art))
	{
		Texture->bForceMiplevelsToBeResident = true; // en pantalla no hay distancia: se quiere nítida
	}
	CardArt.Add(Resource, Art);
	return Art;
}

void AHFHUD::DrawCard(int32 Resource, const FVector2D& Center, float Angle, float Scale, float Alpha, int32 Count)
{
	const AHFGameMode* M = GetHexGameMode();
	if (!M || !M->GetMatch() || Alpha <= 0.0f)
	{
		return;
	}
	const FHFMatch& Match = *M->GetMatch();
	const hf::ResourceDef& Def = Match.GetScenario().resources[static_cast<size_t>(Resource)];
	const FLinearColor Color = FHFMatch::ToLinear(Def.color);
	const float S = Canvas->ClipY / 1080.0f * Scale;
	const float W = CardWidth * S;
	const float H = CardHeight * S;
	if (Count == 0)
	{
		Alpha *= 0.4f; // sin cartas de ese recurso: se ve apagada
	}

	auto Tile = [&](const FVector2D& Offset, const FVector2D& Size, const FLinearColor& TileColor)
	{
		FCanvasTileItem Item(Center + Rotate2D(Offset, Angle) - Size * 0.5f, Size, WithAlpha(TileColor, TileColor.A * Alpha));
		Item.Rotation = FRotator(0.0f, Angle, 0.0f);
		Item.PivotPoint = FVector2D(0.5f, 0.5f);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	};
	auto Text = [&](const FVector2D& Offset, const FString& Value, UFont* TextFont, float TextScale, const FLinearColor& Ink)
	{
		FCanvasTextItem Item(Center + Rotate2D(Offset, Angle), FText::FromString(Value), TextFont, WithAlpha(Ink, Alpha));
		Item.bCentreX = true;
		Item.bCentreY = true;
		Item.Scale = FVector2D(TextScale * S);
		Item.EnableShadow(WithAlpha(FLinearColor::Black, 0.7f * Alpha));
		Canvas->DrawItem(Item);
	};
	const FLinearColor Light = FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.35f);

	Tile(FVector2D(5, 7) * S, FVector2D(W, H), FLinearColor(0, 0, 0, 0.35f));                         // sombra
	Tile(FVector2D::ZeroVector, FVector2D(W, H), FLinearColor(0.06f, 0.06f, 0.07f, 1.0f));               // borde
	Tile(FVector2D::ZeroVector, FVector2D(W - 6 * S, H - 6 * S), Color * 0.45f + FLinearColor(0, 0, 0, 0.55f)); // fondo
	const FVector2D ArtOffset(0, -H * 0.2f);
	const FVector2D ArtSize(W - 6 * S, H * 0.6f - 3 * S);
	if (UTexture* Art = ResourceArt(Resource))
	{
		// Un recorte cuadrado de la textura real del terreno, con su proporción.
		FCanvasTileItem Item(Center + Rotate2D(ArtOffset, Angle) - ArtSize * 0.5f, Art->GetResource(), ArtSize, WithAlpha(FLinearColor::White, Alpha));
		Item.UV0 = FVector2D(0.2f, 0.2f);
		Item.UV1 = FVector2D(0.2f + 0.6f * ArtSize.X / ArtSize.Y, 0.8f);
		Item.Rotation = FRotator(0.0f, Angle, 0.0f);
		Item.PivotPoint = FVector2D(0.5f, 0.5f);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
		// Velo del color del recurso arriba, para que la carta se reconozca aunque dos texturas se parezcan.
		Tile(ArtOffset - FVector2D(0, ArtSize.Y * 0.38f), FVector2D(ArtSize.X, ArtSize.Y * 0.24f), FLinearColor(Color.R, Color.G, Color.B, 0.75f));
	}
	else
	{
		Tile(ArtOffset, ArtSize, Color * 0.8f + FLinearColor(0, 0, 0, 0.2f));
	}

	// Emblema: un polígono distinto por recurso con la inicial encima.
	const FVector2D Emblem = Center + Rotate2D(FVector2D(0, -H * 0.17f), Angle);
	const int32 Sides = 3 + Resource % 4;
	DrawPolygon(Emblem, 30.0f * S, Sides, Angle, WithAlpha(FLinearColor(0.05f, 0.05f, 0.06f, 1.0f), 0.8f * Alpha));
	DrawPolygon(Emblem, 25.0f * S, Sides, Angle, WithAlpha(Light, Alpha));
	const FString Name = FHFMatch::ToFString(Def.name);
	Text(FVector2D(0, -H * 0.17f + 1.0f * S), Name.Left(1).ToUpper(), GEngine->GetLargeFont(), 0.9f, FLinearColor(0.06f, 0.06f, 0.07f));
	Text(FVector2D(0, H * 0.3f), Name, GEngine->GetSmallFont(), 1.1f, FLinearColor(0.97f, 0.95f, 0.9f));

	if (Count >= 0)
	{
		const FVector2D Badge = Center + Rotate2D(FVector2D(W * 0.5f - 12 * S, -H * 0.5f + 12 * S), Angle);
		DrawPolygon(Badge, 17.0f * S, 16, 0.0f, WithAlpha(FLinearColor(0.05f, 0.05f, 0.06f, 1.0f), Alpha));
		DrawPolygon(Badge, 14.0f * S, 16, 0.0f, WithAlpha(Count > 0 ? FLinearColor(1.0f, 0.78f, 0.35f) : FLinearColor(0.4f, 0.4f, 0.4f), Alpha));
		Text(Badge - Center, FString::FromInt(Count), GEngine->GetMediumFont(), 1.0f, FLinearColor(0.06f, 0.06f, 0.07f));
	}
}

void AHFHUD::DrawHUD()
{
	Super::DrawHUD();
	const AHFGameMode* M = GetHexGameMode();
	if (!Canvas || !M || !M->IsInMatch())
	{
		Flights.Reset();
		Leaving.Reset();
		PrevHandPlayer = -1;
		LastHumanPlayer = -1;
		return;
	}
	const FHFMatch& Match = *M->GetMatch();
	const hf::Game& Game = Match.GetGame();
	if (!Match.IsBot(Game.CurrentPlayer()))
	{
		LastHumanPlayer = Game.CurrentPlayer();
	}
	const int32 Hand = HandPlayer();
	const int32 ResourceCount = Match.GetScenario().ResourceCount();
	if (Hand < 0 || ResourceCount <= 0)
	{
		return;
	}
	if (PopTime.Num() != ResourceCount)
	{
		PopTime.Init(-100.0f, ResourceCount);
		ShakeTime.Init(-100.0f, ResourceCount);
	}
	const float T = Now();
	const float S = Canvas->ClipY / 1080.0f;
	const hf::Player& HandOwner = Game.Players()[static_cast<size_t>(Hand)];

	// Ubicación de cada carta en el abanico (una por recurso).
	SlotPositions.SetNum(ResourceCount);
	TArray<float> SlotAngles;
	SlotAngles.SetNum(ResourceCount);
	for (int32 R = 0; R < ResourceCount; ++R)
	{
		const float Offset = R - (ResourceCount - 1) * 0.5f;
		SlotPositions[R] = FVector2D(Canvas->ClipX * 0.5f + Offset * CardSpacing * S,
			Canvas->ClipY - 26.0f * S - CardHeight * S * 0.5f + Offset * Offset * 7.0f * S);
		SlotAngles[R] = Offset * 5.0f;
	}

	// Lo que ya llegó a la mano (las cartas en vuelo todavía no cuentan).
	TArray<int32> Pending;
	Pending.Init(0, ResourceCount);
	for (const FCardFlight& Flight : Flights)
	{
		if (Flight.Player == Hand && Pending.IsValidIndex(Flight.Resource))
		{
			Pending[Flight.Resource] += Flight.Amount;
		}
	}

	// Cartas gastadas: tiemblan y salen volando hacia arriba.
	if (PrevHandPlayer == Hand && PrevCounts.Num() == ResourceCount)
	{
		for (int32 R = 0; R < ResourceCount; ++R)
		{
			const int32 Spent = PrevCounts[R] - HandOwner.resources[static_cast<size_t>(R)];
			if (Spent <= 0)
			{
				continue;
			}
			ShakeTime[R] = T;
			for (int32 K = 0; K < FMath::Min(Spent, 3); ++K)
			{
				Leaving.Add({R, SlotPositions[R], T + K * 0.09f, FMath::FRandRange(-1.0f, 1.0f)});
			}
		}
	}
	PrevCounts.SetNum(ResourceCount);
	for (int32 R = 0; R < ResourceCount; ++R)
	{
		PrevCounts[R] = HandOwner.resources[static_cast<size_t>(R)];
	}
	PrevHandPlayer = Hand;

	// Título de la mano.
	{
		const FString Title = Match.IsBot(Hand) ? FString::Printf(TEXT("Cartas de %s"), *Match.PlayerName(Hand)) : FString::Printf(TEXT("Tus cartas · %s"), *Match.PlayerName(Hand));
		FCanvasTextItem Item(FVector2D(Canvas->ClipX * 0.5f, Canvas->ClipY - 26.0f * S - CardHeight * S - 40.0f * S), FText::FromString(Title),
			GEngine->GetMediumFont(), FLinearColor(0.93f, 0.92f, 0.88f, 0.9f));
		Item.bCentreX = true;
		Item.Scale = FVector2D(S);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	}

	// El abanico.
	for (int32 R = 0; R < ResourceCount; ++R)
	{
		FVector2D Position = SlotPositions[R];
		float Scale = 1.0f;
		const float Pop = (T - PopTime[R]) / 0.35f;
		if (Pop >= 0.0f && Pop < 1.0f)
		{
			Scale += 0.22f * FMath::Sin(Pop * UE_PI);
			Position.Y -= 22.0f * S * FMath::Sin(Pop * UE_PI);
		}
		const float Shake = (T - ShakeTime[R]) / 0.4f;
		if (Shake >= 0.0f && Shake < 1.0f)
		{
			Position.X += FMath::Sin(Shake * 50.0f) * 7.0f * S * (1.0f - Shake);
		}
		DrawCard(R, Position, SlotAngles[R], Scale, 1.0f, FMath::Max(0, HandOwner.resources[static_cast<size_t>(R)] - Pending[R]));
	}

	// Cartas que se van.
	for (int32 I = Leaving.Num() - 1; I >= 0; --I)
	{
		const FLeavingCard& Card = Leaving[I];
		const float A = (T - Card.StartTime) / LeaveSeconds;
		if (A >= 1.0f)
		{
			Leaving.RemoveAtSwap(I);
			continue;
		}
		if (A < 0.0f)
		{
			continue;
		}
		const FVector2D Position = Card.Start + FVector2D(Card.Spin * 90.0f * A, -170.0f * A) * S;
		DrawCard(Card.Resource, Position, Card.Spin * 120.0f * A, 1.0f + 0.15f * A, 1.0f - A, -1);
	}

	// Cartas que vuelan desde las casillas: a tu mano, o hacia el panel del jugador que las recibe.
	for (int32 I = Flights.Num() - 1; I >= 0; --I)
	{
		const FCardFlight& Flight = Flights[I];
		const float A = (T - Flight.StartTime) / FlightSeconds;
		if (A < 0.0f)
		{
			continue;
		}
		const bool bToHand = Flight.Player == Hand && SlotPositions.IsValidIndex(Flight.Resource);
		if (A >= 1.0f)
		{
			if (bToHand)
			{
				PopTime[Flight.Resource] = T;
			}
			Flights.RemoveAtSwap(I);
			continue;
		}
		const FVector2D Target = bToHand ? SlotPositions[Flight.Resource] : FVector2D(190.0f, 45.0f + Flight.Player * 62.0f) * S;
		const float Ease = 1.0f - FMath::Pow(1.0f - A, 3.0f);
		const FVector2D Position = FMath::Lerp(Flight.Start, Target, Ease) - FVector2D(0.0f, 140.0f * S * FMath::Sin(A * UE_PI));
		const float Scale = bToHand ? FMath::Lerp(0.5f, 1.0f, Ease) : FMath::Lerp(0.55f, 0.3f, Ease);
		const float Angle = (1.0f - Ease) * -35.0f + (bToHand ? SlotAngles[Flight.Resource] * Ease : 0.0f);
		DrawCard(Flight.Resource, Position, Angle, Scale, bToHand ? 1.0f : 1.0f - A * 0.5f, -1);
	}
}
