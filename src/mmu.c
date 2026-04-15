/* ** por compatibilidad se omiten tildes **
================================================================================
 TRABAJO PRACTICO 3 - System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================

  Definicion de funciones del manejador de memoria
*/

#include "mmu.h"
#include "i386.h"

#include "kassert.h"

static pd_entry_t* kpd = (pd_entry_t*)KERNEL_PAGE_DIR;
static pt_entry_t* kpt = (pt_entry_t*)KERNEL_PAGE_TABLE_0;

// static const uint32_t identity_mapping_end = 0x003FFFFF;
// static const uint32_t user_memory_pool_end = 0x02FFFFFF;

static paddr_t next_free_kernel_page = 0x100000;
static paddr_t next_free_user_page = 0x400000;

/**
 * kmemset asigna el valor c a un rango de memoria interpretado
 * como un rango de bytes de largo n que comienza en s
 * @param s es el puntero al comienzo del rango de memoria
 * @param c es el valor a asignar en cada byte de s[0..n-1]
 * @param n es el tamaño en bytes a asignar
 * @return devuelve el puntero al rango modificado (alias de s)
*/
static inline void* kmemset(void* s, int c, size_t n) {
  uint8_t* dst = (uint8_t*)s;
  for (size_t i = 0; i < n; i++) {
    dst[i] = c;
  }
  return dst;
}

/**
 * zero_page limpia el contenido de una página que comienza en addr
 * @param addr es la dirección del comienzo de la página a limpiar
*/
static inline void zero_page(paddr_t addr) {
  kmemset((void*)addr, 0x00, PAGE_SIZE);
}


void mmu_init(void) {}


/**
 * mmu_next_free_kernel_page devuelve la dirección física de la próxima página de kernel disponible. 
 * Las páginas se obtienen en forma incremental, siendo la primera: next_free_kernel_page
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de kernel
 */
paddr_t mmu_next_free_kernel_page(void) {
  paddr_t resultado = next_free_kernel_page;
  next_free_kernel_page += PAGE_SIZE;
  return resultado;
}

/**
 * mmu_next_free_user_page devuelve la dirección de la próxima página de usuarix disponible
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de usuarix
 */
paddr_t mmu_next_free_user_page(void) {
  paddr_t resultado = next_free_user_page;
  next_free_user_page += PAGE_SIZE;
  return resultado;
}

/**
 * mmu_init_kernel_dir inicializa las estructuras de paginación vinculadas al kernel y
 * realiza el identity mapping
 * @return devuelve la dirección de memoria de la página donde se encuentra el directorio
 * de páginas usado por el kernel
 */
paddr_t mmu_init_kernel_dir(void) {
  zero_page((paddr_t)KERNEL_PAGE_DIR); //limpiamos cualquier informacion incorrecta
  zero_page((paddr_t)KERNEL_PAGE_TABLE_0); 
  for(uint32_t paginaActual = 0; paginaActual < 1024; paginaActual++){
    kpt[paginaActual].page = paginaActual;
    kpt[paginaActual].attrs = MMU_P + MMU_W;
  }
  kpd[0].pt = (uint32_t)kpt >> 12;
  kpd[0].attrs = MMU_P + MMU_W;
  return (paddr_t)kpd;
}


/**
 * mmu_map_page agrega las entradas necesarias a las estructuras de paginación de modo de que
 * la dirección virtual virt se traduzca en la dirección física phy con los atributos definidos en attrs
 * @param cr3 el contenido que se ha de cargar en un registro CR3 al realizar la traducción
 * @param virt la dirección virtual que se ha de traducir en phy
 * @param phy la dirección física que debe ser accedida (dirección de destino)
 * @param attrs los atributos a asignar en la entrada de la tabla de páginas
 */
void mmu_map_page(uint32_t cr3, vaddr_t virt, paddr_t phy, uint32_t attrs) {
  pd_entry_t* entradaDirectorio = (pd_entry_t*) CR3_TO_PAGE_DIR(cr3);
  entradaDirectorio += VIRT_PAGE_DIR(virt); //ahora entradaDirectorio tendria que apuntar al PDE correspondiente
  if (entradaDirectorio->attrs % 2 == 0){
    paddr_t direccionNuevaPageTable = mmu_next_free_kernel_page();
    zero_page(direccionNuevaPageTable);
    entradaDirectorio->pt = direccionNuevaPageTable >> 12;
    entradaDirectorio->attrs = MMU_P + MMU_W + MMU_U;
  }
  pt_entry_t* entradaTabla = (pt_entry_t*) MMU_ENTRY_PADDR(entradaDirectorio->pt);
  entradaTabla += VIRT_PAGE_TABLE(virt);
  entradaTabla->page = phy >> 12;
  entradaTabla->attrs = attrs;
  tlbflush();
}

/**
 * mmu_unmap_page elimina la entrada vinculada a la dirección virt en la tabla de páginas correspondiente
 * @param virt la dirección virtual que se ha de desvincular
 * @return la dirección física de la página desvinculada
 */
paddr_t mmu_unmap_page(uint32_t cr3, vaddr_t virt) {
  pd_entry_t* entradaDirectorio = (pd_entry_t*) CR3_TO_PAGE_DIR(cr3);
  entradaDirectorio += VIRT_PAGE_DIR(virt); //ahora entradaDirectorio tendria que apuntar al PDE correspondiente
  pt_entry_t* entradaTabla = (pt_entry_t*) MMU_ENTRY_PADDR(entradaDirectorio->pt);
  entradaTabla += VIRT_PAGE_TABLE(virt);
  paddr_t direccionFisica = MMU_ENTRY_PADDR(entradaTabla->page) + VIRT_PAGE_OFFSET(virt);
  entradaTabla->attrs = 0x000;
  tlbflush();
  return direccionFisica;
}

#define DST_VIRT_PAGE 0xA00000
#define SRC_VIRT_PAGE 0xB00000

/**
 * copy_page copia el contenido de la página física localizada en la dirección src_addr a la página física ubicada en dst_addr
 * @param dst_addr la dirección a cuya página queremos copiar el contenido
 * @param src_addr la dirección de la página cuyo contenido queremos copiar
 *
 * Esta función mapea ambas páginas a las direcciones SRC_VIRT_PAGE y DST_VIRT_PAGE, respectivamente, realiza
 * la copia y luego desmapea las páginas. Usar la función rcr3 definida en i386.h para obtener el cr3 actual
 */
void copy_page(paddr_t dst_addr, paddr_t src_addr) {
  mmu_map_page(rcr3(), DST_VIRT_PAGE, dst_addr, MMU_P + MMU_W);
  mmu_map_page(rcr3(), SRC_VIRT_PAGE, src_addr, MMU_P);
  uint8_t* dst_virt_addr = (uint8_t *) DST_VIRT_PAGE;
  uint8_t* src_virt_addr = (uint8_t *) SRC_VIRT_PAGE;
  for (uint32_t i = 0; i < PAGE_SIZE; i++){
    dst_virt_addr[i] = src_virt_addr[i];
  }
  mmu_unmap_page(rcr3(), DST_VIRT_PAGE);
  mmu_unmap_page(rcr3(), SRC_VIRT_PAGE);
}

 /**
 * mmu_init_task_dir inicializa las estructuras de paginación vinculadas a una tarea cuyo código se encuentra en la dirección phy_start
 * @param phy_start es la dirección donde comienzan las dos páginas de código de la tarea asociada a esta llamada
 * @return el contenido que se ha de cargar en un registro CR3 para la tarea asociada a esta llamada
 */
paddr_t mmu_init_task_dir(paddr_t phy_start) {
  pd_entry_t * pagDirectorio = (pd_entry_t*) mmu_next_free_kernel_page();
  zero_page((paddr_t) pagDirectorio);
  pagDirectorio[0].pt = (uint32_t)kpt >> 12;
  pagDirectorio[0].attrs = MMU_P + MMU_W;
  mmu_map_page((uint32_t) pagDirectorio,TASK_CODE_VIRTUAL, phy_start, MMU_P + MMU_U);
  mmu_map_page((uint32_t) pagDirectorio,TASK_CODE_VIRTUAL + 0x1000, phy_start + 0x1000, MMU_P + MMU_U);
  paddr_t pagPila = mmu_next_free_user_page();
  mmu_map_page((uint32_t) pagDirectorio, TASK_STACK_BASE - 0x1000, pagPila, MMU_P + MMU_W + MMU_U);
  mmu_map_page((uint32_t) pagDirectorio, TASK_SHARED_PAGE, SHARED, MMU_P + MMU_U);
  return (paddr_t) pagDirectorio;
}

// COMPLETAR: devuelve true si se atendió el page fault y puede continuar la ejecución 
// y false si no se pudo atender
bool page_fault_handler(vaddr_t virt) {
  print("Atendiendo page fault...", 0, 0, C_FG_WHITE | C_BG_BLACK);
  // Chequeemos si el acceso fue dentro del area on-demand
  // En caso de que si, mapear la pagina
  if (virt >= ON_DEMAND_MEM_START_VIRTUAL && virt <= ON_DEMAND_MEM_END_VIRTUAL){
    mmu_map_page(rcr3(), virt, ON_DEMAND_MEM_START_PHYSICAL, MMU_P + MMU_U + MMU_W);
    return true;
  } else {
    return false;
  }
}
