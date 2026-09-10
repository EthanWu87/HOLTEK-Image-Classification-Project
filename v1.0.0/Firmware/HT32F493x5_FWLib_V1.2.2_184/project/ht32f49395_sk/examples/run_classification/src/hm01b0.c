#include "hm01b0.h"

int hm01b0_init(hm01b0_t *camera, uint8_t *buffer, uint32_t size)
{
  if((camera == NULL) || (buffer == NULL) || (size == 0))
  {
    return -1;
  }

  memset(camera, 0, sizeof(hm01b0_t));

  camera->hi2c.i2cx          = HM01B0_I2C_PORT;
  camera->frame_buffer       = buffer;
  camera->frame_buffer_size  = size;
  camera->frame_ready        = 0;
  camera->model_id           = 0;

  /* QQVGA (160x120 effective image) */
  camera->readout_x          = 0x03;
  camera->readout_y          = 0x03;
  camera->binning_mode       = 0x03;
  camera->qvga_win_en        = 0x01;

  camera->frame_length_lines = 0x0390;
  camera->line_length_pclk   = 0x00D7;

  /* 8-bit mode */
  camera->bit_control        = 0x02;

  /* Clock divider */
  camera->osc_clk_div        = 0x28;

  /* Auto exposure */
  camera->ae_enable          = 0x01;
  camera->ae_target          = 0x5A;
  camera->ae_max_integration = 0x0390;

  /* Group hold / consume */
  camera->group_hold         = 0x01;
  camera->group_consume      = 0x00;

  /* OSC mode */
  camera->osc_mode_en        = 0x01;

  /* Walking-1 test pattern */
  camera->test_pattern       = 0x11;

  hm01b0_mclk_init();

  hm01b0_parallel_init(camera);

  hm01b0_i2c_init(camera);

  if(hm01b0_reset(camera) != 0)
  {
    printf("HM01B0 reset failed!\r\n");
    
    return -2;
  }

  camera->model_id = hm01b0_read_reg16(camera, 0x0000);
  if(camera->model_id != 0x01B0)
  {
    printf("Invalid HM01B0 model ID: 0x%04X\r\n", camera->model_id);
    
    return -3;
  }

  /* HM01B0 QQVGA(8-bit configuration) */
  hm01b0_write_reg8(camera, 0x3059, camera->bit_control);

  hm01b0_write_reg8(camera, 0x0383, camera->readout_x);
  hm01b0_write_reg8(camera, 0x0387, camera->readout_y);
  hm01b0_write_reg8(camera, 0x0390, camera->binning_mode);

  hm01b0_write_reg8(camera, 0x3010, camera->qvga_win_en);

  hm01b0_write_reg16(camera, 0x0340, camera->frame_length_lines);
  hm01b0_write_reg16(camera, 0x0342, camera->line_length_pclk);

  hm01b0_write_reg8(camera, 0x3060, camera->osc_clk_div);

  /* Integration time = frame_length_lines / 2 */
  hm01b0_write_reg16(camera, 0x0202, camera->frame_length_lines / 2);

  /* Auto exposure */
  hm01b0_write_reg8(camera, 0x2100, camera->ae_enable);
  hm01b0_write_reg8(camera, 0x2101, camera->ae_target);

  hm01b0_write_reg8(camera, 0x2105, (uint8_t)((camera->ae_max_integration >> 8) & 0xFF));
  hm01b0_write_reg8(camera, 0x2106, (uint8_t)(camera->ae_max_integration & 0xFF));
  
  hm01b0_write_reg8(camera, 0x0104, camera->group_hold);
  hm01b0_write_reg8(camera, 0x0104, camera->group_consume);

  /* Test pattern */
  // hm01b0_write_reg8(camera, 0x0601, camera->test_pattern);

  hm01b0_write_reg8(camera, 0x3067, camera->osc_mode_en);

  /* Start streaming: PCLK / HREF / VSYNC */
  hm01b0_write_reg8(camera, 0x0100, 0x01);

  return 0;
}

int hm01b0_reset(hm01b0_t *camera)
{
  if(camera == NULL)
  {
    return -1;
  }

  hm01b0_write_reg8(camera, 0x0103, 0x01);

  for(int retries = 0; retries < 10; retries++)
  {
    if(hm01b0_read_reg8(camera, 0x0100) == 0x00)
    {
      return 0;
    }

    delay_ms(100);
  }

  return -1;
}

void hm01b0_mclk_init(void)
{
  gpio_init_type          gpio_init_struct;
  tmr_output_config_type  tmr_output_struct;
  crm_clocks_freq_type    crm_clocks_freq_struct;

  uint16_t timer_period  = 0;
  uint16_t channel_pulse = 0;

  crm_clocks_freq_get(&crm_clocks_freq_struct);

  crm_periph_clock_enable(HM01B0_MCLK_TMR_CLK, TRUE);
  crm_periph_clock_enable(HM01B0_MCLK_CLK, TRUE);

  /* MCLK(PA7) -> TMR3_CH2 */
  gpio_init_struct.gpio_pins           = HM01B0_MCLK_PIN;
  gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
  gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;

  gpio_init(HM01B0_MCLK_PORT, &gpio_init_struct);

  /* Generate HM01B0_MCLK_FREQ with 50% duty cycle */
  timer_period = (uint16_t)((crm_clocks_freq_struct.sclk_freq / HM01B0_MCLK_FREQ) - 1U);

  channel_pulse = (uint16_t)((timer_period + 1U) / 2U);

  tmr_base_init(HM01B0_MCLK_TMR, timer_period, 0);
  tmr_cnt_dir_set(HM01B0_MCLK_TMR, TMR_COUNT_UP);

  tmr_output_default_para_init(&tmr_output_struct);

  tmr_output_struct.oc_mode         = TMR_OUTPUT_CONTROL_PWM_MODE_A;
  tmr_output_struct.oc_output_state = TRUE;
  tmr_output_struct.oc_polarity     = TMR_OUTPUT_ACTIVE_HIGH;

  tmr_output_channel_config(HM01B0_MCLK_TMR, HM01B0_MCLK_TMR_CH, &tmr_output_struct);

  tmr_channel_value_set(HM01B0_MCLK_TMR, HM01B0_MCLK_TMR_CH, channel_pulse);

  tmr_output_enable(HM01B0_MCLK_TMR, TRUE);
  tmr_counter_enable(HM01B0_MCLK_TMR, TRUE);
}

void hm01b0_i2c_init(hm01b0_t *camera)
{
  gpio_init_type gpio_initstructure;
  i2c_handle_type *hi2c;

  if(camera == NULL)
  {
    return;
  }

  hi2c = &camera->hi2c;

  i2c_reset(hi2c->i2cx);

  if(hi2c->i2cx == HM01B0_I2C_PORT)
  {
    /* TXS0108E level translator OE (enable) */
    crm_periph_clock_enable(TXS0108EPWR_OE_GPIO_CLK, TRUE);
    
    gpio_initstructure.gpio_pins           = TXS0108EPWR_OE_PIN;
    gpio_initstructure.gpio_mode           = GPIO_MODE_OUTPUT;
    gpio_initstructure.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_initstructure.gpio_pull           = GPIO_PULL_NONE;
    gpio_initstructure.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    
    gpio_init(TXS0108EPWR_OE_GPIO_PORT, &gpio_initstructure);
    
    gpio_bits_set(TXS0108EPWR_OE_GPIO_PORT, TXS0108EPWR_OE_PIN);

    /* I2C SCL(PB8), SDA(PB9) */
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    
    crm_periph_clock_enable(HM01B0_I2C_CLK, TRUE);
    crm_periph_clock_enable(HM01B0_I2C_SCL_GPIO_CLK, TRUE);
    crm_periph_clock_enable(HM01B0_I2C_SDA_GPIO_CLK, TRUE);
    gpio_pin_remap_config(I2C1_MUX, TRUE);

    gpio_initstructure.gpio_out_type       = GPIO_OUTPUT_OPEN_DRAIN;
    gpio_initstructure.gpio_pull           = GPIO_PULL_UP;
    gpio_initstructure.gpio_mode           = GPIO_MODE_MUX;
    gpio_initstructure.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    
    gpio_initstructure.gpio_pins = HM01B0_I2C_SCL_PIN;
    gpio_init(HM01B0_I2C_SCL_GPIO_PORT, &gpio_initstructure);

    gpio_initstructure.gpio_pins = HM01B0_I2C_SDA_PIN;
    gpio_init(HM01B0_I2C_SDA_GPIO_PORT, &gpio_initstructure);

    i2c_init(hi2c->i2cx, I2C_FSMODE_DUTY_2_1, HM01B0_I2C_SPEED);

    nvic_irq_enable(HM01B0_I2C_EVT_IRQn, 1, 0);
    nvic_irq_enable(HM01B0_I2C_ERR_IRQn, 1, 0);
  }

  i2c_enable(hi2c->i2cx, TRUE);
}

void hm01b0_parallel_init(hm01b0_t *camera)
{
  gpio_init_type        gpio_init_struct;
  dma_init_type         dma_init_struct;
  exint_init_type       exint_init_struct;
  tmr_input_config_type tmr_ic_init_struct;

  if((camera == NULL) || (camera->frame_buffer == NULL) || (camera->frame_buffer_size == 0U))
  {
    return;
  }

  /* Peripheral clocks */
  crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(HM01B0_DATA_GPIO_CLK, TRUE);
  crm_periph_clock_enable(HM01B0_VSYNC_CLK, TRUE);
  crm_periph_clock_enable(HM01B0_HREF_CLK, TRUE);
  crm_periph_clock_enable(HM01B0_PCLK_CLK, TRUE);
  crm_periph_clock_enable(HM01B0_PCLK_TMR_CLK, TRUE);
  crm_periph_clock_enable(HM01B0_DMA_CLK, TRUE);

  /* Data Pins: PC0 ~ PC7 */
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode           = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
  gpio_init_struct.gpio_pins           = HM01B0_DATA_PINS;
  gpio_init(HM01B0_DATA_PORT, &gpio_init_struct);

  /* VSYNC(PA2), HREF(PA4) */
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_pins = HM01B0_VSYNC_PIN;
  gpio_init(HM01B0_VSYNC_PORT, &gpio_init_struct);

  /* HREF(PA4) */
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_pins = HM01B0_HREF_PIN;
  gpio_init(HM01B0_HREF_PORT, &gpio_init_struct);

  /* PCLK(PB0) */
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
  gpio_init_struct.gpio_pins = HM01B0_PCLK_PIN;
  gpio_init(HM01B0_PCLK_PORT, &gpio_init_struct);
  gpio_pin_remap_config(TMR3_MUX_10, TRUE);

  /* VSYNC EXINT */
  gpio_exint_line_config(HM01B0_VSYNC_PORT_SRC, HM01B0_VSYNC_PIN_SRC);
  exint_init_struct.line_enable   = TRUE;
  exint_init_struct.line_mode     = EXINT_LINE_INTERRUPT;
  exint_init_struct.line_select   = HM01B0_VSYNC_EXINT_LINE;
  exint_init_struct.line_polarity = EXINT_TRIGGER_RISING_EDGE;
  exint_init(&exint_init_struct);

  exint_flag_clear(HM01B0_VSYNC_EXINT_LINE);
  nvic_irq_enable(HM01B0_VSYNC_IRQn, 0, 0);

  /* HREF EXINT */
  gpio_exint_line_config(HM01B0_HREF_PORT_SRC, HM01B0_HREF_PIN_SRC);
  exint_init_struct.line_enable   = TRUE;
  exint_init_struct.line_mode     = EXINT_LINE_INTERRUPT;
  exint_init_struct.line_select   = HM01B0_HREF_EXINT_LINE;
  exint_init_struct.line_polarity = EXINT_TRIGGER_RISING_EDGE;
  exint_init(&exint_init_struct);

  exint_flag_clear(HM01B0_HREF_EXINT_LINE);
  nvic_irq_enable(HM01B0_HREF_IRQn, 1, 0);

  /* PCLK -> TMR3_CH3 Input Capture -> DMA request */
  tmr_input_default_para_init(&tmr_ic_init_struct);
  tmr_ic_init_struct.input_channel_select = HM01B0_PCLK_TMR_CH;
  tmr_ic_init_struct.input_mapped_select = TMR_CC_CHANNEL_MAPPED_DIRECT;
  tmr_ic_init_struct.input_polarity_select = TMR_INPUT_RISING_EDGE;
  tmr_input_channel_init(HM01B0_PCLK_TMR, &tmr_ic_init_struct, TMR_CHANNEL_INPUT_DIV_1);
  tmr_dma_request_enable(HM01B0_PCLK_TMR, HM01B0_PCLK_DMA_REQ, TRUE);
  tmr_counter_enable(HM01B0_PCLK_TMR, TRUE);

  /* Flexible DMA mapping: TMR3_CH3 -> DMA1 Channel2 */
  dma_flexible_config(HM01B0_DMA, HM01B0_DMA_FLEX_CHANNEL, HM01B0_PCLK_DMA_FLEX_REQ);

  /*
   * DMA
   *
   * Each PCLK rising edge generates one DMA request.
   * DMA reads the low byte of GPIOC IDT and writes one byte
   * into the HM01B0 frame buffer.
   */
  dma_reset(HM01B0_DMA_CHANNEL);
  dma_init_struct.peripheral_base_addr = (uint32_t)&HM01B0_DATA_PORT->idt;
  dma_init_struct.memory_base_addr = (uint32_t)camera->frame_buffer;
  dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
  dma_init_struct.buffer_size = camera->frame_buffer_size;
  dma_init_struct.peripheral_inc_enable = FALSE;
  dma_init_struct.memory_inc_enable     = TRUE;
  dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
  dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
  dma_init_struct.loop_mode_enable = FALSE;
  dma_init_struct.priority         = DMA_PRIORITY_HIGH;
  dma_init(HM01B0_DMA_CHANNEL, &dma_init_struct);
  dma_interrupt_enable(HM01B0_DMA_CHANNEL, DMA_FDT_INT, TRUE);
  nvic_irq_enable(HM01B0_DMA_IRQn, 0, 0);
}

void hm01b0_effective_crop(const hm01b0_t *camera, uint8_t *dstImage, uint16_t dstWidth, uint16_t dstHeight)
{
  uint16_t start_col;
  uint16_t start_row;

  start_col = ((HM01B0_IMAGE_WIDTH_EFFECTIVE - dstWidth) / 2) + (HM01B0_IMAGE_WIDTH_DUMMY / 2);

  start_row = ((HM01B0_IMAGE_HEIGHT_EFFECTIVE - dstHeight) / 2) + (HM01B0_IMAGE_HEIGHT_DUMMY / 2);

  for(uint16_t row = 0; row < dstHeight; row++)
  {
    const uint8_t *source = &camera->frame_buffer[(start_row + row) * HM01B0_IMAGE_WIDTH_ACTIVE + start_col];

    memcpy(&dstImage[row * dstWidth], source, dstWidth);
  }
}

void hm01b0_write_reg8(hm01b0_t *camera, uint16_t address, uint8_t value)
{
  uint8_t data[3];

  if(camera == NULL)
  {
    return;
  }

  *((uint16_t *)data) = __REV16(address);
  data[2] = value;

  i2c_master_transmit(&camera->hi2c, HM01B0_I2C_ADDRESS, data, sizeof(data), HM01B0_I2C_TIMEOUT);
}

void hm01b0_write_reg16(hm01b0_t *camera, uint16_t address, uint16_t value)
{
  uint8_t data[4];

  if(camera == NULL)
  {
    return;
  }

  *((uint16_t *)data + 0) = __REV16(address);
  *((uint16_t *)data + 1) = __REV16(value);

  i2c_master_transmit(&camera->hi2c, HM01B0_I2C_ADDRESS, data, sizeof(data), HM01B0_I2C_TIMEOUT);
}

uint8_t hm01b0_read_reg8(hm01b0_t *camera, uint16_t address)
{
  uint8_t result = 0xFF;

  if(camera == NULL)
  {
    return result;
  }

  i2c_memory_read(&camera->hi2c, I2C_MEM_ADDR_WIDIH_16, HM01B0_I2C_ADDRESS, address, &result, 1, HM01B0_I2C_TIMEOUT);

  return result;
}

uint16_t hm01b0_read_reg16(hm01b0_t *camera, uint16_t address)
{
  uint8_t data[2] = {0, 0};
  uint16_t result = 0xFFFF;

  if(camera == NULL)
  {
    return result;
  }

  if(i2c_memory_read(&camera->hi2c, I2C_MEM_ADDR_WIDIH_16, HM01B0_I2C_ADDRESS, address, data, 2, HM01B0_I2C_TIMEOUT) == I2C_OK)
  {
    result = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
  }

  return result;
}
