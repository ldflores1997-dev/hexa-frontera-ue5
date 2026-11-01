#include "HFGameMode.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "HFBoardActor.h"
#include "HFCameraPawn.h"
#include "HFDice.h"
#include "HFEnvironment.h"
#include "HFHUD.h"
#include "HFPlayerController.h"
#include "Kismet/GameplayStatics.h"

AHFGameMode::AHFGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AHFCameraPawn::StaticClass();
	PlayerControllerClass = AHFPlayerController::StaticClass();
	HUDClass = AHFHUD::StaticClass();
	BoardClass = AHFBoardActor::StaticClass();
}

void AHFGameMode::BeginPlay()
{
	Super::BeginPlay();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Environment = GetWorld()->SpawnActor<AHFEnvironment>(AHFEnvironment::StaticClass(), FTransform::Identity, Params);
	Board = GetWorld()->SpawnActor<AHFBoardActor>(BoardClass ? BoardClass.Get() : AHFBoardActor::StaticClass(), FTransform::Identity, Params);
	Dice = GetWorld()->SpawnActor<AHFDice>(AHFDice::StaticClass(), FTransform::Identity, Params);
	Dice->SetActorHiddenInGame(true);
	Dice->OnSettled.BindUObject(this, &AHFGameMode::OnDiceSettled);

	// Sin PlayerStart en el mapa puede no haber pawn: se crea la cámara a mano.
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (!Cast<AHFCameraPawn>(Controller->GetPawn()))
		{
			if (APawn* Old = Controller->GetPawn())
			{
				Old->Destroy();
			}
			AHFCameraPawn* Camera = GetWorld()->SpawnActor<AHFCameraPawn>(AHFCameraPawn::StaticClass(), FTransform::Identity, Params);
			Controller->Possess(Camera);
		}
	}

	const TArray<FString> Ids = FHFMatch::ListScenarioIds();
	if (Ids.Num() > 0)
	{
		PreviewScenario(Ids[0], 2);
	}
	else
	{
		AddMessage(TEXT("No se encontraron escenarios en Content/Scenarios"));
	}
}

void AHFGameMode::PreviewScenario(const FString& ScenarioId, int32 PlayerCount)
{
	FHFMatchConfig Config;
	Config.ScenarioId = ScenarioId;
	Config.Seed = FMath::RandRange(1, 1000000);
	for (int32 I = 0; I < FMath::Clamp(PlayerCount, 2, 4); ++I)
	{
		Config.PlayerNames.Add(FString::Printf(TEXT("Jugador %d"), I + 1));
		Config.Bots.Add(true);
	}

	TSharedPtr<FHFMatch> Preview = MakeShared<FHFMatch>();
	FString Error;
	if (!Preview->Start(Config, Error))
	{
		LastError = Error;
		LastErrorTime = static_cast<float>(GetWorld()->GetTimeSeconds());
		return;
	}
	bInMatch = false;
	bRollingDice = false;
	Dice->SetActorHiddenInGame(true);
	Match = Preview;
	PreviewSeed = Config.Seed;
	PreviewScenarioId = ScenarioId;
	ApplyDiceFaces();
	ShowWorld();
}

bool AHFGameMode::BeginHexMatch(const FHFMatchConfig& Config, FString& OutError)
{
	TSharedPtr<FHFMatch> NewMatch = MakeShared<FHFMatch>();
	if (!NewMatch->Start(Config, OutError))
	{
		return false;
	}
	Match = NewMatch;
	Match->OnEvents.AddUObject(this, &AHFGameMode::HandleEvents);
	bInMatch = true;
	bRollingDice = false;
	Dice->SetActorHiddenInGame(true);
	Messages.Reset();
	AddMessage(FString::Printf(TEXT("Comienza la partida en %s"), *FHFMatch::ToFString(Match->GetScenario().name)));
	AddMessage(TEXT("Preparación: cada jugador coloca 2 puestos con su ruta"));
	NextBotTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + BotDelaySeconds;
	ResetTurnTimer();
	ApplyDiceFaces();
	ShowWorld();
	return true;
}

void AHFGameMode::ReturnToMenu()
{
	const FString Id = Match.IsValid() ? FHFMatch::ToFString(Match->GetScenario().id) : FString();
	const int32 Players = Match.IsValid() ? static_cast<int32>(Match->GetGame().Players().size()) : 2;
	PreviewScenario(Id, Players);
}

void AHFGameMode::ShowWorld()
{
	if (!Match.IsValid())
	{
		return;
	}
	Board->Rebuild(*Match);
	Environment->ApplyScenario(*Match, Board->GetBoardExtent());
	if (AHFCameraPawn* Camera = GetCameraPawn())
	{
		Camera->FrameBoard(Board->GetBoardExtent());
	}
	++StateVersion;
}

AHFCameraPawn* AHFGameMode::GetCameraPawn() const
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	return Controller ? Cast<AHFCameraPawn>(Controller->GetPawn()) : nullptr;
}

AHFHUD* AHFGameMode::GetHexHUD() const
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	return Controller ? Cast<AHFHUD>(Controller->GetHUD()) : nullptr;
}

bool AHFGameMode::ApplyHumanAction(const hf::Action& Action)
{
	if (!IsInMatch() || Match->IsBotTurn() || bRollingDice)
	{
		return false;
	}
	if (Action.type == hf::ActionType::RollDice && Action.dice[0] < 0)
	{
		if (Match->GetGame().GetPhase() != hf::Phase::AwaitRoll)
		{
			ShowError(TEXT("Ya tiraste los dados este turno"));
			return false;
		}
		StartDiceRoll();
		return true;
	}
	FString Error;
	if (!Match->Apply(Action, Error))
	{
		LastError = Error;
		LastErrorTime = static_cast<float>(GetWorld()->GetTimeSeconds());
		return false;
	}
	return true;
}

void AHFGameMode::ShowError(const FString& Message)
{
	LastError = Message;
	LastErrorTime = static_cast<float>(GetWorld()->GetTimeSeconds());
}

void AHFGameMode::StartDiceRoll()
{
	// La bandeja va en el borde del tablero del lado de la cámara, y los dados ruedan hacia el fondo.
	float Yaw = -30.0f;
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0); Controller && Controller->PlayerCameraManager)
	{
		Yaw = static_cast<float>(Controller->PlayerCameraManager->GetCameraRotation().Yaw);
	}
	const FVector Forward = FRotator(0.0f, Yaw, 0.0f).Vector();
	const FVector TrayLocation = -Forward * (Board->GetBoardExtent() + Dice->TraySize.Y * 0.5f + 180.0f);
	bRollingDice = true;
	Dice->Throw(TrayLocation, Yaw);
	if (AHFCameraPawn* Camera = GetCameraPawn())
	{
		Camera->FocusOn(TrayLocation, Dice->MaxRollSeconds);
	}
	AddMessage(FString::Printf(TEXT("%s tira los dados…"), *Match->PlayerName(Match->GetGame().CurrentPlayer())));
	++StateVersion;
}

void AHFGameMode::OnDiceSettled(int32 First, int32 Second)
{
	bRollingDice = false;
	if (!IsInMatch() || Match->GetGame().GetPhase() != hf::Phase::AwaitRoll)
	{
		return;
	}
	if (AHFCameraPawn* Camera = GetCameraPawn())
	{
		Camera->FocusOn(Dice->GetActorLocation(), 1.2f); // un momento para ver el resultado
	}
	FString Error;
	if (!Match->Apply(hf::Action::RollWith(First, Second), Error))
	{
		ShowError(Error);
	}
}

void AHFGameMode::ApplyDiceFaces()
{
	if (!Match.IsValid())
	{
		return;
	}
	const hf::Scenario& Scenario = Match->GetScenario();
	TArray<FHFDieFaceLook> Looks[hf::kDiceCount];
	for (int32 Die = 0; Die < hf::kDiceCount; ++Die)
	{
		for (int32 Face = 0; Face < hf::kDieFaces; ++Face)
		{
			const hf::DieFace& Def = Scenario.dice[Die][Face];
			FHFDieFaceLook Look;
			switch (Def.kind)
			{
			case hf::DieFaceKind::Terrain:
				Look.Color = FHFMatch::ToLinear(Scenario.terrains[static_cast<size_t>(Def.terrain)].color);
				break;
			case hf::DieFaceKind::Event:
				Look.Color = FLinearColor(0.45f, 0.03f, 0.02f);
				Look.Symbol = FHFDieFaceLook::ESymbol::Event;
				break;
			case hf::DieFaceKind::Wild:
				Look.Color = FLinearColor(0.95f, 0.68f, 0.12f);
				Look.Symbol = FHFDieFaceLook::ESymbol::Wild;
				break;
			}
			Looks[Die].Add(Look);
		}
	}
	Dice->SetFaces(Looks[0], Looks[1]);
}

void AHFGameMode::FlashRolledTerrains()
{
	const hf::Game& Game = Match->GetGame();
	const hf::Scenario& Scenario = Match->GetScenario();
	const int32 TerrainA = Game.RolledTerrain(0);
	const int32 TerrainB = Game.RolledTerrain(1);
	TArray<int32> Tiles;
	const auto& BoardTiles = Game.GetBoard().Tiles();
	for (int32 T = 0; T < static_cast<int32>(BoardTiles.size()); ++T)
	{
		const hf::Tile& Tile = BoardTiles[static_cast<size_t>(T)];
		if (Tile.terrain >= 0 && (Tile.terrain == TerrainA || Tile.terrain == TerrainB) && Tile.blockedRounds == 0)
		{
			Tiles.Add(T);
		}
	}
	const int32 Lead = TerrainA >= 0 ? TerrainA : TerrainB;
	const int32 Resource = Lead >= 0 ? Scenario.terrains[static_cast<size_t>(Lead)].resource : -1;
	Board->FlashTiles(Tiles, Resource >= 0 ? FHFMatch::ToLinear(Scenario.resources[static_cast<size_t>(Resource)].color) : FLinearColor(1.0f, 0.85f, 0.45f));
}

float AHFGameMode::GetTurnTimeLeft() const
{
	if (!IsInMatch() || GetTurnSeconds() <= 0 || Match->GetGame().GetPhase() == hf::Phase::Finished)
	{
		return -1.0f;
	}
	return FMath::Max(0.0f, TurnDeadline - static_cast<float>(GetWorld()->GetTimeSeconds()));
}

void AHFGameMode::ResetTurnTimer()
{
	TimerPlayer = Match.IsValid() ? Match->GetGame().CurrentPlayer() : -1;
	TurnDeadline = static_cast<float>(GetWorld()->GetTimeSeconds()) + static_cast<float>(GetTurnSeconds());
}

void AHFGameMode::OnTurnTimeout()
{
	// Se acabó el tiempo: los dados se tiran solos, el turno termina o (en la preparación) se coloca la pieza.
	const hf::Game& Game = Match->GetGame();
	FString Error;
	switch (Game.GetPhase())
	{
	case hf::Phase::Setup:
		Match->Apply(Match->ChooseBotAction(), Error);
		break;
	case hf::Phase::AwaitRoll:
		AddMessage(TEXT("¡Se acabó el tiempo! Los dados se tiran solos"));
		StartDiceRoll();
		break;
	case hf::Phase::ChooseWild:
		AddMessage(TEXT("¡Se acabó el tiempo! El comodín se elige solo"));
		Match->Apply(hf::Action::Wild(hf::Bot::BestWildTerrain(Game, Game.CurrentPlayer())), Error);
		break;
	case hf::Phase::Main:
		AddMessage(FString::Printf(TEXT("Se acabó el tiempo de %s"), *Match->PlayerName(Game.CurrentPlayer())));
		Match->Apply(hf::Action::End(), Error);
		break;
	default:
		break;
	}
}

void AHFGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Board->SetDayState(Environment->GetSunDirection(), Environment->GetNightAmount());
	if (!IsInMatch())
	{
		return;
	}
	if (bRollingDice)
	{
		TurnDeadline += DeltaSeconds; // el reloj se pausa mientras ruedan los dados
		return;
	}
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (GetTurnSeconds() > 0 && !Match->IsBotTurn() && Match->GetGame().GetPhase() != hf::Phase::Finished && Now >= TurnDeadline)
	{
		OnTurnTimeout();
		return;
	}
	if (!Match->IsBotTurn() || Now < NextBotTime)
	{
		return;
	}
	const hf::Action Action = Match->ChooseBotAction();
	if (Action.type == hf::ActionType::RollDice)
	{
		StartDiceRoll();
	}
	else
	{
		Match->ApplyBotAction(Action);
	}
	NextBotTime = Now + BotDelaySeconds;
}

void AHFGameMode::AddMessage(const FString& Message)
{
	if (Message.IsEmpty())
	{
		return;
	}
	Messages.Add(Message);
	while (Messages.Num() > 8)
	{
		Messages.RemoveAt(0);
	}
}

void AHFGameMode::HandleEvents(const std::vector<hf::GameEvent>& Events)
{
	const hf::Game& Game = Match->GetGame();
	AHFCameraPawn* Camera = GetCameraPawn();
	AHFHUD* Hud = GetHexHUD();
	bool bPiecesChanged = false;
	bool bStatusChanged = false;

	for (const hf::GameEvent& Event : Events)
	{
		// Los mensajes de producción se agrupan aparte para no llenar el registro.
		if (Event.type != hf::EventType::Produced)
		{
			AddMessage(Match->DescribeEvent(Event));
		}
		switch (Event.type)
		{
		case hf::EventType::OutpostBuilt:
		case hf::EventType::CenterBuilt:
			bPiecesChanged = true;
			if (Camera && !Match->IsBot(Event.player))
			{
				Camera->FocusOn(Board->VertexWorld(Event.index), 1.2f);
			}
			break;
		case hf::EventType::RouteBuilt:
		case hf::EventType::RouteDestroyed:
			bPiecesChanged = true;
			break;
		case hf::EventType::DiceRolled:
			bStatusChanged = true;
			// Con comodín, las casillas se iluminan recién cuando se elige el terreno.
			if (Game.GetPhase() != hf::Phase::ChooseWild)
			{
				FlashRolledTerrains();
			}
			break;
		case hf::EventType::WildChosen:
			FlashRolledTerrains();
			break;
		case hf::EventType::Produced:
			if (Hud)
			{
				Hud->QueueCardFlight(Event.player, Event.resource, Event.amount, Board->TileWorld(Event.tile));
			}
			break;
		case hf::EventType::Lava:
		case hf::EventType::DustStorm:
		case hf::EventType::Frost:
		case hf::EventType::PlatformSwap:
			bStatusChanged = true;
			bPiecesChanged = true;
			Board->FlashTiles({Event.tile}, FLinearColor(1.0f, 0.3f, 0.05f));
			if (Camera)
			{
				Camera->FocusOn(Board->TileWorld(Event.tile), 2.0f);
			}
			break;
		case hf::EventType::Boosted:
		case hf::EventType::TurnStarted:
			bStatusChanged = true;
			break;
		default:
			break;
		}
	}

	// Resumen de lo producido por jugador.
	TMap<int32, TMap<int32, int32>> Produced;
	for (const hf::GameEvent& Event : Events)
	{
		if (Event.type == hf::EventType::Produced)
		{
			Produced.FindOrAdd(Event.player).FindOrAdd(Event.resource) += Event.amount;
		}
	}
	for (const auto& PlayerEntry : Produced)
	{
		TArray<FString> Parts;
		for (const auto& ResourceEntry : PlayerEntry.Value)
		{
			Parts.Add(FString::Printf(TEXT("%d %s"), ResourceEntry.Value, *Match->ResourceName(ResourceEntry.Key)));
		}
		AddMessage(FString::Printf(TEXT("%s recibe %s"), *Match->PlayerName(PlayerEntry.Key), *FString::Join(Parts, TEXT(", "))));
	}

	if (bStatusChanged)
	{
		Board->RefreshStatuses(*Match); // si cambió algún terreno, rearma el tablero
	}
	if (bPiecesChanged)
	{
		Board->RefreshPieces(*Match);
	}
	// El reloj vuelve a empezar con cada turno y, en la preparación, con cada puesto + ruta
	// (en el orden serpiente el último jugador coloca dos veces seguidas).
	const bool bNewSetupStep = Game.GetPhase() == hf::Phase::Setup && !Game.SetupNeedsRoute();
	if (Game.CurrentPlayer() != TimerPlayer || bNewSetupStep)
	{
		ResetTurnTimer();
	}
	NextBotTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + BotDelaySeconds;
	++StateVersion;
}
