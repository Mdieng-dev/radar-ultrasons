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
 
int angle = 0, tim11 = 0;
 
osThreadId_t tid_Servo, tid_Capteur;

void TIM11_IRQHandler(void){
    TIM11->SR &= ~(1 << 0);
    osThreadFlagsSet((osThreadId_t)tid_Capteur, (1<<0));
}

void servo_set_angle(int deg){
	double pulse_width;
	pulse_width = 500+((2000*deg)/180);// 2 ms - 180° & 0.5 ms - 0°
	TIM10->CCR1 = (int)pulse_width;
}
 
double distance_cm(void){
    uint32_t timeout, start_time, current_time, flag, d;

    // Envoi de l'impulsion de 10 µs sur TRIG (PF7)
    TIM11->PSC = 9;
    TIM11->ARR = 199;
    GPIOF->ODR &= ~(1 << 7); //TRIG à 0
    osThreadFlagsWait((1<<0), osFlagsWaitAll, osWaitForever);

    TIM11->PSC = 9;
    TIM11->ARR = 199;
    GPIOF->ODR |= (1 << 7);  //TRIG à 1
    osThreadFlagsWait((1<<0), osFlagsWaitAll, osWaitForever);

    GPIOF->ODR &= ~(1 << 7); //TRIG à 0

    TIM11->PSC = 199;
    TIM11->ARR = 0xFFFFFFFF;
    
    //Attendre que ECHO (PF8) passe à l'état haut
    timeout = TIM11->CNT;
    flag = osThreadFlagsWait((1<<1), osFlagsWaitAll, 30);
    if(!(flag = osOK)) return -1.0f;
		
    start_time = TIM11->CNT;
		flag = osThreadFlagsWait((1<<1), osFlagsWaitAll, 30);
		if(!(flag = osOK)) return -1.0f;

    d = current_time - start_time;
		
		return d;
}

double distance2_cm(void) {
    uint32_t start_time, current_time;
    
    // Envoi de l'impulsion de 10 µs sur TRIG (PF7)
    TIM11->PSC = 9;
    TIM11->ARR = 199;
    GPIOF->ODR &= ~(1 << 7); //TRIG à 0
    osThreadFlagsWait((1<<0), osFlagsWaitAll, osWaitForever);

    TIM11->PSC = 9;
    TIM11->ARR = 199;
    GPIOF->ODR |= (1 << 7);  //TRIG à 1
    osThreadFlagsWait((1<<0), osFlagsWaitAll, osWaitForever);

    GPIOF->ODR &= ~(1 << 7); //TRIG à 0

    // 2. Attendre que ECHO (PI4) passe à l'état haut (avec timeout court)
    uint32_t timeout = TIM2->CNT;
    while ((GPIOI->IDR & (1 << 4)) == 0) {
        if ((TIM2->CNT - timeout) > 30000) return -1.0f; // Timeout 30ms
    }

    // 3. Capturer le temps de début de l'écho
    start_time = TIM2->CNT;

    // 4. Attendre que ECHO repasse à l'état bas en surveillant le temps écoulé
    while ((GPIOI->IDR & (1 << 4)) != 0) {
        current_time = TIM2->CNT;
        if ((current_time - start_time) > 30000) return -1.0f; // Timeout de sécurité (~5m max)
    }

    // 5. Calcul de la durée et conversion en cm
    uint32_t duration = current_time - start_time;
    return (float)duration / 58.0f;
}
 
static void Thread_Servo (void *argument)
{
  (void)argument;

  // Initialisations propres à la tâche (si besoin) ici
	int i = 0, sens;
	
  while (1)
  {
    // Votre boucle de test ici (mesure de distance, balayage du servo, etc.)
		servo_set_angle(i);
		if(i>=180) sens = -1;
		if(i<=0) sens = 1;
		i += sens;
		angle = i;

    osDelay(10);   // période de la boucle : 100 ms
  }
}

static void Thread_Capteur(void *argument)
{
    while(1){
        distance_cm();
    }
}

static int Init_Thread (void)
{
  tid_Capteur = osThreadNew(Thread_Capteur, NULL, NULL);
  tid_Servo = osThreadNew(Thread_Servo, NULL, NULL);
  if ((tid_Capteur && tid_Servo)  == NULL) {
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
  GPIOF->MODER |= (1<<16); //Broche PF8 en sortie

  NVIC_EnableIRQ(EXTI);
	
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

    //INIT TIM11 (TEMPO+IMPULSION)
    RCC->APB1ENR |= (1 << 0); // Activer TIM11

    TIM11->PSC = 199;          //200MHz/200 = 1 MHz (1 tick = 1 µs)
    TIM11->ARR = 0xFFFFFFFF;

    NVIC_EnableIRQ(TIM11_IRQn);
    NVIC_SetPriority(TIM11_IRQn, 1);

    TIM11->CR1 |= (1 << 0);    //Démarrage du timer
	
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
		Init_Thread();

	  /* Start thread execution */
	  osKernelStart();
	#endif

  /* Infinite loop  (ne s'exécute pas avec le RTOS!!!)*/
  while (1)
  {
  }
}