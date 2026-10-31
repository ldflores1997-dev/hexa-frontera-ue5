#include "HFHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "GlobalRenderResources.h"
#include "HFBoardActor.h"
#include "HFGameMode.h"
#include "HFMatch.h"
#include "HFPlayerController.h"
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
const FLinearColor PanelColor(FColor(0x21, 0x24, 0x2C, 199));
const FLinearColor TextColor(FColor(0xED, 0xEB, 0xE0));
const FLinearColor MutedColor(FColor(0x9A, 0x9A, 0x95));
const FLinearColor AccentColor(FColor(0xFF, 0xC7, 0x59));
const FLinearColor DangerColor(FColor(0xFF, 0x73, 0x59));

FSlateFontInfo Font(int32 Size, bool bBold = false)
{
	return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
}

TSharedRef<SWidget> Panel(TSharedRef<SWidget> Inner)
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(PanelColor)
		.Padding(FMargin(16.0f, 12.0f))
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

// Texto centrado que ocupa todo el ancho del panel (con HAlign_Center el ajuste de línea queda angosto).
TSharedRef<STextBlock> Centered(TSharedRef<STextBlock> Text)
{
	Text->SetJustification(ETextJustify::Center);
	return Text;
}

TSharedRef<SButton> Button(TAttribute<FText> Text, TFunction<void()> OnClick, TAttribute<bool> Enabled = true)
{
	return SNew(SButton)
		.IsEnabled(Enabled)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(10.0f, 5.0f))
		.OnClicked_Lambda([OnClick]()
		{
			OnClick();
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Text(Text).Font(Font(12, true))
		];
}

FText Str(const FString& Value)
{
	return FText::FromString(Value);
}

// Color del jugador aclarado para leerse sobre los paneles. Igual que en el panel de jugadores.
FSlateColor PlayerTextColor(const AHFGameMode* Mode, int32 Player)
{
	const FLinearColor Color = Mode && Mode->GetBoard() && Mode->GetBoard()->PlayerColors.IsValidIndex(Player) ? Mode->GetBoard()->PlayerColors[Player] : TextColor;
	return FSlateColor(FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.25f));
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
	ShowMenu();
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

TSharedRef<SWidget> AHFHUD::BuildMenu()
{
	TSharedRef<SVerticalBox> ScenarioButtons = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Scenarios.Num(); ++Index)
	{
		ScenarioButtons->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SButton)
			.ContentPadding(FMargin(12.0f, 8.0f))
			.OnClicked_Lambda([this, Index]()
			{
				SelectedScenario = Index;
				if (AHFGameMode* Mode = GetHexGameMode())
				{
					Mode->PreviewScenario(Scenarios[Index].Id, PlayerCount);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Str(Scenarios[Index].Name))
				.Font(Font(15, true))
				.ColorAndOpacity_Lambda([this, Index]() { return FSlateColor(SelectedScenario == Index ? AccentColor : TextColor); })
			]
		];
	}

	TSharedRef<SHorizontalBox> CountButtons = SNew(SHorizontalBox);
	for (int32 Count = 2; Count <= 4; ++Count)
	{
		CountButtons->AddSlot().AutoWidth().Padding(0, 0, 6, 0)
		[
			SNew(SButton)
			.ContentPadding(FMargin(14.0f, 6.0f))
			.OnClicked_Lambda([this, Count]()
			{
				PlayerCount = Count;
				if (AHFGameMode* Mode = GetHexGameMode(); Mode && Scenarios.IsValidIndex(SelectedScenario))
				{
					Mode->PreviewScenario(Scenarios[SelectedScenario].Id, PlayerCount);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Count))
				.Font(Font(14, true))
				.ColorAndOpacity_Lambda([this, Count]() { return FSlateColor(PlayerCount == Count ? AccentColor : TextColor); })
			]
		];
	}

	TSharedRef<SHorizontalBox> TimerButtons = SNew(SHorizontalBox);
	for (int32 Seconds : {60, 90, 120, 0})
	{
		TimerButtons->AddSlot().AutoWidth().Padding(0, 0, 6, 0)
		[
			SNew(SButton)
			.ContentPadding(FMargin(10.0f, 6.0f))
			.OnClicked_Lambda([this, Seconds]() { TurnSeconds = Seconds; return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(Str(Seconds > 0 ? FString::Printf(TEXT("%d s"), Seconds) : FString(TEXT("Sin límite"))))
				.Font(Font(13, true))
				.ColorAndOpacity_Lambda([this, Seconds]() { return FSlateColor(TurnSeconds == Seconds ? AccentColor : TextColor); })
			]
		];
	}

	TSharedRef<SVerticalBox> SeatRows = SNew(SVerticalBox);
	for (int32 Seat = 0; Seat < 4; ++Seat)
	{
		SeatRows->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this, Seat]() { return Seat < PlayerCount ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Label(Str(FString::Printf(TEXT("Asiento %d"), Seat + 1)), 13)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(TAttribute<FText>::CreateLambda([this, Seat]() { return Str(Bots[Seat] ? TEXT("Bot") : TEXT("Humano")); }),
					[this, Seat]() { Bots[Seat] = !Bots[Seat]; })
			]
		];
	}

	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(48.0f))
		[
			SNew(SBox).WidthOverride(420.0f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Label(Str(TEXT("HEXA FRONTERA")), 30, true, AccentColor)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[Label(Str(TEXT("Recursos, comercio y mundos que cambian")), 13, false, MutedColor)]
					+ SVerticalBox::Slot().AutoHeight()[Label(Str(TEXT("Mundo")), 14, true)]
					+ SVerticalBox::Slot().AutoHeight()[ScenarioButtons]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 12)
					[
						Label(TAttribute<FText>::CreateLambda([this]()
						{
							return Scenarios.IsValidIndex(SelectedScenario) ? Str(Scenarios[SelectedScenario].Description) : FText::GetEmpty();
						}), 12, false, MutedColor)
					]
					+ SVerticalBox::Slot().AutoHeight()[Label(Str(TEXT("Jugadores")), 14, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[CountButtons]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)[SeatRows]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[Label(Str(TEXT("Reloj por turno")), 14, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[TimerButtons]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
						[
							Button(Str(TEXT("Otro tablero")), [this]()
							{
								if (AHFGameMode* Mode = GetHexGameMode(); Mode && Scenarios.IsValidIndex(SelectedScenario))
								{
									Mode->PreviewScenario(Scenarios[SelectedScenario].Id, PlayerCount);
								}
							})
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							Button(Str(TEXT("Comenzar partida")), [this]() { StartMatch(); },
								TAttribute<bool>::CreateLambda([this]() { return Scenarios.IsValidIndex(SelectedScenario); }))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
					[
						Label(Str(TEXT("Cámara: clic derecho + arrastrar para girar, rueda para zoom, WASD para mover, Q/E para rotar.\nEspacio: tirar los dados o terminar el turno. Esc: cancelar.")),
							11, false, MutedColor)
					]
				)
			]
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
			SNew(SBox).WidthOverride(340.0f)[BuildPlayersPanel()]
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
			SNew(SBox).WidthOverride(330.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[BuildActionsPanel()]
				+ SScrollBox::Slot().Padding(0, 10, 0, 0)[BuildMarketPanel()]
			]
		]
		// Contratos (abajo a la derecha)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(330.0f)[BuildContractsPanel()]
		]
		// Registro (abajo a la izquierda)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(460.0f)
			[
				Panel(Label(TAttribute<FText>::CreateLambda([Mode]()
				{
					const AHFGameMode* M = Mode();
					return M ? Str(FString::Join(M->GetMessages(), TEXT("\n"))) : FText::GetEmpty();
				}), 12, false, TextColor))
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
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12, 0, 0)
					[
						Button(Str(TEXT("Volver al menú")), [this]()
						{
							if (AHFGameMode* M = GetHexGameMode())
							{
								M->ReturnToMenu();
							}
							ShowMenu();
						})
					]
				)
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
	const int32 Count = static_cast<int32>(Mode->GetMatch()->GetGame().Players().size());
	for (int32 Player = 0; Player < Count; ++Player)
	{
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				Label(TAttribute<FText>::CreateLambda([this, Player]()
				{
					const AHFGameMode* M = GetHexGameMode();
					if (!M || !M->GetMatch()) return FText::GetEmpty();
					const FHFMatch& Match = *M->GetMatch();
					const hf::Game& Game = Match.GetGame();
					const bool bCurrent = Game.CurrentPlayer() == Player && Game.GetPhase() != hf::Phase::Finished;
					return Str(FString::Printf(TEXT("%s%s   Prestigio %d / %d"), bCurrent ? TEXT("» ") : TEXT(""), *Match.PlayerName(Player),
						Game.Prestige(Player), Game.PrestigeTarget()));
				}), 15, true, PlayerTextColor(Mode, Player))
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
				}), 12, false, MutedColor)
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
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Label(Str(TEXT("Acciones")), 15, true)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(Str(TEXT("Tirar dados")), [this, UseTool]()
			{
				UseTool(EHFTool::None);
				if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::Roll());
			},
				TAttribute<bool>::CreateLambda([HumanPhase]() { return HumanPhase(hf::Phase::AwaitRoll); }))
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
				TAttribute<bool>::CreateLambda([HumanPhase]() { return HumanPhase(hf::Phase::Main); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
		[
			Button(Str(TEXT("Menú")), [this]()
			{
				if (AHFGameMode* M = GetHexGameMode()) M->ReturnToMenu();
				ShowMenu();
			})
		]);
}

TSharedRef<SWidget> AHFHUD::BuildMarketPanel()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[Label(Str(TEXT("Mercado")), 15, true)];
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
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Label(Str(TEXT("Contratos")), 15, true)]
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
	Tile(FVector2D(0, -H * 0.2f), FVector2D(W - 6 * S, H * 0.6f - 3 * S), Color * 0.8f + FLinearColor(0, 0, 0, 0.2f));

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
