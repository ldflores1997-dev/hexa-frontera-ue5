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
  - remolinos de polvo en el cráter de Marte y nieve en el nevado;
  - **fauna**: gaviotas sobre los palmares, cóndores planeando sobre el nevado, un dron entre los domos de Marte y dirigibles de seda que rodean la Ciudad flotante.

  Están en `AddDecoration`, `TickLife` y `TickCreatures`, en `HFBoardActor.cpp`.

Todo eso está hecho con cubos, conos y esferas de un color. Para llegar al aspecto fotorrealista, cada casilla toma modelos y materiales de verdad desde su **carpeta de bioma** (sección 1). No hace falta tocar C++.

## 0. Texturas y piezas del juego (ya aplicadas)

El repo ya trae en `Content/Biomas` texturas fotográficas de [Poly Haven](https://polyhaven.com) (gratis, licencia CC0) y piezas modeladas en Blender:

| Qué | De dónde sale | Dónde queda |
|---|---|---|
| Superficie de las 21 casillas | `Tools/unreal/texturas.json` | `Biomas/<mundo>/<terreno>/Superficie/MI_<terreno>` |
| Mar, arena o pasto alrededor del tablero | `_entorno` en `texturas.json` | `Biomas/<mundo>/_Entorno/MI_Suelo` |
| Marco del tablero (madera; metal en la Ciudad flotante) | `_juego` y `_entorno` | `Biomas/_Comun/MI_Madera_Marco`, `_Entorno/MI_Marco` |
| Piezas de madera pintada, fichas de terciado, fieltro de la bandeja | `_juego` | `Biomas/_Comun/MI_*` |
| Humo, vapor, polvo, tormenta y escarcha translúcidos | `M_Humo` | `Biomas/_Comun` |
| Dado redondeado, ficha biselada, puesto, centro y ruta | `Tools/blender/piezas.py` | `Biomas/_Comun/Piezas` |

Las cartas de recurso usan como ilustración la textura del terreno que produce ese recurso.

**Relieve**: la tapa de cada casilla sube y baja según el mapa de altura de su textura (piedras que sobresalen, grietas que se hunden), con sombras reales. Cuántos cm se elige con `relief` en `texturas.json`, y se regenera con `python Tools/relieves.py` (necesita `pip install pillow`); queda en `Content/Relieves`. El borde y el lugar de la ficha quedan planos.

**Para cambiar una textura**: elegí otra en polyhaven.com, poné su id en `Tools/unreal/texturas.json` y corré, con el editor cerrado:

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" Unreal/HexaFrontera/HexaFrontera.uproject -run=pythonscript -script=Tools/unreal/aplicar_texturas.py -EnablePlugins=PythonScriptPlugin
```

**Para cambiar las piezas**: editá `Tools/blender/piezas.py`, corré `blender --background --python Tools/blender/piezas.py` y después el script de arriba. Un mundo puede tener piezas propias en `Biomas/<mundo>/_Piezas/SM_Puesto` (y `SM_Centro`, `SM_Ruta`, `SM_Ficha`); la ranura de material 1 es el techo, que se pinta más oscuro.

**Para mirar el resultado sin jugar**: `-HFDemo=<mundo>` arranca una partida entre 4 bots, y `-HFShots=45,110` guarda capturas en `Saved/Screenshots` a esos segundos y cierra el juego:

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" Unreal/HexaFrontera/HexaFrontera.uproject -game -HFDemo=andes_valles
```

## 1. Biomas realistas: una carpeta por casilla

El juego busca los recursos de cada terreno en:

```
/Game/Biomas/<mundo>/<terreno>/<categoría>/
```

Por ejemplo, `/Game/Biomas/islas_volcanicas/palmar/Plantas/`. Los ids de mundos y terrenos son los de los `.json` de `Content/Scenarios/`.

**Para crear todas las carpetas de una vez**: en el editor, **Tools → Execute Python Script…** y elegí `Tools/unreal/crear_biomas.py`. Necesita el plugin *Python Editor Script Plugin* (Edit → Plugins).

| Categoría | Qué poner | Cómo se usa |
|---|---|---|
| `Superficie` | un material de superficie (Megascans *Surfaces*) | pinta la tapa de la casilla |
| `Casilla` | una malla de casilla (la de Blender, sección 3) | reemplaza la casilla generada |
| `Relieve` | una malla grande: volcán, montaña, cráter, domo | va en el centro y se escala para entrar en la casilla (`LandmarkHeight`) |
| `Plantas` | árboles, arbustos, cactus, corales | `PlantsPerTile` por casilla, a `PlantHeight` cm, meciéndose con el viento |
| `Rocas` | rocas y piedras grandes | `RocksPerTile` por casilla, a `RockHeight` cm, inclinadas y semienterradas |
| `Suelo` | pasto, flores, piedritas, hojas | `GroundCoverPerTile` copias instanciadas, a `GroundCoverHeight` cm |
| `Aves` | una malla de ave | `BirdsPerTile` aves dando vueltas sobre la casilla |
| `Fauna` | un animal: llama, cabra, cangrejo, robot | `FaunaPerTile` animales que caminan, se paran a pastar y siguen |

Reglas:

- **Cada subcarpeta se recorre entera.** Podés mover ahí la carpeta completa de un recurso de Fab, con sus texturas y materiales. Para moverla, arrastrala en el Content Browser y elegí *Move Here*.
- **Con cualquier modelo en `Relieve`, `Plantas`, `Rocas`, `Suelo`, `Aves` o `Fauna`, esa casilla deja de usar las formas básicas**, incluida su fauna simple. El humo, las chispas, las luces y las olas se mantienen.
- **El tamaño original no importa.** Cada malla se escala al alto configurado, con ±25% de variación, y se apoya por su base.
- **Aves y Fauna tienen que ser mallas estáticas.** Un animal esquelético de Fab se convierte abriéndolo y usando el botón *Make Static Mesh* de la barra del editor de mallas. Queda quieto en su pose, y el juego lo mueve entero. Si caminan de costado o de espaldas, cambiá `FaunaYawOffset` (90, 180 o -90).
- **Los tamaños y cantidades** se cambian en `Config/DefaultGame.ini`, sección `[/Script/HexaFrontera.HFBoardActor]`. Por ejemplo `PlantHeight=150.0` o `GroundCoverPerTile=200`.
- El empaquetado incluye siempre `/Game/Biomas` (`DirectoriesToAlwaysCook` en `DefaultGame.ini`).

Si preferís no mover los recursos, se pueden nombrar en `DefaultGame.ini`. Lo que va ahí manda sobre las carpetas. Las rutas de este ejemplo son inventadas: copiá las tuyas con clic derecho → *Copy Reference*.

```ini
+TerrainVisuals=(Scenario="andes_valles",Terrain="puna",Plants=("/Game/Fab/Megascans/3D/Pampas_Grass/SM_Pampas_Grass.SM_Pampas_Grass"),Fauna=("/Game/Fab/Llama/SM_Llama.SM_Llama"))
```

### Los modelos de Fab no están en el repositorio

La licencia de Fab no deja publicar sus modelos, y además pesan unos 6 GB. Por eso `Content/Fab` está en `.gitignore`: cada uno los agrega desde su propia biblioteca de Fab.

1. Agregá a tu biblioteca los modelos que figuran en `Tools/unreal/modelos_fab.json`.
2. Bajalos en formato glTF (*_ue_high.zip*) a `Saved/FabDescargas`.
3. Con el editor cerrado, corré `Tools/unreal/importar_fab.py` y después `Tools/unreal/ordenar_fab.py`. El primero los importa igual que el botón *Añadir al proyecto* y el segundo los reparte en las casillas (escribe el bloque `TerrainVisuals` de `DefaultGame.ini`).

Sin esos modelos el juego funciona igual: las casillas usan sus decoraciones de formas simples.

Todo lo que se agrega a una casilla (plantas, rocas, pasto, relieve, animales que caminan, partículas) se achica o se corre lo necesario para que la pieza entera quede dentro del círculo que entra en el hexágono (`DecorationRadius`). Solo las aves y los dirigibles pueden pasar por encima de otras casillas.

### Qué buscar en Fab para cada casilla

Abrí **Window → Fab** y filtrá por *Megascans* y gratis. Son búsquedas sugeridas, no nombres exactos.

| Mundo | Terreno | Superficie | Relieve / Plantas / Rocas | Suelo | Aves / Fauna |
|---|---|---|---|---|---|
| Islas | Campo de obsidiana | volcanic rock, lava field | basalt rocks, obsidian | volcanic gravel | — |
| Islas | Arrecife | beach sand, wet sand | coral (Plantas), beach rocks | seashells, pebbles | seagull / crab |
| Islas | Palmar | jungle floor | palm tree, tropical bush | fern, jungle grass | seagull, parrot |
| Islas | Fumarola | sulfur, yellow volcanic | sulfur rocks | volcanic gravel | — |
| Islas | Cráter activo | lava, volcanic rock | volcano (Relieve) | — | — |
| Marte | Casquete de hielo | ice, frozen ground | ice rocks | snow chunks | — |
| Marte | Regolito | red desert rock, Mars | desert boulders | red pebbles | rover (Fauna) |
| Marte | Campo solar | red sand | solar panel (Rocas, para que queden quietos) | red pebbles | — |
| Marte | Planta de oxígeno | red sand | habitat dome (Relieve) | pebbles | drone (Aves) |
| Marte | Cráter de impacto | crater, desert | crater (Relieve) | rocks | — |
| Andes | Mina de cobre | mountain rock, quarry | cliff rocks, ore rocks | gravel | — |
| Andes | Puna | dry grass, highland | pampas grass, ichu, shrubs | dry grass | llama, alpaca |
| Andes | Viñedo | soil, farmland | grapevine | weeds | — |
| Andes | Valle de maíz | farmland | corn plant | grass | bird |
| Andes | Salar | salt flat | salt rocks | salt crust | flamingo |
| Andes | Nevado | snow, rock | snowy mountain (Relieve) | snow rocks | condor (Aves) |
| Ciudad | Caldera | metal plate, rust | boiler, pipes | bolts, debris | — |
| Ciudad | Fundición | brass plate | anvil, crates | metal scraps | — |
| Ciudad | Carbonera | coal | coal pile | coal pieces | — |
| Ciudad | Telar | wooden planks | loom, fabric rolls | cloth scraps | — |
| Ciudad | Núcleo de levitación | sci-fi metal | crystal (Relieve) | crystals | — |

Para el agua del arrecife, la casilla mantiene sus olas y el material de `Superficie` se aplica a la tapa animada.

Las secciones siguientes son la forma anterior de configurar solo la superficie y la malla de la casilla; siguen funcionando.

## 2. Materiales de Quixel Megascans (gratis con Unreal)

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

## 3. Casillas modeladas en Blender

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

## 4. Más calidad

- **Project Settings → Rendering**: activá *Hardware Ray Tracing* para Lumen si tu placa lo soporta (RTX / RX 6000 o superior).
- Activá **Nanite** en las mallas de plantas, rocas y suelo (doble clic en la malla → *Nanite Settings → Enable*). Así pueden tener mucho detalle sin bajar los cuadros por segundo.
- El color de las piezas de cada jugador se cambia en `PlayerColors` del tablero.
