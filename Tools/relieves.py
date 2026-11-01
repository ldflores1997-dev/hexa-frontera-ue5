"""
Arma el relieve de cada casilla a partir del mapa de altura (Displacement) de su textura de Poly Haven.

Uso (desde la carpeta del repo, con Python 3 y Pillow: pip install pillow):
    python Tools/relieves.py

Lee Tools/unreal/texturas.json y escribe Unreal/HexaFrontera/Content/Relieves/<mundo>/<terreno>.r16.
El tablero sube y baja la tapa de la casilla según ese archivo, alineado con la textura (misma escala en cm).

Formato del .r16: "HFR1", tamaño (uint32), cm por repetición (float), cm de relieve (float),
y tamaño × tamaño alturas uint16 (0 = lo más bajo, 65535 = lo más alto), fila por fila.
"""

import json
import os
import struct
import urllib.request

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PICKS = os.path.join(ROOT, "Tools", "unreal", "texturas.json")
CACHE = os.path.join(ROOT, "Unreal", "HexaFrontera", "Saved", "PolyHaven")
OUT = os.path.join(ROOT, "Unreal", "HexaFrontera", "Content", "Relieves")
SIZE = 128            # alturas por lado de una repetición: ~2 cm cada una con 250 cm de textura
DEFAULT_TILING = 250  # el mismo valor por defecto que el material M_Casilla


def download(texture_id):
    path = os.path.join(CACHE, f"{texture_id}_Displacement.jpg")
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return path
    os.makedirs(CACHE, exist_ok=True)
    request = urllib.request.Request(f"https://api.polyhaven.com/files/{texture_id}", headers={"User-Agent": "HexaFrontera"})
    files = json.load(urllib.request.urlopen(request))
    url = files["Displacement"]["2k"]["jpg"]["url"]
    request = urllib.request.Request(url, headers={"User-Agent": "HexaFrontera"})
    with urllib.request.urlopen(request) as response, open(path, "wb") as out:
        out.write(response.read())
    return path


def heights(texture_id):
    # Los mapas de Poly Haven en jpg son de 8 bits; al achicarlos a 128 se promedian y quedan suaves.
    image = Image.open(download(texture_id)).convert("L").resize((SIZE, SIZE), Image.LANCZOS)
    values = list(image.tobytes())
    # Se estira entre los percentiles 1 y 99: así el relieve pedido se nota igual en todas las texturas.
    ordered = sorted(values)
    low, high = ordered[len(ordered) // 100], ordered[len(ordered) * 99 // 100]
    span = max(1, high - low)
    return [min(65535, max(0, (v - low) * 65535 // span)) for v in values]


def main():
    picks = json.load(open(PICKS, encoding="utf-8"))
    cache = {}
    for world, terrains in picks.items():
        if world.startswith("_"):
            continue
        for terrain, pick in terrains.items():
            amplitude = float(pick.get("relief", 0))
            path = os.path.join(OUT, world, f"{terrain}.r16")
            if amplitude <= 0:
                if os.path.exists(path):
                    os.remove(path)
                continue
            if pick["id"] not in cache:
                cache[pick["id"]] = heights(pick["id"])
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as out:
                out.write(b"HFR1" + struct.pack("<Iff", SIZE, float(pick.get("tiling", DEFAULT_TILING)), amplitude))
                out.write(struct.pack(f"<{SIZE * SIZE}H", *cache[pick["id"]]))
            print(f"Relieve: {world}/{terrain} = {pick['id']} ({amplitude:g} cm)")


main()
