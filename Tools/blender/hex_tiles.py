"""
Genera una casilla hexagonal detallada por cada terreno de cada escenario y la exporta a FBX
para importarla en Unreal (ver Docs/GRAFICOS.md).

Uso (desde la carpeta del repo):
    blender --background --python Tools/blender/hex_tiles.py
    blender --background --python Tools/blender/hex_tiles.py -- --scenario islas_volcanicas

Cada FBX queda en Tools/blender/out/<escenario>/<terreno>.fbx con estas medidas, que son las
que espera AHFBoardActor para reemplazar una casilla:
  - hexágono de radio 1 m (centro a vértice), con dos vértices sobre el eje Y como en el tablero
  - base en Z = 0 y alto 1 m (Unreal lo escala según la altura del terreno)
"""

import argparse
import json
import math
import os
import random
import sys

import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCENARIO_DIR = os.path.join(ROOT, "Unreal", "HexaFrontera", "Content", "Scenarios")
OUT_DIR = os.path.join(ROOT, "Tools", "blender", "out")

# Si en Unreal las casillas aparecen giradas 30 grados respecto del tablero, cambiar a 30.
ROTATE_DEG = 0.0
RADIUS = 0.965  # deja una pequeña separación entre casillas
BEVEL = 0.04

# Relieve de la tapa superior por tipo de decoración (amplitud en metros).
RELIEF = {
    "rocks": 0.10, "crater": -0.12, "mountain": 0.25, "volcano": 0.18, "crystals": 0.06,
    "trees": 0.04, "grass": 0.03, "crops": 0.015, "vines": 0.015, "coral": 0.05,
    "salt": 0.005, "panels": 0.01, "domes": 0.02, "chimneys": 0.01, "gears": 0.01, "banners": 0.01,
}


def hex_to_rgb(text):
    text = text.lstrip("#")
    srgb = [int(text[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    # Blender espera color lineal en Base Color
    return [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb]


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.textures):
        for item in list(block):
            if item.users == 0:
                block.remove(item)


def make_material(name, color, roughness):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    return material


def make_tile(terrain, seed):
    random.seed(seed)
    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=RADIUS, depth=1.0, location=(0, 0, 0.5),
                                        rotation=(0, 0, math.radians(30 + ROTATE_DEG)))
    tile = bpy.context.active_object
    tile.name = terrain["id"]

    # Tapa superior subdividida para poder darle relieve.
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    for polygon in tile.data.polygons:
        polygon.select = polygon.normal.z > 0.9
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.inset(thickness=BEVEL * 1.5)
    bpy.ops.mesh.subdivide(number_cuts=24)
    bpy.ops.object.mode_set(mode="OBJECT")

    amplitude = RELIEF.get(terrain.get("decoration", ""), 0.02)
    if amplitude != 0.0:
        texture = bpy.data.textures.new(f"{terrain['id']}_noise", type="CLOUDS")
        texture.noise_scale = 0.35
        group = tile.vertex_groups.new(name="top")
        group.add([v.index for v in tile.data.vertices if v.co.z > 0.49 and math.hypot(v.co.x, v.co.y) < RADIUS * 0.85], 1.0, "REPLACE")
        displace = tile.modifiers.new("Relieve", "DISPLACE")
        displace.texture = texture
        displace.strength = amplitude
        displace.mid_level = 0.5
        displace.vertex_group = "top"
        displace.direction = "Z"

    bevel = tile.modifiers.new("Bisel", "BEVEL")
    bevel.width = BEVEL
    bevel.segments = 3
    bevel.limit_method = "ANGLE"
    tile.modifiers.new("Suavizado", "WEIGHTED_NORMAL")

    color = hex_to_rgb(terrain.get("color", "#888888"))
    roughness = 0.35 if terrain.get("decoration") in ("salt", "crystals", "coral") else 0.8
    tile.data.materials.append(make_material(f"M_{terrain['id']}", color, roughness))
    bpy.ops.object.shade_smooth()
    return tile


def export_fbx(obj, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
                             use_mesh_modifiers=True, mesh_smooth_type="FACE", axis_forward="-Y", axis_up="Z")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario", help="id del escenario (por defecto, todos)")
    args = parser.parse_args(argv)

    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0

    files = sorted(f for f in os.listdir(SCENARIO_DIR) if f.endswith(".json"))
    for file_name in files:
        with open(os.path.join(SCENARIO_DIR, file_name), encoding="utf-8") as handle:
            scenario = json.load(handle)
        if args.scenario and scenario["id"] != args.scenario:
            continue
        for index, terrain in enumerate(scenario["terrains"]):
            clear_scene()
            tile = make_tile(terrain, seed=index * 101 + len(scenario["id"]))
            path = os.path.join(OUT_DIR, scenario["id"], f"{terrain['id']}.fbx")
            export_fbx(tile, path)
            print(f"Exportado {path}")


if __name__ == "__main__":
    main()
