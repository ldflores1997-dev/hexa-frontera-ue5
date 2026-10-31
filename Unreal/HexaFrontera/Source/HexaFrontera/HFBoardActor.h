// Tablero 3D: genera las casillas hexagonales, sus decoraciones, las piezas y los resaltados.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFBoardActor.generated.h"

class FHFMatch;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

// Reemplazo visual de un terreno (por ejemplo, un material de Megascans o una malla de Blender).
USTRUCT(BlueprintType)
struct FHFTerrainVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	FString Scenario;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	FString Terrain;

	// Material para la casilla procedural.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TSoftObjectPtr<UMaterialInterface> Material;

	// Malla que reemplaza la casilla procedural: hexágono de radio 100 (1 m), base en Z = 0, alto 100.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TSoftObjectPtr<UStaticMesh> Mesh;
};

UCLASS(Config = Game)
class HEXAFRONTERA_API AHFBoardActor : public AActor
{
	GENERATED_BODY()

public:
	AHFBoardActor();

	virtual void Tick(float DeltaSeconds) override;

	// Arma todo el tablero a partir del estado de la partida.
	void Rebuild(const FHFMatch& Match);
	void RefreshPieces(const FHFMatch& Match);
	void RefreshStatuses(const FHFMatch& Match);
	void FlashTiles(const TArray<int32>& Tiles, const FLinearColor& Color);
	void SetHighlights(const TArray<int32>& Vertices, const TArray<int32>& Edges, const TArray<int32>& Tiles, const FLinearColor& Color);
	void ClearHighlights();

	FVector VertexWorld(int32 Vertex) const;
	FVector EdgeWorld(int32 Edge) const;
	FVector TileWorld(int32 Tile) const;
	float GetBoardTopZ() const { return BoardTopZ; }
	float GetBoardExtent() const { return BoardExtent; }

	// Devuelven -1 si no hay nada cerca del punto.
	int32 FindNearestVertex(const FVector& WorldPoint, float MaxDistance) const;
	int32 FindNearestEdge(const FVector& WorldPoint, float MaxDistance) const;
	int32 FindTileAt(const FVector& WorldPoint) const;

	// Distancia en cm del centro de una casilla a un vértice.
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	float TileRadius = 400.0f;

	// Altura de la base de cada casilla; se suma la altura del terreno multiplicada por TerrainHeightScale.
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	float TileBaseHeight = 40.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	float TerrainHeightScale = 160.0f;

	// Separación visual entre casillas (0 = pegadas).
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	float TileGap = 0.035f;

	// Material base para casillas, piezas y decoraciones. Debe tener un parámetro de color (ColorParameter).
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	FSoftObjectPath BaseMaterial = FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	FName ColorParameter = TEXT("Color");

	// Si las casillas se ven "por dentro", activarlo invierte el orden de los triángulos.
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	bool bFlipWinding = false;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	TArray<FHFTerrainVisual> TerrainVisuals;

	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	TArray<FLinearColor> PlayerColors;

private:
	struct FAnimatedPiece
	{
		TWeakObjectPtr<USceneComponent> Component;
		FVector Target;
		float StartTime = 0.0f;
	};

	struct FFlash
	{
		TWeakObjectPtr<UPointLightComponent> Light;
		float StartTime = 0.0f;
		float PeakIntensity = 0.0f;
	};

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LoadedBaseMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> TileMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TileComponents; // decoraciones y casillas de reemplazo
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PieceComponents;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> StatusComponents;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> HighlightComponents;
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> DecorationLights;
	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> MaterialCache;

	TArray<FAnimatedPiece> AnimatedPieces;
	TArray<FFlash> Flashes;
	TSet<int64> ShownPieces;

	FString ScenarioId;
	TArray<FVector2D> TileCenters;   // cm, plano XY
	TArray<float> TileHeights;       // cm
	TArray<int32> TileTerrains;
	TArray<FVector2D> VertexPositions;
	TArray<float> VertexHeights;
	TArray<FIntPoint> EdgeVertices;
	float BoardTopZ = 0.0f;
	float BoardExtent = 0.0f;

	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color);
	UMaterialInterface* TerrainMaterialOverride(const FString& TerrainId) const;
	UStaticMesh* TerrainMeshOverride(const FString& TerrainId) const;
	UStaticMeshComponent* AddShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color,
		TArray<TObjectPtr<UStaticMeshComponent>>& Owner, const FRotator& Rotation = FRotator::ZeroRotator);
	void BuildTileMesh(int32 Tile, const TArray<FVector2D>& Corners, float Height, const FLinearColor& Color, UMaterialInterface* Override);
	void AddDecoration(int32 Tile, const FString& Decoration, const FLinearColor& TerrainColor);
	void ClearComponents(TArray<TObjectPtr<UStaticMeshComponent>>& Components);
};
