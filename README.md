# Hexa Frontera

Juego de mesa 3D original para **Unreal Engine 5**, programado en **C++**. Se juega en un tablero de hexágonos donde se producen recursos, se comercia y se construye. Cada mundo tiene un evento que cambia el tablero durante la partida.

| Mundo | Recursos | Evento |
|---|---|---|
| **Islas volcánicas** | obsidiana, coral, palma, azufre | **Erupción**: la lava convierte una casilla en campo de obsidiana y quema las pasarelas que la tocan |
| **Colonia en Marte** | hielo, mineral, energía, oxígeno | **Tormenta de polvo**: 3 casillas no producen durante 2 rondas |
| **Andes y valles** | cobre, lana, uva, maíz, sal | **Helada**: una casilla y sus vecinas producen la mitad |
| **Ciudad flotante** | vapor, latón, carbón, seda | **Desacople**: dos plataformas intercambian su lugar |

Las reglas completas están en [Docs/REGLAS.md](Docs/REGLAS.md).

## Estado

- **Núcleo de reglas** (C++17 puro, en `Unreal/HexaFrontera/Source/HexaFrontera/Core/`): completo y con 20 tests automáticos. Incluye partidas completas entre bots en los 4 mundos, con 2 a 4 jugadores.
- **Capa de Unreal**:
  - tablero 3D procedural con decoraciones por terreno;
  - piezas animadas;
  - iluminación por mundo con Lumen, cielo físico, nubes volumétricas y niebla;
  - cámara de mesa;
  - interfaz con menú, mercado, intercambio y contratos;
  - bots.

  Está escrita para UE 5.4 o posterior, **pero todavía no se compiló en el motor**: la primera compilación puede dar errores menores. Ver [Docs/COMPILAR.md](Docs/COMPILAR.md).
- **Gráficos fotorrealistas**: el juego arranca con formas generadas por código. Para el aspecto realista se agregan materiales de Quixel Megascans y casillas modeladas en Blender, sin tocar código. Ver [Docs/GRAFICOS.md](Docs/GRAFICOS.md).

## Estructura

```
Unreal/HexaFrontera/              proyecto de Unreal (abrir HexaFrontera.uproject)
  Source/HexaFrontera/Core/       reglas del juego, sin dependencias del motor
  Source/HexaFrontera/HF*.cpp     tablero 3D, cámara, controles, interfaz, ambiente
  Content/Scenarios/*.json        los 4 mundos (se pueden agregar más sin programar)
  Config/                         Lumen, Nanite, sombras virtuales, modo de juego
CoreTests/                        tests y simulador de equilibrio (CMake)
Tools/blender/hex_tiles.py        genera casillas detalladas por terreno y las exporta a FBX
Docs/                             reglas, compilación y guía de gráficos
```

## Tests del núcleo

Se corren en cualquier PC con CMake y un compilador de C++17. No hace falta Unreal.

```bash
cmake -S CoreTests -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/hf_simulate 200   # equilibrio: duración y victorias por asiento en cada mundo
```
