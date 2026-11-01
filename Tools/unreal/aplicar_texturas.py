# Arma los materiales del juego con texturas de Poly Haven elegidas en texturas.json:
#   - una superficie por terreno en /Game/Biomas/<mundo>/<terreno>/Superficie/ (el tablero la usa sola);
#   - el suelo y el marco de cada mundo en /Game/Biomas/<mundo>/_Entorno/;
#   - los materiales compartidos (formas, madera, humo, resina, agua) y las piezas de Blender en /Game/Biomas/_Comun/.
#
# En el editor: Tools → Execute Python Script… y elegí este archivo.
# Sin abrir el editor:
#   UnrealEditor-Cmd.exe HexaFrontera.uproject -run=pythonscript -script=<ruta>\aplicar_texturas.py -EnablePlugins=PythonScriptPlugin
#
# Las imágenes bajadas quedan en Saved/PolyHaven (no se suben al repo) y no se vuelven a bajar.
# Las piezas se generan antes con: blender --background --python Tools/blender/piezas.py
import json
import os
import urllib.request

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ROOT = "/Game/Biomas"
COMMON = f"{ROOT}/_Comun"
TEXTURES = f"{ROOT}/_Texturas"
RESOLUTION = "2k"
CACHE = os.path.join(unreal.Paths.project_saved_dir(), "PolyHaven")
SCENARIOS = os.path.join(unreal.Paths.project_content_dir(), "Scenarios")
PIECES = os.path.join(REPO, "Tools", "blender", "out", "piezas")
# Mapa de Poly Haven → sufijo del asset. "arm" trae oclusión, rugosidad y metal en R, G y B.
MAPS = {"Diffuse": "Color", "nor_dx": "Normal", "arm": "ARM"}

assets = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
editor = unreal.EditorAssetLibrary


# ---------------------------------------------------------------- Texturas

def download(texture_id, map_name):
    path = os.path.join(CACHE, f"{texture_id}_{map_name}.jpg")
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return path
    os.makedirs(CACHE, exist_ok=True)
    request = urllib.request.Request(f"https://api.polyhaven.com/files/{texture_id}", headers={"User-Agent": "HexaFrontera"})
    files = json.load(urllib.request.urlopen(request))
    url = files[map_name][RESOLUTION]["jpg"]["url"]
    request = urllib.request.Request(url, headers={"User-Agent": "HexaFrontera"})
    with urllib.request.urlopen(request) as response, open(path, "wb") as out:
        out.write(response.read())
    return path


def import_texture(texture_id, map_name):
    name = f"T_{texture_id}_{MAPS[map_name]}"
    existing = f"{TEXTURES}/{texture_id}/{name}"
    if editor.does_asset_exist(existing):
        return unreal.load_asset(existing)
    task = unreal.AssetImportTask()
    task.filename = download(texture_id, map_name)
    task.destination_path = f"{TEXTURES}/{texture_id}"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    assets.import_asset_tasks([task])
    texture = unreal.load_asset(existing)
    if map_name == "nor_dx":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif map_name == "arm":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    editor.save_loaded_asset(texture)
    return texture


def texture_set(texture_id):
    return {MAPS[m]: import_texture(texture_id, m) for m in MAPS}


def srgb_to_linear(hex_color):
    channels = [int(hex_color[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    return [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in channels]


# ---------------------------------------------------------------- Grafo de materiales

class Graph:
    """Ayuda para armar el grafo de un material nodo por nodo."""

    def __init__(self, path):
        folder, name = path.rsplit("/", 1)
        if editor.does_asset_exist(path):
            editor.delete_asset(path)
        self.material = assets.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        # Las piezas importadas son mallas Nanite: sin este permiso el juego las dibuja con el material gris.
        self.material.set_editor_property("used_with_nanite", True)
        self.material.set_editor_property("used_with_instanced_static_meshes", True)
        self.count = 0

    def node(self, cls, **props):
        expression = library.create_material_expression(self.material, cls, -1800 + (self.count % 8) * 220, -600 + (self.count // 8) * 160)
        self.count += 1
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def link(self, source, target, pin="", output=""):
        library.connect_material_expressions(source, output, target, pin)

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def vector(self, name, color):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*color))

    def constant(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def constant3(self, r, g, b):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))

    def mask(self, source, r=False, g=False, b=False, output=""):
        expression = self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False)
        self.link(source, expression, "", output)
        return expression

    def op(self, cls, a, b, a_out="", b_out=""):
        expression = self.node(cls)
        self.link(a, expression, "A", a_out)
        self.link(b, expression, "B", b_out)
        return expression

    def mul(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionMultiply, a, b, a_out, b_out)

    def lerp(self, a, b, alpha, a_out="", b_out=""):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, expression, "A", a_out)
        self.link(b, expression, "B", b_out)
        self.link(alpha, expression, "Alpha")
        return expression

    def sample(self, parameter, sampler, default, uv=None):
        expression = self.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=parameter,
                               sampler_type=sampler, texture=unreal.load_asset(default))
        if uv:
            self.link(uv, expression, "UVs")
        return expression

    def output(self, source, prop, out=""):
        library.connect_material_property(source, out, prop)

    def save(self):
        library.recompile_material(self.material)
        editor.save_loaded_asset(self.material)
        return self.material


COLOR = (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "/Engine/EngineResources/DefaultTexture")
NORMAL = (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "/Engine/EngineMaterials/DefaultNormal")
# El muestreador de máscaras necesita por defecto una textura comprimida como máscara (sin sRGB): el motor no trae
# ninguna, así que main() pone acá la ARM de la primera textura importada antes de armar los materiales.
MASKS = [unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, ""]
P = unreal.MaterialProperty


def build_casilla():
    """Casillas y suelos: proyección por coordenadas de mundo. La tapa toma X/Y y los costados X+Y/Z,
    así la textura no se estira en los laterales y mide lo mismo en todas las casillas."""
    g = Graph(f"{COMMON}/M_Casilla")
    world = g.node(unreal.MaterialExpressionWorldPosition)
    normal = g.node(unreal.MaterialExpressionVertexNormalWS)
    tiling = g.scalar("Tiling", 250.0)
    top_uv = g.op(unreal.MaterialExpressionDivide, g.mask(world, r=True, g=True), tiling)
    side_u = g.op(unreal.MaterialExpressionAdd, g.mask(world, r=True), g.mask(world, g=True))
    side_uv = g.op(unreal.MaterialExpressionDivide, g.op(unreal.MaterialExpressionAppendVector, side_u, g.mask(world, b=True)), tiling)
    # 1 en la tapa, 0 en los costados; el bisel queda del lado de la tapa.
    up = g.node(unreal.MaterialExpressionAbs)
    g.link(g.mask(normal, b=True), up)
    shifted = g.node(unreal.MaterialExpressionSubtract, const_b=0.45)
    g.link(up, shifted, "A")
    sharpen = g.node(unreal.MaterialExpressionMultiply, const_b=4.0)
    g.link(shifted, sharpen, "A")
    blend = g.node(unreal.MaterialExpressionSaturate)
    g.link(sharpen, blend)

    def projected(parameter, kind):
        side, top = g.sample(parameter, *kind, side_uv), g.sample(parameter, *kind, top_uv)
        return g.lerp(side, top, blend, "RGB", "RGB")

    color = projected("ColorTex", COLOR)
    normal_map = projected("NormalTex", NORMAL)
    arm = projected("ARMTex", MASKS)
    # Color: textura × tinte, empujada un poco hacia el color del terreno para que se reconozca de un vistazo,
    # y más oscura en los costados, como la casilla generada.
    tinted = g.mul(color, g.vector("Tint", (1, 1, 1, 1)))
    toward = g.mul(tinted, g.mul(g.vector("TerrainColor", (0.5, 0.5, 0.5, 1)), g.constant(2.0)))
    mixed = g.lerp(tinted, toward, g.scalar("ColorMix", 0.25))
    side_shade = g.node(unreal.MaterialExpressionLinearInterpolate, const_a=0.55, const_b=1.0)
    g.link(blend, side_shade, "Alpha")
    g.output(g.mul(mixed, side_shade), P.MP_BASE_COLOR)
    g.output(normal_map, P.MP_NORMAL)
    g.output(g.mask(arm, r=True), P.MP_AMBIENT_OCCLUSION)
    g.output(g.mul(g.mask(arm, g=True), g.scalar("RoughnessScale", 1.0)), P.MP_ROUGHNESS)
    g.output(g.mask(arm, b=True), P.MP_METALLIC)
    g.output(g.vector("Emissive", (0, 0, 0, 1)), P.MP_EMISSIVE_COLOR)
    return g.save()


def build_forma():
    """Formas básicas (árboles, rocas, cristales…): color con una variación sutil para que no parezcan plástico."""
    g = Graph(f"{COMMON}/M_Forma")
    color = g.vector("Color", (0.5, 0.5, 0.5, 1))
    noise = g.node(unreal.MaterialExpressionNoise, scale=0.02, levels=3, output_min=0.85, output_max=1.08)
    g.output(g.mul(color, noise), P.MP_BASE_COLOR)
    g.output(g.scalar("Roughness", 0.6), P.MP_ROUGHNESS)
    g.output(g.mul(color, g.scalar("Glow", 0.0)), P.MP_EMISSIVE_COLOR)
    return g.save()


def build_madera():
    """Madera natural o pintada (piezas, fichas, marco, bandeja). Color + Paint pintan la veta sin taparla."""
    g = Graph(f"{COMMON}/M_Madera")
    uv = g.mul(g.node(unreal.MaterialExpressionTextureCoordinate), g.scalar("UVScale", 1.0))
    wood = g.sample("ColorTex", *COLOR, uv)
    normal_map = g.sample("NormalTex", *NORMAL, uv)
    arm = g.sample("ARMTex", *MASKS, uv)
    luminance = g.op(unreal.MaterialExpressionDotProduct, wood, g.constant3(0.3, 0.59, 0.11), "RGB")
    painted = g.mul(g.vector("Color", (1, 1, 1, 1)), g.mul(luminance, g.constant(1.8)))
    paint = g.scalar("Paint", 0.0)
    g.output(g.lerp(wood, painted, paint, "RGB"), P.MP_BASE_COLOR)
    g.output(normal_map, P.MP_NORMAL, "RGB")
    g.output(g.mask(arm, r=True, output="RGB"), P.MP_AMBIENT_OCCLUSION)
    # La pintura deja la superficie un poco más satinada que la madera cruda.
    g.output(g.lerp(g.mask(arm, g=True, output="RGB"), g.constant(0.42), paint), P.MP_ROUGHNESS)
    g.output(g.mask(arm, b=True, output="RGB"), P.MP_METALLIC)
    return g.save()


def build_humo():
    """Humo, vapor, polvo y destellos: translúcido, con bordes suaves y opacidad que el juego va bajando."""
    g = Graph(f"{COMMON}/M_Humo")
    g.material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g.material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL)
    color = g.vector("Color", (0.6, 0.6, 0.6, 1))
    fresnel = g.node(unreal.MaterialExpressionFresnel, exponent=2.0, base_reflect_fraction=0.0)
    soft = g.node(unreal.MaterialExpressionOneMinus)
    g.link(fresnel, soft)
    g.output(color, P.MP_BASE_COLOR)
    g.output(g.mul(color, g.scalar("Glow", 0.0)), P.MP_EMISSIVE_COLOR)
    g.output(g.mul(soft, g.scalar("Opacity", 0.6)), P.MP_OPACITY)
    return g.save()


def build_resina():
    """Dados: resina pulida, con reflejos marcados."""
    g = Graph(f"{COMMON}/M_Resina")
    g.output(g.vector("Color", (0.9, 0.86, 0.76, 1)), P.MP_BASE_COLOR)
    g.output(g.constant(0.16), P.MP_ROUGHNESS)
    g.output(g.constant(0.7), P.MP_SPECULAR)
    return g.save()


def build_agua():
    """Mar y agua del arrecife: dos capas de ondas que se desplazan en sentidos distintos."""
    g = Graph(f"{COMMON}/M_Agua")
    world = g.mask(g.node(unreal.MaterialExpressionWorldPosition), r=True, g=True)
    wave_size = g.scalar("WaveSize", 260.0)  # cm de cada onda: chicas en el arrecife, más largas en el mar
    waves = []
    for factor, speed in ((1.0, (0.03, 0.018)), (1.6, (-0.022, 0.026))):
        uv = g.op(unreal.MaterialExpressionDivide, world, g.mul(wave_size, g.constant(factor)))
        panner = g.node(unreal.MaterialExpressionPanner, speed_x=speed[0], speed_y=speed[1])
        g.link(uv, panner, "Coordinate")
        waves.append(g.sample("NormalTex", *NORMAL, panner))
    mixed = g.lerp(waves[0], waves[1], g.constant(0.5), "RGB", "RGB")
    flat = g.lerp(g.constant3(0, 0, 1), mixed, g.scalar("WaveStrength", 0.6))
    # El agua absorbe luz: el color del terreno se oscurece para que no parezca plástico pintado.
    g.output(g.mul(g.vector("Color", (0.02, 0.2, 0.28, 1)), g.scalar("Depth", 0.45)), P.MP_BASE_COLOR)
    g.output(flat, P.MP_NORMAL)
    g.output(g.constant(0.04), P.MP_ROUGHNESS)
    g.output(g.constant(0.9), P.MP_SPECULAR)
    return g.save()


# ---------------------------------------------------------------- Instancias

def instance(path, parent, textures=None, vectors=None, scalars=None):
    folder, name = path.rsplit("/", 1)
    if editor.does_asset_exist(path):
        editor.delete_asset(path)
    mi = assets.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(mi, parent)
    for key, value in (textures or {}).items():
        library.set_material_instance_texture_parameter_value(mi, key, value)
    for key, value in (vectors or {}).items():
        library.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(*value, 1))
    for key, value in (scalars or {}).items():
        library.set_material_instance_scalar_parameter_value(mi, key, value)
    library.update_material_instance(mi)
    editor.save_loaded_asset(mi)
    return mi


def textured(texture_id):
    maps = texture_set(texture_id)
    return {"ColorTex": maps["Color"], "NormalTex": maps["Normal"], "ARMTex": maps["ARM"]}


def import_pieces():
    if not os.path.isdir(PIECES):
        unreal.log_warning(f"Texturas: no están las piezas de Blender en {PIECES}; corré Tools/blender/piezas.py")
        return
    tasks = []
    for name in sorted(os.listdir(PIECES)):
        if name.endswith(".fbx"):
            task = unreal.AssetImportTask()
            task.filename = os.path.join(PIECES, name)
            task.destination_path = f"{COMMON}/Piezas"
            task.destination_name = name[:-4]
            task.automated = True
            task.replace_existing = True
            task.save = True
            tasks.append(task)
    assets.import_asset_tasks(tasks)
    unreal.log(f"Texturas: {len(tasks)} piezas importadas")


def main():
    picks = json.load(open(os.path.join(HERE, "texturas.json"), encoding="utf-8"))
    worlds = {w: t for w, t in picks.items() if not w.startswith("_")}

    # Las instancias se borran antes que sus materiales padre, que se rehacen en cada corrida.
    for world, terrains in worlds.items():
        for terrain in terrains:
            path = f"{ROOT}/{world}/{terrain}/Superficie/MI_{terrain}"
            if editor.does_asset_exist(path):
                editor.delete_asset(path)
    for path in editor.list_assets(COMMON, recursive=False):
        if "/MI_" in path:
            editor.delete_asset(path)
    for world in worlds:
        if editor.does_directory_exist(f"{ROOT}/{world}/_Entorno"):
            for path in editor.list_assets(f"{ROOT}/{world}/_Entorno"):
                editor.delete_asset(path)

    first = next(iter(next(iter(worlds.values())).values()))["id"]
    MASKS[1] = texture_set(first)["ARM"].get_path_name()
    casilla, forma, madera, humo, resina, agua = build_casilla(), build_forma(), build_madera(), build_humo(), build_resina(), build_agua()

    made = 0
    for world, terrains in worlds.items():
        scenario = json.load(open(os.path.join(SCENARIOS, f"{world}.json"), encoding="utf-8"))
        colors = {t["id"]: t["color"] for t in scenario["terrains"]}
        for terrain, pick in terrains.items():
            scalars = {"Tiling": pick["tiling"]} if "tiling" in pick else {}
            instance(f"{ROOT}/{world}/{terrain}/Superficie/MI_{terrain}", casilla, textured(pick["id"]),
                     {"TerrainColor": srgb_to_linear(colors[terrain]), "Tint": pick.get("tint", [1, 1, 1]),
                      "Emissive": pick.get("emissive", [0, 0, 0])}, scalars)
            made += 1

        # Suelo alrededor del tablero y marco propio del mundo.
        env = picks.get("_entorno", {}).get(world, {})
        ground = srgb_to_linear(scenario["environment"]["ground_color"])
        if "agua" in env:
            instance(f"{ROOT}/{world}/_Entorno/MI_Suelo", agua, {"NormalTex": texture_set(env["agua"])["Normal"]}, {"Color": ground},
                     {"WaveSize": 700.0, "Depth": 0.6})
        elif "id" in env:
            instance(f"{ROOT}/{world}/_Entorno/MI_Suelo", casilla, textured(env["id"]),
                     {"TerrainColor": ground}, {"Tiling": env.get("tiling", 500), "ColorMix": 0.35})
        if "marco" in env:
            instance(f"{ROOT}/{world}/_Entorno/MI_Marco", madera, textured(env["marco"]), scalars={"UVScale": 2.0})

    game = picks["_juego"]
    instance(f"{COMMON}/MI_Madera_Pieza", madera, textured(game["madera_pieza"]), scalars={"Paint": 0.85})
    instance(f"{COMMON}/MI_Madera_Ficha", madera, textured(game["madera_ficha"]))
    instance(f"{COMMON}/MI_Madera_Marco", madera, textured(game["madera_marco"]), scalars={"UVScale": 2.0})
    instance(f"{COMMON}/MI_Fieltro", madera, textured(game["fieltro"]), {"Color": [0.04, 0.3, 0.14]}, {"Paint": 0.95, "UVScale": 3.0})
    import_pieces()
    unreal.log(f"Texturas: {made} superficies, materiales del juego listos")


main()
