# Reglas de Hexa Frontera

Se juega de 2 a 4 jugadores. Gana el primero que llega a la meta de **Prestigio** en su turno. La meta depende del mundo (15 o 16 con 2 jugadores) y baja 1 por cada jugador después del segundo.

## Preparación

Se juega en orden serpiente: 1, 2, 3, 4 y después 4, 3, 2, 1. Cada jugador coloca un **puesto** en un vértice y una **ruta** pegada a ese puesto, sin pagar. Dos puestos nunca pueden quedar en vértices vecinos. Con el segundo puesto se recibe 1 recurso por cada casilla que lo toca.

Cada jugador empieza con monedas y 2 de Influencia. Los que juegan más tarde reciben más monedas para compensar.

## El turno

1. **Refuerzo (opcional)**: gastá 1 de Influencia para reforzar una casilla junto a tus construcciones. Esa casilla da +1 a cada construcción en este Pulso.
2. **Pulso**: se revela una carta. Hay tres tipos:
   - **Dos terrenos** (x1): todas las casillas de esos terrenos producen.
   - **Un terreno** (x1): producen solo las casillas de ese terreno.
   - **Evento del mundo**: ocurre el evento y nadie produce.

   Cada construcción tocada por una casilla que produce recibe la intensidad de la carta multiplicada por el nivel (puesto 1, centro 2). Cuando el mazo se termina, se mezcla de nuevo.
3. **Acciones**, en cualquier orden y tantas veces como puedas pagar:
   - **Puesto**: en un vértice conectado a una ruta tuya y lejos de otros puestos.
   - **Ruta**: en una arista conectada a tus rutas o construcciones. No puede atravesar un puesto ajeno.
   - **Centro**: mejora un puesto propio. Produce el doble y da +1 de Influencia al inicio de cada turno tuyo, hasta un máximo de 6.
   - **Obra pública**: cuesta 1 de cada recurso y da 1 de Prestigio. Como máximo 3 por jugador. No ocupa lugar en el tablero.
   - **Mercado**: comprar y vender recursos por monedas. Cada compra sube el precio de ese recurso y cada venta lo baja. Al terminar cada ronda los precios vuelven de a poco a su equilibrio.
   - **Intercambio** con otro jugador: tiene que aceptarlo. Los bots aceptan si reciben al menos lo mismo que entregan.
4. **Terminar el turno.**

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

Copiá un `.json` de `Unreal/HexaFrontera/Content/Scenarios/` y cambiá recursos, terrenos, costos, evento, contratos y ambiente. La suma de `count` de los terrenos tiene que ser igual a la cantidad de casillas: 19 con radio 2, 37 con radio 3. Si algo está mal, el juego muestra qué. Probá el equilibrio con `hf_simulate`. Para que aparezca en el simulador y en los tests, agregá su id en `ScenarioIds()` de `CoreTests/TestUtil.h`.
