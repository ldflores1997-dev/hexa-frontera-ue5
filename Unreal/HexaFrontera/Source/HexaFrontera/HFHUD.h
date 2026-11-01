// Interfaz en Slate hecha en C++ (sin assets binarios): menú de inicio y tablero de juego.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HFHUD.generated.h"

class AHFGameMode;
class AHFPlayerController;
class SWidget;
class SWeakWidget;
class UTexture;

// Pantallas del menú de inicio, en orden.
enum class EMenuStep : uint8
{
	Title,   // portada: Jugar, Opciones, Salir
	World,   // paso 1: elegir el mundo
	Players, // paso 2: jugadores, bots y reloj
	Options,
};

UCLASS()
class HEXAFRONTERA_API AHFHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Dibuja la mano de cartas de recursos y las cartas que vuelan (con Canvas, debajo de los paneles de Slate).
	virtual void DrawHUD() override;

	// Una carta sale volando desde la casilla que produjo hasta la mano del jugador.
	void QueueCardFlight(int32 Player, int32 Resource, int32 Amount, const FVector& WorldLocation);

	// Esc: en el menú vuelve un paso; en la partida cierra lo abierto o pausa.
	void HandleEscape();
	bool IsPaused() const { return bPaused; }

private:
	struct FCardFlight
	{
		int32 Player = -1;
		int32 Resource = -1;
		int32 Amount = 1;
		FVector2D Start = FVector2D::ZeroVector;
		float StartTime = 0.0f;
	};

	struct FLeavingCard
	{
		int32 Resource = -1;
		FVector2D Start = FVector2D::ZeroVector;
		float StartTime = 0.0f;
		float Spin = 0.0f;
	};

	// Reloj por turno elegido en el menú (0 = sin límite).
	int32 TurnSeconds = 90;

	// Mano de cartas
	TArray<FCardFlight> Flights;
	TArray<FLeavingCard> Leaving;
	TArray<int32> PrevCounts;
	TArray<float> PopTime;
	TArray<float> ShakeTime;
	TArray<FVector2D> SlotPositions;
	int32 PrevHandPlayer = -1;
	int32 LastHumanPlayer = -1;

	int32 HandPlayer() const;
	float Now() const;
	void DrawCard(int32 Resource, const FVector2D& Center, float Angle, float Scale, float Alpha, int32 Count);
	void DrawPolygon(const FVector2D& Center, float Radius, int32 Sides, float Angle, const FLinearColor& Color);
	// Ilustración de la carta: la textura de color del terreno que produce el recurso, o nullptr.
	UTexture* ResourceArt(int32 Resource);

	// Por recurso del mundo de CardArtScenario; se vacía al cambiar de mundo.
	UPROPERTY()
	TMap<int32, TObjectPtr<UTexture>> CardArt;
	TMap<int32, float> CardArtRetry; // cuándo volver a buscar una ilustración que no apareció
	FString CardArtScenario;
	struct FScenarioInfo
	{
		FString Id;
		FString Name;
		FString Description;
	};

	// Menú
	EMenuStep MenuStep = EMenuStep::Title;
	TArray<FScenarioInfo> Scenarios;
	int32 SelectedScenario = 0;
	int32 PlayerCount = 2;
	bool Bots[4] = {false, true, true, true};

	// Pausa y opciones
	bool bPaused = false;
	bool bOptionsOpen = false;
	TArray<FIntPoint> Resolutions;

	// Intercambio
	bool bTradeOpen = false;
	bool bAwaitingConsent = false;
	int32 TradePartner = -1;
	TArray<int32> TradeGive;
	TArray<int32> TradeGet;

	TSharedPtr<SWidget> Content;
	TSharedPtr<SWeakWidget> Holder;

	AHFGameMode* GetHexGameMode() const;
	AHFPlayerController* GetHexController() const;

	void SetContent(TSharedRef<SWidget> NewContent);
	void ShowMenu();
	void SetMenuStep(EMenuStep Step);
	// Elige un mundo (da la vuelta en los extremos) y muestra su tablero detrás del menú.
	void SelectScenario(int32 Index);
	void SetPaused(bool bPause);
	void GoToMenu();
	void QuitGame();
	void ShowGame();
	void StartMatch();
	void ProposeTrade();
	void ResetTrade();

	TSharedRef<SWidget> BuildMenu();
	TSharedRef<SWidget> BuildTitle();
	TSharedRef<SWidget> BuildWorldStep();
	TSharedRef<SWidget> BuildPlayersStep();
	TSharedRef<SWidget> BuildOptions(TFunction<void()> OnBack);
	TSharedRef<SWidget> BuildPause();
	TSharedRef<SWidget> BuildGame();
	TSharedRef<SWidget> BuildTimer();
	TSharedRef<SWidget> BuildPlayersPanel();
	TSharedRef<SWidget> BuildActionsPanel();
	TSharedRef<SWidget> BuildMarketPanel();
	TSharedRef<SWidget> BuildContractsPanel();
	TSharedRef<SWidget> BuildTradePanel();
	TSharedRef<SWidget> BuildWildPanel();
};
