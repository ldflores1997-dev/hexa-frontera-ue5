// Recursos opcionales de Content/Biomas (los arma Tools/unreal/aplicar_texturas.py).
// Si un recurso no existe, el juego sigue con las formas y colores generados por código.
#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

namespace HFAssets
{
inline const FString Common = TEXT("/Game/Biomas/_Comun");

// Carga "/Game/Carpeta/Nombre" si el paquete existe; si no, devuelve nullptr sin avisos en el registro.
template <typename T>
T* LoadIfExists(const FString& PackagePath)
{
	if (!FPackageName::DoesPackageExist(PackagePath))
	{
		return nullptr;
	}
	return LoadObject<T>(nullptr, *(PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath)));
}
}
