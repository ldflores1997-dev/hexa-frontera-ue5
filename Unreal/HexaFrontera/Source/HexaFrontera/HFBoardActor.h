// Tablero 3D: genera las casillas hexagonales, sus decoraciones, las piezas y los resaltados.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFBoardActor.generated.h"

class FHFMatch;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

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

	// Lo mismo que las subcarpetas de BiomeFolder (ver Docs/GRAFICOS.md), para usar recursos sin moverlos.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TArray<TSoftObjectPtr<UStaticMesh>> Plants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TArray<TSoftObjectPtr<UStaticMesh>> Rocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TArray<TSoftObjectPtr<UStaticMesh>> GroundCover;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TSoftObjectPtr<UStaticMesh> Landmark;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TArray<TSoftObjectPtr<UStaticMesh>> Birds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexa Frontera")
	TArray<TSoftObjectPtr<UStaticMesh>> Fauna;
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
	// Hace saltar las fichas con ese número (el que salió en los dados).
	void HighlightNumber(int32 Number);
	// Sol y noche del ambiente: los paneles siguen al sol y de noche se prenden luces en las construcciones.
	void SetDayState(const FVector& InSunDirection, float InNight);

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

	// Carpeta donde se buscan los recursos de cada bioma: <BiomeFolder>/<mundo>/<terreno>/ (ver Docs/GRAFICOS.md).
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	FString BiomeFolder = TEXT("/Game/Biomas");

	// Alto en cm al que se escala cada recurso, sin importar su tamaño original (con ±25% de variación).
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float PlantHeight = 120.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float RockHeight = 40.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float GroundCoverHeight = 18.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float LandmarkHeight = 260.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float BirdSize = 45.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float FaunaHeight = 32.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	int32 PlantsPerTile = 8;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	int32 RocksPerTile = 6;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	int32 GroundCoverPerTile = 120;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	int32 BirdsPerTile = 2;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	int32 FaunaPerTile = 2;

	// Giro que se suma a los animales y aves si avanzan de costado o de espaldas (depende de cómo se exportó el modelo).
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float FaunaYawOffset = 0.0f;

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

	// Movimiento continuo de una decoración (viento, engranajes, banderas, paneles, brillos).
	enum class EMotion : uint8 { Sway, Spin, Flutter, Twinkle, TrackSun };
	struct FMotion
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		EMotion Kind = EMotion::Sway;
		FVector Pivot = FVector::ZeroVector;  // punto alrededor del que gira (base de una planta, mástil)
		FVector Offset = FVector::ZeroVector; // posición original menos el pivote
		FQuat Rotation = FQuat::Identity;     // rotación original
		FVector Scale = FVector::OneVector;
		float Phase = 0.0f;
		float Amount = 1.0f;
	};

	// Partículas hechas con formas simples: humo, chispas, vapor, polvo, nieve y destellos.
	enum class EPuff : uint8 { Smoke, Ember, Steam, Dust, Snow, Glint };
	struct FEmitter
	{
		FVector Origin = FVector::ZeroVector;
		EPuff Kind = EPuff::Smoke;
		float Interval = 0.5f;
		float Spread = 30.0f;
		float NextTime = 0.0f;
	};
	struct FPuff
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Born = 0.0f;
		float Life = 1.0f;
		float StartScale = 0.1f;
		float EndScale = 0.1f;
	};

	// Superficie de agua con olas (se recalculan los vértices de la tapa en cada cuadro).
	struct FWater
	{
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		FVector2D Center = FVector2D::ZeroVector;
		float Height = 0.0f;
		FLinearColor Color = FLinearColor::White;
		TArray<FVector> Positions;
		TArray<float> Weights; // 0 en el borde (queda pegado al bisel), 1 en el centro
		TArray<FVector2D> UVs;
	};

	struct FGlow
	{
		TWeakObjectPtr<UPointLightComponent> Light;
		float Intensity = 0.0f;
		float Phase = 0.0f;
	};

	// Recursos de un bioma: lo que hay en <BiomeFolder>/<mundo>/<terreno>/ más lo que agrega TerrainVisuals.
	struct FBiomeAssets
	{
		UMaterialInterface* Material = nullptr;
		UStaticMesh* Tile = nullptr;
		UStaticMesh* Landmark = nullptr;
		TArray<UStaticMesh*> Plants;
		TArray<UStaticMesh*> Rocks;
		TArray<UStaticMesh*> GroundCover;
		TArray<UStaticMesh*> Birds;
		TArray<UStaticMesh*> Fauna;

		// Con cualquier modelo propio, la casilla deja de usar las formas básicas.
		bool ReplacesShapes() const
		{
			return Landmark || Plants.Num() || Rocks.Num() || GroundCover.Num() || Birds.Num() || Fauna.Num();
		}
	};

	// Animales: aves que vuelan en círculos, cóndores que planean, drones, dirigibles y animales que pastan.
	enum class ECreature : uint8 { Bird, Glider, Drone, Airship, Walker };
	enum class EPart : uint8 { Fixed, WingLeft, WingRight, Rotor };
	struct FCreaturePart
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		EPart Kind = EPart::Fixed;
		FVector Offset = FVector::ZeroVector; // respecto del cuerpo, mirando hacia +X
		FQuat Rotation = FQuat::Identity;
	};
	struct FCreature
	{
		ECreature Kind = ECreature::Bird;
		TArray<FCreaturePart> Parts;
		FVector Center = FVector::ZeroVector; // centro de la ronda o de la zona donde camina
		float Radius = 100.0f;
		float Speed = 0.5f;   // radianes por segundo en vuelo; cm por segundo al caminar
		float Phase = 0.0f;
		// Solo para los que caminan.
		FVector Location = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
		float Yaw = 0.0f;
		float WaitUntil = 0.0f;
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
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> NumberTexts;
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> BuildingLights;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PuffPool;
	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GroundCoverComponents;

	TArray<FAnimatedPiece> AnimatedPieces;
	TArray<FFlash> Flashes;
	TSet<int64> ShownPieces;
	TArray<FMotion> Motions;
	TArray<FEmitter> Emitters;
	TArray<FPuff> Puffs;
	TArray<FWater> Waters;
	TArray<FGlow> Glows;
	TArray<FCreature> Creatures;
	TArray<int32> NumberTiles; // casilla de cada texto de NumberTexts
	TArray<float> BuildingLightPower; // intensidad de noche de cada luz de BuildingLights
	int32 HighlightedNumber = 0;
	float HighlightStart = -100.0f;
	FVector SunDirection = FVector::UpVector;
	float Night = 0.0f;

	FString ScenarioId;
	TArray<FVector2D> TileCenters;   // cm, plano XY
	TArray<float> TileHeights;       // cm
	TArray<int32> TileTerrains;
	TArray<int32> TileNumbers;
	TArray<FVector2D> VertexPositions;
	TArray<float> VertexHeights;
	TArray<FIntPoint> EdgeVertices;
	float BoardTopZ = 0.0f;
	float BoardExtent = 0.0f;

	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color);
	FBiomeAssets LoadBiome(const FString& TerrainId) const;
	UStaticMeshComponent* AddAsset(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, float Scale);
	// Escala que lleva la malla al alto pedido, y posición que apoya su base en Spot.
	float FitScale(const UStaticMesh* Mesh, float Height) const;
	FVector GroundedLocation(const UStaticMesh* Mesh, const FVector& Spot, const FRotator& Rotation, float Scale) const;
	void AddBiomeAssets(int32 Tile, const FBiomeAssets& Biome, bool bHasNumber);
	void AddCreature(ECreature Kind, const FVector& Center, float Radius, float Speed, float Phase, const FLinearColor& Color, UStaticMesh* Mesh = nullptr);
	void TickCreatures(float Now, float DeltaSeconds);
	UStaticMeshComponent* AddShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color,
		TArray<TObjectPtr<UStaticMeshComponent>>& OutComponents, const FRotator& Rotation = FRotator::ZeroRotator);
	void BuildTileMesh(int32 Tile, const TArray<FVector2D>& Corners, float Height, const FLinearColor& Color, UMaterialInterface* Override, bool bWater);
	void AddDecoration(int32 Tile, const FString& Decoration, const FString& TerrainId, const FLinearColor& TerrainColor, bool bHasNumber);
	void AddNumberToken(int32 Tile, int32 Number);
	void ClearComponents(TArray<TObjectPtr<UStaticMeshComponent>>& Components);
	void AddMotion(UStaticMeshComponent* Component, EMotion Kind, const FVector& Pivot, float Amount, float Phase);
	void AddEmitter(const FVector& Origin, EPuff Kind, float Interval, float Spread);
	void SpawnPuff(const FEmitter& Emitter, float Now);
	void TickLife(float Now, float DeltaSeconds);
};
