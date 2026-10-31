# Compilar y jugar

## Requisitos

- Windows 10/11 con una placa de video compatible con DirectX 12. Para Lumen se recomienda una GTX 1070 / RX 5600 o superior.
- **Unreal Engine 5.4 o posterior**, instalado desde el Epic Games Launcher.
- **Visual Studio 2022** con estas cargas de trabajo:
  - "Desarrollo de juegos con C++", con el componente *Unreal Engine installer*;
  - "Desarrollo para el escritorio con C++".
  - En componentes individuales: el SDK de Windows 10/11 más reciente y *MSVC v143*.
- Git y Git LFS.

## Revisar e instalar automáticamente

En la carpeta del repo hay dos scripts de PowerShell:

1. **Revisar qué falta**: hacé clic derecho en `Tools\windows\revisar_pc.ps1` y elegí **Ejecutar con PowerShell**. Revisa Windows, RAM, placa de video, espacio en disco, Visual Studio, Epic Games Launcher, Unreal y Git. Solo lee, no cambia nada. Deja un informe en `Tools\windows\revisar_pc.txt`.
2. **Instalar lo que falta**: abrí PowerShell **como administrador**, entrá a la carpeta del repo y corré:
   ```
   powershell -ExecutionPolicy Bypass -File Tools\windows\instalar_requisitos.ps1
   ```
   Instala Visual Studio 2022 Community con las cargas de C++ para juegos, el Epic Games Launcher y Git LFS.
3. **Unreal Engine** se instala a mano, porque pide iniciar sesión: abrí el Epic Games Launcher y andá a **Unreal Engine → Biblioteca → +**. Elegí la versión 5.4 o posterior y tocá **Instalar**.

## Primera vez


1. Cloná el repo: `git clone` y después `git lfs install`.
2. En `Unreal/HexaFrontera/`, hacé clic derecho sobre `HexaFrontera.uproject` y elegí **Generate Visual Studio project files**. Si usás otra versión de UE, primero elegí **Switch Unreal Engine version**.
3. Abrí `HexaFrontera.sln`, elegí la configuración **Development Editor** y la plataforma **Win64**, y compilá con **Ctrl+Shift+B**.
4. Abrí `HexaFrontera.uproject`. La primera vez el editor compila shaders: tarda varios minutos.
5. Creá el mapa de la mesa. Se hace una sola vez:
   1. **File → New Level → Empty Level**.
   2. **File → Save Current Level As…** → `Content/Maps/Mesa`.

   No hace falta poner nada en el mapa: el juego arma el cielo, la luz y el tablero por código.
6. Apretá **Play**. En el menú elegí el mundo y la cantidad de jugadores, marcá cada asiento como Humano o Bot y apretá **Comenzar partida**.

## Controles

| Acción | Control |
|---|---|
| Construir | Botón en el panel derecho y clic en un punto resaltado del tablero |
| Girar la cámara | Clic derecho + arrastrar, o Q / E |
| Zoom | Rueda del mouse |
| Mover la cámara | WASD o flechas |
| Tirar los dados / terminar el turno | Espacio |
| Cancelar la herramienta | Esc |

## Si algo falla

- **Error de compilación**: copiá el primer error de la ventana *Output* de Visual Studio y pasámelo, y lo corrijo.
- **Las casillas se ven "por dentro" o no se ven**: en `Config/DefaultGame.ini` cambiá `bFlipWinding=False` por `True`.
- **Todo se ve gris o blanco**: el material base del motor no tiene el parámetro de color. Creá un material `M_Color` con un parámetro vectorial llamado `Color` conectado a *Base Color*, y en `Config/DefaultGame.ini`, sección `[/Script/HexaFrontera.HFBoardActor]`, agregá `BaseMaterial=/Game/M_Color.M_Color`.
- **"No se encontraron escenarios"**: verificá que existan los `.json` en `Content/Scenarios/`.

## Empaquetar el juego

En **Platforms → Windows → Package Project**. Los `.json` de los escenarios se copian solos: ya están configurados en `DefaultGame.ini`.
