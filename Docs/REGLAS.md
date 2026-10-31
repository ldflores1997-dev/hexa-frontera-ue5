# Reglas de Hexa Frontera

Se juega de 2 a 4 jugadores. Gana el primero que llega a la meta de **Prestigio** en su turno. La meta depende del mundo (15 o 16 con 2 jugadores) y baja 1 por cada jugador después del segundo.

## El tablero

Cada casilla que produce lleva un **número del 2 al 12**. El 7 no aparece en el tablero. En cada tanda de 18 casillas hay un 2, un 12 y dos de cada número del 3 al 11. Los números que más salen (6 y 8) nunca quedan uno al lado del otro. El tablero muestra el número arriba de cada casilla: los rojos son los que más salen.

## Preparación

Se juega en orden serpiente: 1, 2, 3, 4 y después 4, 3, 2, 1. Cada jugador coloca un **puesto** en un vértice y una **ruta** pegada a ese puesto, sin pagar. Dos puestos nunca pueden quedar en vértices vecinos. Con el segundo puesto se recibe 1 recurso por cada casilla que lo toca.

Cada jugador empieza con monedas y 2 de Influencia. Los que juegan más tarde reciben más monedas para compensar.

## El turno

1. **Refuerzo (opcional)**: gastá 1 de Influencia para reforzar una casilla junto a tus construcciones. En esta tirada esa casilla produce aunque no salga su número: 1 por puesto y 2 por centro. Si sale su número, da 1 más a cada construcción.
2. **Dados**: se tiran dos dados.
   - Producen todas las casillas cuyo número es la suma. Cada construcción que toca una de esas casillas recibe 2 por puesto y 4 por centro.
   - **Con 7** nadie produce por número: ocurre el **evento del mundo**. Sale 1 de cada 6 tiradas.
3. **Acciones**, en cualquier orden y tantas veces como puedas pagar:
   - **Puesto**: en un vértice conectado a una ruta tuya y lejos de otros puestos.
   - **Ruta**: en una arista conectada a tus rutas o construcciones. No puede atravesar un puesto ajeno.
   - **Centro**: mejora un puesto propio. Produce el doble y da +1 de Influencia al inicio de cada turno tuyo, hasta un máximo de 6.
   - **Obra pública**: cuesta 1 de cada recurso y da 1 de Prestigio. Como máximo 3 por jugador. No ocupa lugar en el tablero.
   - **Mercado**: comprar y vender recursos por monedas. Cada compra sube el precio de ese recurso y cada venta lo baja. Al terminar cada ronda los precios vuelven de a poco a su equilibrio.
   - **Intercambio** con otro jugador: tiene que aceptarlo. Los bots aceptan si reciben al menos lo mismo que entregan.
4. **Terminar el turno.**

Cada turno tiene un **reloj** (se elige en el menú: 60, 90 o 120 segundos, o sin límite). Si se acaba, los dados se tiran solos o el turno termina. En la preparación, el puesto o la ruta se colocan en el mejor lugar disponible.

## Prestigio

- Puesto: 1. Centro: 2. Obra pública: 1.
- **Contratos**: objetivos públicos de cada mundo. El primero que cumple uno se lo queda.

  | Contrato | Se cumple con |
  |---|---|
  | Red de rutas | Una cantidad de rutas |
  | Centros | Una cantidad de centros |
  | Alcance de terreno | Tocar una cantidad de casillas de un terreno |
  | Diversidad | Tocar al menos 2 casillas de cada uno de N recursos |
  | Monedas | Juntar una cantidad de monedas |

## Eventos del mundo

| Mundo | Evento |
|---|---|
| Islas volcánicas | La lava cubre una casilla y la convierte en campo de obsidiana (conserva su número; si no tenía, recibe uno). Las pasarelas de sus 6 lados se destruyen. |
| Colonia en Marte | Tormenta de polvo: 3 casillas no producen durante 2 rondas. |
| Andes y valles | Helada: una casilla y sus vecinas producen la mitad (redondeando hacia abajo) durante la ronda. |
| Ciudad flotante | Desacople: dos plataformas intercambian su terreno, su número y su estado. |

## Crear un mundo nuevo

Copiá un `.json` de `Unreal/HexaFrontera/Content/Scenarios/` y cambiá recursos, terrenos, costos, evento, contratos y ambiente. La suma de `count` de los terrenos tiene que ser igual a la cantidad de casillas: 19 con radio 2, 37 con radio 3. Si algo está mal, el juego muestra qué. Probá el equilibrio con `hf_simulate`. Para que aparezca en el simulador y en los tests, agregá su id en `ScenarioIds()` de `CoreTests/TestUtil.h`.
