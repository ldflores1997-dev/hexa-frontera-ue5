#include "HFCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

AHFCameraPawn::AHFCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->bDoCollisionTest = false;
	Arm->bEnableCameraLag = true;
	Arm->CameraLagSpeed = 6.0f;
	Arm->bEnableCameraRotationLag = true;
	Arm->CameraRotationLagSpeed = 8.0f;
	Arm->TargetArmLength = TargetArmLength;
	Arm->SetRelativeRotation(FRotator(TargetPitch, TargetYaw, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm);
	Camera->SetFieldOfView(45.0f);
}

void AHFCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ReturnHomeAt > 0.0f && static_cast<float>(GetWorld()->GetTimeSeconds()) >= ReturnHomeAt)
	{
		TargetLocation = HomeLocation;
		ReturnHomeAt = -1.0f;
	}

	SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, 3.5f));
	Arm->TargetArmLength = FMath::FInterpTo(Arm->TargetArmLength, TargetArmLength, DeltaSeconds, 6.0f);
	Arm->SetRelativeRotation(FRotator(TargetPitch, TargetYaw, 0.0f));
}

void AHFCameraPawn::Orbit(float DeltaYaw, float DeltaPitch)
{
	TargetYaw += DeltaYaw;
	TargetPitch = FMath::Clamp(TargetPitch + DeltaPitch, -85.0f, -15.0f);
}

void AHFCameraPawn::Zoom(float Steps)
{
	TargetArmLength = FMath::Clamp(TargetArmLength * FMath::Pow(0.88f, Steps), MinArmLength, MaxArmLength);
}

void AHFCameraPawn::Pan(const FVector2D& Direction, float DeltaSeconds)
{
	if (Direction.IsNearlyZero())
	{
		return;
	}
	// El desplazamiento sigue la orientación de la cámara.
	const FRotator Yaw(0.0f, TargetYaw, 0.0f);
	const FVector Forward = Yaw.RotateVector(FVector::ForwardVector);
	const FVector Right = Yaw.RotateVector(FVector::RightVector);
	const float Speed = PanSpeed * (TargetArmLength / 3600.0f);
	TargetLocation += (Forward * Direction.Y + Right * Direction.X) * Speed * DeltaSeconds;
	TargetLocation.X = FMath::Clamp(TargetLocation.X, static_cast<double>(-PanLimit), static_cast<double>(PanLimit));
	TargetLocation.Y = FMath::Clamp(TargetLocation.Y, static_cast<double>(-PanLimit), static_cast<double>(PanLimit));
	HomeLocation = TargetLocation;
	ReturnHomeAt = -1.0f;
}

void AHFCameraPawn::FocusOn(const FVector& Location, float Seconds)
{
	TargetLocation = FVector(Location.X, Location.Y, HomeLocation.Z);
	ReturnHomeAt = static_cast<float>(GetWorld()->GetTimeSeconds()) + Seconds;
}

void AHFCameraPawn::FrameBoard(float BoardExtent)
{
	PanLimit = BoardExtent * 1.2f;
	HomeLocation = FVector(0.0f, 0.0f, 100.0f);
	TargetLocation = HomeLocation;
	TargetArmLength = FMath::Clamp(BoardExtent * 2.6f, MinArmLength, MaxArmLength);
	MaxArmLength = FMath::Max(MaxArmLength, BoardExtent * 4.0f);
}
