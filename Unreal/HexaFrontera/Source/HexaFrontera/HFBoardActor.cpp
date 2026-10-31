#include "HFBoardActor.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HFMatch.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const float PieceDropHeight = 600.0f;
const float PieceDropSeconds = 0.45f;
const float FlashSeconds = 1.6f;

int64 PieceKey(int32 Kind, int32 Index, int32 Level)
{
	return (static_cast<int64>(Kind) << 40) | (static_cast<int64>(Index) << 8) | Level;
}

FLinearColor Shade(const FLinearColor& Color, float Factor)
{
	return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, 1.0f);
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

UMaterialInterface* AHFBoardActor::TerrainMaterialOverride(const FString& TerrainId) const
{
	for (const FHFTerrainVisual& Visual : TerrainVisuals)
	{
		if (Visual.Scenario == ScenarioId && Visual.Terrain == TerrainId && !Visual.Material.IsNull())
		{
			return Visual.Material.LoadSynchronous();
		}
	}
	return nullptr;
}

UStaticMesh* AHFBoardActor::TerrainMeshOverride(const FString& TerrainId) const
{
	for (const FHFTerrainVisual& Visual : TerrainVisuals)
	{
		if (Visual.Scenario == ScenarioId && Visual.Terrain == TerrainId && !Visual.Mesh.IsNull())
		{
			return Visual.Mesh.LoadSynchronous();
		}
	}
	return nullptr;
}

UStaticMeshComponent* AHFBoardActor::AddShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color,
	TArray<TObjectPtr<UStaticMeshComponent>>& Owner, const FRotator& Rotation)
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
	Owner.Add(Component);
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
		if (UStaticMesh* Replacement = TerrainMeshOverride(TerrainId))
		{
			const float Scale = TileRadius * (1.0f - TileGap) / 100.0f;
			UStaticMeshComponent* Custom = AddShape(Replacement, FVector(TileCenters[T], 0.0f),
				FVector(Scale, Scale, TileHeights[T] / 100.0f), Color, TileComponents);
			if (UMaterialInterface* Override = TerrainMaterialOverride(TerrainId))
			{
				Custom->SetMaterial(0, Override);
			}
			else
			{
				Custom->EmptyOverrideMaterials(); // usa los materiales importados con la malla
			}
		}
		else
		{
			BuildTileMesh(T, Corners, TileHeights[T], Color, TerrainMaterialOverride(TerrainId));
		}
		AddDecoration(T, FHFMatch::ToFString(Terrain.decoration), Color);
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

void AHFBoardActor::BuildTileMesh(int32 Tile, const TArray<FVector2D>& Corners, float Height, const FLinearColor& Color, UMaterialInterface* Override)
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

	// Tapa superior
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
	if (Override)
	{
		Mesh->SetMaterial(0, Override);
	}
	else if (UMaterialInstanceDynamic* Material = MaterialFor(Color))
	{
		Mesh->SetMaterial(0, Material);
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TileMeshes.Add(Mesh);
}

void AHFBoardActor::AddDecoration(int32 Tile, const FString& Decoration, const FLinearColor& TerrainColor)
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

	// Las formas básicas del motor miden 100 cm y tienen el pivote en el centro.
	if (Decoration == TEXT("trees"))
	{
		for (int32 I = 0; I < 7; ++I)
		{
			const FVector P = RandomSpot();
			const float S = Random.FRandRange(0.7f, 1.2f);
			AddShape(CylinderMesh, P + FVector(0, 0, 30 * S), FVector(0.12f, 0.12f, 0.6f) * S, FLinearColor(0.25f, 0.14f, 0.06f), TileComponents);
			AddShape(ConeMesh, P + FVector(0, 0, 95 * S), FVector(0.55f, 0.55f, 0.9f) * S, Shade(TerrainColor, 0.7f), TileComponents);
		}
	}
	else if (Decoration == TEXT("rocks") || Decoration == TEXT("crater"))
	{
		const int32 Count = Decoration == TEXT("crater") ? 10 : 6;
		for (int32 I = 0; I < Count; ++I)
		{
			const float S = Random.FRandRange(0.3f, 0.8f);
			AddShape(SphereMesh, RandomSpot() + FVector(0, 0, 10 * S), FVector(S, S * 0.8f, S * 0.5f), Shade(TerrainColor, Random.FRandRange(0.6f, 1.1f)), TileComponents, Spin());
		}
	}
	else if (Decoration == TEXT("crystals"))
	{
		for (int32 I = 0; I < 6; ++I)
		{
			const float S = Random.FRandRange(0.5f, 1.1f);
			AddShape(ConeMesh, RandomSpot() + FVector(0, 0, 50 * S), FVector(0.18f, 0.18f, 1.0f) * S, Shade(TerrainColor, 1.4f), TileComponents,
				FRotator(Random.FRandRange(-15.0f, 15.0f), Random.FRandRange(0.0f, 360.0f), 0.0f));
		}
	}
	else if (Decoration == TEXT("volcano") || Decoration == TEXT("mountain"))
	{
		const bool bVolcano = Decoration == TEXT("volcano");
		AddShape(ConeMesh, Base + FVector(0, 0, 110), FVector(4.2f, 4.2f, 2.2f), Shade(TerrainColor, 0.8f), TileComponents);
		AddShape(ConeMesh, Base + FVector(0, 0, 200), FVector(1.6f, 1.6f, 0.8f),
			bVolcano ? FLinearColor(1.0f, 0.25f, 0.02f) : FLinearColor(0.97f, 0.98f, 1.0f), TileComponents);
		if (bVolcano)
		{
			UPointLightComponent* Glow = NewObject<UPointLightComponent>(this);
			Glow->SetupAttachment(Root);
			Glow->SetRelativeLocation(Base + FVector(0, 0, 280));
			Glow->SetLightColor(FLinearColor(1.0f, 0.35f, 0.05f));
			Glow->SetIntensity(20000.0f);
			Glow->SetAttenuationRadius(900.0f);
			Glow->RegisterComponent();
			DecorationLights.Add(Glow);
		}
	}
	else if (Decoration == TEXT("coral"))
	{
		for (int32 I = 0; I < 9; ++I)
		{
			const float S = Random.FRandRange(0.2f, 0.45f);
			const FLinearColor Coral = I % 2 == 0 ? FLinearColor(0.95f, 0.35f, 0.3f) : FLinearColor(0.95f, 0.65f, 0.75f);
			AddShape(SphereMesh, RandomSpot() + FVector(0, 0, 8), FVector(S, S, S * 1.4f), Coral, TileComponents);
		}
	}
	else if (Decoration == TEXT("panels"))
	{
		for (int32 I = 0; I < 6; ++I)
		{
			AddShape(CubeMesh, RandomSpot() + FVector(0, 0, 25), FVector(0.9f, 0.6f, 0.04f), FLinearColor(0.05f, 0.08f, 0.2f), TileComponents,
				FRotator(25.0f, 30.0f, 0.0f));
		}
	}
	else if (Decoration == TEXT("domes"))
	{
		for (int32 I = 0; I < 3; ++I)
		{
			const float S = Random.FRandRange(0.9f, 1.4f);
			AddShape(SphereMesh, RandomSpot(), FVector(S, S, S * 0.8f), FLinearColor(0.85f, 0.9f, 0.92f), TileComponents);
		}
	}
	else if (Decoration == TEXT("grass") || Decoration == TEXT("crops"))
	{
		const bool bCrops = Decoration == TEXT("crops");
		for (int32 I = 0; I < 14; ++I)
		{
			const float S = Random.FRandRange(0.25f, 0.45f);
			AddShape(bCrops ? CylinderMesh : ConeMesh, RandomSpot() + FVector(0, 0, 25 * S), FVector(0.12f, 0.12f, 0.5f) * S * 2.0f,
				bCrops ? FLinearColor(0.9f, 0.75f, 0.2f) : Shade(TerrainColor, 1.2f), TileComponents);
		}
	}
	else if (Decoration == TEXT("vines"))
	{
		for (int32 Row = -2; Row <= 2; ++Row)
		{
			AddShape(CubeMesh, Base + FVector(Row * 55.0f, 0, 22), FVector(0.12f, 3.6f, 0.45f), FLinearColor(0.25f, 0.4f, 0.12f), TileComponents);
		}
	}
	else if (Decoration == TEXT("salt"))
	{
		for (int32 I = 0; I < 7; ++I)
		{
			AddShape(CubeMesh, RandomSpot() + FVector(0, 0, 4), FVector(Random.FRandRange(0.4f, 0.9f), Random.FRandRange(0.4f, 0.9f), 0.08f),
				FLinearColor(0.97f, 0.97f, 0.95f), TileComponents, Spin());
		}
	}
	else if (Decoration == TEXT("chimneys"))
	{
		for (int32 I = 0; I < 4; ++I)
		{
			const float S = Random.FRandRange(0.8f, 1.4f);
			AddShape(CylinderMesh, RandomSpot() + FVector(0, 0, 70 * S), FVector(0.3f, 0.3f, 1.4f * S), FLinearColor(0.35f, 0.3f, 0.28f), TileComponents);
		}
	}
	else if (Decoration == TEXT("gears"))
	{
		for (int32 I = 0; I < 3; ++I)
		{
			const float S = Random.FRandRange(0.8f, 1.3f);
			AddShape(CylinderMesh, RandomSpot() + FVector(0, 0, 50 * S), FVector(S, S, 0.15f), FLinearColor(0.75f, 0.55f, 0.2f), TileComponents,
				FRotator(90.0f, Random.FRandRange(0.0f, 360.0f), 0.0f));
		}
	}
	else if (Decoration == TEXT("banners"))
	{
		for (int32 I = 0; I < 4; ++I)
		{
			const FVector P = RandomSpot();
			AddShape(CylinderMesh, P + FVector(0, 0, 70), FVector(0.05f, 0.05f, 1.4f), FLinearColor(0.3f, 0.2f, 0.1f), TileComponents);
			AddShape(CubeMesh, P + FVector(0, 22, 110), FVector(0.03f, 0.45f, 0.6f), Shade(TerrainColor, 1.4f), TileComponents);
		}
	}
}

void AHFBoardActor::RefreshPieces(const FHFMatch& Match)
{
	ClearComponents(PieceComponents);
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
		const int32 Owner = Routes[static_cast<size_t>(E)];
		if (Owner < 0)
		{
			continue;
		}
		const FVector A = VertexWorld(EdgeVertices[E].X);
		const FVector B = VertexWorld(EdgeVertices[E].Y);
		const FVector Mid = (A + B) * 0.5f + FVector(0, 0, 12);
		const float Length = FVector::Dist2D(A, B) * 0.62f;
		const FRotator Rotation = (B - A).GetSafeNormal2D().Rotation();
		const FLinearColor Color = PlayerColors[Owner % PlayerColors.Num()];
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
