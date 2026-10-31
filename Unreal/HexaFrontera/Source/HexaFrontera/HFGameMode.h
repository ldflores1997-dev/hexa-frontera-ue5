// Coordina la partida: arma el mundo, recibe acciones de la interfaz, hace jugar a los bots
// y traduce los eventos del núcleo a efectos visuales.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HFMatch.h"
#include "HFGameMode.generated.h"

class AHFBoardActor;
class AHFCameraPawn;
class AHFEnvironment;

UCLASS()
class HEXAFRONTERA_API AHFGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHFGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Muestra el tablero de un mundo detrás del menú (misma semilla que la partida).
	void PreviewScenario(const FString& ScenarioId, int32 PlayerCount);
	bool BeginHexMatch(const FHFMatchConfig& Config, FString& OutError);
	void ReturnToMenu();

	// Acción de un jugador humano. Devuelve false y guarda el error para mostrarlo.
	bool ApplyHumanAction(const hf::Action& Action);
	void ShowError(const FString& Message);

	bool IsInMatch() const { return bInMatch && Match.IsValid(); }
	const FHFMatch* GetMatch() const { return Match.Get(); }
	AHFBoardActor* GetBoard() const { return Board; }
	int64 GetPreviewSeed() const { return PreviewSeed; }
	const FString& GetPreviewScenarioId() const { return PreviewScenarioId; }

	// Cambia cada vez que el estado cambia; la interfaz lo usa para refrescarse.
	int32 GetStateVersion() const { return StateVersion; }
	const TArray<FString>& GetMessages() const { return Messages; }
	const FString& GetLastError() const { return LastError; }
	float GetLastErrorTime() const { return LastErrorTime; }

	UPROPERTY(EditDefaultsOnly, Category = "Hexa Frontera")
	TSubclassOf<AHFBoardActor> BoardClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hexa Frontera")
	float BotDelaySeconds = 0.8f;

private:
	UPROPERTY()
	TObjectPtr<AHFBoardActor> Board;

	UPROPERTY()
	TObjectPtr<AHFEnvironment> Environment;

	TSharedPtr<FHFMatch> Match;
	bool bInMatch = false;
	float NextBotTime = 0.0f;
	int32 StateVersion = 0;
	int64 PreviewSeed = 1;
	FString PreviewScenarioId;
	TArray<FString> Messages;
	FString LastError;
	float LastErrorTime = -100.0f;

	void HandleEvents(const std::vector<hf::GameEvent>& Events);
	void ShowWorld();
	void AddMessage(const FString& Message);
	AHFCameraPawn* GetCameraPawn() const;
};
