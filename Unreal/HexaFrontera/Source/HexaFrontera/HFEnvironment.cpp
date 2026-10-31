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
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetDynamicShadowCascades(4);
	Sun->bUseTemperature = false;

	// Luz de luna: azulada y suave, sin teñir el cielo (el cielo lo define el sol bajo el horizonte).
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(false);
	Moon->SetLightColor(FLinearColor(0.55f, 0.65f, 1.0f));
	Moon->SetIntensity(0.0f);
	Moon->SetCastShadows(false);
	Moon->bUseTemperature = false;

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

	// Sol: la elevación del escenario es la altura del mediodía; el día arranca a media mañana.
	NoonElevation = Env.sunElevation;
	SunIntensity = Env.sunIntensity;
	SunColor = FHFMatch::ToLinear(Env.sunColor);
	DayTime = 0.33f;
	UpdateSky();

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
	// La exposición automática no compensa del todo la noche: tiene que verse más oscura.
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.AutoExposureMinBrightness = 1.0f;

	SkyLight->RecaptureSky();
}

void AHFEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DayLengthMinutes <= 0.0f)
	{
		return;
	}
	DayTime = FMath::Fmod(DayTime + DeltaSeconds / (DayLengthMinutes * 60.0f), 1.0f);
	UpdateSky();

	// La luz del cielo con captura en tiempo real se actualiza sola; igual se fuerza cada tanto
	// para que el cambio de color al atardecer no se demore.
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (Now - LastRecapture > 4.0f)
	{
		LastRecapture = Now;
		SkyLight->RecaptureSky();
	}
}

void AHFEnvironment::UpdateSky()
{
	// El sol sube hasta la elevación del mediodía y baja a -12° a medianoche (crepúsculo azul, no negro total).
	const float Arc = FMath::Sin((DayTime - 0.25f) * 2.0f * UE_PI); // -1 medianoche, 1 mediodía
	const float Elevation = Arc >= 0.0f ? Arc * NoonElevation : Arc * 12.0f;
	const float Yaw = 35.0f + (DayTime - 0.5f) * 220.0f; // sale por un lado y se pone por el otro
	Sun->SetWorldRotation(FRotator(-Elevation, Yaw, 0.0f));
	SunDirection = -FRotator(-Elevation, Yaw, 0.0f).Vector();

	// Atardecer: el sol se pone naranja cerca del horizonte.
	const float Low = 1.0f - FMath::Clamp(Elevation / 20.0f, 0.0f, 1.0f);
	Sun->SetLightColor(FLinearColor::LerpUsingHSV(SunColor, FLinearColor(1.0f, 0.45f, 0.18f), Low * 0.8f));
	Sun->SetIntensity(SunIntensity * FMath::Clamp((Elevation + 4.0f) / 14.0f, 0.0f, 1.0f));

	NightAmount = FMath::Clamp((4.0f - Elevation) / 12.0f, 0.0f, 1.0f);
	Moon->SetWorldRotation(FRotator(-35.0f, Yaw + 180.0f, 0.0f));
	Moon->SetIntensity(0.6f * NightAmount);
}
