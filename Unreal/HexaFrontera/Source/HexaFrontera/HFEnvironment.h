// Ambiente de cada mundo: sol, cielo físico, nubes volumétricas, niebla, suelo y postproceso.
// Se crea por código para que el juego funcione en un mapa vacío.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFEnvironment.generated.h"

class FHFMatch;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UVolumetricCloudComponent;

UCLASS()
class HEXAFRONTERA_API AHFEnvironment : public AActor
{
	GENERATED_BODY()

public:
	AHFEnvironment();

	virtual void Tick(float DeltaSeconds) override;

	void ApplyScenario(const FHFMatch& Match, float BoardExtent);

	// Dirección hacia el sol (normalizada) y cuánto es de noche (0 día pleno, 1 noche cerrada).
	FVector GetSunDirection() const { return SunDirection; }
	float GetNightAmount() const { return NightAmount; }

	// Minutos que dura un día completo. 0 deja el sol quieto.
	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	float DayLengthMinutes = 8.0f;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> Moon;

	float NoonElevation = 40.0f;
	float SunIntensity = 8.0f;
	FLinearColor SunColor = FLinearColor::White;
	float DayTime = 0.25f; // 0 medianoche, 0.25 mañana, 0.5 mediodía, 0.75 tarde
	FVector SunDirection = FVector::UpVector;
	float NightAmount = 0.0f;
	float LastRecapture = 0.0f;

	void UpdateSky();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Ground;
};
