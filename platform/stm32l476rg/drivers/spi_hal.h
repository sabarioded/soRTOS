#ifndef SPI_HAL
#define SPI_HAL

#include "device_registers.h"
#include "gpio.h"
#include "gpio_hal.h"
#include "spi.h"
#include "arch_ops.h"

#define TIMEOUT_VAL           10000000U

/* SPI Register Bit Definitions */
#define SPI_CR1_CPHA_Pos      (0U)
#define SPI_CR1_CPHA          (1U << SPI_CR1_CPHA_Pos) /* Clock phase */
#define SPI_CR1_CPOL_Pos      (1U)
#define SPI_CR1_CPOL          (1U << SPI_CR1_CPOL_Pos) /* Clock polarity */
#define SPI_CR1_MSTR_Pos      (2U)
#define SPI_CR1_MSTR          (1U << SPI_CR1_MSTR_Pos) /* Master mode */
#define SPI_CR1_BR_Pos        (3U)
#define SPI_CR1_BR_Mask       (0x7U << SPI_CR1_BR_Pos) /* Baud rate prescaler */
#define SPI_CR1_SPE_Pos       (6U)
#define SPI_CR1_SPE           (1U << SPI_CR1_SPE_Pos) /* SPI enable */
#define SPI_CR1_LSBFIRST_Pos  (7U)
#define SPI_CR1_LSBFIRST      (1U << SPI_CR1_LSBFIRST_Pos) /* LSB-first */
#define SPI_CR1_SSI_Pos       (8U)
#define SPI_CR1_SSI           (1U << SPI_CR1_SSI_Pos) /* Internal SS level */
#define SPI_CR1_SSM_Pos       (9U)
#define SPI_CR1_SSM           (1U << SPI_CR1_SSM_Pos) /* Software slave mgmt */

#define SPI_CR2_RXNEIE_Pos    (6U)
#define SPI_CR2_RXNEIE        (1U << SPI_CR2_RXNEIE_Pos) /* RX not empty IRQ */
#define SPI_CR2_TXEIE_Pos     (7U)
#define SPI_CR2_TXEIE         (1U << SPI_CR2_TXEIE_Pos) /* TX empty IRQ */
#define SPI_CR2_DS_Pos        (8U)
#define SPI_CR2_DS_Mask       (0xFU << SPI_CR2_DS_Pos) /* Data size field */
#define SPI_CR2_FRXTH_Pos     (12U)
#define SPI_CR2_FRXTH         (1U << SPI_CR2_FRXTH_Pos) /* RX threshold */

#define SPI_SR_RXNE_Pos       (0U)
#define SPI_SR_RXNE           (1U << SPI_SR_RXNE_Pos) /* RX not empty */
#define SPI_SR_TXE_Pos        (1U)
#define SPI_SR_TXE            (1U << SPI_SR_TXE_Pos) /* TX empty */
#define SPI_SR_BSY_Pos        (7U)
#define SPI_SR_BSY            (1U << SPI_SR_BSY_Pos) /* Busy flag */

/* RCC Definitions for SPI */
#define RCC_APB2ENR_SPI1EN    (1U << 12)
#define RCC_APB1ENR1_SPI2EN   (1U << 14)
#define RCC_APB1ENR1_SPI3EN   (1U << 15)

/* Configuration Enums */
typedef enum { 
  SPI_MODE_SLAVE = 0, 
  SPI_MODE_MASTER = 1 
} SPI_Mode_t;

typedef enum { 
  SPI_CPOL_LOW = 0, 
  SPI_CPOL_HIGH = 1 
} SPI_CPOL_t;

typedef enum { 
  SPI_CPHA_1EDGE = 0, 
  SPI_CPHA_2EDGE = 1 
} SPI_CPHA_t;

typedef enum {
  SPI_BAUDRATE_PRESCALER_2 = 0,
  SPI_BAUDRATE_PRESCALER_4 = 1,
  SPI_BAUDRATE_PRESCALER_8 = 2,
  SPI_BAUDRATE_PRESCALER_16 = 3,
  SPI_BAUDRATE_PRESCALER_32 = 4,
  SPI_BAUDRATE_PRESCALER_64 = 5,
  SPI_BAUDRATE_PRESCALER_128 = 6,
  SPI_BAUDRATE_PRESCALER_256 = 7
} SPI_BaudRate_t;

typedef struct {
  SPI_Mode_t Mode;      /* Master/slave selection */
  SPI_CPOL_t CPOL;      /* Clock polarity */
  SPI_CPHA_t CPHA;      /* Clock phase */
  SPI_BaudRate_t BaudRate; /* Prescaler selection */
  uint8_t DataSize;     /* 4 to 16, typically 8 */
  uint8_t LsbFirst;     /* 0 = MSB First, 1 = LSB First */
} SPI_Config_t;

/* Enable clocks and configure GPIOs for the SPI peripheral */
static inline void spi_hal_enable_clocks_and_pins(SPI_TypeDef *SPIx) {
  if (SPIx == SPI1) {
    /* Enable SPI1 Clock */
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* Configure GPIOs
     * PA5: SCK  (AF5)
     * PA6: MISO (AF5)
     * PA7: MOSI (AF5)
     */
    gpio_init(GPIO_PORT_A, 5, GPIO_MODE_AF, GPIO_PULL_NONE, 5);
    gpio_init(GPIO_PORT_A, 6, GPIO_MODE_AF, GPIO_PULL_NONE, 5);
    gpio_init(GPIO_PORT_A, 7, GPIO_MODE_AF, GPIO_PULL_NONE, 5);
  } else if (SPIx == SPI2) {
    /* Enable SPI2 Clock */
    RCC->APB1ENR1 |= RCC_APB1ENR1_SPI2EN;

    /* Configure GPIOs
     * PB13: SCK  (AF5)
     * PB14: MISO (AF5)
     * PB15: MOSI (AF5)
     */
    gpio_init(GPIO_PORT_B, 13, GPIO_MODE_AF, GPIO_PULL_NONE, 5);
    gpio_init(GPIO_PORT_B, 14, GPIO_MODE_AF, GPIO_PULL_NONE, 5);
    gpio_init(GPIO_PORT_B, 15, GPIO_MODE_AF, GPIO_PULL_NONE, 5);
  } else if (SPIx == SPI3) {
    /* Enable SPI3 Clock */
    RCC->APB1ENR1 |= RCC_APB1ENR1_SPI3EN;

    /* Configure GPIOs
     * PC10: SCK  (AF6)
     * PC11: MISO (AF6)
     * PC12: MOSI (AF6)
     */
    gpio_init(GPIO_PORT_C, 10, GPIO_MODE_AF, GPIO_PULL_NONE, 6);
    gpio_init(GPIO_PORT_C, 11, GPIO_MODE_AF, GPIO_PULL_NONE, 6);
    gpio_init(GPIO_PORT_C, 12, GPIO_MODE_AF, GPIO_PULL_NONE, 6);
  }
}

/* Initialize the SPI hardware with the provided configuration */
static inline void spi_hal_init(void *hal_handle, void *config_ptr) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;
  SPI_Config_t *cfg = (SPI_Config_t *)config_ptr;

  if (!SPIx || !cfg) {
    return;
  }

  spi_hal_enable_clocks_and_pins(SPIx);

  /* Disable SPI before configuration */
  SPIx->CR1 &= ~SPI_CR1_SPE;

  /* Configure CR1 */
  uint32_t cr1 = 0;
  if (cfg->Mode == SPI_MODE_MASTER) {
    cr1 |= SPI_CR1_MSTR;
    /* Software Slave Management (SSM=1, SSI=1) for simple master mode */
    cr1 |= (SPI_CR1_SSM | SPI_CR1_SSI);
  }

  if (cfg->CPOL == SPI_CPOL_HIGH)
    cr1 |= SPI_CR1_CPOL;
  if (cfg->CPHA == SPI_CPHA_2EDGE)
    cr1 |= SPI_CR1_CPHA;
  if (cfg->LsbFirst)
    cr1 |= SPI_CR1_LSBFIRST;

  /* Baud rate prescaler selection */
  cr1 |= ((uint32_t)cfg->BaudRate << SPI_CR1_BR_Pos);

  SPIx->CR1 = cr1;

  /* Configure CR2 */
  uint32_t cr2 = 0;
  /* Data Size: DS field is (nbits - 1). For 8-bit, DS = 0x7 */
  uint32_t ds_val = (cfg->DataSize > 0) ? (cfg->DataSize - 1) : 0x7;
  cr2 |= (ds_val << SPI_CR2_DS_Pos);

  /* FIFO Reception Threshold: set for 8-bit to trigger RXNE per byte */
  if (cfg->DataSize <= 8) {
    cr2 |= SPI_CR2_FRXTH;
  }

  SPIx->CR2 = cr2;

  /* Enable SPI */
  SPIx->CR1 |= SPI_CR1_SPE;
}

/* Transfer a single byte (blocking, polled) */
static inline uint8_t spi_hal_transfer_byte(void *hal_handle, uint8_t byte) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;

  /* Wait until Transmit Buffer is Empty */
  uint32_t wait = TIMEOUT_VAL;
  while (!(SPIx->SR & SPI_SR_TXE)) {
    if (wait-- == 0U) {
      break;
    }
    arch_nop();
  }
  
  /* Writing DR starts the transfer and clocks RX */
  *((volatile uint8_t *)&SPIx->DR) = byte;

  /* Wait until recive buffer is not empty */
  wait = TIMEOUT_VAL;
  while (!(SPIx->SR & SPI_SR_RXNE)) {
    if (wait-- == 0U) {
      break;
    }
    arch_nop();
  }

  /* Read DR clears RXNE */
  return *((volatile uint8_t *)&SPIx->DR);
}

/* Enable or disable the SPI RX interrupt */
static inline void spi_hal_enable_rx_irq(void *hal_handle, uint8_t enable) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;
  if (enable) {
    SPIx->CR2 |= SPI_CR2_RXNEIE;
  } else {
    SPIx->CR2 &= ~SPI_CR2_RXNEIE;
  }
}

/* Enable or disable the SPI TX interrupt */
static inline void spi_hal_enable_tx_irq(void *hal_handle, uint8_t enable) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;
  if (enable) {
    SPIx->CR2 |= SPI_CR2_TXEIE;
  } else {
    SPIx->CR2 &= ~SPI_CR2_TXEIE;
  }
}

/* Status helpers for ISR-driven transfers */
static inline uint8_t spi_hal_rx_ready(void *hal_handle) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;
  return (SPIx != NULL) && (SPIx->SR & SPI_SR_RXNE);
}

static inline uint8_t spi_hal_tx_ready(void *hal_handle) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;
  return (SPIx != NULL) && (SPIx->SR & SPI_SR_TXE);
}

static inline uint8_t spi_hal_read_byte(void *hal_handle) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;
  return *((volatile uint8_t *)&SPIx->DR);
}

static inline void spi_hal_write_byte(void *hal_handle, uint8_t byte) {
  SPI_TypeDef *SPIx = (SPI_TypeDef *)hal_handle;
  *((volatile uint8_t *)&SPIx->DR) = byte;
}

#endif /* SPI_HAL */
