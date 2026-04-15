#include "task_lib.h"

#define WIDTH TASK_VIEWPORT_WIDTH
#define HEIGHT TASK_VIEWPORT_HEIGHT

#define SHARED_SCORE_BASE_VADDR (PAGE_ON_DEMAND_BASE_VADDR + 0xF00)
#define CANT_PONGS 3


void task(void) {
	screen pantalla;
	task_draw_box(pantalla, 0, 0, WIDTH, HEIGHT, ' ', C_BG_MAGENTA);
	task_draw_box(pantalla, WIDTH / 2 - 15, HEIGHT / 2 - 2, 30, 4, ' ', C_BG_LIGHT_GREY);
	task_print(pantalla, "Pong Scoreboard", (WIDTH / 2) - 8, HEIGHT / 2 - 2, C_FG_BLACK + C_BG_LIGHT_GREY);
	task_print(pantalla, "Puntajes de Pong 1: ", (WIDTH / 2) - 12, (HEIGHT / 2) - 1, C_FG_BLACK + C_BG_LIGHT_GREY);
	task_print(pantalla, "Puntajes de Pong 2: ", (WIDTH / 2) - 12, (HEIGHT / 2), C_FG_BLACK + C_BG_LIGHT_GREY);
	task_print(pantalla, "Puntajes de Pong 3: ", (WIDTH / 2) - 12, (HEIGHT / 2) + 1, C_FG_BLACK + C_BG_LIGHT_GREY);
	uint32_t* memoriaCompartida = (uint32_t*) SHARED_SCORE_BASE_VADDR; 
	// ¿Una tarea debe terminar en nuestro sistema?
	while (true){
	// Completar:
	// - Pueden definir funciones auxiliares para imprimir en pantalla
	// - Pueden usar `task_print`, `task_print_dec`, etc.
		for(uint8_t i = 0; i < CANT_PONGS; i++){
			uint32_t puntaje = memoriaCompartida[i*2];
			task_print_dec(pantalla, puntaje, 2, (WIDTH / 2) + 8, (HEIGHT / 2) + i - 1,  C_FG_CYAN + C_BG_LIGHT_GREY);
			puntaje = memoriaCompartida[i*2 + 1];
			task_print_dec(pantalla, puntaje, 2, (WIDTH / 2) + 11, (HEIGHT / 2) + i - 1,  C_FG_MAGENTA + C_BG_LIGHT_GREY);
		}
		syscall_draw(pantalla);
	}
}
