// Entrada del jugador: clic en el tablero para construir, cámara con mouse y teclado.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HFPlayerController.generated.h"

class AHFCameraPawn;
class AHFGameMode;

enum class EHFTool : uint8
{
	None,
	Outpost,
	Route,
	Center,
	Boost,
};

UCLASS()
class HEXAFRONTERA_API AHFPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHFPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	void SetTool(EHFTool NewTool);
	// La herramienta real: en la preparación se elige sola (puesto o ruta).
	EHFTool GetEffectiveTool() const;

private:
	EHFTool Tool = EHFTool::None;
	bool bOrbiting = false;
	FVector2D LastMouse = FVector2D::ZeroVector;
	int32 SeenStateVersion = -1;
	EHFTool SeenTool = EHFTool::None;

	AHFGameMode* GetHexGameMode() const;
	AHFCameraPawn* GetCameraPawn() const;
	bool IsHumanTurn() const;
	bool MouseOnBoard(FVector& OutPoint) const;
	void UpdateHighlights();

	void OnLeftClick();
	void OnRightPressed();
	void OnRightReleased();
	void OnZoomIn();
	void OnZoomOut();
	void OnCancel();
	void OnRevealOrEnd();
};
