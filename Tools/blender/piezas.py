"""
Genera las piezas del juego con bordes biselados y las exporta a FBX: dado, ficha de número, puesto,
centro y ruta. Tools/unreal/aplicar_texturas.py las importa a /Game/Biomas/_Comun/.

Uso (desde la carpeta del repo):
    blender --background --python Tools/blender/piezas.py

Medidas (1 m de Blender = 100 cm en Unreal):
  - SM_Dado: cubo de 1 m con el pivote en el centro, como el cubo del motor que usa la física.
  - SM_Ficha: disco de 95 cm por 8 cm, con la base en Z = 0.
  - SM_Puesto y SM_Centro: base en Z = 0. Ranura 0 = cuerpo, ranura 1 = techo (el juego la oscurece).
  - SM_Ruta: tablón de 1 m de largo sobre X, con el pivote en el centro (el juego lo estira).
"""

import os

import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DIR = os.path.join(ROOT, "Tools", "blender", "out", "piezas")


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for block in (bpy.data.meshes, bpy.data.materials):
        for item in list(block):
            if item.users == 0:
                block.remove(item)


def material(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def mesh_object(name, vertices, faces, slot=0):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj["slot"] = slot
    return obj


def box(name, center, size, slot=0):
    cx, cy, cz = center
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    v = [(cx + x * hx, cy + y * hy, cz + z * hz) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
    f = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    return mesh_object(name, v, f, slot)


def gable(name, length, width, base, top, slot=1):
    """Techo a dos aguas: prisma triangular a lo largo de X."""
    hx, hy = length / 2, width / 2
    v = [(-hx, -hy, base), (-hx, hy, base), (-hx, 0, top), (hx, -hy, base), (hx, hy, base), (hx, 0, top)]
    f = [(0, 2, 1), (3, 4, 5), (0, 3, 5, 2), (1, 2, 5, 4), (0, 1, 4, 3)]
    return mesh_object(name, v, f, slot)


def pyramid(name, half, base, top, slot=1):
    v = [(-half, -half, base), (half, -half, base), (half, half, base), (-half, half, base), (0, 0, top)]
    f = [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (3, 2, 1, 0)]
    return mesh_object(name, v, f, slot)


def finish(parts, name, materials, bevel, segments=2):
    """Une las partes, asigna las ranuras de material y bisela los bordes."""
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        for index, mat in enumerate(materials):
            if len(part.data.materials) <= index:
                part.data.materials.append(material(mat))
        for polygon in part.data.polygons:
            polygon.material_index = part["slot"]
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    obj = bpy.context.active_object
    obj.name = name
    # Las caras armadas a mano pueden quedar al revés: todas miran hacia afuera.
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    modifier = obj.modifiers.new("Bisel", "BEVEL")
    modifier.width = bevel
    modifier.segments = segments
    modifier.limit_method = "ANGLE"
    obj.modifiers.new("Suavizado", "WEIGHTED_NORMAL")
    bpy.ops.object.shade_smooth()
    return obj


def export_fbx(obj):
    os.makedirs(OUT_DIR, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT_DIR, f"{obj.name}.fbx"), use_selection=True, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_UNITS", use_mesh_modifiers=True, mesh_smooth_type="FACE",
                             axis_forward="-Y", axis_up="Z")
    print(f"Piezas: {obj.name}")


def main():
    clear_scene()
    # Dado: aristas bien redondeadas, como los de resina.
    export_fbx(finish([box("dado", (0, 0, 0), (1, 1, 1))], "SM_Dado", ["Resina"], 0.12, 6))

    # Ficha: disco con el canto redondeado.
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=0.475, depth=0.08, location=(0, 0, 0.04))
    token = bpy.context.active_object
    token["slot"] = 0
    export_fbx(finish([token], "SM_Ficha", ["Madera"], 0.02, 3))

    # Puesto: casita con techo a dos aguas y chimenea.
    parts = [
        box("cuerpo", (0, 0, 0.275), (0.55, 0.55, 0.55)),
        gable("techo", 0.66, 0.68, 0.55, 0.92),
        box("chimenea", (0.14, 0.16, 0.82), (0.09, 0.09, 0.22), 1),
    ]
    export_fbx(finish(parts, "SM_Puesto", ["Cuerpo", "Techo"], 0.015))

    # Centro: base almenada y torre con techo en punta.
    parts = [box("base", (0, 0, 0.3), (0.85, 0.85, 0.6))]
    for x in (-1, 1):
        for y in (-1, 1):
            parts.append(box(f"almena{x}{y}", (x * 0.34, y * 0.34, 0.68), (0.17, 0.17, 0.16)))
    parts += [
        box("torre", (0, 0, 1.0), (0.42, 0.42, 0.8)),
        pyramid("techo", 0.28, 1.4, 1.85),
    ]
    export_fbx(finish(parts, "SM_Centro", ["Cuerpo", "Techo"], 0.015))

    # Ruta: tablón con los cantos gastados.
    export_fbx(finish([box("ruta", (0, 0, 0), (1.0, 0.22, 0.18))], "SM_Ruta", ["Madera"], 0.03, 3))


main()
