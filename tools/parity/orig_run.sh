#!/usr/bin/env bash
# Ejecuta un .exe del original en DOSBox sin pantalla (Xvfb), envia teclas por cuadro y captura el escritorio.
# Todo en UNA llamada (los procesos en segundo plano no sobreviven entre llamadas; DOSBOX_SANDBOX.md §0).
# Uso: tools/parity/orig_run.sh EXE PREFIJO NCUADROS INTERVALO_SEG "tecla@cuadro down:Tecla@cuadro up:Tecla@cuadro ..."
#   DOSDIR (por defecto /tmp/dos) debe contener EXE, d.conf (DOSBOX_SANDBOX.md §2) y recibe STATE.BIN y las capturas.
# Secuencia de arranque tipica: "space@6 n@14 k@22 space@30" (titulo, joystick=No, Kitten, empezar).
DOSDIR=${DOSDIR:-/tmp/dos}
pkill dosbox 2>/dev/null; pkill Xvfb 2>/dev/null; sleep 1
export DISPLAY=:99
nohup Xvfb :99 -screen 0 800x600x24 >/dev/null 2>&1 &
sleep 2
cd "$DOSDIR" || exit 1
sed "s/^cat.*exe/$1/" d.conf > d2.conf
nohup dosbox -conf d2.conf >/tmp/dos.log 2>&1 &
sleep 1.5
xdotool mousemove 400 300 click 1
rm -f "$2"_*.png
for i in $(seq 1 "$3"); do
  import -window root "$2_$(printf %03d "$i").png"
  for kf in $5; do
    k=${kf%@*}; f=${kf#*@}
    if [ "$f" = "$i" ]; then
      case $k in
        down:*) xdotool keydown "${k#down:}";;
        up:*)   xdotool keyup "${k#up:}";;
        *)      xdotool key --clearmodifiers "$k";;
      esac
    fi
  done
  sleep "$4"
done
pkill dosbox; pkill Xvfb
