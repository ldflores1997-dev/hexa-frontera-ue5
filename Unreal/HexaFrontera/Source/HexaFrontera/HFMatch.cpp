#include "HFMatch.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString ScenarioDirectory()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Scenarios"));
}
}

bool FHFMatch::LoadScenario(const FString& ScenarioId, hf::Scenario& OutScenario, FString& OutError)
{
	const FString Path = FPaths::Combine(ScenarioDirectory(), ScenarioId + TEXT(".json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("No se encontró el escenario %s"), *Path);
		return false;
	}
	std::string Error;
	if (!hf::LoadScenarioFromJson(ToStd(Text), OutScenario, Error))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *ScenarioId, *ToFString(Error));
		return false;
	}
	return true;
}

TArray<FString> FHFMatch::ListScenarioIds()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(ScenarioDirectory(), TEXT("*.json")), true, false);
	TArray<FString> Ids;
	for (const FString& File : Files)
	{
		Ids.Add(FPaths::GetBaseFilename(File));
	}
	Ids.Sort();
	return Ids;
}

FLinearColor FHFMatch::ToLinear(const hf::Color& Color)
{
	return FLinearColor::FromSRGBColor(FColor(
		static_cast<uint8>(FMath::Clamp(Color.r, 0.0f, 1.0f) * 255.0f),
		static_cast<uint8>(FMath::Clamp(Color.g, 0.0f, 1.0f) * 255.0f),
		static_cast<uint8>(FMath::Clamp(Color.b, 0.0f, 1.0f) * 255.0f)));
}

bool FHFMatch::Start(const FHFMatchConfig& InConfig, FString& OutError)
{
	hf::Scenario Scenario;
	if (!LoadScenario(InConfig.ScenarioId, Scenario, OutError))
	{
		return false;
	}
	std::vector<std::string> Names;
	for (const FString& Name : InConfig.PlayerNames)
	{
		Names.push_back(ToStd(Name));
	}
	std::string Error;
	if (!Game.Init(Scenario, Names, static_cast<uint64_t>(InConfig.Seed), Error))
	{
		OutError = ToFString(Error);
		return false;
	}
	Config = InConfig;
	BotFlags = InConfig.Bots;
	BotFlags.SetNum(InConfig.PlayerNames.Num());
	BotActionsThisTurn = 0;
	LastPlayer = Game.CurrentPlayer();
	return true;
}

bool FHFMatch::Apply(const hf::Action& Action, FString& OutError)
{
	hf::ApplyResult Result = Game.Apply(Action);
	if (!Result.ok)
	{
		OutError = ToFString(Result.error);
		return false;
	}
	if (Game.CurrentPlayer() != LastPlayer)
	{
		LastPlayer = Game.CurrentPlayer();
		BotActionsThisTurn = 0;
	}
	else
	{
		++BotActionsThisTurn;
	}
	OnEvents.Broadcast(Result.events);
	return true;
}

bool FHFMatch::IsBotTurn() const
{
	return Game.GetPhase() != hf::Phase::Finished && IsBot(Game.CurrentPlayer());
}

hf::Action FHFMatch::ChooseBotAction() const
{
	return hf::Bot::ChooseAction(Game, BotActionsThisTurn);
}

bool FHFMatch::ApplyBotAction(const hf::Action& Action)
{
	if (!IsBotTurn())
	{
		return false;
	}
	FString Error;
	if (!Apply(Action, Error))
	{
		// No debería pasar (los tests lo cubren); si pasa, el bot termina el turno.
		UE_LOG(LogTemp, Warning, TEXT("Acción ilegal del bot: %s"), *Error);
		return Apply(hf::Action::End(), Error);
	}
	return true;
}

bool FHFMatch::BotAcceptsTrade(int32 Bot, const std::vector<int>& BotGives, const std::vector<int>& BotGets) const
{
	const hf::Player& Player = Game.Players()[static_cast<size_t>(Bot)];
	int Gives = 0;
	int Gets = 0;
	for (size_t R = 0; R < BotGives.size(); ++R)
	{
		if (Player.resources[R] < BotGives[R])
		{
			return false;
		}
		Gives += BotGives[R];
		Gets += BotGets[R];
	}
	return Gets >= Gives && Gives > 0;
}

FString FHFMatch::PlayerName(int32 Player) const
{
	if (Player < 0 || Player >= static_cast<int32>(Game.Players().size()))
	{
		return TEXT("?");
	}
	return ToFString(Game.Players()[static_cast<size_t>(Player)].name);
}

FString FHFMatch::ResourceName(int32 Resource) const
{
	const auto& Resources = GetScenario().resources;
	return Resource >= 0 && Resource < static_cast<int32>(Resources.size()) ? ToFString(Resources[static_cast<size_t>(Resource)].name) : TEXT("?");
}

FString FHFMatch::TerrainName(int32 Terrain) const
{
	const auto& Terrains = GetScenario().terrains;
	return Terrain >= 0 && Terrain < static_cast<int32>(Terrains.size()) ? ToFString(Terrains[static_cast<size_t>(Terrain)].name) : TEXT("?");
}

FString FHFMatch::PieceName(hf::PieceKind Kind) const
{
	return ToFString(GetScenario().Piece(Kind).name);
}

FString FHFMatch::CostText(const std::vector<int>& Cost) const
{
	TArray<FString> Parts;
	for (size_t R = 0; R < Cost.size(); ++R)
	{
		if (Cost[R] > 0)
		{
			Parts.Add(FString::Printf(TEXT("%d %s"), Cost[R], *ResourceName(static_cast<int32>(R))));
		}
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("gratis");
}

FString FHFMatch::DescribeRoll() const
{
	if (!Game.HasRolled())
	{
		return TEXT("Todavía no se tiraron los dados");
	}
	TArray<FString> Faces;
	for (int32 Die = 0; Die < hf::kDiceCount; ++Die)
	{
		const hf::DieFace Face = Game.LastFace(Die);
		switch (Face.kind)
		{
		case hf::DieFaceKind::Terrain: Faces.Add(TerrainName(Face.terrain)); break;
		case hf::DieFaceKind::Event: Faces.Add(FString::Printf(TEXT("¡%s!"), *ToFString(GetScenario().worldEvent.name))); break;
		case hf::DieFaceKind::Wild: Faces.Add(TEXT("Comodín")); break;
		}
	}
	FString Text = FString::Join(Faces, TEXT(" + "));
	if (Game.RolledTerrain(0) >= 0 && Game.RolledTerrain(0) == Game.RolledTerrain(1))
	{
		Text += TEXT(" · ¡dobles!");
	}
	if (Game.GetPhase() == hf::Phase::ChooseWild)
	{
		Text += TEXT(" · elegí el terreno");
	}
	return Text;
}

FString FHFMatch::DescribeEvent(const hf::GameEvent& Event) const
{
	const FString Who = PlayerName(Event.player);
	auto TileName = [this](int32 Tile)
	{
		const auto& Tiles = Game.GetBoard().Tiles();
		return Tile >= 0 && Tile < static_cast<int32>(Tiles.size()) ? TerrainName(Tiles[static_cast<size_t>(Tile)].terrain) : FString(TEXT("?"));
	};
	switch (Event.type)
	{
	case hf::EventType::TurnStarted: return FString::Printf(TEXT("Turno de %s"), *Who);
	case hf::EventType::DiceRolled: return FString::Printf(TEXT("%s tira los dados: %s"), *Who, *DescribeRoll());
	case hf::EventType::Produced: return FString::Printf(TEXT("%s recibe %d %s"), *Who, Event.amount, *ResourceName(Event.resource));
	case hf::EventType::Boosted: return FString::Printf(TEXT("%s refuerza %s"), *Who, *TileName(Event.tile));
	case hf::EventType::OutpostBuilt: return FString::Printf(TEXT("%s construye %s"), *Who, *PieceName(hf::PieceKind::Outpost));
	case hf::EventType::RouteBuilt: return FString::Printf(TEXT("%s construye %s"), *Who, *PieceName(hf::PieceKind::Route));
	case hf::EventType::CenterBuilt: return FString::Printf(TEXT("%s construye %s"), *Who, *PieceName(hf::PieceKind::Center));
	case hf::EventType::WorkBuilt: return FString::Printf(TEXT("%s financia una %s"), *Who, *PieceName(hf::PieceKind::Work));
	case hf::EventType::RouteDestroyed: return FString::Printf(TEXT("La lava destruye una %s de %s"), *PieceName(hf::PieceKind::Route), *Who);
	case hf::EventType::MarketTrade:
		return Event.amount > 0 ? FString::Printf(TEXT("%s compra %s"), *Who, *ResourceName(Event.resource))
		                        : FString::Printf(TEXT("%s vende %s"), *Who, *ResourceName(Event.resource));
	case hf::EventType::PlayerTraded: return FString::Printf(TEXT("%s intercambia con %s"), *Who, *PlayerName(Event.index));
	case hf::EventType::Lava: return FString(TEXT("¡Erupción! Una casilla se cubre de lava"));
	case hf::EventType::DustStorm: return FString::Printf(TEXT("Tormenta sobre %s (%d rondas)"), *TileName(Event.tile), Event.amount);
	case hf::EventType::Frost: return Event.tile == Event.tile2 ? FString::Printf(TEXT("Helada en %s y alrededores"), *TileName(Event.tile)) : FString();
	case hf::EventType::PlatformSwap: return FString::Printf(TEXT("Desacople: %s y %s cambian de lugar"), *TileName(Event.tile), *TileName(Event.tile2));
	case hf::EventType::ContractClaimed:
		return FString::Printf(TEXT("%s cumple el contrato «%s»"), *Who, *ToFString(GetScenario().contracts[static_cast<size_t>(Event.index)].name));
	case hf::EventType::WildChosen: return FString::Printf(TEXT("%s elige %s con el comodín"), *Who, *TerrainName(Event.index));
	case hf::EventType::FinalRound: return FString::Printf(TEXT("¡%s llegó a la meta! Última ronda: gana quien tenga más prestigio"), *Who);
	case hf::EventType::Won: return FString::Printf(TEXT("¡%s gana la partida!"), *Who);
	}
	return FString();
}
