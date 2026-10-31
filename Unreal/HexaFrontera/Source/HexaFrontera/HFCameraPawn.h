// Cámara de mesa: órbita, zoom, desplazamiento y enfoque suave en lo que pasa.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HFCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class HEXAFRONTERA_API AHFCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AHFCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	void Orbit(float DeltaYaw, float DeltaPitch);
	void Zoom(float Steps);
	void Pan(const FVector2D& Direction, float DeltaSeconds);
	void FocusOn(const FVector& Location, float Seconds = 1.5f);
	void FrameBoard(float BoardExtent);

	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	float MinArmLength = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	float MaxArmLength = 6500.0f;

	UPROPERTY(EditAnywhere, Category = "Hexa Frontera")
	float PanSpeed = 2200.0f;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> Arm;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	FVector TargetLocation = FVector::ZeroVector;
	FVector HomeLocation = FVector::ZeroVector;
	float TargetArmLength = 3600.0f;
	float TargetYaw = -30.0f;
	float TargetPitch = -55.0f;
	float ReturnHomeAt = -1.0f;
	float PanLimit = 3000.0f;
};
