#include "stm32f7xx.h"                  // Device header

extern void configuration_overclock(void);
extern int Init_GUIThread (void);
extern void Init_Ethernet (void);

#ifdef _RTE_
#include "RTE_Components.h"             // Component selection
#endif
#ifdef RTE_CMSIS_RTOS2                  // when RTE component CMSIS RTOS2 is used
#include "cmsis_os2.h"                  // ::CMSIS:RTOS2
#endif

/*----------------------------------------------------------------------------
 *      Tâche de test (vide) : à compléter par les étudiants
 *---------------------------------------------------------------------------*/
 
int angle = 0;
 
osThreadId_t tid_Test;

 void servo_set_angle(int deg){
	double pulse_width;
	pulse_width = 500+((2000*deg)/180);// 2 ms - 180° & 0.5 ms - 0°
	TIM10->CCR1 = (int)pulse_width;
}
 
void distance_cm(void){
}
 
static void Thread_Test (void *argument)
{
  (void)argument;

  // Initialisations propres à la tâche (si besoin) ici
	int i = 0, sens;
	
  while (1)
  {
    // Votre boucle de test ici (mesure de distance, balayage du servo, etc.)
		servo_set_angle(i);
		if(i>180) sens = -1;
		else sens = 1;
		i += sens;
		angle = i;

    osDelay(100);   // période de la boucle : 100 ms
  }
}

static int Init_TestThread (void)
{
  tid_Test = osThreadNew(Thread_Test, NULL, NULL);
  if (tid_Test == NULL) {
    return -1;
  }
  return 0;
}


int main(void)
{
  configuration_overclock(); //pour pouvoir utiliser l'écran LCD

	/**********************************
	Début Initialisation périphérique
	**********************************/
	
	//INIT GPIO
	RCC->AHB1ENR |= (1<<8); //Activation GPIO I
	RCC->AHB1ENR |= (1<<5); //Activation GPIO F
	
	GPIOI->MODER |= (1<<4); //Broche PI2 en sortie
	GPIOF->MODER |= (1<<14); //Broche PF7 en sortie
	
	//INIT DAC
	RCC->APB1ENR |= (1<<29); //Activer DAC
	RCC->AHB1ENR |= (1<<0);
	
	GPIOA->MODER |= (3<<8); //Broche PA4 en analog
	DAC->CR |= (1<<0); //Lancer DAC
	
	//INIT TIM10 (PWM)
	RCC->APB2ENR |= (1<<17); //Activer TIM10
	
	GPIOF->MODER |= (2<<12); //Broche PF6 en alternate
	GPIOF->AFR[0] |= (3<<24);
	
	TIM10->CCMR1 |= (6<<4);
	TIM10->CCER |= (1<<0);
	
	TIM10->PSC = 199;
	TIM10->ARR = 19999;
	
	TIM10->CR1 |= (1<<0); //Démarrage du timer
	
	//INIT ADC3
	RCC->APB2ENR |= (1<<10);
	RCC->AHB1ENR |= (1<<5);
	
	GPIOF->MODER |= (3<<20);
	
	ADC3->SQR3 |= (8<<0);
	ADC3->CR2 |= (1<<0);

	/**********************************
	Fin Initialisation périphérique
	**********************************/

	/**********************************
	Utilisation RTOS (Keil RTX5)
	**********************************/
	#ifdef RTE_CMSIS_RTOS2
	  /* Initialize CMSIS-RTOS2 */
	  osKernelInitialize ();

	  /* Create thread functions that start executing */
		Init_Ethernet();
		Init_GUIThread();
		Init_TestThread();   // tâche de test (boucle avec osDelay)

	  /* Start thread execution */
	  osKernelStart();
	#endif

  /* Infinite loop  (ne s'exécute pas avec le RTOS!!!)*/
  while (1)
  {
  }
}