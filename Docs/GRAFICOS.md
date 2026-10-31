# Gráficos realistas

El juego arranca con casillas y decoraciones generadas por código. La escena ya tiene:

- iluminación global con Lumen;
- cielo físico con un tono distinto en cada mundo;
- nubes volumétricas, niebla volumétrica, sombras virtuales y bloom;
- **ciclo de día y noche** (8 minutos por día, `DayLengthMinutes` en `AHFEnvironment`): atardeceres naranjas, luz de luna y ventanas que se prenden en puestos y centros;
- **terrenos vivos**, todo por código:
  - árboles, pasto, cultivos, viñedos y corales que se mecen con el viento;
  - olas en el arrecife;
  - volcán con lava que late, humo y chispas;
  - vapor en las fumarolas;
  - chimeneas que humean;
  - engranajes que giran y banderas que flamean;
  - paneles solares que siguen al sol;
  - cristales y salares que destellan;
  - remolinos de polvo en el cráter de Marte y nieve en el nevado.

  Están en `AddDecoration` y `TickLife`, en `HFBoardActor.cpp`.

Para llegar al aspecto fotorrealista se reemplazan los materiales y las mallas de cada terreno. Todo se configura en `Config/DefaultGame.ini`, sin tocar C++.

## 1. Materiales de Quixel Megascans (gratis con Unreal)

1. En el editor abrí **Window → Fab** (o Quixel Bridge en versiones anteriores) y descargá superficies para cada terreno. Por ejemplo:

   | Mundo | Terreno | Superficie sugerida |
   |---|---|---|
   | Islas | Campo de obsidiana | "Volcanic Rock" |
   | Islas | Arrecife | "Beach Sand" con agua poco profunda |
   | Islas | Palmar | "Jungle Floor" |
   | Islas | Fumarola | "Sulfur" |
   | Marte | Regolito | "Red Desert Rock" |
   | Marte | Casquete | "Ice Sheet" |
   | Andes | Mina | "Mountain Rock" |
   | Andes | Puna | "Dry Grass" |
   | Andes | Salar | "Salt Flat" |
   | Ciudad | Fundición | "Brass Plate" |
   | Ciudad | Carbonera | "Coal" |

2. Copiá la ruta del material instancia: clic derecho en el asset → **Copy Reference**.
3. En `Config/DefaultGame.ini`, sección `[/Script/HexaFrontera.HFBoardActor]`, agregá una línea por terreno:

```ini
+TerrainVisuals=(Scenario="islas_volcanicas",Terrain="palmar",Material="/Game/Fab/Megascans/Surfaces/Jungle_Floor/MI_Jungle_Floor.MI_Jungle_Floor")
```

Los ids de mundos y terrenos están en los `.json` de `Content/Scenarios/`.

## 2. Casillas modeladas en Blender

`Tools/blender/hex_tiles.py` genera una casilla con bisel y relieve por cada terreno:

```bash
blender --background --python Tools/blender/hex_tiles.py
```

1. Importá los FBX de `Tools/blender/out/<mundo>/` con **Import**, y activá **Build Nanite** para que el relieve no cueste rendimiento.
2. Asignales el material de Megascans.
3. Agregá la malla en la misma línea:

```ini
+TerrainVisuals=(Scenario="islas_volcanicas",Terrain="palmar",Mesh="/Game/Tiles/palmar.palmar",Material="/Game/Fab/.../MI_Jungle_Floor.MI_Jungle_Floor")
```

La malla tiene que ser un hexágono de 1 m de radio, con la base en Z = 0 y 1 m de alto. El juego la escala al tamaño y la altura de cada casilla. Si aparece girada 30°, cambiá `ROTATE_DEG` en el script y volvé a exportar.

## 3. Más calidad

- **Project Settings → Rendering**: activá *Hardware Ray Tracing* para Lumen si tu placa lo soporta (RTX / RX 6000 o superior).
- Reemplazá las decoraciones (árboles, rocas, cristales) por modelos de Fab. Están en `AHFBoardActor::AddDecoration`, en `HFBoardActor.cpp`.
- El color de las piezas de cada jugador se cambia en `PlayerColors` del tablero.
