; Cueva de volcado POR ITERACION (diferencial por iteracion, plan PARIDAD §9 "Siguiente" (1)).
; El disparador (CAVE_TRIG) es la direccion DS de un byte: se empieza a registrar la primera vez que es != 0.
; Se engancha donde el bucle del callejon llama a update_animation (igual que cave.asm), pero mira CADA llamada
; (una por iteracion del bucle; en DOSBox son cientos por tick BIOS):
;   - mientras enemy_active == 0 mantiene en PREVBUF una copia de DS (la iteracion anterior);
;   - la primera vez que enemy_active != 0 escribe la iteracion anterior COMPLETA (registro 1) y, desde ahi, para cada
;     iteracion escribe solo las PALABRAS de DS que cambiaron respecto a la anterior (registros delta).
;   - se detiene cuando pasaron CAVE_TICKLIM ticks BIOS desde el primer registro, o tras CAVE_MAXREC registros.
; Ventanas de DS: A = 0000..2E3F y B = 5900..5B1F (lo demas del segmento no se registra).
; Formato de STATE.BIN: registros consecutivos
;   [contador:2][tick BIOS:2][nd:2]  y despues
;     nd == 0xFFFF : DUMP_LEN bytes = ventana A y despues ventana B completas (solo el registro 1)
;     si no        : nd pares [offset en DS:2][palabra nueva:2]
; Todos los registros son del mismo punto del bucle (tras process_keyboard y poll_joystick, antes de update_animation).
; Lector: tools/orig_state/read_iter.py.
; Modo VIDEO (build_dump_exe.py --video, N4 del plan): ademas de DS se registra la RAM de video CGA (B800:0000..3FFF, 16 KB).
;   Cada registro lleva DETRAS de la parte de DS una seccion de video:
;     registro completo (nd == 0xFFFF): 0x4000 bytes de video
;     registro delta                  : [nv:2] y nv pares [offset 0..3FFE:2][palabra nueva:2]
;   La copia previa (VPREV) y el buffer de salida (VOUT) viven en un segmento DOS libre (VSEG), fuera de la imagen del .exe
;   (el .exe pide toda la memoria: maxalloc=0xFFFF), porque en el segmento de codigo ya no caben 16 KB mas.
bits 16
%ifdef VIDEO
VIDEO_SEG    equ 0xb800
VIDEO_LEN    equ 0x4000
VSEG         equ 0x3000                 ; VPREV = VSEG:0000..3FFF ; VOUT = VSEG:4000.. (hasta 8 KB palabras x 4 + 2)
VOUT         equ 0x4000
%endif
org CAVE_ORG
A_LEN        equ 0x2e40                 ; ventana A: DS:0000..2E3F (juego, enemigo, nivel actual, window_open_state hasta 0x2e1f)
B_BASE       equ 0x5900                 ; ventana B: DS:5900..5B1F (estado de sonido: chase_*, tone_*, ambient_*, ...)
B_LEN        equ 0x220
DUMP_LEN     equ A_LEN+B_LEN            ; tamano del registro completo = ventana A seguida de ventana B
MAXREC       equ CAVE_MAXREC
TICKLIM      equ CAVE_TICKLIM
ENEMY_ACTIVE equ CAVE_TRIG              ; DS:byte que dispara el registro (por defecto enemy_active=0x1cb8; key_up=0x6b8, ...; ver build_dump_exe.py --trig)
cave:
    pushf
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    cld
    cmp word [cs:finished],0
    jne .out
    mov [cs:gds],ds
    mov ax,0x40
    mov es,ax
    mov ax,[es:0x6c]
    mov [cs:cur_tick],ax
    cmp word [cs:recording],0
    jne .delta
    cmp byte [ENEMY_ACTIVE],0           ; DS = DS del juego aqui
    jne .start
    ; sin perro: copiar esta iteracion a PREVBUF
    push cs
    pop es
    xor si,si
    mov di,prevbuf
    mov cx,A_LEN/2
    rep movsw
    mov si,B_BASE
    mov cx,B_LEN/2
    rep movsw
%ifdef VIDEO
    push ds
    mov ax,VIDEO_SEG
    mov ds,ax
    mov ax,VSEG
    mov es,ax
    xor si,si
    xor di,di
    mov cx,VIDEO_LEN/2
    rep movsw
    pop ds
%endif
    mov ax,[cs:cur_tick]
    mov [cs:prev_tick],ax
    mov word [cs:have_prev],1
    jmp .out
.start:
    cmp word [cs:have_prev],0
    je .out                             ; el perro ya estaba: sin iteracion anterior que registrar
    mov word [cs:recording],1
    mov ax,[cs:prev_tick]
    mov [cs:first_tick],ax
    ; registro 1: cabecera + PREVBUF completo
    inc word [cs:hdr_count]
    mov [cs:hdr_tick],ax
    mov word [cs:hdr_nd],0xffff
    push cs
    pop ds
    mov dx,hdr_count
    mov cx,6
    call append
    mov dx,prevbuf
    mov cx,DUMP_LEN
    call append
%ifdef VIDEO
    mov ax,VSEG
    mov ds,ax
    xor dx,dx
    mov cx,VIDEO_LEN
    call append
%endif
    mov ds,[cs:gds]
    inc word [cs:nrec]
.delta:
    ; fin por tiempo?
    mov ax,[cs:cur_tick]
    sub ax,[cs:first_tick]
    cmp ax,TICKLIM
    jae .stop
    ; construir el delta (DS actual contra PREVBUF) en OUTBUF y dejar PREVBUF = DS actual
    push cs
    pop es
    mov di,outbuf+6
    xor si,si
    mov bx,prevbuf
    xor dx,dx
    mov cx,A_LEN/2
    mov byte [cs:pass],0
.l:
    lodsw                               ; ax = [ds:si]; si += 2
    cmp ax,[cs:bx]
    je .same
    mov [cs:bx],ax
    mov [es:di+2],ax
    mov ax,si
    sub ax,2
    mov [es:di],ax
    add di,4
    inc dx
.same:
    add bx,2
    loop .l
    cmp byte [cs:pass],0
    jne .done
    mov byte [cs:pass],1                ; segunda pasada: ventana B (bx, di y dx continuan)
    mov si,B_BASE
    mov cx,B_LEN/2
    jmp .l
.done:
    inc word [cs:hdr_count]
    mov ax,[cs:hdr_count]
    mov [cs:outbuf],ax
    mov ax,[cs:cur_tick]
    mov [cs:outbuf+2],ax
    mov [cs:outbuf+4],dx
    push cs
    pop ds
    mov dx,outbuf
    mov cx,di
    sub cx,outbuf
    call append
    mov ds,[cs:gds]
%ifdef VIDEO
    ; delta de video: B800 contra VPREV (que queda = B800 actual); la seccion [nv][pares] se arma en VSEG:VOUT
    mov ax,VSEG
    mov es,ax
    mov ax,VIDEO_SEG
    mov ds,ax
    xor si,si
    xor bx,bx
    mov di,VOUT+2
    xor dx,dx
    mov cx,VIDEO_LEN/2
.vl:
    lodsw
    cmp ax,[es:bx]
    je .vsame
    mov [es:bx],ax
    mov [es:di+2],ax
    mov ax,si
    sub ax,2
    mov [es:di],ax
    add di,4
    inc dx
.vsame:
    add bx,2
    loop .vl
    mov [es:VOUT],dx
    mov cx,di
    sub cx,VOUT
    mov ax,es
    mov ds,ax
    mov dx,VOUT
    call append
    mov ds,[cs:gds]
%endif
    inc word [cs:nrec]
    cmp word [cs:nrec],MAXREC
    jb .out
.stop:
    mov word [cs:finished],1
.out:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    jmp near ORIG_TARGET

; append: anade CX bytes desde DS:DX a STATE.BIN (lo crea la primera vez). Preserva registros.
append:
    push ax
    push bx
    push cx
    push dx
    push ds
    mov [cs:ap_seg],ds
    mov [cs:ap_off],dx
    mov [cs:ap_len],cx
    push cs
    pop ds
    cmp word [created],0
    jne .open
    mov dx,fname
    xor cx,cx
    mov ah,0x3c
    int 0x21
    jc .fail
    mov bx,ax
    mov ah,0x3e
    int 0x21
    mov word [created],1
.open:
    mov dx,fname
    mov ax,0x3d01
    int 0x21
    jc .fail
    mov bx,ax
    mov ax,0x4202
    xor cx,cx
    xor dx,dx
    int 0x21
    mov ds,[cs:ap_seg]
    mov dx,[cs:ap_off]
    mov cx,[cs:ap_len]
    mov ah,0x40
    int 0x21
    push cs
    pop ds
    mov ah,0x3e
    int 0x21
.fail:
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    ret
gds:        dw 0
pass:       dw 0
cur_tick:   dw 0
prev_tick:  dw 0
first_tick: dw 0
have_prev:  dw 0
recording:  dw 0
finished:   dw 0
nrec:       dw 0
created:    dw 0
ap_seg:     dw 0
ap_off:     dw 0
ap_len:     dw 0
hdr_count:  dw 0
hdr_tick:   dw 0
hdr_nd:     dw 0
fname:      db 'STATE.BIN',0
prevbuf:    times DUMP_LEN db 0
outbuf:     times (6 + 4 * (DUMP_LEN / 2)) db 0
