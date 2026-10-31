#include "HFEnvironment.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "HFMatch.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AHFEnvironment::AHFEnvironment()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetDynamicShadowCascades(4);
	Sun->bUseTemperature = false;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->bEnableVolumetricFog = true;

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;

	Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(Root);
	Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		Ground->SetStaticMesh(Plane.Object);
	}
}

void AHFEnvironment::ApplyScenario(const FHFMatch& Match, float BoardExtent)
{
	const hf::EnvironmentDef& Env = Match.GetScenario().environment;

	// Sol: la elevación define el clima de la escena (atardecer bajo, mediodía alto).
	Sun->SetWorldRotation(FRotator(-Env.sunElevation, 35.0f, 0.0f));
	Sun->SetLightColor(FHFMatch::ToLinear(Env.sunColor));
	Sun->SetIntensity(Env.sunIntensity);

	// Cielo: el tinte de Rayleigh cambia el color del cielo (azul en la Tierra, ocre en Marte).
	const FLinearColor Sky = FHFMatch::ToLinear(Env.skyTint);
	Atmosphere->SetRayleighScattering(FLinearColor(
		0.005802f * (0.4f + Sky.R), 0.013558f * (0.4f + Sky.G), 0.0331f * (0.4f + Sky.B)));
	Atmosphere->SetMieScatteringScale(Env.ground == "sand" ? 0.02f : 0.004f);

	Clouds->SetVisibility(Env.clouds);
	if (Env.ground == "clouds")
	{
		// La ciudad flotante queda por encima de una capa de nubes.
		Clouds->SetLayerBottomAltitude(0.3f);
		Clouds->SetLayerHeight(2.0f);
	}
	else
	{
		Clouds->SetLayerBottomAltitude(5.0f);
		Clouds->SetLayerHeight(10.0f);
	}

	Fog->SetFogInscatteringColor(FHFMatch::ToLinear(Env.fogColor));
	Fog->SetFogDensity(Env.fogDensity);
	Fog->SetFogHeightFalloff(0.2f);
	Fog->SetVolumetricFog(true);

	// Suelo alrededor del tablero: mar, arena, pasto o nubes.
	const float GroundScale = FMath::Max(400.0f, BoardExtent * 0.5f);
	Ground->SetRelativeScale3D(FVector(GroundScale, GroundScale, 1.0f));
	Ground->SetRelativeLocation(FVector(0, 0, Env.ground == "clouds" ? -2500.0f : -5.0f));
	Ground->SetVisibility(Env.ground != "clouds");
	if (UMaterialInterface* Base = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")).TryLoad()))
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
		Material->SetVectorParameterValue(TEXT("Color"), FHFMatch::ToLinear(Env.groundColor));
		Ground->SetMaterial(0, Material);
	}

	// Postproceso cinematográfico.
	FPostProcessSettings& Settings = PostProcess->Settings;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = 0.6f;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.35f;
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(1.08f, 1.08f, 1.08f, 1.0f);

	SkyLight->RecaptureSky();
}
