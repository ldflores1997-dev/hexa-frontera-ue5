# Reglas de Hexa Frontera

Se juega de 2 a 4 jugadores. La meta de **Prestigio** depende del mundo (17 o 18 con 2 jugadores) y baja 1 por cada jugador después del segundo.

Cuando alguien llega a la meta empieza la **ronda final**: se juega hasta completar la ronda, así todos tienen la misma cantidad de turnos. Gana quien tenga más prestigio; si hay empate, gana el que juega más tarde en la ronda.

## Los dados de terreno

Se juega con dos dados de 6 caras. Las caras no tienen números: muestran **terrenos**, con su color.

- **Dado de terreno** (claro): cada terreno que produce, más una cara de **evento del mundo**, de color rojo oscuro con un cono. Si sobran caras, son comodines.
- **Dado de región** (bronce): cada terreno que produce, y **comodines** en las caras que sobran, dorados con una esfera.

En los mundos de 4 recursos el dado de terreno tiene 4 terrenos, un comodín y el evento, y el de región tiene 4 terrenos y 2 comodines. En Andes y valles, con 5 recursos, cada dado tiene los 5 terrenos más el evento o un comodín.

Cada mundo puede definir sus propios dados en el JSON (ver el final).

## Preparación

Se juega en orden serpiente: 1, 2, 3, 4 y después 4, 3, 2, 1. Cada jugador coloca un **puesto** en un vértice y una **ruta** pegada a ese puesto, sin pagar. Dos puestos nunca pueden quedar en vértices vecinos. Con el segundo puesto se recibe 1 recurso por cada casilla que lo toca.

Cada jugador empieza con monedas y 2 de Influencia. Los que juegan más tarde reciben más monedas para compensar.

## El turno

1. **Refuerzo (opcional)**: gastá 1 de Influencia para reforzar una casilla junto a tus construcciones. En esta tirada esa casilla produce aunque no salga su terreno: 1 por puesto y 2 por centro. Si sale su terreno, da 1 más a cada construcción.
2. **Dados**: se tiran los dos dados de terreno.
   - Producen todas las casillas de los **terrenos que muestran los dados**. Cada construcción que toca una de esas casillas recibe 1 por puesto y 2 por centro.
   - **Dobles**: si los dos dados muestran el mismo terreno, produce el doble.
   - **Evento**: si el dado de terreno muestra el evento, ocurre el **evento del mundo** y el terreno del otro dado produce igual. Sale 1 de cada 6 tiradas.
   - **Comodín**: el que tiró elige qué terreno produce. Con dos comodines, el terreno elegido produce el doble.
3. **Acciones**, en cualquier orden y tantas veces como puedas pagar:
   - **Puesto**: en un vértice conectado a una ruta tuya y lejos de otros puestos.
   - **Ruta**: en una arista conectada a tus rutas o construcciones. No puede atravesar un puesto ajeno.
   - **Centro**: mejora un puesto propio. Produce el doble y da +1 de Influencia al inicio de cada turno tuyo, hasta un máximo de 6.
   - **Obra pública**: cuesta 1 de cada recurso y da 1 de Prestigio. Como máximo 5 por jugador. No ocupa lugar en el tablero.
   - **Mercado**: comprar y vender recursos por monedas. Cada compra sube el precio de ese recurso y cada venta lo baja. Al terminar cada ronda los precios vuelven de a poco a su equilibrio.
   - **Intercambio** con otro jugador: tiene que aceptarlo. Los bots aceptan si reciben al menos lo mismo que entregan.
4. **Terminar el turno.**

Cada turno tiene un **reloj** (se elige en el menú: 60, 90 o 120 segundos, o sin límite). Si se acaba, los dados se tiran solos, el comodín se elige solo o el turno termina. En la preparación, el puesto o la ruta se colocan en el mejor lugar disponible.

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
| Islas volcánicas | La lava cubre una casilla y la convierte en campo de obsidiana. Las pasarelas de sus 6 lados se destruyen. |
| Colonia en Marte | Tormenta de polvo: 3 casillas no producen durante 2 rondas. |
| Andes y valles | Helada: una casilla y sus vecinas producen la mitad (redondeando hacia abajo) durante la ronda. |
| Ciudad flotante | Desacople: dos plataformas intercambian su terreno y su estado. |

## Crear un mundo nuevo

Copiá un `.json` de `Unreal/HexaFrontera/Content/Scenarios/` y cambiá recursos, terrenos, costos, evento, contratos y ambiente. La suma de `count` de los terrenos tiene que ser igual a la cantidad de casillas: 19 con radio 2, 37 con radio 3. Para dados propios, agregá `"dice": {"terrain": [...], "region": [...]}`, con 6 caras cada uno: ids de terreno, `"event"` o `"wild"`. Cada terreno que produce tiene que aparecer en algún dado. Si algo está mal, el juego muestra qué. Probá el equilibrio con `hf_simulate`. Para que aparezca en el simulador y en los tests, agregá su id en `ScenarioIds()` de `CoreTests/TestUtil.h`.
