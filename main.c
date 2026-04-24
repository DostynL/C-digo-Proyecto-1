/**
 * Proyecto1 - Versión Alternativa (secuencia 9)
 * Autores: Jonathan Cofiño, Dostyn López
 * Fecha: 21/04/2026
 * UVG - Microprocesadores
 */

#include <stdint.h>

/* ----------------------------------------------------------------
   MAPA DE REGISTROS - STM32F446RE
   ---------------------------------------------------------------- */

#define BASE_RCC   0x40023800UL
#define BASE_PA    0x40020000UL
#define BASE_PB    0x40020400UL
#define BASE_PC    0x40020800UL
#define BASE_UART2 0x40004400UL

#define REG_RCC_AHB1  (*(volatile uint32_t *)(BASE_RCC   + 0x30))
#define REG_RCC_APB1  (*(volatile uint32_t *)(BASE_RCC   + 0x40))
#define REG_PA_MODE   (*(volatile uint32_t *)(BASE_PA    + 0x00))
#define REG_PB_MODE   (*(volatile uint32_t *)(BASE_PB    + 0x00))
#define REG_PC_MODE   (*(volatile uint32_t *)(BASE_PC    + 0x00))
#define REG_PA_PULL   (*(volatile uint32_t *)(BASE_PA    + 0x0C))
#define REG_PA_IN     (*(volatile uint32_t *)(BASE_PA    + 0x10))
#define REG_PA_SET    (*(volatile uint32_t *)(BASE_PA    + 0x18))
#define REG_PB_SET    (*(volatile uint32_t *)(BASE_PB    + 0x18))
#define REG_PC_SET    (*(volatile uint32_t *)(BASE_PC    + 0x18))
#define REG_PA_AFL    (*(volatile uint32_t *)(BASE_PA    + 0x20))
#define REG_U2_ESTADO (*(volatile uint32_t *)(BASE_UART2 + 0x00))
#define REG_U2_DATO   (*(volatile uint32_t *)(BASE_UART2 + 0x04))
#define REG_U2_BAUD   (*(volatile uint32_t *)(BASE_UART2 + 0x08))
#define REG_U2_CTRL   (*(volatile uint32_t *)(BASE_UART2 + 0x0C))
#define TICK_CTRL     (*(volatile uint32_t *)(0xE000E010))
#define TICK_CARGA    (*(volatile uint32_t *)(0xE000E014))
#define TICK_VALOR    (*(volatile uint32_t *)(0xE000E018))

/* ----------------------------------------------------------------
   PINES - LEDs de secuencia (8 Principales)
   ---------------------------------------------------------------- */

#define PIN_L1  (1u << 5)    /* PA5  - D13 */
#define PIN_L2  (1u << 6)    /* PA6  - D12 */
#define PIN_L3  (1u << 7)    /* PA7  - D11 */
#define PIN_L4  (1u << 6)    /* PB6  - D10 */
#define PIN_L5  (1u << 7)    /* PC7  - D9  */
#define PIN_L6  (1u << 4)    /* PB4  - D5  */
#define PIN_L7  (1u << 3)    /* PB3  - D3  */
#define PIN_L8  (1u << 10)   /* PA10 - D2  */

#define RESET_PA  (PIN_L1 | PIN_L2 | PIN_L3 | PIN_L8)
#define RESET_PB  (PIN_L4 | PIN_L6 | PIN_L7)
#define RESET_PC  (PIN_L5)

/* ----------------------------------------------------------------
   PINES - Botones
   ---------------------------------------------------------------- */

#define BOTON_SUBIR  (1u << 0)   /* PA0 */
#define BOTON_BAJAR  (1u << 1)   /* PA1 */
#define BOTON_PARAR  (1u << 9)   /* PA9 - D8 */

/* ----------------------------------------------------------------
   CONSTANTES
   ---------------------------------------------------------------- */

#define MS_LENTO   250u
#define MS_NORMAL   125u
#define MS_VELOZ    62u
#define MS_REBOTE   10u
#define RELOJ_HZ    16000000UL

/* ----------------------------------------------------------------
   ESTADO GLOBAL
   ---------------------------------------------------------------- */

volatile uint32_t ms_transcurridos = 0;
volatile uint32_t ms_ultimo_paso   = 0;
volatile uint32_t ms_ultimo_boton  = 0;

uint32_t intervalo_ms = MS_LENTO;
uint8_t  indice_vel   = 0;
uint8_t  paso_actual  = 1;
uint8_t  en_pausa     = 0;
uint8_t  prev_subir   = 0;
uint8_t  prev_bajar   = 0;
uint8_t  prev_parar   = 0;

/* ----------------------------------------------------------------
   PROTOTIPOS
   ---------------------------------------------------------------- */

void sistema_init(void);
void pines_salida_init(void);
void pines_entrada_init(void);
void uart_init(void);
void tick_init(void);
void leds_apagar(void);
void leds_escribir(uint8_t paso);
void botones_procesar(void);
void secuencia_avanzar(void);
void uart_char(char c);
void uart_texto(const char *txt);
void uart_numero(uint32_t n);
void uart_reporte_vel(void);

/* ================================================================
   MAIN
   ================================================================ */

int main(void) {
    sistema_init();

    for (;;) {
        botones_procesar();
        if (!en_pausa &&
            (ms_transcurridos - ms_ultimo_paso) >= intervalo_ms) {
            ms_ultimo_paso = ms_transcurridos;
            secuencia_avanzar();
        }
    }
}

/* ================================================================
   LÓGICA DE SECUENCIA
   ================================================================ */

void secuencia_avanzar(void) {
    paso_actual = (paso_actual >= 8u) ? 1u : paso_actual + 1u;
    leds_escribir(paso_actual);
}

/* ================================================================
   BOTONES
   ================================================================ */

void botones_procesar(void) {
    uint32_t pines = REG_PA_IN;
    uint8_t s = (pines & BOTON_SUBIR) ? 1u : 0u;
    uint8_t b = (pines & BOTON_BAJAR) ? 1u : 0u;
    uint8_t p = (pines & BOTON_PARAR) ? 1u : 0u;
    uint32_t dt = ms_transcurridos - ms_ultimo_boton;

    if (s && !prev_subir && dt >= MS_REBOTE) {
        if (indice_vel < 2u) {
            indice_vel++;
            intervalo_ms = (indice_vel == 1u) ? MS_NORMAL : MS_VELOZ;
            uart_reporte_vel();
        } else {
            uart_texto("\r\n[LIMITE] Velocidad maxima");
        }
        ms_ultimo_boton = ms_transcurridos;
    } else if (b && !prev_bajar && dt >= MS_REBOTE) {
        if (indice_vel > 0u) {
            indice_vel--;
            intervalo_ms = (indice_vel == 0u) ? MS_LENTO : MS_NORMAL;
            uart_reporte_vel();
        } else {
            uart_texto("\r\n[LIMITE] Velocidad minima");
        }
        ms_ultimo_boton = ms_transcurridos;
    } else if (p && !prev_parar && dt >= MS_REBOTE) {
        en_pausa = !en_pausa;
        uart_texto(en_pausa ? "\r\n[PAUSA]" : "\r\n[SIGUE]");
        if (!en_pausa) ms_ultimo_paso = ms_transcurridos;
        ms_ultimo_boton = ms_transcurridos;
    }

    prev_subir = s;
    prev_bajar = b;
    prev_parar = p;
}

/* ================================================================
   LEDs
   ================================================================ */

void leds_apagar(void) {
    REG_PA_SET = (RESET_PA << 16);
    REG_PB_SET = (RESET_PB << 16);
    REG_PC_SET = (RESET_PC << 16);
}

void leds_escribir(uint8_t paso) {
    leds_apagar();
    switch (paso) {
        case 1: case 7:
            REG_PA_SET = PIN_L1 | PIN_L8;                              break;
        case 2: case 6:
            REG_PA_SET = PIN_L2; REG_PB_SET = PIN_L7;                 break;
        case 3: case 5:
            REG_PA_SET = PIN_L3; REG_PB_SET = PIN_L6;                 break;
        case 4:
            REG_PB_SET = PIN_L4; REG_PC_SET = PIN_L5;                 break;
        case 8:
            REG_PA_SET = PIN_L1|PIN_L2|PIN_L3|PIN_L8;
            REG_PB_SET = PIN_L4|PIN_L6|PIN_L7;
            REG_PC_SET = PIN_L5;                                        break;
        default: break;
    }
}

/* ================================================================
   UART
   ================================================================ */

void uart_char(char c) {
    while (!(REG_U2_ESTADO & (1u << 7)));
    REG_U2_DATO = (uint32_t)c;
}

void uart_texto(const char *txt) {
    while (*txt) uart_char(*txt++);
}

void uart_numero(uint32_t n) {
    if (n == 0) { uart_char('0'); return; }
    char tmp[10]; int pos = 0;
    while (n > 0) { tmp[pos++] = '0' + (char)(n % 10); n /= 10; }
    for (int k = pos - 1; k >= 0; k--) uart_char(tmp[k]);
}

void uart_reporte_vel(void) {
    const char *nombres[] = { "LENTO", "NORMAL", "VELOZ" };
    uart_texto("\r\n[VEL] Modo: ");  uart_texto(nombres[indice_vel]);
    uart_texto(" | ");               uart_numero(intervalo_ms);
    uart_texto(" ms");
}

/* ================================================================
   INICIALIZACIÓN
   ================================================================ */

void tick_init(void) {
    TICK_CARGA = (RELOJ_HZ / 1000u) - 1u;
    TICK_VALOR = 0u;
    TICK_CTRL  = (1u << 0) | (1u << 1);
}

void uart_init(void) {
    REG_RCC_APB1 |= (1u << 17);

    REG_PA_MODE &= ~(3u << 4);
    REG_PA_MODE |=  (2u << 4);
    REG_PA_AFL  &= ~(0xFu << 8);
    REG_PA_AFL  |=  (7u   << 8);

    REG_U2_BAUD  = 0x8B;
    REG_U2_CTRL |= (1u<<13) | (1u<<3) | (1u<<2);
}

void pines_entrada_init(void) {
    REG_PA_MODE &= ~((3u << 0) | (3u << 2) | (3u << 18));
    REG_PA_PULL &= ~((3u << 0) | (3u << 2) | (3u << 18));
    REG_PA_PULL |=  ((2u << 0) | (2u << 2) | (2u << 18));
}

void pines_salida_init(void) {
    /* Puerto A: PA5, PA6, PA7, PA10 (Sin el PA8) */
    REG_PA_MODE &= ~((3u<<10)|(3u<<12)|(3u<<14)|(3u<<20));
    REG_PA_MODE |=  ((1u<<10)|(1u<<12)|(1u<<14)|(1u<<20));

    /* Puerto B: PB3, PB4, PB6 (Sin PB8 ni PB9) */
    REG_PB_MODE &= ~((3u<<6)|(3u<<8)|(3u<<12));
    REG_PB_MODE |=  ((1u<<6)|(1u<<8)|(1u<<12));

    /* Puerto C: PC7 */
    REG_PC_MODE &= ~(3u << 14);
    REG_PC_MODE |=  (1u << 14);
}

void sistema_init(void) {
    REG_RCC_AHB1 |= (1u<<0) | (1u<<1) | (1u<<2);

    pines_salida_init();
    pines_entrada_init();
    uart_init();
    tick_init();

    indice_vel   = 0u;
    intervalo_ms = MS_LENTO;
    paso_actual  = 1u;
    en_pausa     = 0u;
    ms_ultimo_paso  = 0u;
    ms_ultimo_boton = 0u;

    leds_escribir(paso_actual);
}
