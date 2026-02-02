#ifndef DEVICE_REGISTERS_H
#define DEVICE_REGISTERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/************* MEMORY BUS & PERIPHERAL BASE ADDRESSES *****************/
#define PERIPH_BASE             0x40000000UL
#define SCS_BASE                0xE000E000UL /* Cortex M4 System Control Space Base Address */    

#define APB1PERIPH_BASE         PERIPH_BASE 
#define APB2PERIPH_BASE         (PERIPH_BASE + 0x00010000UL)
#define AHB1PERIPH_BASE         (PERIPH_BASE + 0x00020000UL) 
#define AHB2PERIPH_BASE         (PERIPH_BASE + 0x08000000UL)  

/************* Clock / Power / Flash base addresses *****************/
#define RCC_BASE                (AHB1PERIPH_BASE + 0x1000UL) /* 0x40021000 */
#define FLASH_BASE              (AHB1PERIPH_BASE + 0x2000UL) /* 0x40022000 */
#define PWR_BASE                (APB1PERIPH_BASE + 0x7000UL) /* 0x40007000 */
#define I2C1_BASE               (APB1PERIPH_BASE + 0x5400UL)
#define I2C2_BASE               (APB1PERIPH_BASE + 0x5800UL)
#define I2C3_BASE               (APB1PERIPH_BASE + 0x5C00UL)
#define SPI1_BASE               (APB2PERIPH_BASE + 0x3000UL)
#define SPI2_BASE               (APB1PERIPH_BASE + 0x3800UL)
#define SPI3_BASE               (APB1PERIPH_BASE + 0x3C00UL)
#define DMA1_BASE               (AHB1PERIPH_BASE + 0x0000UL)
#define DMA2_BASE               (AHB1PERIPH_BASE + 0x0400UL)
#define ADC1_BASE               (AHB2PERIPH_BASE + 0x08000000UL)
#define ADC2_BASE               (AHB2PERIPH_BASE + 0x08000100UL)
#define ADC3_BASE               (AHB2PERIPH_BASE + 0x08000200UL)
#define SYSCFG_BASE             (APB2PERIPH_BASE + 0x0000UL)
#define EXTI_BASE               (APB2PERIPH_BASE + 0x0400UL)
#define IWDG_BASE               (APB1PERIPH_BASE + 0x3000UL)
#define DAC_BASE                (APB1PERIPH_BASE + 0x7400UL)
#define TIM2_BASE               (APB1PERIPH_BASE + 0x0000UL)
#define RTC_BASE                (APB1PERIPH_BASE + 0x2800UL)

/************* GPIO Port base addresses (AHB2) *****************/
#define GPIOA_BASE              (AHB2PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE              (AHB2PERIPH_BASE + 0x0400UL)
#define GPIOC_BASE              (AHB2PERIPH_BASE + 0x0800UL)
#define GPIOD_BASE              (AHB2PERIPH_BASE + 0x0C00UL)
#define GPIOE_BASE              (AHB2PERIPH_BASE + 0x1000UL)
#define GPIOH_BASE              (AHB2PERIPH_BASE + 0x1C00UL)

/************* UART/USART base addresses *****************/
#define USART2_BASE             (APB1PERIPH_BASE + 0x4400UL)
#define USART3_BASE             (APB1PERIPH_BASE + 0x4800UL)
#define UART4_BASE              (APB1PERIPH_BASE + 0x4C00UL)
#define UART5_BASE              (APB1PERIPH_BASE + 0x5000UL)
#define USART1_BASE             (APB2PERIPH_BASE + 0x3800UL)
#define LPUART1_BASE            (APB1PERIPH_BASE + 0x8000UL)

/************* SysTick base *****************/
#define SYSTICK_BASE            (SCS_BASE + 0x0010UL) /* 0xE000E010 */

/************* SCB base *****************/
#define SCB_BASE                (SCS_BASE + 0x0D00UL) /* 0xE000ED00 */

/************* System Control Block registers *****************/
#define SCB_CPACR               (SCS_BASE + 0x0D88UL) /* 0xE000ED88 */
#define SCB_CPACR_REG           (*(volatile uint32_t *)SCB_CPACR)

/************* NVIC base *****************/
#define NVIC_BASE               (SCS_BASE + 0x0100UL) /* 0xE000E100UL */

/************* FPU base *****************/
#define FPU_FPCCR               (SCS_BASE + 0x0F34UL) /* 0xE000EF34 */
#define FPU_FPCCR_REG           (*(volatile uint32_t *)FPU_FPCCR)

/************* REGISTER STRUCTURES *****************/
/************* RCC Registers *****************/
typedef struct {
    volatile uint32_t CR; /* 0x00 Clock control register */
    volatile uint32_t ICSCR; /* 0x04 Internal clock sources calibration register */
    volatile uint32_t CFGR; /* 0x08 Clock configuration register */
    volatile uint32_t PLLCFGR; /* 0x0C PLL configuration register */

    volatile uint32_t PLLSAI1CFGR; /* 0x10 PLLSAI1 configuration register */
    volatile uint32_t PLLSAI2CFGR; /* 0x14 PLLSAI2 configuration register */
    volatile uint32_t CIER; /* 0x18 Clock interrupt enable register */
    volatile uint32_t CIFR; /* 0x1C Clock interrupt flag register */
    volatile uint32_t CICR; /* 0x20 Clock interrupt clear register */
    volatile uint32_t RESERVED0; /* 0x24 */

    volatile uint32_t AHB1RSTR; /* 0x28 AHB1 peripheral reset register */
    volatile uint32_t AHB2RSTR; /* 0x2C AHB2 peripheral reset register */
    volatile uint32_t AHB3RSTR; /* 0x30 AHB3 peripheral reset register */
    volatile uint32_t RESERVED1; /* 0x34 */

    volatile uint32_t APB1RSTR1; /* 0x38 APB1 peripheral reset register 1 */
    volatile uint32_t APB1RSTR2; /* 0x3C APB1 peripheral reset register 2 */
    volatile uint32_t APB2RSTR; /* 0x40 APB2 peripheral reset register */
    volatile uint32_t RESERVED2; /* 0x44 */

    volatile uint32_t AHB1ENR; /* 0x48 AHB1 peripheral clock enable register */
    volatile uint32_t AHB2ENR; /* 0x4C AHB2 peripheral clock enable register */
    volatile uint32_t AHB3ENR; /* 0x50 AHB3 peripheral clock enable register */
    volatile uint32_t RESERVED3; /* 0x54 */

    volatile uint32_t APB1ENR1; /* 0x58 APB1 peripheral clock enable register 1 */
    volatile uint32_t APB1ENR2; /* 0x5C APB1 peripheral clock enable register 2 */
    volatile uint32_t APB2ENR; /* 0x60 APB2 peripheral clock enable register */
    volatile uint32_t RESERVED4; /* 0x64 */

    volatile uint32_t AHB1SMENR; /* 0x68 AHB1 clocks enable in Sleep/Stop */
    volatile uint32_t AHB2SMENR; /* 0x6C AHB2 clocks enable in Sleep/Stop */
    volatile uint32_t AHB3SMENR; /* 0x70 AHB3 clocks enable in Sleep/Stop */
    volatile uint32_t APB1SMENR1; /* 0x74 APB1 clocks enable in Sleep/Stop 1 */
    volatile uint32_t APB1SMENR2; /* 0x78 APB1 clocks enable in Sleep/Stop 2 */
    volatile uint32_t APB2SMENR; /* 0x7C APB2 clocks enable in Sleep/Stop */

    volatile uint32_t RESERVED5; /* 0x80 */
    volatile uint32_t RESERVED6; /* 0x84 */

    volatile uint32_t CCIPR; /* 0x88 Peripherals independent clock config */
    volatile uint32_t RESERVED7; /* 0x8C */

    volatile uint32_t BDCR; /* 0x90 Backup domain control */
    volatile uint32_t CSR; /* 0x94 Clock control & status */
    volatile uint32_t CRRCR; /* 0x98 Clock recovery RC */
    volatile uint32_t CCIPR2; /* 0x9C Peripherals independent clock config 2 */
} RCC_t;

/************* GPIO Registers *****************/
typedef struct {
    volatile uint32_t MODER; /* 0x00 Mode register */
    volatile uint32_t OTYPER; /* 0x04 Output type register */
    volatile uint32_t OSPEEDR; /* 0x08 Output speed register */
    volatile uint32_t PUPDR; /* 0x0C Pull-up/pull-down register */
    volatile uint32_t IDR; /* 0x10 Input data register */
    volatile uint32_t ODR; /* 0x14 Output data register */
    volatile uint32_t BSRR; /* 0x18 Bit set/reset register */
    volatile uint32_t LCKR; /* 0x1C Configuration lock register */
    volatile uint32_t AFR[2]; /* 0x20/0x24 Alternate function registers */
} GPIO_t;

/************* USART/UART Registers *****************/
typedef struct {
    volatile uint32_t CR1; /* 0x00 Control register 1 */
    volatile uint32_t CR2; /* 0x04 Control register 2 */
    volatile uint32_t CR3; /* 0x08 Control register 3 */
    volatile uint32_t BRR; /* 0x0C Baud rate register */
    volatile uint32_t GTPR; /* 0x10 Guard time and prescaler register */
    volatile uint32_t RTOR; /* 0x14 Receiver timeout register */
    volatile uint32_t RQR; /* 0x18 Request register */
    volatile uint32_t ISR; /* 0x1C Interrupt and status register */
    volatile uint32_t ICR; /* 0x20 Interrupt flag clear register */
    volatile uint32_t RDR; /* 0x24 Receive data register */
    volatile uint32_t TDR; /* 0x28 Transmit data register */
} USART_TypeDef;

/************* I2C Registers *****************/
typedef struct {
    volatile uint32_t CR1; /* 0x00 Control register 1 */
    volatile uint32_t CR2; /* 0x04 Control register 2 */
    volatile uint32_t OAR1; /* 0x08 Own address register 1 */
    volatile uint32_t OAR2; /* 0x0C Own address register 2 */
    volatile uint32_t TIMINGR; /* 0x10 Timing register */
    volatile uint32_t TIMEOUTR; /* 0x14 Timeout register */
    volatile uint32_t ISR; /* 0x18 Interrupt and status register */
    volatile uint32_t ICR; /* 0x1C Interrupt clear register */
    volatile uint32_t PECR; /* 0x20 PEC register */
    volatile uint32_t RXDR; /* 0x24 Receive data register */
    volatile uint32_t TXDR; /* 0x28 Transmit data register */
} I2C_TypeDef;

/************* SPI Registers *****************/
typedef struct {
    volatile uint32_t CR1; /* 0x00 Control register 1 */
    volatile uint32_t CR2; /* 0x04 Control register 2 */
    volatile uint32_t SR;  /* 0x08 Status register */
    volatile uint32_t DR;  /* 0x0C Data register */
    volatile uint32_t CRCPR; /* 0x10 CRC polynomial register */
    volatile uint32_t RXCRCR; /* 0x14 RX CRC register */
    volatile uint32_t TXCRCR; /* 0x18 TX CRC register */
    volatile uint32_t I2SCFGR; /* 0x1C I2S configuration register */
    volatile uint32_t I2SPR; /* 0x20 I2S prescaler register */
} SPI_TypeDef;

/************* DMA Registers *****************/
typedef struct {
    volatile uint32_t ISR;  /* 0x00 Interrupt status register */
    volatile uint32_t IFCR; /* 0x04 Interrupt flag clear register */
} DMA_TypeDef;

typedef struct {
    volatile uint32_t CCR;   /* 0x00 Channel configuration register */
    volatile uint32_t CNDTR; /* 0x04 Number of data register */
    volatile uint32_t CPAR;  /* 0x08 Peripheral address register */
    volatile uint32_t CMAR;  /* 0x0C Memory address register */
} DMA_Channel_TypeDef;

/************* ADC Registers *****************/
typedef struct {
    volatile uint32_t ISR;  /* 0x00 Interrupt and status register */
    volatile uint32_t IER;  /* 0x04 Interrupt enable register */
    volatile uint32_t CR;   /* 0x08 Control register */
    volatile uint32_t CFGR; /* 0x0C Configuration register */
    volatile uint32_t CFGR2; /* 0x10 Configuration register 2 */
    volatile uint32_t SMPR1; /* 0x14 Sampling time register 1 */
    volatile uint32_t SMPR2; /* 0x18 Sampling time register 2 */
    volatile uint32_t TR1;  /* 0x20 Watchdog threshold register 1 */
    volatile uint32_t TR2;  /* 0x24 Watchdog threshold register 2 */
    volatile uint32_t TR3;  /* 0x28 Watchdog threshold register 3 */
    volatile uint32_t SQR1; /* 0x30 Regular sequence register 1 */
    volatile uint32_t SQR2; /* 0x34 Regular sequence register 2 */
    volatile uint32_t SQR3; /* 0x38 Regular sequence register 3 */
    volatile uint32_t SQR4; /* 0x3C Regular sequence register 4 */
    volatile uint32_t DR;   /* 0x40 Regular data register */
} ADC_TypeDef;

/************* SYSCFG Registers *****************/
typedef struct {
    volatile uint32_t MEMRMP;    /* 0x00 */
    volatile uint32_t CFGR1;     /* 0x04 */
    volatile uint32_t EXTICR[4]; /* 0x08-0x14 */
    volatile uint32_t CFGR2;     /* 0x18 */
    volatile uint32_t SWPR;      /* 0x1C */
    volatile uint32_t SKR;       /* 0x20 */
} SYSCFG_TypeDef;

/************* EXTI Registers *****************/
typedef struct {
    volatile uint32_t IMR1;  /* 0x00 */
    volatile uint32_t EMR1;  /* 0x04 */
    volatile uint32_t RTSR1; /* 0x08 */
    volatile uint32_t FTSR1; /* 0x0C */
    volatile uint32_t SWIER1; /* 0x10 */
    volatile uint32_t PR1;   /* 0x14 */
} EXTI_TypeDef;

/************* IWDG Registers *****************/
typedef struct {
    volatile uint32_t KR;  /* 0x00 Key register */
    volatile uint32_t PR;  /* 0x04 Prescaler register */
    volatile uint32_t RLR; /* 0x08 Reload register */
    volatile uint32_t SR;  /* 0x0C Status register */
} IWDG_TypeDef;

/************* DAC Registers *****************/
typedef struct {
    volatile uint32_t CR;      /* 0x00 Control register */
    volatile uint32_t SWTRIGR; /* 0x04 Software trigger register */
    volatile uint32_t DHR12R1; /* 0x08 Channel 1 12-bit right-aligned data */
    volatile uint32_t DHR12L1; /* 0x0C Channel 1 12-bit left-aligned data */
    volatile uint32_t DHR8R1;  /* 0x10 Channel 1 8-bit right-aligned data */
    volatile uint32_t DHR12R2; /* 0x14 Channel 2 12-bit right-aligned data */
    volatile uint32_t DHR12L2; /* 0x18 Channel 2 12-bit left-aligned data */
    volatile uint32_t DHR8R2;  /* 0x1C Channel 2 8-bit right-aligned data */
    volatile uint32_t DHR12RD; /* 0x20 Dual 12-bit right-aligned data */
    volatile uint32_t DHR12LD; /* 0x24 Dual 12-bit left-aligned data */
    volatile uint32_t DHR8RD;  /* 0x28 Dual 8-bit right-aligned data */
    volatile uint32_t DOR1;    /* 0x2C Channel 1 data output register */
    volatile uint32_t DOR2;    /* 0x30 Channel 2 data output register */
    volatile uint32_t SR;      /* 0x34 Status register */
} DAC_TypeDef;

/************* TIM Registers *****************/
typedef struct {
    volatile uint32_t CR1;   /* 0x00 */
    volatile uint32_t CR2;   /* 0x04 */
    volatile uint32_t SMCR;  /* 0x08 */
    volatile uint32_t DIER;  /* 0x0C */
    volatile uint32_t SR;    /* 0x10 */
    volatile uint32_t EGR;   /* 0x14 */
    volatile uint32_t CCMR1; /* 0x18 */
    volatile uint32_t CCMR2; /* 0x1C */
    volatile uint32_t CCER;  /* 0x20 */
    volatile uint32_t CNT;   /* 0x24 */
    volatile uint32_t PSC;   /* 0x28 */
    volatile uint32_t ARR;   /* 0x2C */
    volatile uint32_t CCR1;  /* 0x34 */
    volatile uint32_t CCR2;  /* 0x38 */
    volatile uint32_t CCR3;  /* 0x3C */
    volatile uint32_t CCR4;  /* 0x40 */
} TIM_TypeDef;

/************* RTC Registers *****************/
typedef struct {
    volatile uint32_t TR;   /* 0x00 Time register */
    volatile uint32_t DR;   /* 0x04 Date register */
    volatile uint32_t CR;   /* 0x08 Control register */
    volatile uint32_t ISR;  /* 0x0C Initialization and status register */
    volatile uint32_t PRER; /* 0x10 Prescaler register */
    volatile uint32_t WUTR; /* 0x14 Wakeup timer register */
    volatile uint32_t CALIBR; /* 0x18 Calibration register */
    volatile uint32_t ALRMAR; /* 0x1C Alarm A register */
    volatile uint32_t ALRMBR; /* 0x20 Alarm B register */
    volatile uint32_t WPR;  /* 0x24 Write protection register */
    volatile uint32_t SSR;  /* 0x28 Sub second register */
    volatile uint32_t SHIFTR; /* 0x2C Shift control register */
    volatile uint32_t TSTR; /* 0x30 Timestamp time register */
    volatile uint32_t TSDR; /* 0x34 Timestamp date register */
    volatile uint32_t TSSSR; /* 0x38 Timestamp sub second register */
    volatile uint32_t CALR; /* 0x3C Calibration register */
    volatile uint32_t TAFCR; /* 0x40 Tamper and alternate function configuration register */
} RTC_TypeDef;

/************* SysTick Registers *****************/
typedef struct {
    volatile uint32_t CSR; /* 0xE000E010 Control and Status */
    volatile uint32_t RVR; /* 0xE000E014 Reload value */
    volatile uint32_t CVR; /* 0xE000E018 Current value */
    volatile uint32_t CALIB; /* 0xE000E01C Calibration */
} SysTick_t;

/************* FLASH Registers *****************/
typedef struct {
    volatile uint32_t ACR; /* 0x00 Access control */
    volatile uint32_t PDKEYR; /* 0x04 Power-down key */
    volatile uint32_t KEYR; /* 0x08 Flash key */
    volatile uint32_t OPTKEYR; /* 0x0C Option key */
    volatile uint32_t SR; /* 0x10 Status */
    volatile uint32_t CR; /* 0x14 Control */
    volatile uint32_t ECCR; /* 0x18 ECC */
    volatile uint32_t RESERVED0; /* 0x1C */
    volatile uint32_t OPTR; /* 0x20 Option */
    volatile uint32_t PCROP1SR; /* 0x24 Bank1 PCROP Start */
    volatile uint32_t PCROP1ER; /* 0x28 Bank1 PCROP End */
    volatile uint32_t WRP1AR; /* 0x2C Bank1 WRP area A */
    volatile uint32_t WRP1BR; /* 0x30 Bank1 WRP area B */
    volatile uint32_t PCROP2SR; /* 0x34 Bank2 PCROP Start */
    volatile uint32_t PCROP2ER; /* 0x38 Bank2 PCROP End */
    volatile uint32_t WRP2AR; /* 0x3C Bank2 WRP area A */
    volatile uint32_t WRP2BR; /* 0x40 Bank2 WRP area B */
} FLASH_t;

/************* PWR Registers *****************/
typedef struct {
    volatile uint32_t CR1; /* 0x00 Power control register 1 */
    volatile uint32_t CR2; /* 0x04 */
    volatile uint32_t CR3; /* 0x08 */
    volatile uint32_t CR4; /* 0x0C */
    volatile uint32_t SR1; /* 0x10 */
    volatile uint32_t SR2; /* 0x14 */
    volatile uint32_t SCR; /* 0x18 */
    volatile uint32_t CR5; /* 0x1C */
    volatile uint32_t PUCRA; /* 0x20 Port A pull-up */
    volatile uint32_t PDCRA; /* 0x24 Port A pull-down */
    volatile uint32_t PUCRB; /* 0x28 Port B pull-up */
    volatile uint32_t PDCRB; /* 0x2C Port B pull-down */
    volatile uint32_t PUCRC; /* 0x30 Port C pull-up */
    volatile uint32_t PDCRC; /* 0x34 Port C pull-down */
    volatile uint32_t PUCRD; /* 0x38 Port D pull-up */
    volatile uint32_t PDCRD; /* 0x3C Port D pull-down */
    volatile uint32_t PUCRE; /* 0x40 Port E pull-up */
    volatile uint32_t PDCRE; /* 0x44 Port E pull-down */
    volatile uint32_t PUCRF; /* 0x48 Port F pull-up */
    volatile uint32_t PDCRF; /* 0x4C Port F pull-down */
    volatile uint32_t PUCRG; /* 0x50 Port G pull-up */
    volatile uint32_t PDCRG; /* 0x54 Port G pull-down */
    volatile uint32_t PUCRH; /* 0x58 Port H pull-up */
    volatile uint32_t PDCRH; /* 0x5C Port H pull-down */
    volatile uint32_t PUCRI; /* 0x60 Port I pull-up */
    volatile uint32_t PDCRI; /* 0x64 Port I pull-down */
} PWR_t;

/************* SCB Registers *****************/
typedef struct {
    volatile uint32_t CPUID;   /* 0x00 */
    volatile uint32_t ICSR;    /* 0x04 */
    volatile uint32_t VTOR;    /* 0x08 */
    volatile uint32_t AIRCR;   /* 0x0C */
    volatile uint32_t SCR;     /* 0x10 */
    volatile uint32_t CCR;     /* 0x14 */
    volatile uint8_t  SHPR[12];/* 0x18–0x23: SHPR1–3 (4 bytes each) */
    volatile uint32_t SHCSR;   /* 0x24 */
} SCB_t;

/************* POINTERS TO INSTANCES *****************/
#define RCC       ((RCC_t   *) RCC_BASE)
#define FLASH     ((FLASH_t *) FLASH_BASE)
#define PWR       ((PWR_t   *) PWR_BASE)

#define GPIOA     ((GPIO_t  *) GPIOA_BASE)
#define GPIOB     ((GPIO_t  *) GPIOB_BASE)
#define GPIOC     ((GPIO_t  *) GPIOC_BASE)
#define GPIOD     ((GPIO_t  *) GPIOD_BASE)
#define GPIOE     ((GPIO_t  *) GPIOE_BASE)
#define GPIOH     ((GPIO_t  *) GPIOH_BASE)

#define USART1    ((USART_TypeDef *) USART1_BASE)
#define USART2    ((USART_TypeDef *) USART2_BASE)
#define USART3    ((USART_TypeDef *) USART3_BASE)
#define UART4     ((USART_TypeDef *) UART4_BASE)
#define UART5     ((USART_TypeDef *) UART5_BASE)
#define LPUART1   ((USART_TypeDef *) LPUART1_BASE)

#define I2C1      ((I2C_TypeDef *) I2C1_BASE)
#define I2C2      ((I2C_TypeDef *) I2C2_BASE)
#define I2C3      ((I2C_TypeDef *) I2C3_BASE)

#define SPI1      ((SPI_TypeDef *) SPI1_BASE)
#define SPI2      ((SPI_TypeDef *) SPI2_BASE)
#define SPI3      ((SPI_TypeDef *) SPI3_BASE)

#define DMA1      ((DMA_TypeDef *) DMA1_BASE)
#define DMA2      ((DMA_TypeDef *) DMA2_BASE)

#define ADC1      ((ADC_TypeDef *) ADC1_BASE)
#define ADC2      ((ADC_TypeDef *) ADC2_BASE)
#define ADC3      ((ADC_TypeDef *) ADC3_BASE)

#define SYSCFG    ((SYSCFG_TypeDef *) SYSCFG_BASE)
#define EXTI      ((EXTI_TypeDef *) EXTI_BASE)

#define IWDG      ((IWDG_TypeDef *) IWDG_BASE)

#define DAC       ((DAC_TypeDef *) DAC_BASE)

#define TIM2      ((TIM_TypeDef *) TIM2_BASE)

#define RTC       ((RTC_TypeDef *) RTC_BASE)

#define SYSTICK   ((SysTick_t *) SYSTICK_BASE)

#define SCB       ((SCB_t *)SCB_BASE)

/************* NVIC definitions *****************/
#define NVIC_ISER0              (*((volatile uint32_t *)(NVIC_BASE + 0x000)))
#define NVIC_ISER1              (*((volatile uint32_t *)(NVIC_BASE + 0x004)))
#define NVIC_IPR(irq)           (*((volatile uint8_t *)(NVIC_BASE + 0x300UL + (irq))))
#define USART2_IRQn             38


#ifdef __cplusplus
}
#endif

#endif /* DEVICE_REGISTERS_H */
