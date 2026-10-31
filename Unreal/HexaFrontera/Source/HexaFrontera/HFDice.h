// Dos dados con física que se tiran sobre una bandeja junto al tablero.
// Cuando se quedan quietos avisan qué cara quedó arriba; ese resultado es el que usa la partida.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFDice.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

DECLARE_DELEGATE_TwoParams(FHFOnDiceSettled, int32 /*First*/, int32 /*Second*/);

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

	UMaterialInstanceDynamic* MakeMaterial(const FLinearColor& Color);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Scale, UMaterialInstanceDynamic* Material);
	void BuildTray();
	UStaticMeshComponent* BuildDie(int32 Index);
	static int32 TopFace(const UStaticMeshComponent* Die, float& OutAlignment);
	void Finish();
};
