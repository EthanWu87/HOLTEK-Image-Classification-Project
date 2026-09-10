#ifndef _HM01B0_H_
#define _HM01B0_H_

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "ht32f493x5.h"
#include "ht32f493x5_board.h"
#include "i2c_application.h"
#include "ht32f493x5_exint.h"
#include "ht32f493x5_spi.h"
#include "ht32f493x5_dma.h"

/**
 * HM01B0 QQVGA Timing & Frame Structure
 *
 * 1 VSYNC Frame = 122 HREF Pulses
 *   1 Top Border Row + 120 Active Rows + 1 Bottom Border Row
 *
 * 1 HREF Line = 164 PCLK Cycles
 *   2 Left Border Bytes + 160 Active Bytes + 2 Right Border Bytes
 */
#define HM01B0_IMAGE_WIDTH_ACTIVE       164
#define HM01B0_IMAGE_WIDTH_DUMMY        4
#define HM01B0_IMAGE_WIDTH_EFFECTIVE    (HM01B0_IMAGE_WIDTH_ACTIVE - HM01B0_IMAGE_WIDTH_DUMMY)

#define HM01B0_IMAGE_HEIGHT_ACTIVE      122
#define HM01B0_IMAGE_HEIGHT_DUMMY       2
#define HM01B0_IMAGE_HEIGHT_EFFECTIVE   (HM01B0_IMAGE_HEIGHT_ACTIVE - HM01B0_IMAGE_HEIGHT_DUMMY)

#define HM01B0_IMAGE_SIZE_BYTES         (HM01B0_IMAGE_WIDTH_ACTIVE * HM01B0_IMAGE_HEIGHT_ACTIVE)

#define HM01B0_I2C_TIMEOUT              0x10000
#define HM01B0_I2C_SPEED                100000
#define HM01B0_I2C_ADDRESS              0x48

#define HM01B0_MCLK_FREQ                12000000

/* TXS0108EPWR OE(PD2) PIN (voltage translator) */
#define TXS0108EPWR_OE_PIN              GPIO_PINS_2
#define TXS0108EPWR_OE_GPIO_PORT        GPIOD
#define TXS0108EPWR_OE_GPIO_CLK         CRM_GPIOD_PERIPH_CLOCK

/* I2C SCL(PB8), SDA(PB9) */
#define HM01B0_I2C_PORT                 I2C1
#define HM01B0_I2C_CLK                  CRM_I2C1_PERIPH_CLOCK
#define HM01B0_I2C_SCL_PIN              GPIO_PINS_8
#define HM01B0_I2C_SCL_GPIO_PORT        GPIOB
#define HM01B0_I2C_SCL_GPIO_CLK         CRM_GPIOB_PERIPH_CLOCK
#define HM01B0_I2C_SDA_PIN              GPIO_PINS_9
#define HM01B0_I2C_SDA_GPIO_PORT        GPIOB
#define HM01B0_I2C_SDA_GPIO_CLK         CRM_GPIOB_PERIPH_CLOCK
#define HM01B0_I2C_EVT_IRQn             I2C1_EVT_IRQn
#define HM01B0_I2C_ERR_IRQn             I2C1_ERR_IRQn

/* 8-Bit Data Bus: PC0 ~ PC7 */
#define HM01B0_DATA_PORT                GPIOC
#define HM01B0_DATA_PINS                (GPIO_PINS_0 | GPIO_PINS_1 | GPIO_PINS_2 | GPIO_PINS_3 | \
                                         GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6 | GPIO_PINS_7)
#define HM01B0_DATA_GPIO_CLK            CRM_GPIOC_PERIPH_CLOCK

/* VSYNC(PA2) */
#define HM01B0_VSYNC_PIN                GPIO_PINS_2
#define HM01B0_VSYNC_PORT               GPIOA
#define HM01B0_VSYNC_CLK                CRM_GPIOA_PERIPH_CLOCK
#define HM01B0_VSYNC_PORT_SRC           GPIO_PORT_SOURCE_GPIOA
#define HM01B0_VSYNC_PIN_SRC            GPIO_PINS_SOURCE2
#define HM01B0_VSYNC_EXINT_LINE         EXINT_LINE_2
#define HM01B0_VSYNC_IRQn               EXINT2_IRQn

/* HREF(PA4) */
#define HM01B0_HREF_PIN                 GPIO_PINS_4
#define HM01B0_HREF_PORT                GPIOA
#define HM01B0_HREF_CLK                 CRM_GPIOA_PERIPH_CLOCK
#define HM01B0_HREF_PORT_SRC            GPIO_PORT_SOURCE_GPIOA
#define HM01B0_HREF_PIN_SRC             GPIO_PINS_SOURCE4
#define HM01B0_HREF_EXINT_LINE          EXINT_LINE_4
#define HM01B0_HREF_IRQn                EXINT4_IRQn

/* MCLK(PA7) - TMR3_CH2 */
#define HM01B0_MCLK_PIN                 GPIO_PINS_7
#define HM01B0_MCLK_PORT                GPIOA
#define HM01B0_MCLK_CLK                 CRM_GPIOA_PERIPH_CLOCK
#define HM01B0_MCLK_TMR                 TMR3
#define HM01B0_MCLK_TMR_CLK             CRM_TMR3_PERIPH_CLOCK
#define HM01B0_MCLK_TMR_CH              TMR_SELECT_CHANNEL_2

/* PCLK(PB0) - TMR3_CH3 Input Capture -> DMA */
#define HM01B0_PCLK_PIN                 GPIO_PINS_0
#define HM01B0_PCLK_PORT                GPIOB
#define HM01B0_PCLK_CLK                 CRM_GPIOB_PERIPH_CLOCK
#define HM01B0_PCLK_TMR                 TMR3
#define HM01B0_PCLK_TMR_CLK             CRM_TMR3_PERIPH_CLOCK
#define HM01B0_PCLK_TMR_CH              TMR_SELECT_CHANNEL_3
#define HM01B0_PCLK_DMA_REQ             TMR_C3_DMA_REQUEST
#define HM01B0_PCLK_DMA_FLEX_REQ        DMA_FLEXIBLE_TMR3_CH3

/* DMA */
#define HM01B0_DMA                      DMA1
#define HM01B0_DMA_CHANNEL              DMA1_CHANNEL2
#define HM01B0_DMA_FLEX_CHANNEL         FLEX_CHANNEL2
#define HM01B0_DMA_IRQn                 DMA1_Channel2_IRQn
#define HM01B0_DMA_FDT_FLAG             DMA1_FDT2_FLAG
#define HM01B0_DMA_CLK                  CRM_DMA1_PERIPH_CLOCK


typedef struct
{
  i2c_handle_type hi2c;

  uint8_t *frame_buffer;
  uint32_t frame_buffer_size;

  /* Runtime state */
  volatile uint8_t frame_ready;
  uint16_t model_id;

  /* Sensor configuration */
  uint8_t  readout_x;
  uint8_t  readout_y;
  uint8_t  binning_mode;
  uint8_t  qvga_win_en;

  uint16_t frame_length_lines;
  uint16_t line_length_pclk;

  uint8_t  bit_control;
  uint8_t  osc_clk_div;

  uint8_t  ae_enable;
  uint8_t  ae_min_mean;
  uint8_t  ae_target;
  uint16_t ae_max_integration;

  uint8_t  group_hold;
  uint8_t  group_consume;

  uint8_t  osc_mode_en;
  uint8_t  test_pattern;
} hm01b0_t;

#ifdef __cplusplus
extern "C" {
#endif

extern hm01b0_t g_hm01b0;

int  hm01b0_init(hm01b0_t *camera, uint8_t *buffer, uint32_t size);
void hm01b0_mclk_init(void);
void hm01b0_i2c_init(hm01b0_t *camera);
void hm01b0_parallel_init(hm01b0_t *camera);

int hm01b0_reset(hm01b0_t *camera);

void hm01b0_effective_crop(const hm01b0_t *camera, uint8_t *dstImage, uint16_t dstWidth, uint16_t dstHeight);

void hm01b0_write_reg8(hm01b0_t *camera, uint16_t address, uint8_t value);
void hm01b0_write_reg16(hm01b0_t *camera, uint16_t address, uint16_t value);
uint8_t hm01b0_read_reg8(hm01b0_t *camera, uint16_t address);
uint16_t hm01b0_read_reg16(hm01b0_t *camera, uint16_t address);

#ifdef __cplusplus
}
#endif

#endif
