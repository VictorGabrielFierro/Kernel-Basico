; ** por compatibilidad se omiten tildes **
; ==============================================================================
; TALLER System Programming - Arquitectura y Organizacion de Computadoras - FCEN
; ==============================================================================

%include "print.mac"

global start


; COMPLETAR - Agreguen declaraciones extern según vayan necesitando
extern A20_enable
extern A20_check
extern GDT_DESC
extern IDT_DESC
extern screen_draw_layout
extern idt_init
extern pic_enable
extern pic_reset
extern mmu_init_kernel_dir
extern copy_page
extern mmu_init_task_dir
extern tss_init
extern tasks_screen_draw
extern sched_init
extern tasks_init

; COMPLETAR - Definan correctamente estas constantes cuando las necesiten
%define CS_RING_0_SEL 0008h
%define DS_RING_0_SEL 0018h


BITS 16
;; Saltear seccion de datos
jmp start

;;
;; Seccion de datos.
;; -------------------------------------------------------------------------- ;;
start_rm_msg db     'Iniciando kernel en Modo Real'
start_rm_len equ    $ - start_rm_msg

start_pm_msg db     'Iniciando kernel en Modo Protegido'
start_pm_len equ    $ - start_pm_msg

;;
;; Seccion de código.
;; -------------------------------------------------------------------------- ;;

;; Punto de entrada del kernel.
BITS 16
start:
    ; COMPLETAR - Deshabilitar interrupciones
    cli

    ; Cambiar modo de video a 80 X 50
    mov ax, 0003h
    int 10h ; set mode 03h
    xor bx, bx
    mov ax, 1112h
    int 10h ; load 8x8 font

    ; COMPLETAR - Imprimir mensaje de bienvenida - MODO REAL
    ; (revisar las funciones definidas en print.mac y los mensajes se encuentran en la
    ; sección de datos)
    print_text_rm start_rm_msg, start_rm_len, 0Fh, 0h, 0h

    ; COMPLETAR - Habilitar A20
    ; (revisar las funciones definidas en a20.asm)
    call A20_check
    cmp AX, 1
    je .cargarGDT
    call A20_enable

    ; COMPLETAR - Cargar la GDT
    .cargarGDT:
    lgdt [GDT_DESC]

    ; COMPLETAR - Setear el bit PE del registro CR0
    mov  eax, cr0
    or al,1
    mov  cr0, eax

    ; COMPLETAR - Saltar a modo protegido (far jump)
    ; (recuerden que un far jmp se especifica como jmp CS_selector:address)
    ; Pueden usar la constante CS_RING_0_SEL definida en este archivo
    jmp CS_RING_0_SEL:modo_protegido

BITS 32
modo_protegido:
    ; COMPLETAR - A partir de aca, todo el codigo se va a ejectutar en modo protegido
    ; Establecer selectores de segmentos DS, ES, GS, FS y SS en el segmento de datos de nivel 0
    ; Pueden usar la constante DS_RING_0_SEL definida en este archivo
    mov AX, DS_RING_0_SEL
    mov DS, AX
    mov ES, AX
    mov GS, AX
    mov FS, AX
    mov SS, AX

    ; COMPLETAR - Establecer el tope y la base de la pila
    mov ESP, 25000h
    mov EBP, ESP

    ; COMPLETAR - Imprimir mensaje de bienvenida - MODO PROTEGIDO

    print_text_pm start_pm_msg, start_pm_len, 0Fh, 2h, 0h

    ; COMPLETAR - Inicializar pantalla
    call screen_draw_layout
    
   
    ; Inicializar el directorio de paginas
    call mmu_init_kernel_dir

    ; Cargar directorio de paginas
    mov EAX, 00025000h
    mov CR3, EAX

    ; Habilitar paginacion
    mov EAX, CR0
    or EAX, 80000000h
    mov CR0, EAX

    ; aca testeamos que funcionara la función copy_page
    ; push 00000000h
    ; push 00600000h
    ; call copy_page

    ; Inicializar tss
    call tss_init

    ; Inicializar el scheduler
    call sched_init

    ; Inicializar las tareas
    call tasks_init

    ; COMPLETAR - Inicializar y cargar la IDT
    call idt_init
    lidt [IDT_DESC]

    ; COMPLETAR - Reiniciar y habilitar el controlador de interrupciones
    call pic_reset
    call pic_enable

    ; El PIT (Programmable Interrupt Timer) corre a 1193182Hz.
    ; Cada iteracion del clock decrementa un contador interno, cuando éste llega
    ; a cero se emite la interrupción. El valor inicial es 0x0 que indica 65536,
    ; es decir 18.206 Hz
    ; Ajustamos el PIT para que la velocidad de ejecución de las tareas sea agradable 
    mov ax, 0x007FF
    out 0x40, al
    rol ax, 8
    out 0x40, al

    ; Cargar tarea inicial
    call tasks_screen_draw
    mov AX, 0058h
    ltr AX

    ; COMPLETAR - Habilitar interrupciones
    ; NOTA: Pueden chequear que las interrupciones funcionen forzando a que se
    ;       dispare alguna excepción (lo más sencillo es usar la instrucción
    ;       `int3`)
    ;sti
    ;int3

    ; Probar Sys_call
    ;int 88
    ;int 98

    ; Probar generar una excepción
    ;excepciones funcionan bien
    ;xor ECX, ECX ; EDX = 0
    ;idiv ECX

    ; Inicializar el directorio de paginas de la tarea de prueba
    push 0x00500000
    call mmu_init_task_dir ;En EAX tenemos el valor a cargar en CR3

    ; Cargar directorio de paginas de la tarea
    mov CR3, EAX ;cargamos el page directory de la tarea ficticia
    mov dword[0x07000010], 0x12345678
    mov dword[0x07000110], 0x87654321

    ; Restaurar directorio de paginas del kernel   
    mov EAX, 00025000h
    mov CR3, EAX

    ; Saltar a la primera tarea: Idle
    jmp 0060h:0

    ; Ciclar infinitamente 
    mov eax, 0xFFFF
    mov ebx, 0xFFFF
    mov ecx, 0xFFFF
    mov edx, 0xFFFF
    jmp $

;; -------------------------------------------------------------------------- ;;

%include "a20.asm"
