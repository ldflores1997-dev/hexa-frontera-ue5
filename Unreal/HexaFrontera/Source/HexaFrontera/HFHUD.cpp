#include "HFHUD.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HFBoardActor.h"
#include "HFGameMode.h"
#include "HFMatch.h"
#include "HFPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
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
						Label(Str(TEXT("Cámara: clic derecho + arrastrar para girar, rueda para zoom, WASD para mover, Q/E para rotar.\nEspacio: revelar el Pulso o terminar el turno. Esc: cancelar.")),
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
		// Turno, Pulso y ayuda (arriba al centro)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.0f, 20.0f))
		[
			SNew(SBox).WidthOverride(520.0f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						Label(TAttribute<FText>::CreateLambda([Mode]()
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
						}))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4)
					[
						Label(TAttribute<FText>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							return M && M->GetMatch() ? Str(TEXT("Pulso: ") + M->GetMatch()->DescribePulse()) : FText::GetEmpty();
						}), 14, true)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						Label(TAttribute<FText>::CreateLambda([Mode]()
						{
							const AHFGameMode* M = Mode();
							if (!M || !M->GetMatch()) return FText::GetEmpty();
							const FHFMatch& Match = *M->GetMatch();
							const hf::Game& Game = Match.GetGame();
							if (Game.GetPhase() == hf::Phase::Finished) return Str(TEXT("Partida terminada"));
							if (Match.IsBotTurn()) return Str(FString::Printf(TEXT("Juega %s…"), *Match.PlayerName(Game.CurrentPlayer())));
							switch (Game.GetPhase())
							{
							case hf::Phase::Setup:
								return Str(Game.SetupNeedsRoute()
									? FString::Printf(TEXT("Colocá una %s junto a tu %s"), *Match.PieceName(hf::PieceKind::Route), *Match.PieceName(hf::PieceKind::Outpost))
									: FString::Printf(TEXT("Elegí dónde va tu %s (puntos resaltados)"), *Match.PieceName(hf::PieceKind::Outpost)));
							case hf::Phase::AwaitPulse:
								return Str(TEXT("Revelá el Pulso (Espacio). Antes podés reforzar una casilla con Influencia."));
							default:
								return Str(TEXT("Construí, comerciá o terminá el turno (Espacio)"));
							}
						}), 12, false, MutedColor)
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
		return M && M->IsInMatch() && !M->GetMatch()->IsBotTurn() && M->GetMatch()->GetGame().GetPhase() == Phase;
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
			Button(Str(TEXT("Revelar Pulso")), [this]() { if (AHFGameMode* M = GetHexGameMode()) M->ApplyHumanAction(hf::Action::Reveal()); },
				TAttribute<bool>::CreateLambda([HumanPhase]() { return HumanPhase(hf::Phase::AwaitPulse); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			Button(Str(TEXT("Reforzar casilla (1 Influencia)")), [UseTool]() { UseTool(EHFTool::Boost); },
				TAttribute<bool>::CreateLambda([this, HumanPhase]()
				{
					if (!HumanPhase(hf::Phase::AwaitPulse)) return false;
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
