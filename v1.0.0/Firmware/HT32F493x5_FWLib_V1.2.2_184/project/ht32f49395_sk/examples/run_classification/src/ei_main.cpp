#include "ei_main.h"                  
#include "ht32f493x5_board.h"          
#include "ht32f493x5_clock.h"
#include "ht32f493x5_conf.h"
#include <string.h>
#include "ei_run_classifier.h"         
#include "edge-impulse-sdk/porting/ei_classifier_porting.h" 
#include "math.h"
#include "hm01b0.h"

#define EI_TARGET_WIDTH  96
#define EI_TARGET_HEIGHT 96
#define EI_BUFFER_SIZE 9216

extern uint8_t hm01b0_frame_buffer[HM01B0_IMAGE_SIZE_BYTES];

static uint8_t ei_img_crop_buffer[EI_BUFFER_SIZE];   // crop img form 160*120 to 96*96 (center crop) 

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
	uint8_t *src = &ei_img_crop_buffer[offset];
	
	for(size_t i = 0; i < length; i++)
	{
		uint8_t gray = src[i];
		uint32_t rgb = (gray << 16) | (gray << 8) | gray;
		out_ptr[i] = (float)rgb;
	}
	
	return 0;
}

int ei_main(void)
{
	ei_impulse_result_t result = {0};

	while(1)
	{
		ei_printf("Edge Impulse standalone inferencing (ht32f493x5)\n");

		/* check the DSP input size */
		if(sizeof(ei_img_crop_buffer) != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
		{
			ei_printf("The size of your buffer is not correct. Expected %d items, but had %u\n",
											EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, sizeof(ei_img_crop_buffer));
			return 1;          
		}

		while(1)
		{
			ei_printf("\nStarting inferencing\r\n");

			if(g_hm01b0.frame_ready)
			{
				exint_flag_clear(HM01B0_VSYNC_EXINT_LINE);
				nvic_irq_disable(HM01B0_VSYNC_IRQn);

				g_hm01b0.frame_ready = 0;
				
				hm01b0_effective_crop(&g_hm01b0, ei_img_crop_buffer, EI_TARGET_WIDTH, EI_TARGET_HEIGHT);

				printf("IMG_S");
        for(int i = 0; i < EI_BUFFER_SIZE; i++)
        {
          while(usart_flag_get(USART1, USART_TDBE_FLAG) == RESET);
          usart_data_transmit(USART1, ei_img_crop_buffer[i]);
        }
        printf("IMG_E\r\n");
				
				exint_flag_clear(HM01B0_VSYNC_EXINT_LINE);
				nvic_irq_enable(HM01B0_VSYNC_IRQn, 0, 0);
				
				signal_t features_signal;
				features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
				features_signal.get_data = &raw_feature_get_data;
	
				/* run classifier */
				EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
				printf("Run classifier success\n");
				
				if(res != 0)
				{
					return 1;
				}
				
				printf("INF_S");   // inference results start header 
				ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
						result.timing.dsp, result.timing.classification, result.timing.anomaly);

				#if EI_CLASSIFIER_OBJECT_DETECTION == 1
					for (uint32_t i = 0; i < result.bounding_boxes_count; i++)
					{
						ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
						ei_printf("  %s (", bb.label);
						ei_printf_float(bb.value);
						ei_printf(") [ x: %u, y: %u, width: %u, height: %u ]\r\n",
							bb.x,
							bb.y,
							bb.width,
							bb.height);
					}

				#else
					for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
						ei_printf("  %s: ", result.classification[i].label);
						ei_printf_float(result.classification[i].value);
						ei_printf("\r\n");
					}
				#endif
				printf("INF_E\r\n");
			}
		}
	}
}
