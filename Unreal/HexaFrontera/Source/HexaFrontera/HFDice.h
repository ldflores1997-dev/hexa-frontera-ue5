// Dos dados de terreno con física que se tiran sobre una bandeja junto al tablero.
// Cada cara muestra un terreno (su color), el evento del mundo o un comodín. Cuando se quedan
// quietos avisan qué cara (1-6) quedó arriba; la partida traduce esa cara con los dados del escenario.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFDice.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

DECLARE_DELEGATE_TwoParams(FHFOnDiceSettled, int32 /*First*/, int32 /*Second*/);

// Cómo se ve una cara: el color del terreno, o los símbolos del evento y del comodín.
struct FHFDieFaceLook
{
	enum class ESymbol : uint8 { Terrain, Event, Wild };
	FLinearColor Color = FLinearColor::White;
	ESymbol Symbol = ESymbol::Terrain;
};

UCLASS()
class HEXAFRONTERA_API AHFDice : public AActor
{
	GENERATED_BODY()

public:
	AHFDice();

	virtual void Tick(float DeltaSeconds) override;

	// Arma la bandeja en Location (mirando hacia Facing) y tira los dados desde el lado de la cámara.
	void Throw(const FVector& Location, float FacingYaw);
	bool IsRolling() const { return bRolling; }
	// Caras de cada dado (6 por dado, en el orden 1-6). Si cambian, los dados se rearman en la próxima tirada.
	void SetFaces(const TArray<FHFDieFaceLook>& First, const TArray<FHFDieFaceLook>& Second);

	// Último resultado (0 si todavía no se tiró).
	int32 GetValue(int32 Index) const { return Values[Index]; }

	FHFOnDiceSettled OnSettled;

	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	float DieSize = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	FVector2D TraySize = FVector2D(520.0f, 360.0f);

	// Segundos máximos de espera: si un dado queda trabado se lee igual.
	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	float MaxRollSeconds = 6.0f;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TrayParts;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Dice[2];

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	bool bRolling = false;
	float RollStart = 0.0f;
	float StillSince = -1.0f;
	int32 Nudges = 0;
	int32 Values[2] = {0, 0};
	TArray<FHFDieFaceLook> Looks[2];
	bool bLooksChanged = false;

	// Preferred: nombre de un material de Biomas/_Comun con parámetro Color (M_Resina, M_Forma).
	UMaterialInstanceDynamic* MakeMaterial(const FLinearColor& Color, const TCHAR* Preferred = nullptr);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Scale, UMaterialInterface* Material);
	void BuildTray();
	UStaticMeshComponent* BuildDie(int32 Index);
	static int32 TopFace(const UStaticMeshComponent* Die, float& OutAlignment);
	void Finish();
};
