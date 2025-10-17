//  /******************************************************************************
//  * @file           : main.c
//  * @brief          : Main program body
//  * (c) EE2028 Teaching Team
//  ******************************************************************************/
//
//
///* Includes ------------------------------------------------------------------*/
//#include "main.h"
//#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_accelero.h"
//#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_tsensor.h"
//#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01.h"
//#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_hsensor.h"
//#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_magneto.h"
//#include "stdio.h"
//
//extern void initialise_monitor_handles(void);	// for semi-hosting support (printf)
//
//int main(void)
//{
//	initialise_monitor_handles(); // for semi-hosting support (printf)
//
//	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
//	HAL_Init();
//
//	/* Peripheral initializations using BSP functions */
//	//BSP_ACCELERO_Init();
//	//BSP_TSENSOR_Init();
//	BSP_HSENSOR_Init();
//	BSP_MAGNETO_Init();
//	BSP_LED_Init(LED2);
//
//	while (1)
//	{
//		/*
//		float accel_data[3];
//		int16_t accel_data_i16[3] = { 0 };			// array to store the x, y and z readings.
//		BSP_ACCELERO_AccGetXYZ(accel_data_i16);		// read accelerometer
//		// the function above returns 16 bit integers which are acceleration in mg (9.8/1000 m/s^2).
//		// Converting to float to print the actual acceleration.
//		accel_data[0] = (float)accel_data_i16[0] * (9.8/1000.0f);
//		accel_data[1] = (float)accel_data_i16[1] * (9.8/1000.0f);
//		accel_data[2] = (float)accel_data_i16[2] * (9.8/1000.0f);
//
//		float temp_data;
//		temp_data = BSP_TSENSOR_ReadTemp();			// read temperature sensor
//
//		printf("Accel X : %f; Accel Y : %f; Accel Z : %f; Temperature : %f\n", accel_data[0], accel_data[1], accel_data[2], temp_data);
//
//		HAL_Delay(1000);	// read once a ~second.
//		*/
//
//
//
//
//
//		  uint32_t tickstart = HAL_GetTick();
//		  uint32_t wait = 1000;
//		  int counter = 1;
//		  int timeelapsed = 0;
//
//
//		  while ((HAL_GetTick() - tickstart) < wait)
//		  {
//			  if (counter == 1){
//			  BSP_LED_Toggle(LED2);
//			  		//humidity
//			  		float humidity = BSP_HSENSOR_ReadHumidity();
//			  		printf("Humidity: %f\n",humidity);
//			  		int16_t XYZ[3];
//			  		BSP_MAGNETO_GetXYZ(XYZ);
//			  		printf(" X : %d;  Y : %d;  Z : %d; \n", XYZ[0], XYZ[1], XYZ[2]);
//			  		counter = 2;
//			  }
//			  if (counter ==2){
//				   timeelapsed = HAL_GetTick() - tickstart;
//				  printf("time used to run in ms:%d\n",timeelapsed);
//				  counter = 0;
//			  }
//
//		  }
//
//
//	}
//
//}
//
//
//


/******************************************************************************
* @file           : main.c
* @brief          : Main program body
* (c) EE2028 Teaching Team
******************************************************************************/


/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stdio.h"
#include "string.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_tsensor.h"


static void UART1_Init(void);

UART_HandleTypeDef huart1;

int main(void)
{
        int seconds_count = 0;
        /* Reset of all peripherals, Initializes Systick etc. */
        HAL_Init();
        BSP_TSENSOR_Init();
        /* UART initialization  */
        UART1_Init();
        while (1)
        {
        		float temp_data;
        		temp_data = BSP_TSENSOR_ReadTemp();			// read temperature sensor

               seconds_count++;
               char message1[] = "Welcome to EE2028 !!!\r\n";       // Fixed message
               // Be careful about the buffer size used. Here, we assume that seconds_count does not exceed 6 decimal digits
               char message_print[32];        // UART transmit buffer. See the comment in the line above.
               sprintf(message_print, "%d: %s", seconds_count, message1);
               HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF); //Sending in normal mode
               HAL_Delay(1000);
        }
}

static void UART1_Init(void)
{
        /* Pin configuration for UART. BSP_COM_Init() can do this automatically */
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_USART1_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* Configuring UART1 */
        huart1.Instance = USART1;
        huart1.Init.BaudRate = 115200;
        huart1.Init.WordLength = UART_WORDLENGTH_8B;
        huart1.Init.StopBits = UART_STOPBITS_1;
        huart1.Init.Parity = UART_PARITY_NONE;
        huart1.Init.Mode = UART_MODE_TX_RX;
        huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart1.Init.OverSampling = UART_OVERSAMPLING_16;
        huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
        huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
        if (HAL_UART_Init(&huart1) != HAL_OK)
        {
          while(1);
        }

}

