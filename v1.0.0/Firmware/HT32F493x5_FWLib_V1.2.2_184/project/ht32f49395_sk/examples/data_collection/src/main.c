#include "ht32f493x5_board.h"
#include "ht32f493x5_clock.h"
#include "i2c_application.h"
#include "ht32f493x5_int.h"
#include "hm01b0.h"

#define UART_BAUD_RATE    2250000

#define EI_TARGET_WIDTH   96
#define EI_TARGET_HEIGHT  96
#define EI_BUFFER_SIZE    (EI_TARGET_WIDTH * EI_TARGET_HEIGHT)

hm01b0_t g_hm01b0;

static uint8_t hm01b0_frame_buffer[HM01B0_IMAGE_SIZE_BYTES];

static uint8_t edge_impulse_clean_buffer[EI_BUFFER_SIZE];

int main(void)
{
  int width_offset, height_offset, start_col, start_row;
  
  system_clock_config();

  ht32_board_init();

  uart_print_init(UART_BAUD_RATE);

  printf("\r\n--- System Starting ---\r\n");

  if(hm01b0_init(&g_hm01b0, hm01b0_frame_buffer, sizeof(hm01b0_frame_buffer)) != 0)
  {
    printf("HM01B0 initialization failed!\r\n");

    while(1)
    {
      
    }
  }

  printf("HM01B0 Model ID: 0x%04X\r\n", g_hm01b0.model_id);
  printf("HM01B0 initialized.\r\n");

  while(1)
  {
    if(g_hm01b0.frame_ready)
    {
      exint_flag_clear(HM01B0_VSYNC_EXINT_LINE);
      nvic_irq_disable(HM01B0_VSYNC_IRQn);

      g_hm01b0.frame_ready = 0;

      hm01b0_effective_crop(&g_hm01b0, edge_impulse_clean_buffer, EI_TARGET_WIDTH, EI_TARGET_HEIGHT);

      printf("IMG_S");
      for(int i = 0; i < EI_BUFFER_SIZE; i++)
      {
        while(usart_flag_get(USART1, USART_TDBE_FLAG) == RESET);
        usart_data_transmit(USART1, edge_impulse_clean_buffer[i]);
      }
      printf("IMG_E\r\n");

      exint_flag_clear(HM01B0_VSYNC_EXINT_LINE);
      nvic_irq_enable(HM01B0_VSYNC_IRQn, 0, 0);
    }
  }
}
