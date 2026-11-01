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
	// Sol y noche del ambiente: los paneles siguen al sol y de noche se prenden luces en las construcciones.
	void SetDayState(const FVector& InSunDirection, float InNight);

	FVector VertexWorld(int32 Vertex) const;
	FVector EdgeWorld(int32 Edge) const;
	FVector TileWorld(int32 Tile) const;
	float GetBoardTopZ() const { return BoardTopZ; }
	float GetBoardExtent() const { return BoardExtent; }

	// Material de Superficie del terreno en el mundo actual (la carpeta de bioma o TerrainVisuals), o nullptr.
	UMaterialInterface* LoadSurface(const FString& TerrainId) const;

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
	// Si no existe (no se corrió aplicar_texturas.py), se usa el material de formas del motor.
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera")
	FSoftObjectPath BaseMaterial = FSoftObjectPath(TEXT("/Game/Biomas/_Comun/M_Forma.M_Forma"));

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

	// Distancia a la cámara (cm) en la que el pasto empieza a desvanecerse y en la que ya no se dibuja.
	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float GroundCoverFadeStart = 5000.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Hexa Frontera|Biomas")
	float GroundCoverFadeEnd = 7500.0f;

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
		FVector2D TileCenter = FVector2D::ZeroVector; // las partículas no salen de esta casilla
		float Ceiling = 0.0f;                         // ni suben más que esto
		float Floor = 0.0f;                           // ni bajan de la tapa de la casilla
	};
	struct FPuff
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material; // translúcido, si existe M_Humo
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Born = 0.0f;
		float Life = 1.0f;
		float StartScale = 0.1f;
		float EndScale = 0.1f;
		FVector2D TileCenter = FVector2D::ZeroVector;
		float Ceiling = 0.0f;
		float Floor = 0.0f;
		float Opacity = 0.5f;
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

	// Mapa de altura de la textura de un terreno (Content/Relieves, lo arma Tools/relieves.py).
	struct FHeightField
	{
		int32 Size = 0;
		float Tiling = 250.0f;   // cm por repetición, igual que la textura
		float Amplitude = 0.0f;  // cm entre lo más bajo y lo más alto
		TArray<uint16> Data;
		bool IsValid() const { return Size > 1 && Amplitude > 0.0f && Data.Num() == Size * Size; }
		// Altura en cm (entre -Amplitude/2 y +Amplitude/2) en un punto del tablero, alineada con la textura.
		float Sample(const FVector2D& Point) const;
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
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> PaintedWoodCache;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> SmokeMaterial;
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
	TArray<FMotion> StatusMotions; // las de tormentas, heladas y refuerzos, que se rehacen con cada estado
	TArray<FEmitter> Emitters;
	TArray<FPuff> Puffs;
	TArray<FWater> Waters;
	TArray<FGlow> Glows;
	TArray<FCreature> Creatures;
	TArray<float> BuildingLightPower; // intensidad de noche de cada luz de BuildingLights
	FVector SunDirection = FVector::UpVector;
	float Night = 0.0f;

	FString ScenarioId;
	TArray<FVector2D> TileCenters;   // cm, plano XY
	TArray<float> TileHeights;       // cm
	TArray<int32> TileTerrains;
	TArray<FVector2D> VertexPositions;
	TArray<float> VertexHeights;
	TArray<FIntPoint> EdgeVertices;
	float BoardTopZ = 0.0f;
	float BoardExtent = 0.0f;

	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color, float Glow = 0.0f);
	// Madera pintada del color pedido (piezas de los jugadores); sin el material, el color liso de siempre.
	UMaterialInterface* PaintedWood(const FLinearColor& Color);
	// Translúcido y con brillo propio (humo, tormenta, escarcha, refuerzo); sin M_Humo, un color opaco.
	UMaterialInterface* GlassMaterial(const FLinearColor& Color, float Opacity, float Glow);
	// Malla de pieza del mundo actual (<BiomeFolder>/<mundo>/_Piezas/<Name>) o la común; nullptr si no hay.
	UStaticMesh* PieceMesh(const TCHAR* Name) const;
	UStaticMeshComponent* AddPiece(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FLinearColor& Color);
	void BuildFrame();
	void TickMotion(FMotion& Motion, float Now, float DeltaSeconds, const FVector& SunFlat);
	FBiomeAssets LoadBiome(const FString& TerrainId) const;
	UStaticMeshComponent* AddAsset(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, float Scale);
	// Escala que lleva la malla al alto pedido, y posición que apoya su base en Spot.
	float FitScale(const UStaticMesh* Mesh, float Height) const;
	FVector GroundedLocation(const UStaticMesh* Mesh, const FVector& Spot, const FRotator& Rotation, float Scale) const;
	void AddBiomeAssets(int32 Tile, const FBiomeAssets& Biome);
	void AddCreature(ECreature Kind, const FVector& Center, float Radius, float Speed, float Phase, const FLinearColor& Color, UStaticMesh* Mesh = nullptr);
	void TickCreatures(float Now, float DeltaSeconds);
	UStaticMeshComponent* AddShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color,
		TArray<TObjectPtr<UStaticMeshComponent>>& OutComponents, const FRotator& Rotation = FRotator::ZeroRotator);
	FHeightField LoadRelief(const FString& TerrainId) const;
	// Relief: si es válido, la tapa se subdivide y sigue el mapa de altura de la textura.
	void BuildTileMesh(int32 Tile, const TArray<FVector2D>& Corners, float Height, const FLinearColor& Color, UMaterialInterface* Override, bool bWater,
		const FHeightField* Relief);
	void AddDecoration(int32 Tile, const FString& Decoration, const FString& TerrainId, const FLinearColor& TerrainColor);
	void ClearComponents(TArray<TObjectPtr<UStaticMeshComponent>>& Components);
	void AddMotion(UStaticMeshComponent* Component, EMotion Kind, const FVector& Pivot, float Amount, float Phase);
	void AddEmitter(int32 Tile, const FVector& Origin, EPuff Kind, float Interval, float Spread, float Rise);
	// Radio del círculo que entra en el hexágono de una casilla, con margen: nada decorativo pasa de ahí.
	float DecorationRadius() const;
	// Radio en planta de una malla a escala 1 (la mitad de la diagonal de su caja).
	float MeshFootprint(const UStaticMesh* Mesh) const;
	void SpawnPuff(const FEmitter& Emitter, float Now);
	void TickLife(float Now, float DeltaSeconds);
};
