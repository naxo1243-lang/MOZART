# Mozart Generative MIDI (VST3)

Port a VST3 / Standalone del generador `midi-scale-generator.html`: 23 géneros, estructura de canción
(intro / verso / estribillo / puente), progresiones, melodía con motivos, bajo, pad y baterías con fills.

El motor (`Source/Engine.cpp`) es un port 1:1 del JavaScript original, incluido el PRNG `mulberry32`:
**la misma semilla produce exactamente las mismas notas que la versión web.**

## Cómo se comporta como plugin

* Es un **instrumento VST3 con salida MIDI**. Al reproducir el transporte del DAW, el patrón generado
  suena **sincronizado al tempo y la posición del host** y se repite en bucle cada N compases.
* Cada rol sale por su propio canal MIDI: Melodía = 1, Armonía = 2, Bajo = 3, Pad = 4, Batería = 10.
* **Drag MIDI into your DAW**: arrastra el archivo .mid (todas las pistas) directamente a una pista del DAW.
  Es la vía universal y funciona aunque el host no enrute MIDI de plugins.
* **Preview**: reloj interno + sintetizador incorporado para escuchar sin DAW (o en el Standalone).
* **Play built-in sound while host plays**: activa el sintetizador interno también con el transporte del host.
* Los ticks de cada pista silencian/activan ese rol (tanto la salida MIDI como el sonido interno).
* **Keep** copia la semilla de la pieza actual al campo Seed para fijarla.
* El estado (ajustes + semilla) se guarda con el proyecto: al reabrirlo suena la misma pieza.

### Enrutar la salida MIDI en el DAW

Carga el plugin en una pista de instrumento y manda su salida MIDI a otra pista con tu sintetizador:
Reaper (envío MIDI / pista de entrada), Bitwig, FL Studio (MIDI out) y otros hosts que soporten salida
MIDI de VST3. **Ableton Live no puede enrutar la salida MIDI de un plugin**: ahí usa el botón de arrastre.

## Compilar

Requisitos: CMake 3.22+, Git, y un compilador C++17 (Visual Studio 2019/2022, Xcode, o GCC/Clang en Linux).
JUCE 8.0.4 se descarga solo con `FetchContent` (necesita internet la primera vez).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

* El VST3 queda en `build/MozartMidi_artefacts/Release/VST3/` y, por defecto, se copia a la carpeta de
  plugins del sistema (usa `-DMOZART_COPY_AFTER_BUILD=OFF` para evitarlo; en Windows puede requerir
  ejecutar la terminal como administrador).
* El ejecutable Standalone queda en `.../Standalone/`.
* Linux: instala antes las dependencias habituales de JUCE (ALSA, X11, freetype, fontconfig, GL).

### Sin instalar nada: GitHub Actions

Sube esta carpeta a un repositorio de GitHub; `.github/workflows/build.yml` compila Windows y macOS
(universal arm64 + x86_64) y deja los binarios como *artifacts* de cada ejecución.
Los binarios de macOS compilados así no están firmados/notarizados: el sistema pedirá permitir su ejecución.

## Estructura

```
Source/Engine.*          motor generativo + escritor de .mid (C++ puro, sin JUCE)
Source/PreviewSynth.h    sintetizador de preview (sin JUCE)
Source/PluginProcessor.* procesador JUCE: transporte, salida MIDI, estado
Source/PluginEditor.*    interfaz
```

## Licencia

JUCE se distribuye bajo AGPLv3 o con licencia comercial. Si vas a distribuir este plugin sin publicar el
código fuente, necesitas una licencia comercial de JUCE.
