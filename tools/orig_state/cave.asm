; Cueva de volcado de estado (tools/state_dump): se engancha donde el juego llama a update_animation.
; Por cada tick BIOS distinto, anade a STATE.BIN: [contador:2][tick:2][DS:0000..DUMP_LEN-1 (0x2c00: cubre rng_seed en 0x2ae5)].
bits 16
org CAVE_ORG
DUMP_LEN equ 0x2c00
cave:
    pushf
    push ax
    push bx
    push cx
    push dx
    push ds
    push es
    mov [cs:gds],ds
    mov ax,0x40
    mov es,ax
    mov ax,[es:0x6c]
    cmp ax,[cs:last_tick]
    je .done
    mov [cs:last_tick],ax
    mov [cs:hdr_tick],ax
    inc word [cs:hdr_count]
    push cs
    pop ds                      ; DS = CS para el nombre y la cabecera
    cmp word [created],0
    jne .open
    mov dx,fname
    xor cx,cx
    mov ah,0x3c                 ; crear/truncar
    int 0x21
    jc .done
    mov bx,ax
    mov ah,0x3e
    int 0x21
    mov word [created],1
.open:
    mov dx,fname
    mov ax,0x3d01               ; abrir para escritura
    int 0x21
    jc .done
    mov bx,ax
    mov ax,0x4202               ; ir al final
    xor cx,cx
    xor dx,dx
    int 0x21
    mov dx,hdr_count
    mov cx,4
    mov ah,0x40
    int 0x21
    mov ds,[cs:gds]             ; DS del juego
    xor dx,dx
    mov cx,DUMP_LEN
    mov ah,0x40
    int 0x21
    mov ah,0x3e
    int 0x21
.done:
    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    jmp near ORIG_TARGET
gds:        dw 0
last_tick:  dw 0xffff
created:    dw 0
hdr_count:  dw 0
hdr_tick:   dw 0
fname:      db 'STATE.BIN',0
