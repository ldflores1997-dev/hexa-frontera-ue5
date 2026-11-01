#include "HFBoardActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HFMatch.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Modules/ModuleManager.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const float PieceDropHeight = 600.0f;
const float PieceDropSeconds = 0.45f;
const float FlashSeconds = 1.6f;
const int32 MaxPuffs = 300;
const int32 WaterSubdivisions = 7;
const float WaveAmplitude = 6.0f;
const FVector2D WindDirection(0.8f, 0.6f);

int64 PieceKey(int32 Kind, int32 Index, int32 Level)
{
	return (static_cast<int64>(Kind) << 40) | (static_cast<int64>(Index) << 8) | Level;
}

FLinearColor Shade(const FLinearColor& Color, float Factor)
{
	return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, 1.0f);
}

// Fase según la posición: el viento recorre el tablero como una ola.
float WindPhase(const FVector& P, FRandomStream& Random)
{
	return static_cast<float>(FVector2D::DotProduct(FVector2D(P), WindDirection)) * 0.004f + Random.FRandRange(0.0f, 0.6f);
}
}

AHFBoardActor::AHFBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	ConeMesh = Cone.Object;
	SphereMesh = Sphere.Object;

	PlayerColors = {
		FLinearColor(0.80f, 0.08f, 0.06f), // rojo
		FLinearColor(0.05f, 0.25f, 0.85f), // azul
		FLinearColor(0.95f, 0.65f, 0.05f), // ámbar
		FLinearColor(0.92f, 0.92f, 0.90f), // blanco
	};
}

UMaterialInstanceDynamic* AHFBoardActor::MaterialFor(const FLinearColor& Color)
{
	if (!LoadedBaseMaterial)
	{
		LoadedBaseMaterial = Cast<UMaterialInterface>(BaseMaterial.TryLoad());
		if (!LoadedBaseMaterial)
		{
			UE_LOG(LogTemp, Warning, TEXT("No se pudo cargar el material base %s"), *BaseMaterial.ToString());
			return nullptr;
		}
	}
	const FString Key = Color.ToString();
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = MaterialCache.Find(Key))
	{
		return Found->Get();
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(LoadedBaseMaterial, this);
	Material->SetVectorParameterValue(ColorParameter, Color);
	MaterialCache.Add(Key, Material);
	return Material;
}

AHFBoardActor::FBiomeAssets AHFBoardActor::LoadBiome(const FString& TerrainId) const
{
	FBiomeAssets Biome;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FString Folder = BiomeFolder / ScenarioId / TerrainId;

	// Cada subcarpeta se recorre entera: los recursos de Fab traen sus propias carpetas de mallas, texturas y materiales.
	auto Find = [&](const TCHAR* Subfolder, UClass* Class)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(*(Folder / Subfolder)), Assets, true);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		TArray<UObject*> Found;
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.IsInstanceOf(Class))
			{
				if (UObject* Object = Asset.GetAsset())
				{
					Found.Add(Object);
				}
			}
		}
		return Found;
	};
	auto Meshes = [&](const TCHAR* Subfolder, TArray<UStaticMesh*>& Out)
	{
		for (UObject* Object : Find(Subfolder, UStaticMesh::StaticClass()))
		{
			Out.Add(CastChecked<UStaticMesh>(Object));
		}
	};

	// La superficie suele traer el material y su instancia: se prefiere la instancia, que es la que se ajusta.
	for (UObject* Object : Find(TEXT("Superficie"), UMaterialInterface::StaticClass()))
	{
		if (!Biome.Material || Object->IsA<UMaterialInstance>())
		{
			Biome.Material = CastChecked<UMaterialInterface>(Object);
		}
	}
	TArray<UStaticMesh*> Tiles;
	TArray<UStaticMesh*> Landmarks;
	Meshes(TEXT("Casilla"), Tiles);
	Meshes(TEXT("Relieve"), Landmarks);
	Biome.Tile = Tiles.Num() ? Tiles[0] : nullptr;
	Biome.Landmark = Landmarks.Num() ? Landmarks[0] : nullptr;
	Meshes(TEXT("Plantas"), Biome.Plants);
	Meshes(TEXT("Rocas"), Biome.Rocks);
	Meshes(TEXT("Suelo"), Biome.GroundCover);
	Meshes(TEXT("Aves"), Biome.Birds);
	Meshes(TEXT("Fauna"), Biome.Fauna);

	// Lo que se configura en DefaultGame.ini manda sobre las carpetas.
	auto Append = [](const TArray<TSoftObjectPtr<UStaticMesh>>& From, TArray<UStaticMesh*>& To)
	{
		for (const TSoftObjectPtr<UStaticMesh>& Soft : From)
		{
			if (UStaticMesh* Mesh = Soft.LoadSynchronous())
			{
				To.AddUnique(Mesh);
			}
		}
	};
	for (const FHFTerrainVisual& Visual : TerrainVisuals)
	{
		if (Visual.Scenario != ScenarioId || Visual.Terrain != TerrainId)
		{
			continue;
		}
		if (!Visual.Material.IsNull())
		{
			Biome.Material = Visual.Material.LoadSynchronous();
		}
		if (!Visual.Mesh.IsNull())
		{
			Biome.Tile = Visual.Mesh.LoadSynchronous();
		}
		if (!Visual.Landmark.IsNull())
		{
			Biome.Landmark = Visual.Landmark.LoadSynchronous();
		}
		Append(Visual.Plants, Biome.Plants);
		Append(Visual.Rocks, Biome.Rocks);
		Append(Visual.GroundCover, Biome.GroundCover);
		Append(Visual.Birds, Biome.Birds);
		Append(Visual.Fauna, Biome.Fauna);
	}
	return Biome;
}

UStaticMeshComponent* AHFBoardActor::AddAsset(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, float Scale)
{
	// Como AddShape, pero respeta los materiales que trae la malla.
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetupAttachment(Root);
	Component->SetRelativeLocationAndRotation(Location, Rotation);
	Component->SetRelativeScale3D(FVector(Scale));
	Component->RegisterComponent();
	TileComponents.Add(Component);
	return Component;
}

float AHFBoardActor::FitScale(const UStaticMesh* Mesh, float Height) const
{
	const float MeshHeight = 2.0f * static_cast<float>(Mesh->GetBounds().BoxExtent.Z);
	return MeshHeight > 1.0f ? Height / MeshHeight : 1.0f;
}

FVector AHFBoardActor::GroundedLocation(const UStaticMesh* Mesh, const FVector& Spot, const FRotator& Rotation, float Scale) const
{
	// El pivote de un modelo importado puede estar en cualquier lado: se apoya el centro de su base.
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector Bottom(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
	return Spot - Rotation.RotateVector(Bottom * Scale);
}

UStaticMeshComponent* AHFBoardActor::AddShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color,
	TArray<TObjectPtr<UStaticMeshComponent>>& OutComponents, const FRotator& Rotation)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetupAttachment(Root);
	Component->SetRelativeLocationAndRotation(Location, Rotation);
	Component->SetRelativeScale3D(Scale);
	Component->RegisterComponent();
	if (UMaterialInstanceDynamic* Material = MaterialFor(Color))
	{
		Component->SetMaterial(0, Material);
	}
	OutComponents.Add(Component);
	return Component;
}

void AHFBoardActor::ClearComponents(TArray<TObjectPtr<UStaticMeshComponent>>& Components)
{
	for (UStaticMeshComponent* Component : Components)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	Components.Reset();
}

void AHFBoardActor::Rebuild(const FHFMatch& Match)
{
	const hf::Scenario& Scenario = Match.GetScenario();
	const hf::Board& Board = Match.GetGame().GetBoard();
	ScenarioId = FHFMatch::ToFString(Scenario.id);

	for (UProceduralMeshComponent* Mesh : TileMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	TileMeshes.Reset();
	for (UPointLightComponent* Light : DecorationLights)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	DecorationLights.Reset();
	ClearComponents(TileComponents);
	ClearComponents(PieceComponents);
	ClearComponents(StatusComponents);
	ClearHighlights();
	for (UHierarchicalInstancedStaticMeshComponent* Cover : GroundCoverComponents)
	{
		if (Cover)
		{
			Cover->DestroyComponent();
		}
	}
	GroundCoverComponents.Reset();
	Motions.Reset();
	Emitters.Reset();
	Waters.Reset();
	Glows.Reset();
	Creatures.Reset();
	for (FPuff& Puff : Puffs)
	{
		if (UStaticMeshComponent* Component = Puff.Component.Get())
		{
			Component->SetVisibility(false);
		}
	}
	Puffs.Reset();

	const auto& Tiles = Board.Tiles();
	const auto& Vertices = Board.Vertices();
	const auto& Edges = Board.Edges();

	TileCenters.SetNum(static_cast<int32>(Tiles.size()));
	TileHeights.SetNum(static_cast<int32>(Tiles.size()));
	TileTerrains.SetNum(static_cast<int32>(Tiles.size()));
	VertexPositions.SetNum(static_cast<int32>(Vertices.size()));
	VertexHeights.SetNum(static_cast<int32>(Vertices.size()));
	EdgeVertices.SetNum(static_cast<int32>(Edges.size()));

	BoardTopZ = 0.0f;
	BoardExtent = 0.0f;
	for (int32 V = 0; V < VertexPositions.Num(); ++V)
	{
		const hf::Point2& P = Vertices[static_cast<size_t>(V)].position;
		VertexPositions[V] = FVector2D(P.x * TileRadius, P.y * TileRadius);
		BoardExtent = FMath::Max(BoardExtent, static_cast<float>(VertexPositions[V].Size()));
	}
	for (int32 E = 0; E < EdgeVertices.Num(); ++E)
	{
		EdgeVertices[E] = FIntPoint(Edges[static_cast<size_t>(E)].a, Edges[static_cast<size_t>(E)].b);
	}

	TMap<int32, FBiomeAssets> Biomes; // por terreno, para buscar los recursos una sola vez
	for (int32 T = 0; T < TileCenters.Num(); ++T)
	{
		const hf::Tile& Tile = Tiles[static_cast<size_t>(T)];
		const hf::Point2 Center = hf::HexToPoint(Tile.coord);
		TileCenters[T] = FVector2D(Center.x * TileRadius, Center.y * TileRadius);
		TileTerrains[T] = Tile.terrain;
		const hf::TerrainDef& Terrain = Scenario.terrains[static_cast<size_t>(Tile.terrain)];
		TileHeights[T] = TileBaseHeight + Terrain.height * TerrainHeightScale;
		BoardTopZ = FMath::Max(BoardTopZ, TileHeights[T]);

		TArray<FVector2D> Corners;
		for (int32 C = 0; C < 6; ++C)
		{
			Corners.Add((VertexPositions[Tile.vertices[static_cast<size_t>(C)]] - TileCenters[T]) / TileRadius);
		}
		const FLinearColor Color = FHFMatch::ToLinear(Terrain.color);
		const FString TerrainId = FHFMatch::ToFString(Terrain.id);
		if (!Biomes.Contains(Tile.terrain))
		{
			Biomes.Add(Tile.terrain, LoadBiome(TerrainId));
		}
		const FBiomeAssets& Biome = Biomes[Tile.terrain];
		if (Biome.Tile)
		{
			const float Scale = TileRadius * (1.0f - TileGap) / 100.0f;
			UStaticMeshComponent* Custom = AddShape(Biome.Tile, FVector(TileCenters[T], 0.0f),
				FVector(Scale, Scale, TileHeights[T] / 100.0f), Color, TileComponents);
			if (Biome.Material)
			{
				Custom->SetMaterial(0, Biome.Material);
			}
			else
			{
				Custom->EmptyOverrideMaterials(); // usa los materiales importados con la malla
			}
		}
		else
		{
			const FString Decoration = FHFMatch::ToFString(Terrain.decoration);
			BuildTileMesh(T, Corners, TileHeights[T], Color, Biome.Material, Decoration == TEXT("coral"));
		}
		const int32 FirstShape = TileComponents.Num();
		const int32 FirstMotion = Motions.Num();
		const int32 FirstCreature = Creatures.Num();
		AddDecoration(T, FHFMatch::ToFString(Terrain.decoration), TerrainId, Color);
		if (Biome.ReplacesShapes())
		{
			// Con modelos propios, las formas básicas de esta casilla sobran; el humo, las luces y el agua se quedan.
			for (int32 I = FirstShape; I < TileComponents.Num(); ++I)
			{
				if (TileComponents[I])
				{
					TileComponents[I]->DestroyComponent();
				}
			}
			TileComponents.SetNum(FirstShape);
			Motions.SetNum(FirstMotion);
			Creatures.SetNum(FirstCreature);
			AddBiomeAssets(T, Biome);
		}
	}

	for (int32 V = 0; V < VertexHeights.Num(); ++V)
	{
		float Height = 0.0f;
		for (int32 T : Vertices[static_cast<size_t>(V)].tiles)
		{
			Height = FMath::Max(Height, TileHeights[T]);
		}
		VertexHeights[V] = Height;
	}

	RefreshPieces(Match);
	RefreshStatuses(Match);
}

void AHFBoardActor::BuildTileMesh(int32 Tile, const TArray<FVector2D>& Corners, float Height, const FLinearColor& Color, UMaterialInterface* Override, bool bWater)
{
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	const float Outer = TileRadius * (1.0f - TileGap);
	const float Bevel = FMath::Min(30.0f, Height * 0.3f);
	const float Inset = Outer - Bevel;
	const FLinearColor SideColor = Shade(Color, 0.55f);

	auto AddVertex = [&](const FVector& P, const FVector& N, const FLinearColor& C)
	{
		Positions.Add(P);
		Normals.Add(N);
		UVs.Add(FVector2D(P.X / TileRadius, P.Y / TileRadius));
		Colors.Add(C);
		Tangents.Add(FProcMeshTangent(FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal(1e-4f, FVector::ForwardVector), false));
		return Positions.Num() - 1;
	};
	// Ordena el triángulo para que su cara visible apunte hacia `Facing`.
	auto AddTriangle = [&](int32 A, int32 B, int32 C, const FVector& Facing)
	{
		const FVector Normal = FVector::CrossProduct(Positions[B] - Positions[C], Positions[A] - Positions[C]);
		bool bSwap = FVector::DotProduct(Normal, Facing) < 0.0f;
		if (bFlipWinding)
		{
			bSwap = !bSwap;
		}
		Triangles.Add(A);
		Triangles.Add(bSwap ? C : B);
		Triangles.Add(bSwap ? B : C);
	};

	// Tapa superior (el agua la arma aparte, con más vértices para las olas)
	if (!bWater)
	{
		const int32 CenterIndex = AddVertex(FVector(0, 0, Height), FVector::UpVector, Color);
		TArray<int32> TopRing;
		for (const FVector2D& Corner : Corners)
		{
			TopRing.Add(AddVertex(FVector(Corner * Inset, Height), FVector::UpVector, Color));
		}
		for (int32 C = 0; C < 6; ++C)
		{
			AddTriangle(CenterIndex, TopRing[C], TopRing[(C + 1) % 6], FVector::UpVector);
		}
	}

	// Bisel y laterales, con vértices propios por cara para un sombreado plano.
	for (int32 C = 0; C < 6; ++C)
	{
		const FVector2D A2 = Corners[C];
		const FVector2D B2 = Corners[(C + 1) % 6];
		const FVector2D Mid = (A2 + B2) * 0.5f;
		const FVector Out = FVector(Mid.GetSafeNormal(), 0.0f);
		const FVector BevelNormal = (Out + FVector::UpVector).GetSafeNormal();

		const int32 B0 = AddVertex(FVector(A2 * Inset, Height), BevelNormal, Color);
		const int32 B1 = AddVertex(FVector(B2 * Inset, Height), BevelNormal, Color);
		const int32 B2i = AddVertex(FVector(B2 * Outer, Height - Bevel), BevelNormal, SideColor);
		const int32 B3 = AddVertex(FVector(A2 * Outer, Height - Bevel), BevelNormal, SideColor);
		AddTriangle(B0, B1, B2i, BevelNormal);
		AddTriangle(B0, B2i, B3, BevelNormal);

		const int32 S0 = AddVertex(FVector(A2 * Outer, Height - Bevel), Out, SideColor);
		const int32 S1 = AddVertex(FVector(B2 * Outer, Height - Bevel), Out, SideColor);
		const int32 S2 = AddVertex(FVector(B2 * Outer, 0.0f), Out, Shade(SideColor, 0.6f));
		const int32 S3 = AddVertex(FVector(A2 * Outer, 0.0f), Out, Shade(SideColor, 0.6f));
		AddTriangle(S0, S1, S2, Out);
		AddTriangle(S0, S2, S3, Out);
	}

	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(Root);
	Mesh->SetRelativeLocation(FVector(TileCenters[Tile], 0.0f));
	Mesh->bUseAsyncCooking = true;
	Mesh->RegisterComponent();
	Mesh->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, UVs, Colors, Tangents, false);

	if (bWater)
	{
		// Cada uno de los 6 triángulos de la tapa se subdivide en una grilla; Tick mueve los vértices.
		FWater Water;
		Water.Mesh = Mesh;
		Water.Center = TileCenters[Tile];
		Water.Height = Height;
		Water.Color = Color;
		TArray<int32> WaterTriangles;
		const int32 N = WaterSubdivisions;
		for (int32 C = 0; C < 6; ++C)
		{
			const FVector2D A = Corners[C] * Inset;
			const FVector2D B = Corners[(C + 1) % 6] * Inset;
			TArray<int32> RowStart;
			for (int32 I = 0; I <= N; ++I)
			{
				RowStart.Add(Water.Positions.Num());
				for (int32 J = 0; J <= N - I; ++J)
				{
					const FVector2D P = A * (static_cast<float>(I) / N) + B * (static_cast<float>(J) / N);
					Water.Positions.Add(FVector(P, Height));
					Water.Weights.Add(1.0f - FMath::Pow(static_cast<float>(I + J) / N, 3.0f));
					Water.UVs.Add(P / TileRadius);
				}
			}
			for (int32 I = 0; I < N; ++I)
			{
				for (int32 J = 0; J < N - I; ++J)
				{
					const int32 P00 = RowStart[I] + J;
					const int32 P01 = RowStart[I] + J + 1;
					const int32 P10 = RowStart[I + 1] + J;
					WaterTriangles.Append({P00, P10, P01});
					if (J < N - I - 1)
					{
						WaterTriangles.Append({P10, RowStart[I + 1] + J + 1, P01});
					}
				}
			}
		}
		// Orienta todos los triángulos hacia arriba (y respeta bFlipWinding).
		for (int32 K = 0; K < WaterTriangles.Num(); K += 3)
		{
			const FVector& PA = Water.Positions[WaterTriangles[K]];
			const FVector& PB = Water.Positions[WaterTriangles[K + 1]];
			const FVector& PC = Water.Positions[WaterTriangles[K + 2]];
			bool bSwap = FVector::DotProduct(FVector::CrossProduct(PB - PC, PA - PC), FVector::UpVector) < 0.0f;
			if (bFlipWinding)
			{
				bSwap = !bSwap;
			}
			if (bSwap)
			{
				Swap(WaterTriangles[K + 1], WaterTriangles[K + 2]);
			}
		}
		TArray<FVector> WaterNormals;
		TArray<FLinearColor> WaterColors;
		WaterNormals.Init(FVector::UpVector, Water.Positions.Num());
		WaterColors.Init(Color, Water.Positions.Num());
		Mesh->CreateMeshSection_LinearColor(1, Water.Positions, WaterTriangles, WaterNormals, Water.UVs, WaterColors, TArray<FProcMeshTangent>(), false);
		Waters.Add(MoveTemp(Water));
	}

	for (int32 Section = 0; Section < Mesh->GetNumSections(); ++Section)
	{
		if (Override)
		{
			Mesh->SetMaterial(Section, Override);
		}
		else if (UMaterialInstanceDynamic* Material = MaterialFor(Color))
		{
			Mesh->SetMaterial(Section, Material);
		}
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TileMeshes.Add(Mesh);
}

void AHFBoardActor::AddDecoration(int32 Tile, const FString& Decoration, const FString& TerrainId, const FLinearColor& TerrainColor)
{
	FRandomStream Random(static_cast<int32>(Tile * 7919 + static_cast<int32>(GetTypeHash(ScenarioId) & 0x7FFFFF)));
	const FVector Base(TileCenters[Tile], TileHeights[Tile]);
	const float Spread = TileRadius * 0.55f;
	auto RandomSpot = [&]()
	{
		const float Angle = Random.FRandRange(0.0f, 2.0f * UE_PI);
		const float Distance = FMath::Sqrt(Random.FRand()) * Spread;
		return Base + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
	};
	auto Spin = [&]() { return FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f); };
	auto WindPhase = [&](const FVector& P) { return ::WindPhase(P, Random); };

	// Las formas básicas del motor miden 100 cm y tienen el pivote en el centro.
	if (Decoration == TEXT("trees"))
	{
		for (int32 I = 0; I < 7; ++I)
		{
			const FVector P = RandomSpot();
			const float S = Random.FRandRange(0.7f, 1.2f);
			AddShape(CylinderMesh, P + FVector(0, 0, 30 * S), FVector(0.12f, 0.12f, 0.6f) * S, FLinearColor(0.25f, 0.14f, 0.06f), TileComponents);
			UStaticMeshComponent* Crown = AddShape(ConeMesh, P + FVector(0, 0, 95 * S), FVector(0.55f, 0.55f, 0.9f) * S, Shade(TerrainColor, 0.7f), TileComponents);
			AddMotion(Crown, EMotion::Sway, P + FVector(0, 0, 50 * S), 4.0f, WindPhase(P));
		}
		// Un par de aves da vueltas sobre el bosque, una en cada sentido.
		for (int32 I = 0; I < 2; ++I)
		{
			AddCreature(ECreature::Bird, Base + FVector(0, 0, 240 + 50 * I), TileRadius * Random.FRandRange(0.45f, 0.75f),
				Random.FRandRange(0.4f, 0.6f) * (I == 0 ? 1.0f : -1.0f), Random.FRandRange(0.0f, 2.0f * UE_PI), FLinearColor(0.92f, 0.92f, 0.9f));
		}
	}
	else if (Decoration == TEXT("rocks") || Decoration == TEXT("crater"))
	{
		const bool bCrater = Decoration == TEXT("crater");
		const int32 Count = bCrater ? 10 : 6;
		for (int32 I = 0; I < Count; ++I)
		{
			const float S = Random.FRandRange(0.3f, 0.8f);
			AddShape(SphereMesh, RandomSpot() + FVector(0, 0, 10 * S), FVector(S, S * 0.8f, S * 0.5f), Shade(TerrainColor, Random.FRandRange(0.6f, 1.1f)), TileComponents, Spin());
		}
		if (bCrater)
		{
			AddEmitter(Base + FVector(0, 0, 10), EPuff::Dust, 0.35f, Spread * 0.7f); // remolinos de polvo
		}
		else if (TerrainId == TEXT("campo_obsidiana"))
		{
			AddEmitter(Base + FVector(0, 0, 15), EPuff::Ember, 0.9f, Spread * 0.8f); // la lava todavía humea
		}
	}
	else if (Decoration == TEXT("crystals"))
	{
		for (int32 I = 0; I < 6; ++I)
		{
			const float S = Random.FRandRange(0.5f, 1.1f);
			UStaticMeshComponent* Crystal = AddShape(ConeMesh, RandomSpot() + FVector(0, 0, 50 * S), FVector(0.18f, 0.18f, 1.0f) * S, Shade(TerrainColor, 1.4f), TileComponents,
				FRotator(Random.FRandRange(-15.0f, 15.0f), Random.FRandRange(0.0f, 360.0f), 0.0f));
			AddMotion(Crystal, EMotion::Twinkle, Crystal->GetRelativeLocation(), 0.07f, Random.FRandRange(0.0f, 6.0f));
		}
		if (TerrainId == TEXT("fumarola"))
		{
			AddEmitter(Base + FVector(0, 0, 10), EPuff::Steam, 0.3f, Spread * 0.7f);
		}
		else
		{
			AddEmitter(Base + FVector(0, 0, 40), EPuff::Glint, 0.25f, Spread * 0.8f);
		}
	}
	else if (Decoration == TEXT("volcano") || Decoration == TEXT("mountain"))
	{
		const bool bVolcano = Decoration == TEXT("volcano");
		AddShape(ConeMesh, Base + FVector(0, 0, 110), FVector(4.2f, 4.2f, 2.2f), Shade(TerrainColor, 0.8f), TileComponents);
		UStaticMeshComponent* Peak = AddShape(ConeMesh, Base + FVector(0, 0, 200), FVector(1.6f, 1.6f, 0.8f),
			bVolcano ? FLinearColor(1.0f, 0.25f, 0.02f) : FLinearColor(0.97f, 0.98f, 1.0f), TileComponents);
		if (bVolcano)
		{
			// La lava del cráter late: la cima respira, la luz parpadea y salen humo y chispas.
			AddMotion(Peak, EMotion::Twinkle, Peak->GetRelativeLocation(), 0.05f, 0.0f);
			UPointLightComponent* Glow = NewObject<UPointLightComponent>(this);
			Glow->SetupAttachment(Root);
			Glow->SetRelativeLocation(Base + FVector(0, 0, 280));
			Glow->SetLightColor(FLinearColor(1.0f, 0.35f, 0.05f));
			Glow->SetIntensity(20000.0f);
			Glow->SetAttenuationRadius(900.0f);
			Glow->RegisterComponent();
			DecorationLights.Add(Glow);
			Glows.Add({Glow, 20000.0f, static_cast<float>(Random.FRandRange(0.0f, 6.0f))});
			AddEmitter(Base + FVector(0, 0, 250), EPuff::Smoke, 0.28f, 25.0f);
			AddEmitter(Base + FVector(0, 0, 245), EPuff::Ember, 0.08f, 30.0f);
		}
		else
		{
			AddEmitter(Base + FVector(0, 0, 520), EPuff::Snow, 0.12f, Spread * 1.1f);
			// Cóndores planeando en rondas amplias, por encima de la cumbre.
			for (int32 I = 0; I < 2; ++I)
			{
				AddCreature(ECreature::Glider, Base + FVector(0, 0, 700 + 80 * I), TileRadius * Random.FRandRange(0.9f, 1.4f),
					Random.FRandRange(0.18f, 0.25f) * (I == 0 ? 1.0f : -1.0f), Random.FRandRange(0.0f, 2.0f * UE_PI), FLinearColor(0.04f, 0.04f, 0.04f));
			}
		}
	}
	else if (Decoration == TEXT("coral"))
	{
		for (int32 I = 0; I < 9; ++I)
		{
			const float S = Random.FRandRange(0.2f, 0.45f);
			const FLinearColor Coral = I % 2 == 0 ? FLinearColor(0.95f, 0.35f, 0.3f) : FLinearColor(0.95f, 0.65f, 0.75f);
			const FVector P = RandomSpot();
			UStaticMeshComponent* Polyp = AddShape(SphereMesh, P + FVector(0, 0, 8), FVector(S, S, S * 1.4f), Coral, TileComponents);
			AddMotion(Polyp, EMotion::Sway, P - FVector(0, 0, 20), 6.0f, WindPhase(P)); // se mece con la corriente
		}
	}
	else if (Decoration == TEXT("panels"))
	{
		for (int32 I = 0; I < 6; ++I)
		{
			UStaticMeshComponent* Panel = AddShape(CubeMesh, RandomSpot() + FVector(0, 0, 25), FVector(0.9f, 0.6f, 0.04f), FLinearColor(0.05f, 0.08f, 0.2f), TileComponents,
				FRotator(25.0f, 30.0f, 0.0f));
			AddMotion(Panel, EMotion::TrackSun, Panel->GetRelativeLocation(), 1.0f, 0.0f);
		}
	}
	else if (Decoration == TEXT("domes"))
	{
		for (int32 I = 0; I < 3; ++I)
		{
			const float S = Random.FRandRange(0.9f, 1.4f);
			UStaticMeshComponent* Dome = AddShape(SphereMesh, RandomSpot(), FVector(S, S, S * 0.8f), FLinearColor(0.85f, 0.9f, 0.92f), TileComponents);
			AddMotion(Dome, EMotion::Twinkle, Dome->GetRelativeLocation(), 0.025f, Random.FRandRange(0.0f, 6.0f)); // respiran despacio
		}
		// Un dron de mantenimiento recorre los domos.
		AddCreature(ECreature::Drone, Base + FVector(0, 0, 180), TileRadius * 0.4f, 1.0f, Random.FRandRange(0.0f, 6.0f), FLinearColor(0.8f, 0.82f, 0.85f));
	}
	else if (Decoration == TEXT("grass") || Decoration == TEXT("crops"))
	{
		const bool bCrops = Decoration == TEXT("crops");
		for (int32 I = 0; I < 14; ++I)
		{
			const float S = Random.FRandRange(0.25f, 0.45f);
			const FVector P = RandomSpot();
			UStaticMeshComponent* Blade = AddShape(bCrops ? CylinderMesh : ConeMesh, P + FVector(0, 0, 25 * S), FVector(0.12f, 0.12f, 0.5f) * S * 2.0f,
				bCrops ? FLinearColor(0.9f, 0.75f, 0.2f) : Shade(TerrainColor, 1.2f), TileComponents);
			AddMotion(Blade, EMotion::Sway, P, bCrops ? 9.0f : 12.0f, WindPhase(P));
		}
	}
	else if (Decoration == TEXT("vines"))
	{
		for (int32 Row = -2; Row <= 2; ++Row)
		{
			const FVector P = Base + FVector(Row * 55.0f, 0, 22);
			UStaticMeshComponent* Vine = AddShape(CubeMesh, P, FVector(0.12f, 3.6f, 0.45f), FLinearColor(0.25f, 0.4f, 0.12f), TileComponents);
			AddMotion(Vine, EMotion::Sway, P - FVector(0, 0, 22), 3.0f, WindPhase(P));
		}
	}
	else if (Decoration == TEXT("salt"))
	{
		for (int32 I = 0; I < 7; ++I)
		{
			AddShape(CubeMesh, RandomSpot() + FVector(0, 0, 4), FVector(Random.FRandRange(0.4f, 0.9f), Random.FRandRange(0.4f, 0.9f), 0.08f),
				FLinearColor(0.97f, 0.97f, 0.95f), TileComponents, Spin());
		}
		AddEmitter(Base + FVector(0, 0, 14), EPuff::Glint, 0.15f, Spread); // el salar reluce
	}
	else if (Decoration == TEXT("chimneys"))
	{
		for (int32 I = 0; I < 4; ++I)
		{
			const float S = Random.FRandRange(0.8f, 1.4f);
			const FVector P = RandomSpot();
			AddShape(CylinderMesh, P + FVector(0, 0, 70 * S), FVector(0.3f, 0.3f, 1.4f * S), FLinearColor(0.35f, 0.3f, 0.28f), TileComponents);
			AddEmitter(P + FVector(0, 0, 140 * S + 10), EPuff::Smoke, Random.FRandRange(0.45f, 0.7f), 8.0f);
		}
	}
	else if (Decoration == TEXT("gears"))
	{
		for (int32 I = 0; I < 3; ++I)
		{
			const float S = Random.FRandRange(0.8f, 1.3f);
			UStaticMeshComponent* Gear = AddShape(CylinderMesh, RandomSpot() + FVector(0, 0, 50 * S), FVector(S, S, 0.15f), FLinearColor(0.75f, 0.55f, 0.2f), TileComponents,
				FRotator(90.0f, Random.FRandRange(0.0f, 360.0f), 0.0f));
			// Engranajes vecinos giran en sentidos opuestos; los chicos, más rápido.
			AddMotion(Gear, EMotion::Spin, Gear->GetRelativeLocation(), (I % 2 == 0 ? 1.0f : -1.0f) * 45.0f / S, 0.0f);
		}
	}
	else if (Decoration == TEXT("banners"))
	{
		for (int32 I = 0; I < 4; ++I)
		{
			const FVector P = RandomSpot();
			AddShape(CylinderMesh, P + FVector(0, 0, 70), FVector(0.05f, 0.05f, 1.4f), FLinearColor(0.3f, 0.2f, 0.1f), TileComponents);
			UStaticMeshComponent* Flag = AddShape(CubeMesh, P + FVector(0, 22, 110), FVector(0.03f, 0.45f, 0.6f), Shade(TerrainColor, 1.4f), TileComponents);
			AddMotion(Flag, EMotion::Flutter, P + FVector(0, 0, 110), 18.0f, WindPhase(P));
		}
		// Cada telar suelta un dirigible de seda que rodea la ciudad entera.
		AddCreature(ECreature::Airship, FVector(0, 0, TileBaseHeight + TerrainHeightScale + 800 + 90 * (Tile % 3)), BoardExtent * Random.FRandRange(1.1f, 1.35f),
			0.06f * (Tile % 2 == 0 ? 1.0f : -1.0f), Random.FRandRange(0.0f, 2.0f * UE_PI), Shade(TerrainColor, 1.4f));
	}
}

void AHFBoardActor::AddMotion(UStaticMeshComponent* Component, EMotion Kind, const FVector& Pivot, float Amount, float Phase)
{
	FMotion Motion;
	Motion.Component = Component;
	Motion.Kind = Kind;
	Motion.Pivot = Pivot;
	Motion.Offset = Component->GetRelativeLocation() - Pivot;
	Motion.Rotation = Component->GetRelativeRotation().Quaternion();
	Motion.Scale = Component->GetRelativeScale3D();
	Motion.Amount = Amount;
	Motion.Phase = Phase;
	Motions.Add(Motion);
}

void AHFBoardActor::AddEmitter(const FVector& Origin, EPuff Kind, float Interval, float Spread)
{
	FEmitter Emitter;
	Emitter.Origin = Origin;
	Emitter.Kind = Kind;
	Emitter.Interval = Interval;
	Emitter.Spread = Spread;
	Emitter.NextTime = FMath::FRandRange(0.0f, Interval);
	Emitters.Add(Emitter);
}

void AHFBoardActor::AddBiomeAssets(int32 Tile, const FBiomeAssets& Biome)
{
	FRandomStream Random(static_cast<int32>(Tile * 104729 + static_cast<int32>(GetTypeHash(ScenarioId) & 0x7FFFFF)));
	const FVector Base(TileCenters[Tile], TileHeights[Tile]);
	const float Spread = TileRadius * 0.6f;
	auto Pick = [&](const TArray<UStaticMesh*>& From) { return From[Random.RandRange(0, From.Num() - 1)]; };
	auto RandomPoint = [&](float Reach)
	{
		const float Angle = Random.FRandRange(0.0f, 2.0f * UE_PI);
		const float Distance = FMath::Sqrt(Random.FRand()) * Spread * Reach;
		return Base + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
	};
	// Lugares ocupados (centro y radio), para que rocas, plantas y relieve no queden encimados.
	TArray<TPair<FVector2D, float>> Taken;
	auto FreeSpot = [&](float Clearance, FVector& OutSpot)
	{
		for (int32 Try = 0; Try < 10; ++Try)
		{
			const FVector Spot = RandomPoint(1.0f);
			bool bFree = true;
			for (const TPair<FVector2D, float>& Used : Taken)
			{
				bFree &= FVector2D::Distance(FVector2D(Spot), Used.Key) >= Clearance + Used.Value;
			}
			if (bFree)
			{
				Taken.Add({FVector2D(Spot), Clearance});
				OutSpot = Spot;
				return true;
			}
		}
		return false;
	};

	if (Biome.Landmark)
	{
		// El relieve (volcán, nevado, domo) ocupa el centro y se achica si no entra en la casilla.
		const FVector Extent = Biome.Landmark->GetBounds().BoxExtent;
		const float Footprint = FMath::Max(1.0f, static_cast<float>(FMath::Max(Extent.X, Extent.Y)));
		const float Scale = FMath::Min(FitScale(Biome.Landmark, LandmarkHeight), TileRadius * 0.8f / Footprint);
		const FRotator Rotation(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f);
		AddAsset(Biome.Landmark, GroundedLocation(Biome.Landmark, Base, Rotation, Scale), Rotation, Scale);
		Taken.Add({FVector2D(Base), Footprint * Scale * 0.8f});
	}

	for (int32 I = 0; Biome.Rocks.Num() && I < RocksPerTile; ++I)
	{
		FVector Spot;
		if (!FreeSpot(RockHeight * 0.8f, Spot))
		{
			continue;
		}
		UStaticMesh* Mesh = Pick(Biome.Rocks);
		const float Scale = FitScale(Mesh, RockHeight) * Random.FRandRange(0.6f, 1.4f);
		// Inclinadas y un poco hundidas, para que parezcan asentadas y no apoyadas.
		const FRotator Rotation(Random.FRandRange(-8.0f, 8.0f), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-8.0f, 8.0f));
		const float Sink = 0.15f * 2.0f * static_cast<float>(Mesh->GetBounds().BoxExtent.Z) * Scale;
		AddAsset(Mesh, GroundedLocation(Mesh, Spot - FVector(0, 0, Sink), Rotation, Scale), Rotation, Scale);
	}

	for (int32 I = 0; Biome.Plants.Num() && I < PlantsPerTile; ++I)
	{
		FVector Spot;
		if (!FreeSpot(PlantHeight * 0.3f, Spot))
		{
			continue;
		}
		UStaticMesh* Mesh = Pick(Biome.Plants);
		const float Scale = FitScale(Mesh, PlantHeight) * Random.FRandRange(0.75f, 1.25f);
		const FRotator Rotation(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f);
		UStaticMeshComponent* Plant = AddAsset(Mesh, GroundedLocation(Mesh, Spot, Rotation, Scale), Rotation, Scale);
		// Vaivén suave desde la base; el material de la planta suele sumar su propio viento en las hojas.
		AddMotion(Plant, EMotion::Sway, Spot, 1.5f, WindPhase(Spot, Random));
	}

	// Pasto, flores y piedritas: muchas copias en un solo componente instanciado por malla.
	for (UStaticMesh* Mesh : Biome.GroundCover)
	{
		UHierarchicalInstancedStaticMeshComponent* Cover = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		Cover->SetStaticMesh(Mesh);
		Cover->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cover->SetupAttachment(Root);
		Cover->RegisterComponent();
		const float Fit = FitScale(Mesh, GroundCoverHeight);
		const int32 Count = FMath::Max(1, GroundCoverPerTile / Biome.GroundCover.Num());
		for (int32 I = 0; I < Count; ++I)
		{
			const FRotator Rotation(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f);
			const float Scale = Fit * Random.FRandRange(0.7f, 1.3f);
			Cover->AddInstance(FTransform(Rotation, GroundedLocation(Mesh, RandomPoint(1.15f), Rotation, Scale), FVector(Scale)));
		}
		GroundCoverComponents.Add(Cover);
	}

	for (int32 I = 0; Biome.Birds.Num() && I < BirdsPerTile; ++I)
	{
		AddCreature(ECreature::Bird, Base + FVector(0, 0, 240 + 50 * I), TileRadius * Random.FRandRange(0.45f, 0.8f),
			Random.FRandRange(0.35f, 0.55f) * (I % 2 == 0 ? 1.0f : -1.0f), Random.FRandRange(0.0f, 2.0f * UE_PI), FLinearColor::White, Pick(Biome.Birds));
	}
	for (int32 I = 0; Biome.Fauna.Num() && I < FaunaPerTile; ++I)
	{
		AddCreature(ECreature::Walker, Base, Spread, 18.0f, Random.FRandRange(0.0f, 6.0f), FLinearColor::White, Pick(Biome.Fauna));
	}
}

void AHFBoardActor::AddCreature(ECreature Kind, const FVector& Center, float Radius, float Speed, float Phase, const FLinearColor& Color, UStaticMesh* Mesh)
{
	FCreature Creature;
	Creature.Kind = Kind;
	Creature.Center = Center;
	Creature.Radius = Radius;
	Creature.Speed = Speed;
	Creature.Phase = Phase;
	Creature.Location = Center;
	Creature.Target = Center;
	Creature.WaitUntil = FMath::FRandRange(0.0f, 3.0f);
	auto Part = [&](UStaticMesh* Shape, const FVector& Offset, const FVector& Scale, const FLinearColor& PartColor, EPart PartKind, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Component = AddShape(Shape, Center + Offset, Scale, PartColor, TileComponents, Rotation);
		Creature.Parts.Add({Component, PartKind, Offset, Rotation.Quaternion()});
	};

	if (Mesh)
	{
		// Modelo propio: una sola pieza escalada por su alto (los que caminan) o por su envergadura (las aves).
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const float Span = 2.0f * static_cast<float>(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
		const float Scale = Kind == ECreature::Walker ? FitScale(Mesh, FaunaHeight) : BirdSize / FMath::Max(1.0f, Span);
		const FQuat Facing = FRotator(0.0f, FaunaYawOffset, 0.0f).Quaternion();
		// Los que caminan se apoyan por la base; las aves se centran en su cuerpo.
		const FVector Pivot(Bounds.Origin.X, Bounds.Origin.Y, Kind == ECreature::Walker ? Bounds.Origin.Z - Bounds.BoxExtent.Z : Bounds.Origin.Z);
		UStaticMeshComponent* Component = AddAsset(Mesh, Center, Facing.Rotator(), Scale);
		Creature.Parts.Add({Component, EPart::Fixed, -Facing.RotateVector(Pivot * Scale), Facing});
	}
	else
	{
		// Las formas básicas miden 100 cm: una escala de 0.3 da 30 cm.
		switch (Kind)
		{
		case ECreature::Bird: // gaviota
			Part(SphereMesh, FVector::ZeroVector, FVector(0.3f, 0.1f, 0.1f), Color, EPart::Fixed);
			Part(CubeMesh, FVector(0, -16, 0), FVector(0.1f, 0.3f, 0.01f), Shade(Color, 0.65f), EPart::WingLeft);
			Part(CubeMesh, FVector(0, 16, 0), FVector(0.1f, 0.3f, 0.01f), Shade(Color, 0.65f), EPart::WingRight);
			break;
		case ECreature::Glider: // cóndor: alas largas con las puntas abiertas
			Part(SphereMesh, FVector::ZeroVector, FVector(0.45f, 0.16f, 0.14f), Color, EPart::Fixed);
			Part(SphereMesh, FVector(22, 0, 2), FVector(0.1f, 0.1f, 0.1f), FLinearColor(0.85f, 0.85f, 0.82f), EPart::Fixed); // collar blanco
			Part(CubeMesh, FVector(0, -34, 0), FVector(0.18f, 0.6f, 0.015f), Color, EPart::WingLeft);
			Part(CubeMesh, FVector(0, 34, 0), FVector(0.18f, 0.6f, 0.015f), Color, EPart::WingRight);
			break;
		case ECreature::Drone:
			Part(CubeMesh, FVector::ZeroVector, FVector(0.3f, 0.3f, 0.08f), Color, EPart::Fixed);
			for (int32 I = 0; I < 4; ++I)
			{
				const FVector Arm((I % 2 == 0 ? 1.0f : -1.0f) * 18.0f, (I < 2 ? 1.0f : -1.0f) * 18.0f, 6.0f);
				Part(CylinderMesh, Arm, FVector(0.16f, 0.16f, 0.01f), FLinearColor(0.1f, 0.1f, 0.1f), EPart::Rotor);
			}
			break;
		case ECreature::Airship:
			Part(SphereMesh, FVector::ZeroVector, FVector(2.6f, 0.9f, 0.9f), Color, EPart::Fixed);
			Part(CubeMesh, FVector(0, 0, -60), FVector(0.8f, 0.3f, 0.25f), FLinearColor(0.35f, 0.24f, 0.14f), EPart::Fixed); // góndola de madera
			Part(CubeMesh, FVector(-120, 0, 30), FVector(0.5f, 0.02f, 0.4f), Shade(Color, 0.7f), EPart::Fixed);
			Part(CubeMesh, FVector(-120, 0, 0), FVector(0.5f, 0.4f, 0.02f), Shade(Color, 0.7f), EPart::Fixed);
			Part(CylinderMesh, FVector(-50, 0, -60), FVector(0.4f, 0.4f, 0.02f), FLinearColor(0.75f, 0.55f, 0.2f), EPart::Rotor, FRotator(90.0f, 0.0f, 0.0f));
			break;
		case ECreature::Walker:
			return; // sin modelo propio no hay animales que caminen
		}
	}
	Creatures.Add(MoveTemp(Creature));
}

void AHFBoardActor::TickCreatures(float Now, float DeltaSeconds)
{
	for (FCreature& Creature : Creatures)
	{
		const float T = Now + Creature.Phase;
		const float Turn = Creature.Speed >= 0.0f ? 1.0f : -1.0f;
		FVector Location = Creature.Center;
		FQuat Body = FQuat::Identity;
		float Flap = 0.0f;
		switch (Creature.Kind)
		{
		case ECreature::Bird:
		case ECreature::Glider:
		case ECreature::Airship:
		{
			// Rondas: avanzan por la tangente del círculo y se inclinan hacia adentro.
			const float Angle = Creature.Phase + Creature.Speed * Now;
			const float Bob = Creature.Kind == ECreature::Airship ? 25.0f * FMath::Sin(T * 0.4f) : 20.0f * FMath::Sin(T * 0.7f);
			Location += FVector(FMath::Cos(Angle) * Creature.Radius, FMath::Sin(Angle) * Creature.Radius, Bob);
			const float Heading = FMath::RadiansToDegrees(Angle) + 90.0f * Turn;
			const float Bank = Creature.Kind == ECreature::Bird ? 20.0f : Creature.Kind == ECreature::Glider ? 12.0f : 0.0f;
			const float Pitch = Creature.Kind == ECreature::Airship ? 1.5f * FMath::Sin(T * 0.5f) : 0.0f;
			Body = FRotator(Pitch, Heading, -Bank * Turn).Quaternion();
			if (Creature.Kind == ECreature::Bird)
			{
				Flap = 35.0f * FMath::Sin(T * 11.0f);
			}
			else if (Creature.Kind == ECreature::Glider)
			{
				// Planean con las alas casi quietas y cada tanto dan unos aletazos.
				const float Burst = FMath::Pow(FMath::Max(0.0f, FMath::Sin(T * 0.25f)), 12.0f);
				Flap = 4.0f * FMath::Sin(T * 1.3f) + 30.0f * Burst * FMath::Sin(T * 9.0f);
			}
			break;
		}
		case ECreature::Drone:
			Location += FVector(FMath::Sin(T * 0.31f) * Creature.Radius, FMath::Sin(T * 0.23f + 1.3f) * Creature.Radius, 10.0f * FMath::Sin(T * 2.1f));
			Body = FRotator(3.0f * FMath::Sin(T * 1.7f), FMath::RadiansToDegrees(T * 0.2f), 3.0f * FMath::Cos(T * 1.3f)).Quaternion();
			break;
		case ECreature::Walker:
		{
			// Camina hasta un punto, se queda un rato (pastando) y elige otro.
			if (Now >= Creature.WaitUntil)
			{
				const FVector ToTarget = Creature.Target - Creature.Location;
				const float Distance = static_cast<float>(FVector2D(ToTarget).Size());
				if (Distance < 5.0f)
				{
					Creature.WaitUntil = Now + FMath::FRandRange(2.0f, 6.0f);
					const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
					const float Reach = FMath::Sqrt(FMath::FRand()) * Creature.Radius;
					Creature.Target = Creature.Center + FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, 0.0f);
				}
				else
				{
					// Primero gira hacia el destino y recién después avanza.
					const float Wanted = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
					const float Delta = FRotator::NormalizeAxis(Wanted - Creature.Yaw);
					Creature.Yaw += FMath::Clamp(Delta, -90.0f * DeltaSeconds, 90.0f * DeltaSeconds);
					if (FMath::Abs(Delta) < 30.0f)
					{
						const FVector Forward = FRotator(0.0f, Creature.Yaw, 0.0f).Vector();
						Creature.Location += Forward * FMath::Min(Creature.Speed * DeltaSeconds, Distance);
					}
				}
			}
			const bool bWalking = Now >= Creature.WaitUntil;
			Location = Creature.Location + FVector(0, 0, bWalking ? 1.5f * FMath::Abs(FMath::Sin(T * 6.0f)) : 0.0f);
			Body = FRotator(0.0f, Creature.Yaw, 0.0f).Quaternion();
			break;
		}
		}

		for (const FCreaturePart& Part : Creature.Parts)
		{
			UStaticMeshComponent* Component = Part.Component.Get();
			if (!Component)
			{
				continue;
			}
			FVector Offset = Part.Offset;
			FQuat Local = Part.Rotation;
			if (Part.Kind == EPart::WingLeft || Part.Kind == EPart::WingRight)
			{
				// Las dos alas suben y bajan juntas, cada una girando sobre su lado del cuerpo.
				const FQuat Hinge(FVector::ForwardVector, FMath::DegreesToRadians(Part.Kind == EPart::WingLeft ? -Flap : Flap));
				Offset = Hinge.RotateVector(Offset);
				Local = Hinge * Local;
			}
			else if (Part.Kind == EPart::Rotor)
			{
				Local = FQuat(Part.Rotation.GetUpVector(), Now * 40.0f) * Part.Rotation;
			}
			Component->SetRelativeLocationAndRotation(Location + Body.RotateVector(Offset), Body * Local);
		}
	}
}

void AHFBoardActor::SpawnPuff(const FEmitter& Emitter, float Now)
{
	if (Puffs.Num() >= MaxPuffs)
	{
		return;
	}
	// Reusa una forma escondida del depósito o crea una nueva.
	UStaticMeshComponent* Component = nullptr;
	for (UStaticMeshComponent* Candidate : PuffPool)
	{
		if (Candidate && !Candidate->IsVisible())
		{
			Component = Candidate;
			break;
		}
	}
	if (!Component)
	{
		TArray<TObjectPtr<UStaticMeshComponent>> Created;
		Component = AddShape(SphereMesh, FVector::ZeroVector, FVector(0.01f), FLinearColor::White, Created);
		Component->SetCastShadow(false);
		PuffPool.Add(Component);
	}

	const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
	const float Distance = FMath::Sqrt(FMath::FRand()) * Emitter.Spread;
	FPuff Puff;
	Puff.Component = Component;
	Puff.Location = Emitter.Origin + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
	Puff.Born = Now;
	FLinearColor Color = FLinearColor::White;
	switch (Emitter.Kind)
	{
	case EPuff::Smoke:
		Color = FLinearColor(0.22f, 0.21f, 0.2f);
		Puff.Velocity = FVector(WindDirection * 25.0f, FMath::FRandRange(70.0f, 110.0f));
		Puff.Life = FMath::FRandRange(2.5f, 3.5f);
		Puff.StartScale = 0.25f;
		Puff.EndScale = FMath::FRandRange(1.1f, 1.6f);
		break;
	case EPuff::Ember:
		Color = FMath::RandBool() ? FLinearColor(1.0f, 0.35f, 0.02f) : FLinearColor(1.0f, 0.75f, 0.1f);
		Puff.Velocity = FVector(FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(140.0f, 260.0f));
		Puff.Life = FMath::FRandRange(0.8f, 1.4f);
		Puff.StartScale = 0.08f;
		Puff.EndScale = 0.0f;
		break;
	case EPuff::Steam:
		Color = FLinearColor(0.92f, 0.92f, 0.88f);
		Puff.Velocity = FVector(WindDirection * 12.0f, FMath::FRandRange(50.0f, 80.0f));
		Puff.Life = FMath::FRandRange(1.8f, 2.6f);
		Puff.StartScale = 0.12f;
		Puff.EndScale = FMath::FRandRange(0.6f, 0.9f);
		break;
	case EPuff::Dust:
		Color = FLinearColor(0.62f, 0.36f, 0.2f);
		Puff.Velocity = FVector(WindDirection * 60.0f + FVector2D(FMath::FRandRange(-30.0f, 30.0f), FMath::FRandRange(-30.0f, 30.0f)), FMath::FRandRange(10.0f, 40.0f));
		Puff.Life = FMath::FRandRange(1.5f, 2.5f);
		Puff.StartScale = 0.15f;
		Puff.EndScale = 0.55f;
		break;
	case EPuff::Snow:
		Color = FLinearColor(0.97f, 0.98f, 1.0f);
		Puff.Velocity = FVector(WindDirection * 30.0f, FMath::FRandRange(-90.0f, -60.0f));
		Puff.Life = FMath::FRandRange(3.0f, 4.0f);
		Puff.StartScale = 0.06f;
		Puff.EndScale = 0.05f;
		break;
	case EPuff::Glint:
		Color = FLinearColor(1.0f, 1.0f, 0.95f);
		Puff.Velocity = FVector::ZeroVector;
		Puff.Life = 0.45f;
		Puff.StartScale = 0.0f;
		Puff.EndScale = 0.0f; // crece y se apaga (ver TickLife)
		break;
	}
	if (UMaterialInstanceDynamic* Material = MaterialFor(Color))
	{
		Component->SetMaterial(0, Material);
	}
	Component->SetRelativeLocation(Puff.Location);
	Component->SetRelativeScale3D(FVector(FMath::Max(Puff.StartScale, 0.01f)));
	Component->SetVisibility(true);
	Puffs.Add(Puff);
}

void AHFBoardActor::TickLife(float Now, float DeltaSeconds)
{
	// Decoraciones que se mueven.
	const FVector SunFlat = FVector(SunDirection.X, SunDirection.Y, FMath::Max(static_cast<float>(SunDirection.Z), 0.26f)).GetSafeNormal();
	for (FMotion& Motion : Motions)
	{
		UStaticMeshComponent* Component = Motion.Component.Get();
		if (!Component)
		{
			continue;
		}
		const float T = Now + Motion.Phase;
		switch (Motion.Kind)
		{
		case EMotion::Sway:
		{
			// Ráfagas: una onda lenta que sube y baja la intensidad del vaivén.
			const float Gust = 0.65f + 0.35f * FMath::Sin(Now * 0.35f + Motion.Phase * 0.5f);
			const float Angle = Motion.Amount * Gust * (FMath::Sin(T * 2.1f) + 0.3f * FMath::Sin(T * 5.3f));
			const FQuat Tilt(FVector(-WindDirection.Y, WindDirection.X, 0.0f), FMath::DegreesToRadians(Angle));
			Component->SetRelativeLocationAndRotation(Motion.Pivot + Tilt.RotateVector(Motion.Offset), Tilt * Motion.Rotation);
			break;
		}
		case EMotion::Spin:
		{
			const FQuat Turn(Motion.Rotation.GetUpVector(), FMath::DegreesToRadians(Motion.Amount * Now));
			Component->SetRelativeRotation(Turn * Motion.Rotation);
			break;
		}
		case EMotion::Flutter:
		{
			const float Angle = Motion.Amount * (FMath::Sin(T * 3.4f) + 0.5f * FMath::Sin(T * 7.9f));
			const FQuat Wave(FVector::UpVector, FMath::DegreesToRadians(Angle));
			Component->SetRelativeLocationAndRotation(Motion.Pivot + Wave.RotateVector(Motion.Offset), Wave * Motion.Rotation);
			break;
		}
		case EMotion::Twinkle:
			Component->SetRelativeScale3D(Motion.Scale * (1.0f + Motion.Amount * FMath::Sin(T * 2.6f)));
			break;
		case EMotion::TrackSun:
		{
			// El panel apunta al sol; de noche se acuesta mirando al cielo.
			const FVector Target = FMath::Lerp(SunFlat, FVector::UpVector, Night).GetSafeNormal();
			const FQuat Wanted = FRotationMatrix::MakeFromZ(Target).ToQuat();
			const FQuat Current = Component->GetRelativeRotation().Quaternion();
			Component->SetRelativeRotation(FQuat::Slerp(Current, Wanted, FMath::Clamp(DeltaSeconds * 0.8f, 0.0f, 1.0f)));
			break;
		}
		}
	}

	TickCreatures(Now, DeltaSeconds);

	// Luces que laten (lava).
	for (const FGlow& Glow : Glows)
	{
		if (UPointLightComponent* Light = Glow.Light.Get())
		{
			const float Flicker = 0.75f + 0.2f * FMath::Sin(Now * 2.2f + Glow.Phase) + 0.08f * FMath::Sin(Now * 13.0f + Glow.Phase * 3.0f);
			Light->SetIntensity(Glow.Intensity * Flicker * (1.0f + Night * 0.6f));
		}
	}

	// Partículas.
	for (FEmitter& Emitter : Emitters)
	{
		if (Now >= Emitter.NextTime)
		{
			SpawnPuff(Emitter, Now);
			Emitter.NextTime = Now + Emitter.Interval * FMath::FRandRange(0.7f, 1.3f);
		}
	}
	for (int32 I = Puffs.Num() - 1; I >= 0; --I)
	{
		FPuff& Puff = Puffs[I];
		UStaticMeshComponent* Component = Puff.Component.Get();
		const float Alpha = (Now - Puff.Born) / Puff.Life;
		if (!Component || Alpha >= 1.0f)
		{
			if (Component)
			{
				Component->SetVisibility(false);
			}
			Puffs.RemoveAtSwap(I);
			continue;
		}
		Puff.Location += Puff.Velocity * DeltaSeconds;
		Puff.Velocity *= FMath::Pow(0.6f, DeltaSeconds); // el aire las frena
		// Al final se achican hasta desaparecer (los materiales son opacos: no se pueden desvanecer).
		float Scale = FMath::Lerp(Puff.StartScale, Puff.EndScale, Alpha) * FMath::Min(1.0f, (1.0f - Alpha) * 4.0f);
		if (Puff.StartScale == 0.0f && Puff.EndScale == 0.0f)
		{
			Scale = 0.07f * FMath::Sin(Alpha * UE_PI); // destello
		}
		Component->SetRelativeLocation(Puff.Location);
		Component->SetRelativeScale3D(FVector(FMath::Max(Scale, 0.001f)));
	}

	// Olas.
	for (FWater& Water : Waters)
	{
		UProceduralMeshComponent* Mesh = Water.Mesh.Get();
		if (!Mesh)
		{
			continue;
		}
		TArray<FVector> Positions = Water.Positions;
		TArray<FVector> Normals;
		TArray<FLinearColor> Colors;
		Normals.SetNum(Positions.Num());
		Colors.SetNum(Positions.Num());
		for (int32 V = 0; V < Positions.Num(); ++V)
		{
			const float X = Water.Center.X + Positions[V].X;
			const float Y = Water.Center.Y + Positions[V].Y;
			const float A1 = 0.011f * X + 1.6f * Now;
			const float A2 = 0.015f * Y - 1.2f * Now;
			const float A3 = 0.019f * (X + Y) + 2.4f * Now;
			const float H = FMath::Sin(A1) + 0.6f * FMath::Sin(A2) + 0.35f * FMath::Sin(A3);
			const float W = Water.Weights[V] * WaveAmplitude;
			Positions[V].Z = Water.Height + H * W;
			const float DX = (0.011f * FMath::Cos(A1) + 0.35f * 0.019f * FMath::Cos(A3)) * W;
			const float DY = (0.6f * 0.015f * FMath::Cos(A2) + 0.35f * 0.019f * FMath::Cos(A3)) * W;
			Normals[V] = FVector(-DX, -DY, 1.0f).GetSafeNormal();
			// Las crestas se ven un poco más claras.
			const float Crest = FMath::Clamp(H * 0.12f, -0.1f, 0.2f);
			Colors[V] = FLinearColor(Water.Color.R * (1.0f + Crest), Water.Color.G * (1.0f + Crest), Water.Color.B * (1.0f + Crest), 1.0f);
		}
		Mesh->UpdateMeshSection_LinearColor(1, Positions, Normals, Water.UVs, Colors, TArray<FProcMeshTangent>());
	}

	// Luces de las construcciones: se prenden al anochecer.
	for (int32 I = 0; I < BuildingLights.Num(); ++I)
	{
		if (UPointLightComponent* Light = BuildingLights[I])
		{
			Light->SetIntensity(Night * BuildingLightPower[I]);
		}
	}
}

void AHFBoardActor::SetDayState(const FVector& InSunDirection, float InNight)
{
	SunDirection = InSunDirection;
	Night = InNight;
}

void AHFBoardActor::RefreshPieces(const FHFMatch& Match)
{
	ClearComponents(PieceComponents);
	for (UPointLightComponent* Light : BuildingLights)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	BuildingLights.Reset();
	BuildingLightPower.Reset();
	AnimatedPieces.RemoveAll([](const FAnimatedPiece& Piece) { return !Piece.Component.IsValid(); });

	const hf::Game& Game = Match.GetGame();
	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
	TSet<int64> Current;

	auto Animate = [&](const TArray<UStaticMeshComponent*>& Parts, int64 Key)
	{
		Current.Add(Key);
		if (ShownPieces.Contains(Key))
		{
			return;
		}
		for (UStaticMeshComponent* Part : Parts)
		{
			FAnimatedPiece Piece;
			Piece.Component = Part;
			Piece.Target = Part->GetRelativeLocation();
			Piece.StartTime = Now;
			Part->SetRelativeLocation(Piece.Target + FVector(0, 0, PieceDropHeight));
			AnimatedPieces.Add(Piece);
		}
	};

	const auto& Routes = Game.RouteOwners();
	for (int32 E = 0; E < static_cast<int32>(Routes.size()); ++E)
	{
		const int32 RouteOwner = Routes[static_cast<size_t>(E)];
		if (RouteOwner < 0)
		{
			continue;
		}
		const FVector A = VertexWorld(EdgeVertices[E].X);
		const FVector B = VertexWorld(EdgeVertices[E].Y);
		const FVector Mid = (A + B) * 0.5f + FVector(0, 0, 12);
		const float Length = FVector::Dist2D(A, B) * 0.62f;
		const FRotator Rotation = (B - A).GetSafeNormal2D().Rotation();
		const FLinearColor Color = PlayerColors[RouteOwner % PlayerColors.Num()];
		UStaticMeshComponent* Plank = AddShape(CubeMesh, Mid, FVector(Length / 100.0f, 0.22f, 0.18f), Color, PieceComponents, Rotation);
		Animate({Plank}, PieceKey(1, E, 0));
	}

	const auto& Buildings = Game.Buildings();
	for (int32 V = 0; V < static_cast<int32>(Buildings.size()); ++V)
	{
		const hf::Building& Building = Buildings[static_cast<size_t>(V)];
		if (Building.owner < 0)
		{
			continue;
		}
		const FVector P = VertexWorld(V);
		const FLinearColor Color = PlayerColors[Building.owner % PlayerColors.Num()];
		const FLinearColor Roof = Shade(Color, 0.6f);
		TArray<UStaticMeshComponent*> Parts;
		if (Building.level == 1)
		{
			Parts.Add(AddShape(CubeMesh, P + FVector(0, 0, 30), FVector(0.55f, 0.55f, 0.6f), Color, PieceComponents));
			Parts.Add(AddShape(ConeMesh, P + FVector(0, 0, 80), FVector(0.7f, 0.7f, 0.45f), Roof, PieceComponents, FRotator(0, 45, 0)));
		}
		else
		{
			Parts.Add(AddShape(CylinderMesh, P + FVector(0, 0, 45), FVector(0.85f, 0.85f, 0.9f), Color, PieceComponents));
			Parts.Add(AddShape(CylinderMesh, P + FVector(0, 0, 120), FVector(0.4f, 0.4f, 0.7f), Color, PieceComponents));
			Parts.Add(AddShape(ConeMesh, P + FVector(0, 0, 180), FVector(0.55f, 0.55f, 0.5f), Roof, PieceComponents));
		}
		Animate(Parts, PieceKey(0, V, Building.level));

		// Ventanas encendidas de noche: luz cálida, sin sombras para que sea barata.
		UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
		Lamp->SetupAttachment(Root);
		Lamp->SetRelativeLocation(P + FVector(0, 0, Building.level == 1 ? 110.0f : 220.0f));
		Lamp->SetLightColor(FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.68f, 0.32f), Color, 0.15f));
		Lamp->SetAttenuationRadius(Building.level == 1 ? 380.0f : 520.0f);
		Lamp->SetCastShadows(false);
		Lamp->SetIntensity(0.0f);
		Lamp->RegisterComponent();
		BuildingLights.Add(Lamp);
		BuildingLightPower.Add(Building.level == 1 ? 3500.0f : 6000.0f);
	}
	ShownPieces = MoveTemp(Current);
}

void AHFBoardActor::RefreshStatuses(const FHFMatch& Match)
{
	ClearComponents(StatusComponents);
	const hf::Game& Game = Match.GetGame();
	const auto& Tiles = Game.GetBoard().Tiles();
	bool bTerrainChanged = false;
	for (int32 T = 0; T < static_cast<int32>(Tiles.size()); ++T)
	{
		const hf::Tile& Tile = Tiles[static_cast<size_t>(T)];
		if (TileTerrains.IsValidIndex(T) && TileTerrains[T] != Tile.terrain)
		{
			bTerrainChanged = true;
		}
		const FVector Top = TileWorld(T);
		if (Tile.blockedRounds > 0)
		{
			AddShape(SphereMesh, Top + FVector(0, 0, 120), FVector(6.0f, 6.0f, 1.6f), FLinearColor(0.55f, 0.32f, 0.18f), StatusComponents);
		}
		if (Tile.frostRounds > 0)
		{
			AddShape(CylinderMesh, Top + FVector(0, 0, 4), FVector(6.2f, 6.2f, 0.06f), FLinearColor(0.85f, 0.95f, 1.0f), StatusComponents);
		}
		if (Game.BoostedTile() == T)
		{
			AddShape(CylinderMesh, Top + FVector(0, 0, 8), FVector(2.0f, 2.0f, 0.12f), FLinearColor(1.0f, 0.8f, 0.1f), StatusComponents);
		}
	}
	if (bTerrainChanged)
	{
		// Lava o desacople: se rearma el tablero entero (es barato y mantiene todo coherente).
		Rebuild(Match);
	}
}

void AHFBoardActor::FlashTiles(const TArray<int32>& Tiles, const FLinearColor& Color)
{
	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
	for (int32 T : Tiles)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(TileWorld(T) + FVector(0, 0, 150));
		Light->SetLightColor(Color);
		Light->SetAttenuationRadius(TileRadius * 1.4f);
		Light->SetIntensity(0.0f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		FFlash Flash;
		Flash.Light = Light;
		Flash.StartTime = Now;
		Flash.PeakIntensity = 40000.0f;
		Flashes.Add(Flash);
	}
}

void AHFBoardActor::SetHighlights(const TArray<int32>& Vertices, const TArray<int32>& Edges, const TArray<int32>& Tiles, const FLinearColor& Color)
{
	ClearHighlights();
	for (int32 V : Vertices)
	{
		AddShape(SphereMesh, VertexWorld(V) + FVector(0, 0, 25), FVector(0.4f), Color, HighlightComponents);
	}
	for (int32 E : Edges)
	{
		const FVector A = VertexWorld(EdgeVertices[E].X);
		const FVector B = VertexWorld(EdgeVertices[E].Y);
		AddShape(CubeMesh, (A + B) * 0.5f + FVector(0, 0, 15), FVector(FVector::Dist2D(A, B) * 0.5f / 100.0f, 0.1f, 0.1f), Color,
			HighlightComponents, (B - A).GetSafeNormal2D().Rotation());
	}
	for (int32 T : Tiles)
	{
		AddShape(CylinderMesh, TileWorld(T) + FVector(0, 0, 10), FVector(1.5f, 1.5f, 0.1f), Color, HighlightComponents);
	}
}

void AHFBoardActor::ClearHighlights()
{
	ClearComponents(HighlightComponents);
}

void AHFBoardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());

	for (int32 I = AnimatedPieces.Num() - 1; I >= 0; --I)
	{
		FAnimatedPiece& Piece = AnimatedPieces[I];
		USceneComponent* Component = Piece.Component.Get();
		if (!Component)
		{
			AnimatedPieces.RemoveAtSwap(I);
			continue;
		}
		const float Alpha = FMath::Clamp((Now - Piece.StartTime) / PieceDropSeconds, 0.0f, 1.0f);
		// Caída con un pequeño rebote al final.
		const float Eased = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
		const float Bounce = Alpha < 1.0f ? FMath::Sin(Alpha * UE_PI) * 0.06f : 0.0f;
		Component->SetRelativeLocation(Piece.Target + FVector(0, 0, PieceDropHeight * (1.0f - Eased) + Bounce * 100.0f));
		if (Alpha >= 1.0f)
		{
			AnimatedPieces.RemoveAtSwap(I);
		}
	}

	for (int32 I = Flashes.Num() - 1; I >= 0; --I)
	{
		FFlash& Flash = Flashes[I];
		UPointLightComponent* Light = Flash.Light.Get();
		const float Alpha = (Now - Flash.StartTime) / FlashSeconds;
		if (!Light || Alpha >= 1.0f)
		{
			if (Light)
			{
				Light->DestroyComponent();
			}
			Flashes.RemoveAtSwap(I);
			continue;
		}
		Light->SetIntensity(Flash.PeakIntensity * FMath::Sin(Alpha * UE_PI));
	}

	// Los resaltados laten suavemente.
	const float Pulse = 1.0f + 0.15f * FMath::Sin(Now * 5.0f);
	for (UStaticMeshComponent* Highlight : HighlightComponents)
	{
		if (Highlight && Highlight->GetStaticMesh() == SphereMesh)
		{
			Highlight->SetRelativeScale3D(FVector(0.4f * Pulse));
		}
	}

	TickLife(Now, DeltaSeconds);
}

FVector AHFBoardActor::VertexWorld(int32 Vertex) const
{
	if (!VertexPositions.IsValidIndex(Vertex))
	{
		return GetActorLocation();
	}
	return FVector(VertexPositions[Vertex], VertexHeights[Vertex]);
}

FVector AHFBoardActor::EdgeWorld(int32 Edge) const
{
	if (!EdgeVertices.IsValidIndex(Edge))
	{
		return GetActorLocation();
	}
	return (VertexWorld(EdgeVertices[Edge].X) + VertexWorld(EdgeVertices[Edge].Y)) * 0.5f;
}

FVector AHFBoardActor::TileWorld(int32 Tile) const
{
	if (!TileCenters.IsValidIndex(Tile))
	{
		return GetActorLocation();
	}
	return FVector(TileCenters[Tile], TileHeights[Tile]);
}

int32 AHFBoardActor::FindNearestVertex(const FVector& WorldPoint, float MaxDistance) const
{
	const FVector2D Local(GetActorTransform().InverseTransformPosition(WorldPoint));
	int32 Best = -1;
	float BestDistance = MaxDistance;
	for (int32 V = 0; V < VertexPositions.Num(); ++V)
	{
		const float Distance = FVector2D::Distance(Local, VertexPositions[V]);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = V;
		}
	}
	return Best;
}

int32 AHFBoardActor::FindNearestEdge(const FVector& WorldPoint, float MaxDistance) const
{
	const FVector2D Local(GetActorTransform().InverseTransformPosition(WorldPoint));
	int32 Best = -1;
	float BestDistance = MaxDistance;
	for (int32 E = 0; E < EdgeVertices.Num(); ++E)
	{
		const FVector2D Mid = (VertexPositions[EdgeVertices[E].X] + VertexPositions[EdgeVertices[E].Y]) * 0.5f;
		const float Distance = FVector2D::Distance(Local, Mid);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = E;
		}
	}
	return Best;
}

int32 AHFBoardActor::FindTileAt(const FVector& WorldPoint) const
{
	const FVector2D Local(GetActorTransform().InverseTransformPosition(WorldPoint));
	int32 Best = -1;
	float BestDistance = TileRadius;
	for (int32 T = 0; T < TileCenters.Num(); ++T)
	{
		const float Distance = FVector2D::Distance(Local, TileCenters[T]);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = T;
		}
	}
	return Best;
}
