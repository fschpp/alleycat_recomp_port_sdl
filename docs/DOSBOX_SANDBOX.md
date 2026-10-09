# Ejecutar el `cat.exe` original en el sandbox (DOSBox sin pantalla)

Guía para comparar el port (`alleycat_recomp_port_sdl`) contra el juego original
(Alley Cat, IBM 1984) dentro del sandbox de Claude, sin perder tiempo. Probado con
DOSBox 0.74-3 y `cat.exe` de 55 067 bytes (MZ, sin empaquetar).

Resultado: se pueden capturar cuadros del juego original, incluso de escenas a las que
normalmente no se llega (p. ej. la cinematica de victoria del nivel 7), y compararlos con
los cuadros que genera el port.

---

## 0. Reglas aprendidas (leer primero)

1. **Los procesos en segundo plano NO sobreviven entre llamadas a la herramienta `bash`.**
   Xvfb y DOSBox tienen que arrancar, usarse y matarse dentro de **la misma** llamada.
   Lo que no cabe en una llamada se pierde (síntoma: `Can't open display: (null)`).
2. `SDL_VIDEODRIVER=dummy` **no sirve** si quieres capturas ni teclas. Funciona mejor
   **Xvfb + xdotool + ImageMagick (`import`)**: el juego se ve y se le pueden enviar teclas.
3. Las teclas solo llegan si antes haces un clic en la ventana
   (`xdotool mousemove 400 300 click 1`).
4. `apt-get install` funciona en el sandbox (el dominio `archive.ubuntu.com` está permitido).
5. El repo de desensamblado necesario también se clona sin problema:
   `https://github.com/gmegidish/alleycat-disassembly` (el mismo que usa `tools/setup.sh`
   del port). El ASM de ahí es la **fuente de verdad** para leer qué hace el original.

---

## 1. Instalación (una vez por sesión)

```bash
apt-get install -y dosbox xvfb xdotool imagemagick   # ~1 min
python3 -c "import PIL"                               # Pillow ya viene instalado
mkdir -p /tmp/dos && cp /mnt/user-data/uploads/cat.exe /tmp/dos/
cd /home/claude && git clone -q --depth 1 https://github.com/gmegidish/alleycat-disassembly.git
```

## 2. Configuración de DOSBox (`/tmp/dos/d.conf`)

```ini
[sdl]
fullscreen=false
output=surface
[dosbox]
machine=cga
[cpu]
cycles=max
[autoexec]
mount c /tmp/dos
c:
cat.exe
```

`machine=cga` es importante: el juego usa modo CGA 320x200 de 4 colores (paleta 1:
cian/magenta/blanco; el nivel 7 se ve verde/rojo en el port por la paleta que aplica).

## 3. Script de ejecución y captura (`/tmp/dos/run2.sh`)

Hace todo en **una** llamada: arranca Xvfb, arranca DOSBox, envía teclas en cuadros
concretos, captura la pantalla, y lo mata todo al final.

```bash
#!/bin/bash
# uso: run2.sh EXE PREFIJO NCUADROS INTERVALO_SEG "tecla@cuadro tecla@cuadro ..."
pkill dosbox; pkill Xvfb; sleep 1
export DISPLAY=:99
nohup Xvfb :99 -screen 0 800x600x24 >/dev/null 2>&1 &
sleep 2
cd /tmp/dos
sed "s/^cat.*exe/$1/" d.conf > d2.conf          # sustituye la linea del ejecutable
nohup dosbox -conf d2.conf >/tmp/dos.log 2>&1 &
sleep 1.5
xdotool mousemove 400 300 click 1                # dar foco a la ventana
rm -f $2_*.png
for i in $(seq 1 $3); do
  import -window root $2_$(printf %03d $i).png
  for kf in $5; do
    k=${kf%@*}; f=${kf#*@}
    if [ "$f" = "$i" ]; then xdotool key --clearmodifiers $k; fi
  done
  sleep $4
done
pkill dosbox; pkill Xvfb
```

`chmod +x /tmp/dos/run2.sh`. Las capturas son del escritorio de 800x600; el área del
juego es `crop((80,100,720,500))` (640x400, el doble del 320x200 nativo).

## 4. Secuencia de teclas del juego

El título es una animación que no responde hasta pulsar una tecla:

| Cuadro (0,25 s c/u) | Tecla | Efecto |
|---|---|---|
| ~6 | `space` | sale del título |
| ~14 | `n` | "¿Joystick?" → No |
| ~22 | `k` | dificultad Kitten (`h`/`t`/`a` = House Cat/Tomcat/Alley Cat) |
| ~30 | `space` | "Press any key to start" → empieza el juego |

```bash
./run2.sh cat.exe x 160 0.25 "space@6 n@14 k@22 space@30"
```

Si se desfasan los tiempos, mira los cuadros intermedios y ajusta (el título tarda lo mismo
en cada ejecución, pero la máquina del sandbox puede ir más lenta o rápida).

## 5. Llegar a escenas a las que no se llega jugando (parchear el .exe)

No hay forma de "saltar de nivel" por teclado, así que se parchea una **copia** del `.exe`
(nunca el original). Los desplazamientos de archivo dependen de **este** `cat.exe`; por eso
los scripts buscan patrones de bytes en vez de fijar offsets.

Datos útiles:

- Cabecera MZ: `e_cparhdr=32` (512 bytes), `CS=0x723`, así que
  **offset de archivo = 0x7430 + offset dentro del segmento de código** (las etiquetas
  `lab_XXXX` del ASM son offsets de CS).
- Las etiquetas del ASM están en `alleycat-disassembly/src/*.asm`.

### 5.1 Forzar la cinematica de victoria del nivel 7 (`catv7.exe`)

Hace falta: (a) que `level_transition` ejecute `run_victory_sequence` siempre, y (b) que
al empezar el juego se salte directo al despacho del nivel 7.

```python
import struct
d = bytearray(open('/tmp/dos/cat.exe','rb').read())

# (a) level_transition: cmp [6],7 / jnz  y  cmp byte [0x553],0 / jz  -> NOP
pat = bytes.fromhex('2bdbb40bcd10833e06000775')
i = d.find(pat); assert d.count(pat) == 1
j = i + 11                       # el 75 xx
assert d[j] == 0x75
k = j + 2                        # 80 3e 53 05 00
m = k + 5                        # el 74 xx
assert d[k] == 0x80 and d[m] == 0x74
d[j:j+2] = b'\x90\x90'; d[m:m+2] = b'\x90\x90'

# (b1) tabla de saltos de niveles: niveles 0 y 1 -> manejador del nivel 7 (lab_0260)
tab = bytes.fromhex('e203e2035904940349' '03fe02aa026002')
t = d.find(tab); assert d.count(tab) == 1
d[t:t+4] = b'\x60\x02\x60\x02'

# (b2) al empezar (lab_00f3) saltar a lab_01b7 (manejador de muerte) y forzar la ruta
#      "force_level7" (NOP del jz que decide)
base = 0x7430
a = base + 0x1b7
f = d.find(bytes.fromhex('803e18040074 0e'.replace(' ','')), a)
assert f - a < 64
d[f+5:f+7] = b'\x90\x90'
d[base+0xf3:base+0xf3+3] = b'\xe9' + struct.pack('<h', 0x1b7 - (0xf3 + 3))

open('/tmp/dos/catv7.exe','wb').write(d)
```

Uso:

```bash
./run2.sh catv7.exe z 200 0.05 "space@6 n@14 k@22 space@30"
```

El juego sale del menú y arranca la cinematica de victoria (fondo = pantalla de texto del
menú, porque no se limpia; no importa). Cuadros de interés: aproximadamente 45–115.

Para otras escenas: busca el manejador en `entry.asm` (tabla de saltos `jmp word [cs:bx+0x250]`,
niveles 2–7 en `lab_0459`, `lab_0394`, `lab_0349`, `lab_02fe`, `lab_02aa`, `lab_0260`) y
cambia la entrada correspondiente de la tabla de saltos.

## 6. Ver y comparar

Hoja de contacto (cada N cuadros) con Pillow:

```python
from PIL import Image
import glob
fs = sorted(glob.glob('/tmp/dos/z_*.png'))
sel = [Image.open(fs[i]).crop((80,100,720,500)).resize((320,200)) for i in range(30,200,7)][:24]
cols = 4; rows = (len(sel)+cols-1)//cols
sheet = Image.new('RGB', (320*cols, 200*rows))
for i, im in enumerate(sel): sheet.paste(im, ((i%cols)*320, (i//cols)*200))
sheet.save('/tmp/dos/sheet.png')      # luego: view /tmp/dos/sheet.png
```

Para filtrar cuadros repetidos, calcula `hashlib.md5(im.tobytes())` y quédate con los únicos.

### Lado del port (sin SDL2)

Para comparar, ejecuta la misma escena en el port **sin ventana**: un programa de prueba
que llama a la función, con un reloj falso (`l7_tick_fn`) y un gancho (`l7_step_hook`) que
vuelca `cga_mem` a un archivo en cada presentación; después se renderiza con la paleta que
se quiera (0=fondo, 1..3). Plantilla en el historial del proyecto: `/tmp/vis.c`
(compila con `$(make -pn | grep -m1 '^TEST_SRC' | sed 's/.*= //')` y
`-D_POSIX_C_SOURCE=199309L -Iinclude`). Condiciones iniciales a igualar con el original:
`cat_x=0`, `cat_y=0` (valores de arranque del juego original).

Layout de `cga_mem` (CGA 320x200, 4 colores): fila `y` empieza en
`(y>>1)*80 + (y&1)*0x2000`; 4 píxeles por byte, el primero en los bits altos.

## 7. Qué se descubrió con esto (como ejemplo del método)

- Un blit del original usaba `mov cx,0xd04`: **cl = ancho en words (4), ch = alto (13)**; el
  port lo pasaba invertido (13x4) y dibujaba ruido (T84c).
- El original **no borra** el corazón de pasos anteriores: pasa `bp=0xe` a `blit_transparent`
  solo como buffer basura. La estela es el comportamiento correcto.
- `l7_cupid_active` se pone a 0 al final de **cada** pasada de espera (`lab_520c`), así que
  tras la primera pasada se dibujan los 8 corazones (T84d).

Regla práctica: ante un bug visual, **primero** ver el original corriendo y **luego**
leer el ASM; las dimensiones (`cx`), los registros `bp`/`si`/`di` y los flags de bucle son
donde el port más se ha desviado.

## 8. Problemas frecuentes

| Síntoma | Causa / solución |
|---|---|
| `Can't open display: (null)` | Xvfb/DOSBox murieron al terminar la llamada anterior. Todo en una sola llamada. |
| La pantalla se queda en "Do you want to use a Joystick?" | La tecla no llegó: falta el clic de foco, o el tiempo de `n` es anterior a que aparezca el prompt. |
| Todos los cuadros son iguales (1 único) | El juego espera una tecla; revisa la secuencia y los tiempos de la sección 4. |
| `xdotool: Failed creating new xdo instance` | `DISPLAY` no está exportado en esa llamada. |
| El parche falla en `assert` | El `cat.exe` es otra versión; busca los patrones en el ASM y recalcula (sección 5). |
| Sonido: errores de ALSA en `/tmp/dos.log` | Ignorables; no hay tarjeta de sonido en el sandbox. |
