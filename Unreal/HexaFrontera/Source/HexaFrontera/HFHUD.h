// Interfaz en Slate hecha en C++ (sin assets binarios): menú de inicio y tablero de juego.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HFHUD.generated.h"

class AHFGameMode;
class AHFPlayerController;
class SWidget;
class SWeakWidget;

UCLASS()
class HEXAFRONTERA_API AHFHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FScenarioInfo
	{
		FString Id;
		FString Name;
		FString Description;
	};

	// Menú
	TArray<FScenarioInfo> Scenarios;
	int32 SelectedScenario = 0;
	int32 PlayerCount = 2;
	bool Bots[4] = {false, true, true, true};

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
	void ShowGame();
	void StartMatch();
	void ProposeTrade();
	void ResetTrade();

	TSharedRef<SWidget> BuildMenu();
	TSharedRef<SWidget> BuildGame();
	TSharedRef<SWidget> BuildPlayersPanel();
	TSharedRef<SWidget> BuildActionsPanel();
	TSharedRef<SWidget> BuildMarketPanel();
	TSharedRef<SWidget> BuildContractsPanel();
	TSharedRef<SWidget> BuildTradePanel();
};
