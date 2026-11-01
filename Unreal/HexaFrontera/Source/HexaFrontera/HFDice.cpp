#include "HFDice.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HFAssets.h"
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
	ConeMesh = Cone.Object;
}

UMaterialInstanceDynamic* AHFDice::MakeMaterial(const FLinearColor& Color, const TCHAR* Preferred)
{
	// El material de Biomas/_Comun si existe (resina, formas); si no, el del motor.
	UMaterialInterface* Base = Preferred ? HFAssets::LoadIfExists<UMaterialInterface>(HFAssets::Common / Preferred) : nullptr;
	if (!Base)
	{
		Base = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")).TryLoad());
	}
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Materials.Add(Material);
	return Material;
}

UStaticMeshComponent* AHFDice::AddPart(UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Scale, UMaterialInterface* Material)
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
	// Fieltro verde y madera oscura con textura; sin texturas, los mismos colores lisos.
	UMaterialInterface* Felt = HFAssets::LoadIfExists<UMaterialInterface>(HFAssets::Common / TEXT("MI_Fieltro"));
	UMaterialInterface* Wood = HFAssets::LoadIfExists<UMaterialInterface>(HFAssets::Common / TEXT("MI_Madera_Marco"));
	if (!Felt)
	{
		Felt = MakeMaterial(FLinearColor(0.03f, 0.16f, 0.09f));
	}
	if (!Wood)
	{
		Wood = MakeMaterial(FLinearColor(0.32f, 0.17f, 0.07f));
	}
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

void AHFDice::SetFaces(const TArray<FHFDieFaceLook>& First, const TArray<FHFDieFaceLook>& Second)
{
	Looks[0] = First;
	Looks[1] = Second;
	bLooksChanged = true;
}

UStaticMeshComponent* AHFDice::BuildDie(int32 Index)
{
	// El dado de terreno es de piedra clara y el de región de bronce, para distinguirlos; con resina pulida si existe.
	UMaterialInstanceDynamic* Body = MakeMaterial(Index == 0 ? FLinearColor(0.82f, 0.80f, 0.74f) : FLinearColor(0.55f, 0.36f, 0.14f), TEXT("M_Resina"));
	// El cubo del motor es el cuerpo físico (choca como una caja). Si existe el dado de Blender, con las
	// aristas redondeadas, se ve ese y el cubo queda invisible.
	UStaticMeshComponent* Die = AddPart(CubeMesh, Root, FVector::ZeroVector, FVector(DieSize / 100.0f), Body);
	Die->SetCollisionProfileName(TEXT("PhysicsActor"));
	Die->SetLinearDamping(0.25f);
	Die->SetAngularDamping(0.35f);
	Die->SetCastShadow(true);
	UStaticMesh* Rounded = HFAssets::LoadIfExists<UStaticMesh>(HFAssets::Common / TEXT("Piezas/SM_Dado"));
	if (Rounded)
	{
		Die->SetVisibility(false);
		UStaticMeshComponent* Shell = AddPart(Rounded, Die, FVector::ZeroVector, FVector::OneVector, Body);
		Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Cada cara lleva un parche de color apenas sobresaliente (en el espacio local del cubo, que mide 100)
	// y, si es evento o comodín, un símbolo encima. En el dado redondeado el parche se achica para no
	// salirse de la parte plana de la cara.
	for (const FFace& Face : Faces)
	{
		FHFDieFaceLook Look;
		if (Looks[Index].IsValidIndex(Face.Value - 1))
		{
			Look = Looks[Index][Face.Value - 1];
		}
		const FVector Axis = Face.Normal.GetAbs();
		const FVector PatchScale = FVector(Rounded ? 0.7f : 0.78f) * (FVector::OneVector - Axis) + Axis * 0.03f;
		UStaticMeshComponent* Patch = AddPart(CubeMesh, Die, Face.Normal * 50.0f, PatchScale, MakeMaterial(Look.Color, TEXT("M_Forma")));
		Patch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Patch->SetCastShadow(false);
		if (Look.Symbol == FHFDieFaceLook::ESymbol::Terrain)
		{
			continue;
		}
		const bool bEvent = Look.Symbol == FHFDieFaceLook::ESymbol::Event;
		UStaticMeshComponent* Symbol = AddPart(bEvent ? ConeMesh.Get() : SphereMesh.Get(), Die, Face.Normal * 58.0f, FVector(bEvent ? 0.32f : 0.3f),
			MakeMaterial(bEvent ? FLinearColor(0.02f, 0.02f, 0.02f) : FLinearColor(0.98f, 0.97f, 0.92f)));
		Symbol->SetRelativeRotation(FRotationMatrix::MakeFromZ(Face.Normal).Rotator()); // el cono apunta hacia afuera
		Symbol->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Symbol->SetCastShadow(false);
	}
	return Die;
}

void AHFDice::Throw(const FVector& Location, float FacingYaw)
{
	if (TrayParts.Num() == 0)
	{
		BuildTray();
	}
	if (!Dice[0] || bLooksChanged)
	{
		// Mundo nuevo (o primera tirada): se arman los dados con las caras del escenario.
		for (TObjectPtr<UStaticMeshComponent>& Die : Dice)
		{
			if (Die)
			{
				TArray<USceneComponent*> Attached;
				Die->GetChildrenComponents(true, Attached);
				for (USceneComponent* Child : Attached)
				{
					Child->DestroyComponent();
				}
				Die->DestroyComponent();
				Die = nullptr;
			}
		}
		Dice[0] = BuildDie(0);
		Dice[1] = BuildDie(1);
		bLooksChanged = false;
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
