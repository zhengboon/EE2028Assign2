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
#include <stdarg.h>

static void UART1_Init(void);


extern void initialise_monitor_handles(void);	// for semi-hosting support (printu)
static void MX_GPIO_Init(void);
void SystemClock_Config(void);
int buttonpress =0;
UART_HandleTypeDef huart1;


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == BUTTON_EXTI13_Pin)
	{

		printu("\t Blue button is pressed. \n");
		buttonpress += 1;
		printu("button press count: %d\n",buttonpress);
			//debounce delay
	}
}

void printu(const char *format, ...)
{
    char buffer[256];  // Adjust size as needed
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0)
    {
        HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, HAL_MAX_DELAY);
    }
}
int main(void)
{
	initialise_monitor_handles(); // for semi-hosting support (printu)

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	MX_GPIO_Init();
	UART1_Init();

	/* Peripheral initializations using BSP functions */
	//BSP_ACCELERO_Init();
	BSP_TSENSOR_Init();
	BSP_HSENSOR_Init();
	BSP_MAGNETO_Init();
	BSP_PSENSOR_Init();
	BSP_LED_Init(LED2);
	int playermode = 0; //0 for player, 1 for enforcer
	int gamechange =0; //0 for catch and run, 1 for other game,2 for another game
	int magnettresholdflag = 0;
	int magnettresholdcounter = 0;
	void checkbuttonpress(void){
		if (buttonpress == 3 && gamechange ==1){
			playermode = 1 - playermode; //toggle between player and enforcer
			if (playermode ==0){
				printu("You are Player! Avoid being caught by the Enforcer!\n");
			}
			else{
				printu("You are Enforcer! Catch the Player!\n");
			}
		}

		if (buttonpress ==2){
			gamechange = (gamechange + 1) %2;//cycle through 3 games, i hate the requirements
			if (gamechange ==0){
				printu("Game changed to Read light green light!\n");
			}
			else if (gamechange ==1){
				printu("Game changed to catching!\n");
			}
			
		}
		if (buttonpress ==3&& gamechange ==0){
			printu("Enhancements Activated!\n");
			gamechange =2;
		}
		if (magnettresholdflag == 1 && buttonpress ==1 && magnettresholdcounter <3&& gamechange ==1){
			magnettresholdcounter =0;
			magnettresholdflag =0;
			if (playermode == 0){
				printu("Player escaped, good job!\n");
			}
			else{
				printu("Enforcer caught the player, well done!\n");
			}
		}
		else if (magnettresholdflag ==1 && magnettresholdcounter >=3 && gamechange ==1){
			magnettresholdcounter =0;
			magnettresholdflag =0;
			if (playermode ==0){
				printu("Player caught! Better luck next time!\n");
				while (HAL_GPIO_ReadPin(GPIOC, BUTTON_EXTI13_Pin) == GPIO_PIN_RESET);
			}
			else{
				printu("Enforcer failed to catch the player. Try harder!\n");
			}
		}
		buttonpress =0;
		// if (buttonpress >4){
		// 	printu("back to red light green light\n");
		// 	gamechange =0;
		// }		
			
}

	// void myowndelay(uint32_t delay){
	// 	uint32_t tickstart = HAL_GetTick();
	// 	while ((HAL_GetTick() - tickstart) < delay){
	// 	}
	// }


	void ledblinker(int threshold){
		if (threshold ==2)
			{
			BSP_LED_On(LED2);
			HAL_Delay (10);
			BSP_LED_Off(LED2);
			HAL_Delay (10);
			}
		else if (threshold ==1)
			{
			BSP_LED_On(LED2);
			HAL_Delay (20);
			BSP_LED_Off(LED2);
			HAL_Delay (20);
			}
		else if (threshold ==0)
			{
			BSP_LED_On(LED2);
			HAL_Delay (40);
			BSP_LED_Off(LED2);
			HAL_Delay (40);
			}
	}
	
	float temp_data;
	float temp_treshhold = 30; 
	float humidity;
	float humidity_treshhold = 70; //example threshold
	int16_t XYZ[3];
	int inital_magnet = 0;
	int magnet_treshhold[3] = {500,2000,10000}; //example threshold//500, 2000, 10000
	float pressure;
	float pressure_treshhold = 63; //example threshold
	BSP_MAGNETO_GetXYZ(XYZ);
		inital_magnet = abs(XYZ[0]) + abs(XYZ[1]) + abs(XYZ[2]);
	int timer = 0;
	int initialtime = 0;

	while (1)
	{//while start

		  uint32_t tickstart = HAL_GetTick();
		  uint32_t wait = 1000;
		  int counter = 1;//my own haldelay
		  int timeelapsed = 0;
		timer +=1;

		
			
		while ((HAL_GetTick() - tickstart) < wait){
			
			//myowndelay(300); //check button every 100ms
			checkbuttonpress();
			if (gamechange ==0){
			printu("Red Light Green Light\n");
		}

			if (gamechange ==1){
				//counter ==1;
				printu("catch me if you can\n");
				//printu("%d\n",counter);
				BSP_MAGNETO_GetXYZ(XYZ);
				//printu("magnet X : %d;  Y : %d;  Z : %d; \n", XYZ[0], XYZ[1], XYZ[2]);
				int diffmagnet= abs(XYZ[0]) + abs(XYZ[1]) + abs(XYZ[2]) - inital_magnet;
				if (diffmagnet <0){
					diffmagnet = -diffmagnet;
				}
				if (diffmagnet > magnet_treshhold[2]){
					printu("%d",magnet_treshhold[2]);
					ledblinker(2);
					magnettresholdflag =1;

				}
				else if(diffmagnet > magnet_treshhold[1]){
					printu("%d",magnet_treshhold[1]);
					ledblinker(1);
					magnettresholdflag =1;
				}
				else if (diffmagnet > magnet_treshhold[0]){
					printu("%d",magnet_treshhold[0]);
					ledblinker(0);
				}
				

				if (counter == 1){
				//BSP_LED_Toggle(LED2);

					float humidity = BSP_HSENSOR_ReadHumidity();
					temp_data = BSP_TSENSOR_ReadTemp();			// read temperature sensor
					pressure = BSP_PSENSOR_ReadPressure();

					//printu("button press count: %d\n",buttonpress);
					if (pressure > pressure_treshhold || temp_data > temp_treshhold /*|| abs((XYZ[0] + XYZ[1] + XYZ[2]) - inital_magnet) > magnet_treshhold */|| humidity > humidity_treshhold){
						printu("=============================\n");
						if (pressure > pressure_treshhold){
							printu("Pressure threshold exceeded!\n");
							printu("Pressure: %f\n",pressure);

						}
						
						if (temp_data > temp_treshhold){
							printu("Temperature threshold exceeded!\n");
							printu("Temperature: %f\n",temp_data);
						}
						// if (abs((XYZ[0] + XYZ[1] + XYZ[2]) - inital_magnet) > magnet_treshhold){
						// 	printu("Magnetometer threshold exceeded!\n");
						// 	printu("magnet X : %d;  Y : %d;  Z : %d; \n", XYZ[0], XYZ[1], XYZ[2]);
						// 	printu("diff magnet: %d\n", abs((XYZ[0] + XYZ[1] + XYZ[2]) - inital_magnet));
						// }
						if (humidity > humidity_treshhold){
							printu("Humidity threshold exceeded!\n");
							printu("Humidity: %f\n",humidity);

						}
						printu("=============================\n");
					}
					if (magnettresholdflag ==1){
					
						magnettresholdcounter +=1;}
									
					}
					

					counter = 0;
					
			  }
			// 	if (counter ==2){
			// 	   timeelapsed = HAL_GetTick() - tickstart;
			// 	  printu("time used to run in ms:%d\n",timeelapsed);
			// 	  counter = 0;
			//   }
			  //if button once, player mode toggle between 1 and 0
			  //if button twice within 1 second, change to other game
			  //make my own print function instead of the complicated uart thing
		  }
		  	if (gamechange ==2){
				printu("Enhancements Activated!\n");
		  	}
		}

		  


	

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

