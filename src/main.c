/*
**
**                           Main.c
**
**
**********************************************************************/
/*
   Last committed:     $Revision: 00 $
   Last changed by:    $Author: $
   Last changed date:  $Date:  $
   ID:                 $Id:  $

**********************************************************************/
#include "stm32f10x.h"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
//#include "queue.h"
#include "task.h"
//#include "semphr.h"

#include "gpiodef.h"

//TIM1 setup
#define PWM_ARR1 0x0800
#define PWM_PSC1 0x14A
#define DEF_PWM_DUTY1 0x8000



//TIM2 setup: default 1MHz, duty 50%
#define PWM_ARR2 0x48
#define PWM_PSC2 0x01
#define DEF_PWM_DUTY2 0x24


uint32_t RPM = PWM_ARR1;
uint16_t Duty2 = DEF_PWM_DUTY2;
uint16_t ButtonPressed = 0;
uint8_t CurrentStep = 0;
//uint16_t step[8] = {0x01, 0x03 ,0x02, 0x06,0x04, 0x0c,0x08,0x09};
uint16_t step[8] = {0x01, 0x11 ,0x10, 0x0110,0x0100, 0x1100,0x1000,0x1001};
void ButtonsRead ( void *pvParameters )
{
    uint16_t ButtonPressed = 0;
    uint8_t mode = 0;

	while( 1 )
    {
        if (!(GPIOA->IDR & GPIO_Pin_4) && !(GPIOA->IDR & GPIO_Pin_5) && (ButtonPressed & GPIO_Pin_4) && (ButtonPressed & GPIO_Pin_5))
        {
            mode++;
            mode &= 0x01;
        }
        else
        {
            switch (mode)
            {
                case 0:
                    if (!(GPIOA->IDR & GPIO_Pin_4) && (ButtonPressed & GPIO_Pin_4)) RPM +=100;
                    if (!(GPIOA->IDR & GPIO_Pin_5) && (ButtonPressed & GPIO_Pin_5)) RPM -=100;
                    ButtonPressed = GPIOA->IDR & (GPIO_Pin_4 | GPIO_Pin_5);
                    if (RPM < 32) RPM = 32;
                    else if (RPM > 65535) RPM = 65535;
                    TIM1->ARR       = RPM;
                    TIM1->CCR1      = RPM/2;

                    break;
                case 1:
                    if (!(GPIOA->IDR & GPIO_Pin_4) && (ButtonPressed & GPIO_Pin_4)) Duty2 +=1;
                    if (!(GPIOA->IDR & GPIO_Pin_5) && (ButtonPressed & GPIO_Pin_5)) Duty2 -=1;
                    if (Duty2 < 2) Duty2 = 2;
                    else if (Duty2 > PWM_ARR2) Duty2 = PWM_ARR2;
                    TIM2->CCR1      = Duty2;
                    TIM2->CCR2      = Duty2;
                    TIM2->CCR3      = Duty2;
                    TIM2->CCR4      = Duty2;
                    break;
                default:
                    break;
            }
        }
        vTaskDelay( 100 / portTICK_RATE_MS );
    }

}
//------------------------------------------------------------------------------------------------
void Blink2( void *pvParameters )
{
	while( 1 )
    {
        GPIOA->ODR = (step[CurrentStep]<<4 | 0x0C);
        CurrentStep +=2;
        if (CurrentStep == 8) CurrentStep = 0;
		vTaskDelay( RPM / portTICK_RATE_MS );
	}
}
//------------------------------------------------------------------------------------------------
void TIM1_UP_IRQHandler (void)
{
	//	TIM2->SR =0;        // очищаем флаг прерывани€

	if (TIM1->SR & TIM_SR_UIF)
	{
        TIM1->SR &= ~TIM_SR_UIF;        // очищаем флаг прерывани€

        TIM2->CCER = 0; //Disable
        CurrentStep +=1;
        if (CurrentStep == 8) CurrentStep = 0;

        TIM2->CCER = step[CurrentStep]; //Enable
	}
}
//------------------------------------------------------------------------------------------------
void TIM1_init ()
{
    RCC->APB2ENR    |= RCC_APB2ENR_TIM1EN;	//TIM2 RCC enable
//TIM2->CR1       = 0x00000000;       //сброс настроек таймера (можно не делать)
	TIM1->ARR       = PWM_ARR1;           // макс. значение до которого считаем
	TIM1->CR1       |= TIM_CR1_ARPE;    //Ѕуферизаци€ регистра ARR (макс. значение до которого считаем)
	TIM1->PSC       = PWM_PSC1;           // пределитель
	TIM1->CCR1      = DEF_PWM_DUTY1;	//Counter value

	//TIM1->CCMR2 =  = 0x00000000;

	//”станавливаем режим Ў»ћ
	TIM1->CCMR1     |= TIM_CCMR1_OC1M_1
					| TIM_CCMR1_OC1M_2; //(TIM_CCMR1_OC1PE - загрузка значений только по обновлению
										//TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 - режим Ў»ћ 1 <110: PWM mode 1 - In upcounting, channel 1 is active as long as TIMx_CNT<TIMx_CCR1
										//else inactive. In downcounting, channel 1 is inactive (OC1REF=С0Т) as long as
										//TIMx_CNT>TIMx_CCR1 else active (OC1REF=Т1Т).>  )


	TIM1->DIER 		|= TIM_DIER_UIE; //update interrupts enable
	//TIM1->CCER      |= TIM_CCER_CC3E; //разрешаем выход канала 2

	NVIC_EnableIRQ(TIM1_UP_IRQn);              // прерывание по таймеру2

	TIM1->CR1       |= TIM_CR1_CEN;      //разрешаем работу таймера
}
//------------------------------------------------------------------------------------------------
void TIM2_init(void)
{
        RCC->APB1ENR    |= RCC_APB1ENR_TIM2EN;                    //включаю тактирование модул€ таймера TIM1
        //RCC->APB2ENR    |= RCC_APB2ENR_TIM2EN;
        RCC->APB2ENR	|= RCC_APB2ENR_IOPAEN;                    //включаю тактирование порта (если не включали ранее)
		GPIOA->CRL		&= ~ (0xFFFF);
        GPIOA->CRL      |= (GPIO_CRL_MODE0_1 | GPIO_CRL_CNF0_1);  //пин ставим на выход, альтернативна€ функци€
        GPIOA->CRL      |= (GPIO_CRL_MODE1_1 | GPIO_CRL_CNF1_1);
		GPIOA->CRL      |= (GPIO_CRL_MODE2_1 | GPIO_CRL_CNF2_1);
		GPIOA->CRL      |= (GPIO_CRL_MODE3_1 | GPIO_CRL_CNF3_1);

        TIM2->CR1       = 0x00000000;       //сброс настроек таймера (можно не делать)
        TIM2->ARR       = PWM_ARR2;           // макс. значение до которого считаем
        TIM2->CR1       |= TIM_CR1_ARPE;    //Ѕуферизаци€ регистра ARR (макс. значение до которого считаем)
        //TIM2->PSC       = 0x16;           // пределитель
		TIM2->PSC       = PWM_PSC2;           // пределитель
        //”станавливаем режим Ў»ћ
		TIM2->CCMR1     |= TIM_CCMR1_OC1PE
                        | TIM_CCMR1_OC1M_1
                        | TIM_CCMR1_OC1M_2; //(TIM_CCMR1_OC1PE - загрузка значений только по обновлению
                                            //TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 - режим Ў»ћ 1 <110: PWM mode 1 - In upcounting, channel 1 is active as long as TIMx_CNT<TIMx_CCR1
                                            //else inactive. In downcounting, channel 1 is inactive (OC1REF=С0Т) as long as
                                            //TIMx_CNT>TIMx_CCR1 else active (OC1REF=Т1Т).>  )
		TIM2->CCMR1     |= TIM_CCMR1_OC2PE
						| TIM_CCMR1_OC2M_1
						| TIM_CCMR1_OC2M_2;

		TIM2->CCMR2     |= TIM_CCMR2_OC3PE
						| TIM_CCMR2_OC3M_1
						| TIM_CCMR2_OC3M_2;
		TIM2->CCMR2     |= TIM_CCMR2_OC4PE
						| TIM_CCMR2_OC4M_1
						| TIM_CCMR2_OC4M_2;
		//«адаем значение скважности Ў»ћ 1ms
		/*
		TIM2->CCR1      = MIN_PWM_VAL + MAX_THROTTLE_VAL;
		TIM2->CCR2      = MIN_PWM_VAL + MAX_THROTTLE_VAL;
		TIM2->CCR3      = MIN_PWM_VAL + MAX_THROTTLE_VAL;
		TIM2->CCR4      = MIN_PWM_VAL + MAX_THROTTLE_VAL;
		*/
		TIM2->CCR1      = DEF_PWM_DUTY2;
		TIM2->CCR2      = DEF_PWM_DUTY2;
		TIM2->CCR3      = DEF_PWM_DUTY2;
		TIM2->CCR4      = DEF_PWM_DUTY2;


        //TIM2->CCER      |= TIM_CCER_CC1E;   //разрешаем выход канала 1
		//TIM2->CCER      |= TIM_CCER_CC2E;   //разрешаем выход канала 2
		//TIM2->CCER      |= TIM_CCER_CC3E;   //разрешаем выход канала 3
		//TIM2->CCER      |= TIM_CCER_CC4E;   //разрешаем выход канала 4

        TIM2->BDTR      |= TIM_BDTR_MOE;    //включаем выходы (б...! еще раз! Ќахрена?)
        TIM2->DIER      = 0x00000000;       //отрубаем прерывани€ таймера (если нужны, можно включить)
        TIM2->EGR       |= TIM_EGR_UG;      //сброс счетчика в 0
        TIM2->CR1       |= TIM_CR1_CEN;      //разрешаем работу таймера
}
//------------------------------------------------------------------------------------------------
int main(void)
{

    RCC->APB2ENR	|= RCC_APB2ENR_IOPAEN;


	//GPIOA->CRL      |= (GPIO_CRL_MODE4_1 | GPIO_CRL_MODE5_1 | GPIO_CRL_MODE6_1 | GPIO_CRL_MODE7_1);//Output mode, max speed 2 MHz.
    //GPIOA->CRL      &= ~(GPIO_CRL_CNF4 | GPIO_CRL_CNF5 | GPIO_CRL_CNF6 | GPIO_CRL_CNF7 );

    GPIOA->CRL      &=~ (GPIO_CRL_CNF4 | GPIO_CRL_CNF5 );
    GPIOA->CRL      |= (GPIO_CRL_CNF4_1 | GPIO_CRL_CNF5_1 );
    GPIOA->ODR      = 0x30; //Enable pull-up

    TIM1_init ();
    TIM2_init ();

	//xTaskCreate( Blink2,"Blink2", 64, NULL, tskIDLE_PRIORITY, NULL );
    xTaskCreate(ButtonsRead,"ButtonsRead", 64, NULL, tskIDLE_PRIORITY, NULL );
    //xTaskCreate(Sonar,"Sonar", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );



	/* Start the scheduler. */
	vTaskStartScheduler();
    return 0;
}
