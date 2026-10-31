#include "HFGameMode.h"

#include "Engine/World.h"
#include "HFBoardActor.h"
#include "HFCameraPawn.h"
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
	Match = Preview;
	PreviewSeed = Config.Seed;
	PreviewScenarioId = ScenarioId;
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
	Messages.Reset();
	AddMessage(FString::Printf(TEXT("Comienza la partida en %s"), *FHFMatch::ToFString(Match->GetScenario().name)));
	AddMessage(TEXT("Preparación: cada jugador coloca 2 puestos con su ruta"));
	NextBotTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + BotDelaySeconds;
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

bool AHFGameMode::ApplyHumanAction(const hf::Action& Action)
{
	if (!IsInMatch() || Match->IsBotTurn())
	{
		return false;
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

void AHFGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsInMatch() || !Match->IsBotTurn())
	{
		return;
	}
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (Now >= NextBotTime)
	{
		Match->StepBot();
		NextBotTime = Now + BotDelaySeconds;
	}
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
		case hf::EventType::PulseRevealed:
		{
			bStatusChanged = true;
			const hf::PulseCard& Card = Game.LastPulse();
			if (!Card.isEvent)
			{
				TArray<int32> Tiles;
				const auto& BoardTiles = Game.GetBoard().Tiles();
				for (int32 T = 0; T < static_cast<int32>(BoardTiles.size()); ++T)
				{
					const int32 Terrain = BoardTiles[static_cast<size_t>(T)].terrain;
					if ((Terrain == Card.terrainA || Terrain == Card.terrainB) && BoardTiles[static_cast<size_t>(T)].blockedRounds == 0)
					{
						Tiles.Add(T);
					}
				}
				const int32 Resource = Match->GetScenario().terrains[static_cast<size_t>(Card.terrainA)].resource;
				Board->FlashTiles(Tiles, Resource >= 0 ? FHFMatch::ToLinear(Match->GetScenario().resources[static_cast<size_t>(Resource)].color) : FLinearColor::White);
			}
			break;
		}
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
	NextBotTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + BotDelaySeconds;
	++StateVersion;
}
