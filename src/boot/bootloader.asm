BITS 16
ORG 0x7C00

start:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov si, msg_loading
    call print_string

    mov ah, 0x02
    mov al, 4
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x00
    mov bx, 0x0600
    int 0x13

    jc disk_error

    jmp 0x0000:0x0600

disk_error:
    mov si, msg_fail
    call print_string
    hlt

print_string:
    .loop:
        lodsb
        or al, al
        jz .done
        mov ah, 0x0E
        int 0x10
        jmp .loop
    .done:
        ret

msg_loading db "booting...", 0
msg_fail    db "disk error", 0

TIMES 510 - ($ - $$) db 0
DW 0xAA55