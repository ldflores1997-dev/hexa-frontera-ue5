# Crea en el Content Browser las carpetas de cada bioma: /Game/Biomas/<mundo>/<terreno>/<categoría>.
# En el editor: Tools → Execute Python Script… y elegí este archivo (necesita el plugin "Python Editor Script Plugin").
# Los mundos y terrenos se leen de Content/Scenarios/*.json, así que un mundo nuevo también aparece.
import json
import os

import unreal

ROOT = "/Game/Biomas"
CATEGORIES = ["Superficie", "Casilla", "Relieve", "Plantas", "Rocas", "Suelo", "Aves", "Fauna"]

scenarios = os.path.join(unreal.Paths.project_content_dir(), "Scenarios")
created = 0
for name in sorted(os.listdir(scenarios)):
    if not name.endswith(".json"):
        continue
    with open(os.path.join(scenarios, name), encoding="utf-8") as file:
        scenario = json.load(file)
    for terrain in scenario["terrains"]:
        for category in CATEGORIES:
            path = f"{ROOT}/{scenario['id']}/{terrain['id']}/{category}"
            if not unreal.EditorAssetLibrary.does_directory_exist(path):
                unreal.EditorAssetLibrary.make_directory(path)
                created += 1

unreal.log(f"Biomas: {created} carpetas nuevas en {ROOT}")
