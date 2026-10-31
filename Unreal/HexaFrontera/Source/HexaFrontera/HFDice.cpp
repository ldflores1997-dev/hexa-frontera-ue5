#include "HFDice.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Caras del dado en su espacio local: los opuestos suman 7.
struct FFace
{
	FVector Normal;
	int32 Value;
};
const FFace Faces[6] = {
	{FVector(0, 0, 1), 1},
	{FVector(0, 0, -1), 6},
	{FVector(1, 0, 0), 2},
	{FVector(-1, 0, 0), 5},
	{FVector(0, 1, 0), 3},
	{FVector(0, -1, 0), 4},
};

// Posición de los puntos en una cara, en unidades de la grilla de 3 x 3 (-1, 0, 1).
TArray<FVector2D> PipLayout(int32 Value)
{
	switch (Value)
	{
	case 1: return {FVector2D(0, 0)};
	case 2: return {FVector2D(-1, -1), FVector2D(1, 1)};
	case 3: return {FVector2D(-1, -1), FVector2D(0, 0), FVector2D(1, 1)};
	case 4: return {FVector2D(-1, -1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(1, 1)};
	case 5: return {FVector2D(-1, -1), FVector2D(-1, 1), FVector2D(0, 0), FVector2D(1, -1), FVector2D(1, 1)};
	default: return {FVector2D(-1, -1), FVector2D(-1, 0), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(1, 0), FVector2D(1, 1)};
	}
}

const float TrayFloorZ = 10.0f;
const float RimHeight = 70.0f;
const float LidHeight = 320.0f;
}

AHFDice::AHFDice()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
}

UMaterialInstanceDynamic* AHFDice::MakeMaterial(const FLinearColor& Color)
{
	UMaterialInterface* Base = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")).TryLoad());
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Materials.Add(Material);
	return Material;
}

UStaticMeshComponent* AHFDice::AddPart(UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Scale, UMaterialInstanceDynamic* Material)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetStaticMesh(Mesh);
	Part->SetupAttachment(Parent);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Scale);
	if (Material)
	{
		Part->SetMaterial(0, Material);
	}
	Part->RegisterComponent();
	return Part;
}

void AHFDice::BuildTray()
{
	UMaterialInstanceDynamic* Felt = MakeMaterial(FLinearColor(0.03f, 0.16f, 0.09f));
	UMaterialInstanceDynamic* Wood = MakeMaterial(FLinearColor(0.32f, 0.17f, 0.07f));
	const float HalfX = TraySize.X * 0.5f;
	const float HalfY = TraySize.Y * 0.5f;
	const float Rim = 18.0f;

	auto Solid = [this](UStaticMeshComponent* Part)
	{
		Part->SetCollisionProfileName(TEXT("BlockAll"));
		TrayParts.Add(Part);
		return Part;
	};
	Solid(AddPart(CubeMesh, Root, FVector(0, 0, TrayFloorZ - 10.0f), FVector(TraySize.X / 100.0f, TraySize.Y / 100.0f, 0.2f), Felt));
	Solid(AddPart(CubeMesh, Root, FVector(HalfX, 0, TrayFloorZ + RimHeight * 0.5f), FVector(Rim / 100.0f, (TraySize.Y + Rim * 2) / 100.0f, RimHeight / 100.0f), Wood));
	Solid(AddPart(CubeMesh, Root, FVector(-HalfX, 0, TrayFloorZ + RimHeight * 0.5f), FVector(Rim / 100.0f, (TraySize.Y + Rim * 2) / 100.0f, RimHeight / 100.0f), Wood));
	Solid(AddPart(CubeMesh, Root, FVector(0, HalfY, TrayFloorZ + RimHeight * 0.5f), FVector(TraySize.X / 100.0f, Rim / 100.0f, RimHeight / 100.0f), Wood));
	Solid(AddPart(CubeMesh, Root, FVector(0, -HalfY, TrayFloorZ + RimHeight * 0.5f), FVector(TraySize.X / 100.0f, Rim / 100.0f, RimHeight / 100.0f), Wood));

	// Paredes y tapa invisibles: los dados rebotan pero nunca salen de la bandeja.
	for (const FVector& Wall : {FVector(HalfX, 0, 0), FVector(-HalfX, 0, 0), FVector(0, HalfY, 0), FVector(0, -HalfY, 0)})
	{
		const bool bAlongY = Wall.X != 0.0f;
		UStaticMeshComponent* Glass = Solid(AddPart(CubeMesh, Root, Wall + FVector(0, 0, TrayFloorZ + LidHeight * 0.5f),
			bAlongY ? FVector(0.1f, TraySize.Y / 100.0f, LidHeight / 100.0f) : FVector(TraySize.X / 100.0f, 0.1f, LidHeight / 100.0f), nullptr));
		Glass->SetVisibility(false);
	}
	UStaticMeshComponent* Lid = Solid(AddPart(CubeMesh, Root, FVector(0, 0, TrayFloorZ + LidHeight), FVector(TraySize.X / 100.0f, TraySize.Y / 100.0f, 0.1f), nullptr));
	Lid->SetVisibility(false);
}

UStaticMeshComponent* AHFDice::BuildDie(int32 Index)
{
	UMaterialInstanceDynamic* Ivory = MakeMaterial(Index == 0 ? FLinearColor(0.92f, 0.88f, 0.78f) : FLinearColor(0.75f, 0.12f, 0.08f));
	UMaterialInstanceDynamic* PipColor = MakeMaterial(Index == 0 ? FLinearColor(0.02f, 0.02f, 0.03f) : FLinearColor(0.95f, 0.93f, 0.88f));

	UStaticMeshComponent* Die = AddPart(CubeMesh, Root, FVector::ZeroVector, FVector(DieSize / 100.0f), Ivory);
	Die->SetCollisionProfileName(TEXT("PhysicsActor"));
	Die->SetLinearDamping(0.25f);
	Die->SetAngularDamping(0.35f);
	Die->SetCastShadow(true);

	// Puntos: esferas oscuras apenas hundidas en cada cara (en el espacio local del cubo, que mide 100).
	for (const FFace& Face : Faces)
	{
		const FVector U = FMath::Abs(Face.Normal.Z) > 0.5f ? FVector(1, 0, 0) : FVector(0, 0, 1);
		const FVector V = FVector::CrossProduct(Face.Normal, U);
		for (const FVector2D& Pip : PipLayout(Face.Value))
		{
			UStaticMeshComponent* Dot = AddPart(SphereMesh, Die, Face.Normal * 47.0f + U * Pip.X * 27.0f + V * Pip.Y * 27.0f,
				FVector(0.16f, 0.16f, 0.16f), PipColor);
			Dot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Dot->SetCastShadow(false);
		}
	}
	return Die;
}

void AHFDice::Throw(const FVector& Location, float FacingYaw)
{
	if (TrayParts.Num() == 0)
	{
		BuildTray();
		Dice[0] = BuildDie(0);
		Dice[1] = BuildDie(1);
	}
	SetActorLocationAndRotation(Location, FRotator(0.0f, FacingYaw, 0.0f));

	const FTransform Frame = GetActorTransform();
	for (int32 I = 0; I < 2; ++I)
	{
		UStaticMeshComponent* Die = Dice[I];
		Die->SetSimulatePhysics(false);
		// Salen desde el borde cercano a la cámara, uno al lado del otro, y ruedan hacia el fondo.
		const FVector Start(-TraySize.X * 0.32f, (I == 0 ? -1.0f : 1.0f) * TraySize.Y * 0.18f, TrayFloorZ + 150.0f);
		Die->SetWorldLocationAndRotation(Frame.TransformPosition(Start), FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f)));
		Die->SetSimulatePhysics(true);
		const FVector Velocity(FMath::FRandRange(650.0f, 900.0f), FMath::FRandRange(-180.0f, 180.0f), FMath::FRandRange(-50.0f, 120.0f));
		Die->SetPhysicsLinearVelocity(Frame.TransformVectorNoScale(Velocity));
		Die->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-900.0f, 900.0f), FMath::FRandRange(-900.0f, 900.0f), FMath::FRandRange(-600.0f, 600.0f)));
	}
	bRolling = true;
	RollStart = static_cast<float>(GetWorld()->GetTimeSeconds());
	StillSince = -1.0f;
	Nudges = 0;
	SetActorHiddenInGame(false);
}

int32 AHFDice::TopFace(const UStaticMeshComponent* Die, float& OutAlignment)
{
	OutAlignment = -2.0f;
	int32 Value = 1;
	const FTransform Transform = Die->GetComponentTransform();
	for (const FFace& Face : Faces)
	{
		const float Alignment = FVector::DotProduct(Transform.TransformVectorNoScale(Face.Normal), FVector::UpVector);
		if (Alignment > OutAlignment)
		{
			OutAlignment = Alignment;
			Value = Face.Value;
		}
	}
	return Value;
}

void AHFDice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRolling)
	{
		return;
	}
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (Now - RollStart > MaxRollSeconds)
	{
		Finish();
		return;
	}

	bool bStill = Now - RollStart > 0.5f;
	for (UStaticMeshComponent* Die : Dice)
	{
		if (Die->GetPhysicsLinearVelocity().Size() > 4.0f || Die->GetPhysicsAngularVelocityInDegrees().Size() > 8.0f)
		{
			bStill = false;
		}
	}
	if (!bStill)
	{
		StillSince = -1.0f;
		return;
	}
	if (StillSince < 0.0f)
	{
		StillSince = Now;
		return;
	}
	if (Now - StillSince < 0.3f)
	{
		return;
	}

	// Si un dado quedó apoyado de canto contra otro o contra el borde, se lo sacude un poco.
	for (UStaticMeshComponent* Die : Dice)
	{
		float Alignment = 0.0f;
		TopFace(Die, Alignment);
		if (Alignment < 0.96f && Nudges < 3)
		{
			++Nudges;
			StillSince = -1.0f;
			Die->AddImpulse(FVector(FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-60.0f, 60.0f), 250.0f), NAME_None, true);
			Die->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(-400.0f, 400.0f), 0.0f), NAME_None, true);
			return;
		}
	}
	Finish();
}

void AHFDice::Finish()
{
	bRolling = false;
	for (int32 I = 0; I < 2; ++I)
	{
		float Alignment = 0.0f;
		Values[I] = TopFace(Dice[I], Alignment);
	}
	OnSettled.ExecuteIfBound(Values[0], Values[1]);
}
