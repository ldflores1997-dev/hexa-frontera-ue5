#include "HFPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "HFBoardActor.h"
#include "HFCameraPawn.h"
#include "HFGameMode.h"
#include "HFHUD.h"
#include "InputCoreTypes.h"

AHFPlayerController::AHFPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AHFPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void AHFPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AHFPlayerController::OnLeftClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AHFPlayerController::OnRightPressed);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AHFPlayerController::OnRightReleased);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AHFPlayerController::OnZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AHFPlayerController::OnZoomOut);
	// Esc también tiene que andar con el juego en pausa, para poder salir de ella.
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHFPlayerController::OnCancel).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AHFPlayerController::OnRollOrEnd);
}

AHFGameMode* AHFPlayerController::GetHexGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AHFGameMode>() : nullptr;
}

AHFCameraPawn* AHFPlayerController::GetCameraPawn() const
{
	return Cast<AHFCameraPawn>(GetPawn());
}

bool AHFPlayerController::IsHumanTurn() const
{
	const AHFGameMode* Mode = GetHexGameMode();
	return Mode && Mode->IsInMatch() && !Mode->GetMatch()->IsBotTurn();
}

void AHFPlayerController::SetTool(EHFTool NewTool)
{
	Tool = NewTool;
}

EHFTool AHFPlayerController::GetEffectiveTool() const
{
	const AHFGameMode* Mode = GetHexGameMode();
	if (!IsHumanTurn())
	{
		return EHFTool::None;
	}
	const hf::Game& Game = Mode->GetMatch()->GetGame();
	switch (Game.GetPhase())
	{
	case hf::Phase::Setup:
		return Game.SetupNeedsRoute() ? EHFTool::Route : EHFTool::Outpost;
	case hf::Phase::AwaitRoll:
		return Tool == EHFTool::Boost && !Mode->IsRollingDice() ? EHFTool::Boost : EHFTool::None;
	case hf::Phase::Main:
		return Tool == EHFTool::Boost ? EHFTool::None : Tool;
	default:
		return EHFTool::None;
	}
}

bool AHFPlayerController::MouseOnBoard(FVector& OutPoint) const
{
	const AHFGameMode* Mode = GetHexGameMode();
	FVector Origin;
	FVector Direction;
	if (!Mode || !Mode->GetBoard() || !DeprojectMousePositionToWorld(Origin, Direction))
	{
		return false;
	}
	// Plano a media altura de las casillas: suficiente para elegir vértices y aristas.
	const float PlaneZ = Mode->GetBoard()->GetBoardTopZ() * 0.6f;
	if (FMath::Abs(Direction.Z) < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const float Distance = (PlaneZ - Origin.Z) / Direction.Z;
	if (Distance <= 0.0f)
	{
		return false;
	}
	OutPoint = Origin + Direction * Distance;
	return true;
}

void AHFPlayerController::UpdateHighlights()
{
	AHFGameMode* Mode = GetHexGameMode();
	if (!Mode || !Mode->GetBoard())
	{
		return;
	}
	AHFBoardActor* Board = Mode->GetBoard();
	const EHFTool Effective = GetEffectiveTool();
	if (Effective == EHFTool::None)
	{
		Board->ClearHighlights();
		return;
	}
	const hf::Game& Game = Mode->GetMatch()->GetGame();
	const int32 CurrentPlayer = Game.CurrentPlayer();
	TArray<int32> Vertices;
	TArray<int32> Edges;
	TArray<int32> Tiles;
	switch (Effective)
	{
	case EHFTool::Outpost:
		for (int V : Game.LegalOutpostVertices(CurrentPlayer)) Vertices.Add(V);
		break;
	case EHFTool::Route:
		for (int E : Game.LegalRouteEdges(CurrentPlayer)) Edges.Add(E);
		break;
	case EHFTool::Center:
		for (int V : Game.LegalUpgradeVertices(CurrentPlayer)) Vertices.Add(V);
		break;
	case EHFTool::Boost:
	{
		const auto& BoardTiles = Game.GetBoard().Tiles();
		for (int32 T = 0; T < static_cast<int32>(BoardTiles.size()); ++T)
		{
			for (int V : BoardTiles[static_cast<size_t>(T)].vertices)
			{
				if (Game.Buildings()[static_cast<size_t>(V)].owner == CurrentPlayer)
				{
					Tiles.Add(T);
					break;
				}
			}
		}
		break;
	}
	default:
		break;
	}
	const FLinearColor Color = Board->PlayerColors.IsValidIndex(CurrentPlayer) ? Board->PlayerColors[CurrentPlayer] : FLinearColor::White;
	Board->SetHighlights(Vertices, Edges, Tiles, FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.45f));
}

void AHFPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	AHFGameMode* Mode = GetHexGameMode();
	if (Mode)
	{
		const EHFTool Effective = GetEffectiveTool();
		if (Mode->GetStateVersion() != SeenStateVersion || Effective != SeenTool)
		{
			SeenStateVersion = Mode->GetStateVersion();
			SeenTool = Effective;
			UpdateHighlights();
		}
	}

	AHFCameraPawn* Camera = GetCameraPawn();
	if (!Camera)
	{
		return;
	}
	if (bOrbiting)
	{
		float X = 0.0f;
		float Y = 0.0f;
		if (GetMousePosition(X, Y))
		{
			const FVector2D Mouse(X, Y);
			const FVector2D Delta = Mouse - LastMouse;
			LastMouse = Mouse;
			Camera->Orbit(Delta.X * 0.25f, -Delta.Y * 0.2f);
		}
	}

	FVector2D PanInput = FVector2D::ZeroVector;
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) PanInput.Y += 1.0f;
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) PanInput.Y -= 1.0f;
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) PanInput.X += 1.0f;
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) PanInput.X -= 1.0f;
	Camera->Pan(PanInput.GetSafeNormal(), DeltaTime);
	if (IsInputKeyDown(EKeys::Q)) Camera->Orbit(-90.0f * DeltaTime, 0.0f);
	if (IsInputKeyDown(EKeys::E)) Camera->Orbit(90.0f * DeltaTime, 0.0f);
}

void AHFPlayerController::OnLeftClick()
{
	AHFGameMode* Mode = GetHexGameMode();
	const EHFTool Effective = GetEffectiveTool();
	FVector Point;
	if (!Mode || Effective == EHFTool::None || !MouseOnBoard(Point))
	{
		return;
	}
	AHFBoardActor* Board = Mode->GetBoard();
	const float PickRadius = Board->TileRadius * 0.45f;
	bool bDone = false;
	switch (Effective)
	{
	case EHFTool::Outpost:
	{
		const int32 Vertex = Board->FindNearestVertex(Point, PickRadius);
		bDone = Vertex >= 0 && Mode->ApplyHumanAction(hf::Action::Outpost(Vertex));
		break;
	}
	case EHFTool::Route:
	{
		const int32 Edge = Board->FindNearestEdge(Point, PickRadius);
		bDone = Edge >= 0 && Mode->ApplyHumanAction(hf::Action::Route(Edge));
		break;
	}
	case EHFTool::Center:
	{
		const int32 Vertex = Board->FindNearestVertex(Point, PickRadius);
		bDone = Vertex >= 0 && Mode->ApplyHumanAction(hf::Action::Upgrade(Vertex));
		break;
	}
	case EHFTool::Boost:
	{
		const int32 Tile = Board->FindTileAt(Point);
		bDone = Tile >= 0 && Mode->ApplyHumanAction(hf::Action::BoostTile(Tile));
		break;
	}
	default:
		break;
	}
	// Las rutas se suelen construir de a varias: esa herramienta queda activa.
	if (bDone && Tool != EHFTool::Route)
	{
		Tool = EHFTool::None;
	}
}

void AHFPlayerController::OnRightPressed()
{
	float X = 0.0f;
	float Y = 0.0f;
	if (GetMousePosition(X, Y))
	{
		LastMouse = FVector2D(X, Y);
		bOrbiting = true;
	}
}

void AHFPlayerController::OnRightReleased()
{
	bOrbiting = false;
}

void AHFPlayerController::OnZoomIn()
{
	if (AHFCameraPawn* Camera = GetCameraPawn())
	{
		Camera->Zoom(1.0f);
	}
}

void AHFPlayerController::OnZoomOut()
{
	if (AHFCameraPawn* Camera = GetCameraPawn())
	{
		Camera->Zoom(-1.0f);
	}
}

void AHFPlayerController::OnCancel()
{
	// Primero suelta la herramienta elegida; si no había ninguna, el menú decide (volver un paso o pausar).
	if (Tool != EHFTool::None)
	{
		Tool = EHFTool::None;
		return;
	}
	if (AHFHUD* HUD = GetHUD<AHFHUD>())
	{
		HUD->HandleEscape();
	}
}

void AHFPlayerController::OnRollOrEnd()
{
	AHFGameMode* Mode = GetHexGameMode();
	if (!IsHumanTurn())
	{
		return;
	}
	const hf::Phase Phase = Mode->GetMatch()->GetGame().GetPhase();
	if (Phase == hf::Phase::AwaitRoll)
	{
		Tool = EHFTool::None;
		Mode->ApplyHumanAction(hf::Action::Roll());
	}
	else if (Phase == hf::Phase::Main)
	{
		Tool = EHFTool::None;
		Mode->ApplyHumanAction(hf::Action::End());
	}
}
