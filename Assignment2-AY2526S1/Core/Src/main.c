 /******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 * (c) EE2028 Teaching Team
 ******************************************************************************/


/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_accelero.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_tsensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_hsensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_magneto.h"
#include "stdio.h"

extern void initialise_monitor_handles(void);	// for semi-hosting support (printf)
static void MX_GPIO_Init(void);
void SystemClock_Config(void);
int buttonpress =0;


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == BUTTON_EXTI13_Pin)
	{

		printf("\t Blue button is pressed. \n");
		buttonpress += 1;
	}
}
void checkbuttonpress(void){
		if (buttonpress == 1 && gamechange ==1){
			playermode = 1 - playermode; //toggle between player and enforcer
			if (playermode ==0){
				printf("You are Player! Avoid being caught by the Enforcer!\n");
			}
			else{
				printf("You are Enforcer! Catch the Player!\n");
			}
		}

		if (buttonpress ==2){
			gamechange = (gamechange + 1) %3;//cycle through 3 games
			if (gamechange ==0){
				printf("Game changed to Read light green light!\n");
			}
			else if (gamechange ==1){
				printf("Game changed to catching!\n");
			}
			else{
				printf("Game changed to enchacements\n");
			}
		}	
			buttonpress =0;
}
int main(void)
{
	initialise_monitor_handles(); // for semi-hosting support (printf)

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	MX_GPIO_Init();
	/* Peripheral initializations using BSP functions */
	//BSP_ACCELERO_Init();
	BSP_TSENSOR_Init();
	BSP_HSENSOR_Init();
	BSP_MAGNETO_Init();
	BSP_PSENSOR_Init();
	BSP_LED_Init(LED2);
	int playermode = 0; //0 for player, 1 for enforcer
	int gamechange =0; //0 for catch and run, 1 for other game,2 for another game



	float temp_data;
	float humidity;
	int16_t XYZ[3];
	float pressure;


	while (1)
	{//while start

		  uint32_t tickstart = HAL_GetTick();
		  uint32_t wait = 1000;
		  int counter = 1;//my own haldelay
		  int timeelapsed = 0;

		
		

		
			
		while ((HAL_GetTick() - tickstart) < wait){
			checkbuttonpress();
			
			if (gamechange ==0){
			printf("Red Light Green Light\n");
		}

			if (gamechange ==1){
				printf("catch me if you can\n");
				if (counter == 1){
				//BSP_LED_Toggle(LED2);

					float humidity = BSP_HSENSOR_ReadHumidity();
					printf("Humidity: %f\n",humidity);

					BSP_MAGNETO_GetXYZ(XYZ);
					printf("magnet X : %d;  Y : %d;  Z : %d; \n", XYZ[0], XYZ[1], XYZ[2]);

					temp_data = BSP_TSENSOR_ReadTemp();			// read temperature sensor
					printf("Temperature: %f\n",temp_data);

					pressure = BSP_PSENSOR_ReadPressure();
					printf("Pressure: %f\n",pressure);


					printf("button press count: %d\n",buttonpress);

					counter = 2;
					
			  }
				if (counter ==2){
				   timeelapsed = HAL_GetTick() - tickstart;
				  printf("time used to run in ms:%d\n",timeelapsed);
				  counter = 0;
			  }
			  //if button once, player mode toggle between 1 and 0
			  //if button twice within 1 second, change to other game
			  //make my own print function instead of the complicated uart thing
		  }
		  	if (gamechange ==2){
				printf("Enhancements Activated!\n");
		  	}
		}//timer end

		  


	}//while end

}//main end
static void MX_GPIO_Init(void)
{
	__HAL_RCC_GPIOC_CLK_ENABLE();	// Enable AHB2 Bus for GPIOC

	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Configuration of BUTTON_EXTI13_Pin (GPIO-C Pin-13) as AF,
	GPIO_InitStruct.Pin = BUTTON_EXTI13_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// Enable NVIC EXTI line 13
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}





// /******************************************************************************
// * @file           : main.c
// * @brief          : Main program body
// * (c) EE2028 Teaching Team
// ******************************************************************************/


// /* Includes ------------------------------------------------------------------*/
// #include "main.h"
// #include "stdio.h"
// #include "string.h"
// #include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_tsensor.h"


// static void UART1_Init(void);

// UART_HandleTypeDef huart1;

// int main(void)
// {
//         int seconds_count = 0;
//         /* Reset of all peripherals, Initializes Systick etc. */
//         HAL_Init();
//         BSP_TSENSOR_Init();
//         /* UART initialization  */
//         UART1_Init();
//         while (1)
//         {  	float temp_data;
//         	temp_data = BSP_TSENSOR_ReadTemp();			// read temperature sensor
//                seconds_count++;
//                char message1[] = "Welcome to EE2028 !!!\r\n";       // Fixed message
//                // Be careful about the buffer size used. Here, we assume that seconds_count does not exceed 6 decimal digits
//                char message_print[32];        // UART transmit buffer. See the comment in the line above.
//                sprintf(message_print, "%d: %s", seconds_count, message1);
//                HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF); //Sending in normal mode
//                HAL_Delay(1000);
//         }
// }

// static void UART1_Init(void)
// {
//         /* Pin configuration for UART. BSP_COM_Init() can do this automatically */
//         __HAL_RCC_GPIOB_CLK_ENABLE();
//         __HAL_RCC_USART1_CLK_ENABLE();
//         GPIO_InitTypeDef GPIO_InitStruct = {0};
//         GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
//         GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6;
//         GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//         GPIO_InitStruct.Pull = GPIO_NOPULL;
//         GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//         HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

//         /* Configuring UART1 */
//         huart1.Instance = USART1;
//         huart1.Init.BaudRate = 115200;
//         huart1.Init.WordLength = UART_WORDLENGTH_8B;
//         huart1.Init.StopBits = UART_STOPBITS_1;
//         huart1.Init.Parity = UART_PARITY_NONE;
//         huart1.Init.Mode = UART_MODE_TX_RX;
//         huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
//         huart1.Init.OverSampling = UART_OVERSAMPLING_16;
//         huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
//         huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
//         if (HAL_UART_Init(&huart1) != HAL_OK)
//         {
//           while(1);
//         }

// }

