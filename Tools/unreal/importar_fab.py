# Importa los .zip de Megascans bajados de Fab (formato glTF para Unreal, "*_ue_high.zip") igual que el botón
# "Añadir al proyecto" del complemento de Fab: mismo proceso de Interchange, mismos materiales y carpetas
# (/Game/Fab/Megascans/<tipo>/<Nombre>_<id>/<calidad>). Necesita el complemento de Fab instalado en el motor.
#
# Los .zip se buscan en Saved/FabDescargas (los baja Claude con tu sesión de fab.com). Ya importados, se saltean.
# Correr con el editor cerrado:
#   UnrealEditor-Cmd.exe HexaFrontera.uproject -run=pythonscript -script=<ruta>\importar_fab.py -EnablePlugins=PythonScriptPlugin
import json
import os
import re
import zipfile

import unreal

DOWNLOADS = os.path.join(unreal.Paths.project_saved_dir(), "FabDescargas")
# Tipo de Megascans (primera categoría del .json) → carpeta y modo de importación del complemento.
KINDS = {"3d": ("3D", unreal.MegascanImportType.MODEL3D), "3dplant": ("Plants", unreal.MegascanImportType.PLANT)}


def sanitize(text):
    return re.sub(r"[^A-Za-z0-9_]+", "_", text).strip("_")


def pipelines(kind):
    """Los mismos ajustes que FQuixelGltfImporter::ImportGltf3DAsset / ImportGltfPlantAsset."""
    assets = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline")
    gltf = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFPipeline")
    mesh = assets.get_editor_property("mesh_pipeline")
    material = assets.get_editor_property("material_pipeline")
    mesh.set_editor_property("import_static_meshes", True)
    mesh.set_editor_property("import_skeletal_meshes", False)
    mesh.set_editor_property("build_nanite", True)
    material.set_editor_property("material_import", unreal.InterchangeMaterialImportOption.IMPORT_AS_MATERIAL_INSTANCES)
    plant = kind == unreal.MegascanImportType.PLANT
    mesh.set_editor_property("collision", not plant)
    material.get_editor_property("texture_pipeline").set_editor_property("flip_normal_map_green_channel", plant)
    common = assets.get_editor_property("common_meshes_properties")
    common.set_editor_property("recompute_normals", plant)
    common.set_editor_property("compute_weighted_normals", plant)
    megascans = unreal.InterchangeMegascansPipeline()
    megascans.set_editor_property("megascan_import_type", kind)
    return [assets, gltf, megascans]


def import_zip(path):
    folder = path[:-4] + "_extracted"
    if not os.path.isdir(folder):
        with zipfile.ZipFile(path) as archive:
            archive.extractall(folder)
    gltf = next((os.path.join(folder, f) for f in os.listdir(folder) if f.endswith(".gltf")), None)
    meta = next((os.path.join(folder, f) for f in os.listdir(folder) if f.endswith(".json")), None)
    if not gltf or not meta:
        unreal.log_warning(f"Fab: {os.path.basename(path)} no trae glTF y .json")
        return False
    info = json.load(open(meta, encoding="utf-8"))
    categories = info.get("categories") or [""]
    if categories[0] not in KINDS:
        unreal.log_warning(f"Fab: {info.get('name')} es de tipo '{categories[0]}', no se importa")
        return False
    subfolder, kind = KINDS[categories[0]]
    tier = "High" if "_high" in os.path.basename(path) else "Raw" if "_raw" in os.path.basename(path) else "Medium"
    destination = f"/Game/Fab/Megascans/{subfolder}/{sanitize(info['name'])}_{info['id']}/{tier}"
    if unreal.EditorAssetLibrary.does_directory_exist(destination) and unreal.EditorAssetLibrary.list_assets(destination):
        unreal.log(f"Fab: ya estaba {info['name']}")
        return True
    source = unreal.InterchangeManager.create_source_data(gltf)
    stack = pipelines(kind)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines = [unreal.SoftObjectPath(p.get_path_name()) for p in stack]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    ok = manager.import_asset(destination, source, params)
    imported = unreal.EditorAssetLibrary.list_assets(destination)
    meshes = [a for a in imported if unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name == "StaticMesh"]
    unreal.log(f"Fab: {info['name']} → {destination} ({len(meshes)} mallas, {len(imported)} recursos)")
    unreal.EditorAssetLibrary.save_directory(destination)
    return bool(meshes)


def prepare_materials():
    """El juego no puede corregir materiales: sin estos usos, Nanite y el pasto instanciado se ven grises."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    instances = [d for d in registry.get_assets_by_path("/Game/Fab", recursive=True) if str(d.asset_class_path.asset_name) == "MaterialInstanceConstant"]
    roots = {}
    for data in instances:
        material = unreal.load_asset(str(data.package_name))
        while isinstance(material, unreal.MaterialInstance):
            material = material.get_editor_property("parent")
        if material:
            roots[material.get_path_name()] = material
    for material in roots.values():
        changed = False
        for flag in ("used_with_nanite", "used_with_instanced_static_meshes"):
            if not material.get_editor_property(flag):
                material.set_editor_property(flag, True)
                changed = True
        if changed:
            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material, False)
            unreal.log(f"Fab: usos de Nanite e instancias activados en {material.get_name()}")
    # Guardadas de nuevo dejan de recompilarse en cada arranque.
    for data in instances:
        unreal.EditorAssetLibrary.save_asset(str(data.package_name), False)
    unreal.log(f"Fab: {len(roots)} materiales base y {len(instances)} instancias listos para el juego")


def main():
    zips = sorted(f for f in os.listdir(DOWNLOADS) if f.endswith(".zip")) if os.path.isdir(DOWNLOADS) else []
    done = sum(import_zip(os.path.join(DOWNLOADS, f)) for f in zips)
    unreal.log(f"Fab: {done} de {len(zips)} modelos importados")
    prepare_materials()


main()
