// Puente entre Unreal y el núcleo de reglas (hf::Game). No es un UObject:
// lo usan el GameMode, el tablero y la interfaz desde C++.
#pragma once

#include "CoreMinimal.h"

#include "hf/Bot.h"
#include "hf/Game.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FHFOnMatchEvents, const std::vector<hf::GameEvent>&);

struct FHFMatchConfig
{
	FString ScenarioId;
	TArray<FString> PlayerNames;
	TArray<bool> Bots; // un valor por jugador
	int64 Seed = 1;
	int32 TurnSeconds = 90; // reloj por turno; 0 = sin límite
};

class FHFMatch
{
public:
	// Lee Content/Scenarios/<Id>.json
	static bool LoadScenario(const FString& ScenarioId, hf::Scenario& OutScenario, FString& OutError);
	static TArray<FString> ListScenarioIds();

	static FString ToFString(const std::string& Text) { return FString(UTF8_TO_TCHAR(Text.c_str())); }
	static std::string ToStd(const FString& Text) { return std::string(TCHAR_TO_UTF8(*Text)); }
	static FLinearColor ToLinear(const hf::Color& Color);

	bool Start(const FHFMatchConfig& Config, FString& OutError);

	// Aplica una acción del jugador actual y avisa a los suscriptores.
	bool Apply(const hf::Action& Action, FString& OutError);

	bool IsBot(int32 Player) const { return BotFlags.IsValidIndex(Player) && BotFlags[Player]; }
	bool IsBotTurn() const;
	// Lo que haría el bot del turno ahora (sin aplicarlo).
	hf::Action ChooseBotAction() const;
	// Aplica una acción del bot del turno. Devuelve false si no había nada que hacer.
	bool ApplyBotAction(const hf::Action& Action);
	// Un bot acepta un intercambio si recibe al menos lo mismo que entrega y le sirve.
	bool BotAcceptsTrade(int32 Bot, const std::vector<int>& BotGives, const std::vector<int>& BotGets) const;

	const hf::Game& GetGame() const { return Game; }
	const hf::Scenario& GetScenario() const { return Game.GetScenario(); }
	const FHFMatchConfig& GetConfig() const { return Config; }

	FString PlayerName(int32 Player) const;
	FString ResourceName(int32 Resource) const;
	FString TerrainName(int32 Terrain) const;
	FString PieceName(hf::PieceKind Kind) const;
	FString CostText(const std::vector<int>& Cost) const;
	FString DescribeRoll() const;
	FString DescribeEvent(const hf::GameEvent& Event) const;

	FHFOnMatchEvents OnEvents;

private:
	hf::Game Game;
	FHFMatchConfig Config;
	TArray<bool> BotFlags;
	int32 BotActionsThisTurn = 0;
	int32 LastPlayer = -1;
};
