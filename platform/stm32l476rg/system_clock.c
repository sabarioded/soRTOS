#include "system_clock.h"
#include "utils.h"

/*********** FLASH_ACR ***********/
/*
 * FLASH_ACR — Flash Access Control Register
 * FLASH_ACR[2:0] - WS selection
 * VOS1 (High Performance Mode)
 * 0–16 MHz   -> 0 WS
 * 16–32 MHz  -> 1 WS
 * 32–48 MHz  -> 2 WS
 * 48–64 MHz  -> 3 WS
 * 64–80 MHz  -> 4 WS
 *
 * VOS2 (Low Power Mode)
 * 0–6 MHz    -> 0 WS
 * 6–12 MHz   -> 1 WS
 * 12–18 MHz  -> 2 WS
 * 18–26 MHz  -> 3 WS   (max SYSCLK in VOS2)
 * 
 *  MUST configure before switching clock source.
 */
#define FLASH_ACR_LATENCY_POS   0U
#define FLASH_ACR_LATENCY_MASK   (0x7U << FLASH_ACR_LATENCY_POS)
#define FLASH_ACR_PRFTEN        (1U << 8)   /* Prefetch enable */
#define FLASH_ACR_ICEN          (1U << 9)   /* Instruction cache enable */
#define FLASH_ACR_DCEN          (1U << 10)  /* Data cache enable */

/*********** PWR_CR1 ***********/
/* 
 * PWR_CR1 — Power control register 1
 * VOS[10:9] Voltage scaling selection:
 * 01: Range 1 (up to 80 MHz)
 * 10: Range 2 (up to 26 MHz)
 */
#define PWR_CR1_VOS_POS         9U
#define PWR_CR1_VOS_MASK         (0x3U << PWR_CR1_VOS_POS)
#define PWR_CR1_VOS_RANGE1      (0x1U << PWR_CR1_VOS_POS)
#define PWR_CR1_VOS_RANGE2      (0x2U << PWR_CR1_VOS_POS)

/*********** RCC_CR — Clock control register ***********/
/* MSI (Multi-Speed Internal Oscillator) */
#define RCC_CR_MSION        (1U << 0)   /* MSI clock enable */
#define RCC_CR_MSIRDY       (1U << 1)   /* MSI clock ready flag */
#define RCC_CR_MSIPLLEN     (1U << 2)   /* MSI PLL mode enable */
#define RCC_CR_MSIRGSEL     (1U << 3)   /* MSI range selection source */
/*
 * MSIRANGE[7:4] — MSI frequency range:
 * 0000: 100 kHz
 * 0001: 200 kHz
 * 0010: 400 kHz
 * 0011: 800 kHz
 * 0100:   1 MHz
 * 0101:   2 MHz
 * 0110:   4 MHz   (reset)
 * 0111:   8 MHz
 * 1000:  16 MHz
 * 1001:  24 MHz
 * 1010:  32 MHz
 * 1011:  48 MHz
 */
#define RCC_CR_MSIRANGE_POS  4U
#define RCC_CR_MSIRANGE_MASK  (0xFU << RCC_CR_MSIRANGE_POS)

/* HSI16 (High-Speed Internal 16 MHz Oscillator) */
#define RCC_CR_HSION        (1U << 8)   /* HSI16 enable */
#define RCC_CR_HSIKERON     (1U << 9)   /* HSI16 for peripherals */
#define RCC_CR_HSIRDY       (1U << 10)  /* HSI16 ready flag */
#define RCC_CR_HSIASFS      (1U << 12)  /* Auto-start HSI on clock failure */

/* HSE (High-Speed External Oscillator) */
#define RCC_CR_HSEON        (1U << 16) /* Trun on HSE */
#define RCC_CR_HSERDY       (1U << 17) /* Poll to check if HSE ready */
#define RCC_CR_HSEBYP       (1U << 18) /* use external clock */
#define RCC_CR_CSSON        (1U << 19) /* clock security */

/* PLL */
#define RCC_CR_PLLON        (1U << 24) /* Enable PLL */
#define RCC_CR_PLLRDY       (1U << 25) /* Check if PLL ready*/
#define RCC_CR_PLLSAI1ON    (1U << 26) /* Turn on peripherals PLL */
#define RCC_CR_PLLSAI1RDY   (1U << 27) /* Check if peripherals PLL ready */
#define RCC_CR_PLLSAI2ON    (1U << 28) /* Turn on peripherals PLL */
#define RCC_CR_PLLSAI2RDY   (1U << 29) /* Check if peripherals PLL ready */

/*********** RCC_CFGR — Clock configuration register ***********/
/* 
 * SW selects the **requested** SYSCLK source.
 * SWS shows the **current** SYSCLK source.
 * You MUST poll SWS before assuming the clock has switched.
 */
/*
 * SW[1:0] — System Clock Switch
 * 00: MSI selected as system clock
 * 01: HSI16 selected as system clock
 * 10: HSE selected as system clock
 * 11: PLL selected as system clock
 */
#define RCC_CFGR_SW_POS     0U
#define RCC_CFGR_SW_MASK     (0x3U << RCC_CFGR_SW_POS)

/* SWS[3:2] — System clock switch status */
#define RCC_CFGR_SWS_POS    2U
#define RCC_CFGR_SWS_MASK    (0x3U << RCC_CFGR_SWS_POS)

/* HPRE[7:4] — AHB prescaler
 * Divides SYSCLK to create HCLK (core, bus, memory clock).
 * 
 * 0xxx: SYSCLK not divided
 * 1000: SYSCLK / 2
 * 1001: SYSCLK / 4
 * 1010: SYSCLK / 8
 * up to SYSCLK / 512
 */
#define RCC_CFGR_HPRE_POS   4U
#define RCC_CFGR_HPRE_MASK   (0xFU << RCC_CFGR_HPRE_POS)

/* PPRE1[12:8] — APB1 prescaler */
#define RCC_CFGR_PPRE1_POS   8U
#define RCC_CFGR_PPRE1_MASK   (0x7U << RCC_CFGR_PPRE1_POS)
/* 0xx: HCLK not divided -> PCLK1 = HCLK */
#define RCC_CFGR_PPRE1_DIV1  (0x0U << RCC_CFGR_PPRE1_POS)

/* PPRE2[15:11] — APB2 prescaler */
#define RCC_CFGR_PPRE2_POS   11U
#define RCC_CFGR_PPRE2_MASK   (0x7U << RCC_CFGR_PPRE2_POS)
/* 0xx: HCLK not divided -> PCLK2 = HCLK */
#define RCC_CFGR_PPRE2_DIV1  (0x0U << RCC_CFGR_PPRE2_POS)

/*********** RCC_PLLCFGR — PLL configuration register ***********/
/* PLLSRC[1:0] — PLL input source
 * 00: None
 * 01: MSI
 * 10: HSI16
 * 11: HSE
 */
#define RCC_PLLCFGR_PLLSRC_POS   0U
#define RCC_PLLCFGR_PLLSRC_MASK   (0x3U << RCC_PLLCFGR_PLLSRC_POS)
#define RCC_PLLCFGR_PLLSRC_MSI    (0x1U << RCC_PLLCFGR_PLLSRC_POS)
#define RCC_PLLCFGR_PLLSRC_HSI16  (0x2U << RCC_PLLCFGR_PLLSRC_POS)
#define RCC_PLLCFGR_PLLSRC_HSE    (0x3U << RCC_PLLCFGR_PLLSRC_POS)

/* PLLM[7:4] — Division factor M
 * Encoded as (M - 1)
 */
#define RCC_PLLCFGR_PLLM_POS     4U
#define RCC_PLLCFGR_PLLM_MASK     (0xFU << RCC_PLLCFGR_PLLM_POS)

/* PLLN[14:8] — Multiplication factor N */
#define RCC_PLLCFGR_PLLN_POS     8U
#define RCC_PLLCFGR_PLLN_MASK     (0x7FU << RCC_PLLCFGR_PLLN_POS)

/* PLLR[26:25] — Division factor R for SYSCLK
 * 00: /2
 * 01: /4
 * 10: /6
 * 11: /8
 */
#define RCC_PLLCFGR_PLLR_POS     25U
#define RCC_PLLCFGR_PLLR_MASK     (0x3U << RCC_PLLCFGR_PLLR_POS)

/* PLLREN[24] — enable PLLR output */
#define RCC_PLLCFGR_PLLREN       (1U << 24)


/*********** RCC_APB1ENR1 ***********/
#define RCC_APB1ENR1_PWREN (1U << 28) /* power enable */

/*********** Type definitions ***********/
typedef enum {
    SYSTEM_VOS1 = 1,
    SYSTEM_VOS2 = 2,
} system_vos_t;

typedef enum {
    SYSCLK_SRC_MSI = 0,
    SYSCLK_SRC_HSI16 = 1,
    SYSCLK_SRC_HSE = 2,
    SYSCLK_SRC_PLL = 3,
} sysclk_source_t;

typedef struct {
    uint32_t pllm;
    uint32_t plln;
    uint32_t pllr_bits;
} pll_config_t;

/*********** Global variables ***********/
static uint32_t system_clock_val_hz = 4000000UL; /* Assume 4 MHz MSI at reset */

/*********** Static function prototypes ***********/
static void flash_set_latency(sysclock_hz_t sysclk_hz, system_vos_t power);
static int power_set_vos(sysclock_hz_t sysclk_hz, system_vos_t *out_vos);
static int system_clock_set_default(void);
static int system_clock_set_hsi16(void);

/*********** Function definitions ***********/
/* set up flash latency according to required sysclk and power */
static void flash_set_latency(sysclock_hz_t sysclk_hz, system_vos_t power)  {
    uint32_t latency;

    switch(power)
    {
        case SYSTEM_VOS1:
            if(sysclk_hz <= 16000000UL) {
                latency = 0;
            } else if(sysclk_hz <= 32000000UL) {
                latency = 1;
            } else if(sysclk_hz <= 48000000UL) {
                latency = 2;
            } else if(sysclk_hz <= 64000000UL) {
                latency = 3;
            } else { // max sysclk in VOS1 is 80MHz
                latency = 4;
            }
            break;
        case SYSTEM_VOS2:
            if(sysclk_hz <= 6000000UL) {
                latency = 0;
            } else if(sysclk_hz <= 12000000UL) {
                latency = 1;
            } else if(sysclk_hz <= 18000000UL) {
                latency = 2;
            } else { // max sysclk in VOS2 is 26MHz
                latency = 3;
            }
            break;
        default:
            /* invalid power range, default to max latency */
            latency = 4;
            break;
    }   

    uint32_t tmp = FLASH->ACR;       /* save the current register state */
    tmp &= ~FLASH_ACR_LATENCY_MASK;  /* zero the latency section */
    /* set the new latency */
    tmp |= ((latency << FLASH_ACR_LATENCY_POS) & FLASH_ACR_LATENCY_MASK);
    tmp |= FLASH_ACR_PRFTEN;        /* enable prefetch */
    tmp |= FLASH_ACR_ICEN;          /* enable instruction cache */
    tmp |= FLASH_ACR_DCEN;          /* enable data cache */
    FLASH->ACR = tmp;                /* update the register */
}

/* Set power voltage scaling range */
static int power_set_vos(sysclock_hz_t sysclk_hz, system_vos_t *out_vos)
{
    system_vos_t vos;
    /* Enable PWR clock */
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;

    /* Set VOS bits to desired range */
    uint32_t tmp = PWR->CR1;
    tmp &= ~PWR_CR1_VOS_MASK; /* clear VOS bits */
    if(sysclk_hz <= 26000000UL) {
        tmp |= PWR_CR1_VOS_RANGE2;
        vos = SYSTEM_VOS2;
    } else { /* sysclk_hz <= 80000000UL */
        tmp |= PWR_CR1_VOS_RANGE1;
        vos = SYSTEM_VOS1;
    }

    PWR->CR1 = tmp;
    /* wait until VOS bits are set */
    if (wait_for_reg_mask_eq(&PWR->CR1, PWR_CR1_VOS_MASK, tmp & PWR_CR1_VOS_MASK, SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) {
        return SYSTEM_CLOCK_ERR_TIMEOUT;
    }
    if (out_vos) *out_vos = vos;
    return SYSTEM_CLOCK_OK;
}

/* set system clock to default state (MSI 4MHz) */
static int system_clock_set_default(void) {
    RCC->CR |= RCC_CR_MSION; /* Enable MSI */
    if (wait_for_flag_set(&RCC->CR, RCC_CR_MSIRDY, SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) {
        return SYSTEM_CLOCK_ERR_TIMEOUT;
    }

    /* set MSI default frequency */
    RCC->CR &= ~RCC_CR_MSIRANGE_MASK;
    RCC->CR |= (0x6U << RCC_CR_MSIRANGE_POS); /* 4 MHz */

    /* Switch SYSCLK to MSI */
    uint32_t tmp = RCC->CFGR;
    tmp &= ~(RCC_CFGR_SW_MASK); /* clear SW bits */
    tmp |= (SYSCLK_SRC_MSI << RCC_CFGR_SW_POS); /* select MSI */
    RCC->CFGR = tmp;
    /* Wait until MSI is used as system clock */
    if (wait_for_reg_mask_eq(&RCC->CFGR, RCC_CFGR_SWS_MASK, (SYSCLK_SRC_MSI << RCC_CFGR_SWS_POS), SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) {
        return SYSTEM_CLOCK_ERR_TIMEOUT;
    }
    system_clock_val_hz = 4000000UL; /* MSI default frequency after reset */
    
    /* Disable PLL */
    RCC->CR &= ~RCC_CR_PLLON;
    if (wait_for_flag_clear(&RCC->CR, RCC_CR_PLLRDY, SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) {
        return SYSTEM_CLOCK_ERR_TIMEOUT;
    }
    /* Disable HSI16 */
    RCC->CR &= ~RCC_CR_HSION;
    if (wait_for_flag_clear(&RCC->CR, RCC_CR_HSIRDY, SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) {
        return SYSTEM_CLOCK_ERR_TIMEOUT;
    }
    return SYSTEM_CLOCK_OK;
}

/* set sysclock to HSI16 */
static int system_clock_set_hsi16(void) {
    RCC->CR |= RCC_CR_HSION; /* Enable HSI16 */
    if (wait_for_flag_set(&RCC->CR, RCC_CR_HSIRDY, SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) {
        return SYSTEM_CLOCK_ERR_TIMEOUT;
    }
    
    /* Switch SYSCLK to HSI16 */
    uint32_t tmp = RCC->CFGR;
    tmp &= ~(RCC_CFGR_SW_MASK); /* clear SW bits */
    tmp |= (SYSCLK_SRC_HSI16 << RCC_CFGR_SW_POS); /* select HSI16 */
    RCC->CFGR = tmp;

    /* Wait until HSI16 is used as system clock */
    if (wait_for_reg_mask_eq(&RCC->CFGR, RCC_CFGR_SWS_MASK, (SYSCLK_SRC_HSI16 << RCC_CFGR_SWS_POS), SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) {
        return SYSTEM_CLOCK_ERR_TIMEOUT;
    }
    return SYSTEM_CLOCK_OK;
}

/* configure the system clock */
int system_clock_config_hz(sysclock_hz_t target_hz) {
    /* first return to default sysclk for safety */
    if (system_clock_set_default() != SYSTEM_CLOCK_OK) return SYSTEM_CLOCK_ERR_TIMEOUT;

    /* set up power range according to sysclk */
    system_vos_t vos;
    if (power_set_vos(target_hz, &vos) != SYSTEM_CLOCK_OK) return SYSTEM_CLOCK_ERR_TIMEOUT;

    /* set up flash latency according to sysclk*/
    flash_set_latency(target_hz, vos);

    /* configure the clock */
    if(target_hz == SYSCLOCK_HZ_4MHZ) {
        /* already running 4MHz MSI */
        return SYSTEM_CLOCK_OK;
    } else if(target_hz == SYSCLOCK_HZ_16MHZ) {
        /* Enable HSI16 */
        if (system_clock_set_hsi16() != SYSTEM_CLOCK_OK) return SYSTEM_CLOCK_ERR_TIMEOUT;
        system_clock_val_hz = SYSCLOCK_HZ_16MHZ;
        return SYSTEM_CLOCK_OK;
    } else if(target_hz == SYSCLOCK_HZ_24MHZ ||
            target_hz == SYSCLOCK_HZ_32MHZ ||
            target_hz == SYSCLOCK_HZ_48MHZ ||
            target_hz == SYSCLOCK_HZ_64MHZ ||
            target_hz == SYSCLOCK_HZ_80MHZ) {
        
        /* Enable HSI16 */
        if (system_clock_set_hsi16() != SYSTEM_CLOCK_OK) {
            return SYSTEM_CLOCK_ERR_TIMEOUT;
        }

        /* set up PLL config according to target_hz
        * SYSCLK = (HSI16 / PLLM) * PLLN / PLLR
        */
        pll_config_t pll_cfg;
        if(target_hz == SYSCLOCK_HZ_24MHZ) {
            pll_cfg.pllm = 2;
            pll_cfg.plln = 12;
            pll_cfg.pllr_bits = 0x1U; /* /4 */
        } else if(target_hz == SYSCLOCK_HZ_32MHZ) {
            pll_cfg.pllm = 2;
            pll_cfg.plln = 8;
            pll_cfg.pllr_bits = 0x0U; /* /2 */
        } else if(target_hz == SYSCLOCK_HZ_48MHZ) {
            pll_cfg.pllm = 2;
            pll_cfg.plln = 12;
            pll_cfg.pllr_bits = 0x0U; /* /2 */
        } else if(target_hz == SYSCLOCK_HZ_64MHZ) {
            pll_cfg.pllm = 2;
            pll_cfg.plln = 16;
            pll_cfg.pllr_bits = 0x0U; /* /2 */
        } else if(target_hz == SYSCLOCK_HZ_80MHZ) {
            pll_cfg.pllm = 2;
            pll_cfg.plln = 20;
            pll_cfg.pllr_bits = 0x0U; /* /2 */
        } else {
            /* unsupported value */
            return SYSTEM_CLOCK_ERR_UNSUPPORTED;
        }

        /* Disable PLL */
        RCC->CR &= ~RCC_CR_PLLON;
        if (wait_for_flag_clear(&RCC->CR, RCC_CR_PLLRDY, SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) return SYSTEM_CLOCK_ERR_TIMEOUT;

        /* Configure PLL */
        uint32_t tmp = RCC->PLLCFGR;
        tmp &= ~RCC_PLLCFGR_PLLSRC_MASK; /* clear PLLSRC bits */
        tmp |= RCC_PLLCFGR_PLLSRC_HSI16; /* HSI16 as PLL source */
        tmp &= ~RCC_PLLCFGR_PLLM_MASK; /* clear PLLM bits */
        tmp |= ((pll_cfg.pllm - 1) << RCC_PLLCFGR_PLLM_POS) & RCC_PLLCFGR_PLLM_MASK;
        tmp &= ~RCC_PLLCFGR_PLLN_MASK; /* clear PLLN bits */
        tmp |= (pll_cfg.plln << RCC_PLLCFGR_PLLN_POS) & RCC_PLLCFGR_PLLN_MASK;
        tmp &= ~RCC_PLLCFGR_PLLR_MASK; /* clear PLLR bits */
        tmp |= (pll_cfg.pllr_bits << RCC_PLLCFGR_PLLR_POS) & RCC_PLLCFGR_PLLR_MASK;
        tmp |= RCC_PLLCFGR_PLLREN; /* enable PLLR output */
        RCC->PLLCFGR = tmp;

        /* Enable PLL */
        RCC->CR |= RCC_CR_PLLON;
        if (wait_for_flag_set(&RCC->CR, RCC_CR_PLLRDY, SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) return SYSTEM_CLOCK_ERR_TIMEOUT;
        /* Switch SYSCLK to PLL */
        tmp = RCC->CFGR;
        tmp &= ~(RCC_CFGR_SW_MASK); /* clear SW bits */
        tmp |= (SYSCLK_SRC_PLL << RCC_CFGR_SW_POS); /* select PLL */
        RCC->CFGR = tmp;
        /* Wait until PLL is used as system clock */
        if (wait_for_reg_mask_eq(&RCC->CFGR, RCC_CFGR_SWS_MASK, (SYSCLK_SRC_PLL << RCC_CFGR_SWS_POS), SYSTEM_CLOCK_WAIT_MAX_ITER) != 0) return SYSTEM_CLOCK_ERR_TIMEOUT;
        system_clock_val_hz = target_hz;
        return SYSTEM_CLOCK_OK;
    } else {
        /* unsupported value */
        return SYSTEM_CLOCK_ERR_UNSUPPORTED;
    }
}

/* get the system clock */
uint32_t get_system_clock_hz(void)
{
    return system_clock_val_hz;
}
