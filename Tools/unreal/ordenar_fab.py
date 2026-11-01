# Asigna a cada casilla los modelos de Fab elegidos en modelos_fab.json, sin moverlos de donde los dejó Fab.
#
# 1. En el editor, Window → Fab → agregá al proyecto cada modelo de la lista (quedan en /Game/Fab/...).
# 2. Tools → Execute Python Script… y elegí este archivo (o corrélo con UnrealEditor-Cmd, ver Docs/GRAFICOS.md).
#
# Busca cada modelo por su nombre en las carpetas de /Game/Fab y /Game/Megascans y escribe las líneas
# +TerrainVisuals=(...) en Config/DefaultGame.ini, entre las marcas de abajo (lo demás del archivo no se toca).
# Los que no encuentra los lista en el registro: se pueden agregar después y volver a correr el script.
import json
import os
import re

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
INI = os.path.join(unreal.Paths.project_config_dir(), "DefaultGame.ini")
SECTION = "[/Script/HexaFrontera.HFBoardActor]"
BEGIN = "; >>> modelos de Fab (los escribe Tools/unreal/ordenar_fab.py)"
END = "; <<< modelos de Fab"
ROOTS = ["/Game/Fab", "/Game/Megascans"]
# Carpeta de modelos_fab.json → campo de FHFTerrainVisual.
FIELDS = {"Plantas": "Plants", "Suelo": "GroundCover", "Rocas": "Rocks", "Relieve": "Landmark"}


def norm(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def static_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for root in ROOTS:
        for asset in registry.get_assets_by_path(root, recursive=True):
            if str(asset.asset_class_path.asset_name) == "StaticMesh":
                package = str(asset.package_name)
                segments = [norm(s) for s in package.split("/")[:-1]]
                found.append((package, f"{package}.{asset.asset_name}", segments))
    return found


def matches(title, meshes):
    # Fab guarda cada modelo en una carpeta "<Nombre>_<código>": se busca esa carpeta, no una parte del nombre
    # (así "Icelandic Rock" no se confunde con "Icelandic Rocky Ground").
    key = norm(title)
    return sorted({path for _, path, segments in meshes if any(s == key or s.startswith(key + "_") for s in segments)})


def main():
    picks = json.load(open(os.path.join(HERE, "modelos_fab.json"), encoding="utf-8"))
    meshes = static_meshes()
    lines, missing = [], []
    for world, terrains in picks.items():
        if world.startswith("_"):
            continue
        for terrain, models in terrains.items():
            fields = {}
            for model in models:
                # "alias": el nombre que trae el paquete de Megascans cuando no coincide con el de la tienda.
                found = matches(model.get("alias", model["titulo"]), meshes)
                if not found:
                    missing.append(f"{model['titulo']} ({world}/{terrain}) https://www.fab.com/listings/{model['uid']}")
                    continue
                fields.setdefault(FIELDS[model["carpeta"]], []).extend(p for p in found if p not in fields.get(FIELDS[model["carpeta"]], []))
            if not fields:
                continue
            parts = [f'Scenario="{world}"', f'Terrain="{terrain}"']
            for field, paths in fields.items():
                if field == "Landmark":
                    parts.append(f'Landmark="{paths[0]}"')
                else:
                    parts.append(f'{field}=(' + ",".join(f'"{p}"' for p in paths) + ")")
            lines.append("+TerrainVisuals=(" + ",".join(parts) + ")")

    text = open(INI, encoding="utf-8").read()
    block = "\n".join([BEGIN] + lines + [END])
    if BEGIN in text:
        text = text[:text.index(BEGIN)] + block + text[text.index(END) + len(END):]
    else:
        text = text.replace(SECTION, SECTION + "\n" + block, 1)
    open(INI, "w", encoding="utf-8").write(text)

    unreal.log(f"Fab: {len(meshes)} mallas en el proyecto, {len(lines)} casillas con modelos")
    for item in missing:
        unreal.log_warning(f"Fab: falta {item}")


main()
