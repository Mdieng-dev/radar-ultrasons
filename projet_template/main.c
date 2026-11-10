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
 *      T?che de test (vide) : ? compl?ter par les ?tudiants
 *---------------------------------------------------------------------------*/
void Init_GPIOF(void);
void Init_GPIOI(void);
void Init_TIM10(void);
void Init_TIM11(void);
void Init_ADC3(void);
void Init_DAC(void); 

int angle = 0;
double capteur_d_cm;
 
osThreadId_t tid_Servo, tid_Capteur;

void TIM11_IRQHandler(void){
	TIM11->SR &= ~(1 << 0);
	osThreadFlagsSet((osThreadId_t)tid_Capteur, (1<<0));
}

void EXTI0_IRQHandler(void){
	EXTI->PR = (1 << 0);
	osThreadFlagsSet((osThreadId_t)tid_Capteur, (1<<1));
}

void servo_set_angle(int deg){
	double pulse_width;
	pulse_width = 500+((2000*deg)/180);// 2 ms - 180? & 0.5 ms - 0?
	TIM10->CCR1 = (int)pulse_width;
}
 
double distance_cm(void){
	uint32_t t1, t2, flag;
	double v = 0.0343, dt;

	// Envoi de l'impulsion de 10 ?s sur TRIG (PF7)
	GPIOF->ODR &= ~(1 << 7); //TRIG ? 0
	TIM11->PSC = 1;
	TIM11->ARR = 199;
	TIM11->CR1 |= (1 << 0); //D?marrage du timer
	osThreadFlagsWait((1<<0), osFlagsWaitAll, osWaitForever); //Attente de 2?s
	
	GPIOF->ODR |= (1 << 7); //TRIG ? 1
	TIM11->PSC = 9;
	TIM11->ARR = 199;
	osThreadFlagsWait((1<<0), osFlagsWaitAll, osWaitForever); //Attente de 10?s
	GPIOF->ODR &= ~(1 << 7); //TRIG ? 0

	TIM11->PSC = 199;
	TIM11->ARR = 0xFFFF;
	
	//Attendre que ECHO (PF8) passe ? l'?tat haut
	t1 = TIM11->CNT;
	flag = osThreadFlagsWait((1<<1), osFlagsWaitAll, 30); //Attente de l'echo sur PI0 via EXTI0 (FM) avec timeout de 30ms
	if(!(flag = osOK)) return -1.0f;
	
	t2 = TIM11->CNT;
	dt = t2 - t1;
	
	return (dt*v)/2;
}

static void Thread_Servo (void *argument)
{
	// Initialisations propres ? la t?che (si besoin) ici
	int i = 0, sens;

	while (1)
	{
		// Votre boucle de test ici (mesure de distance, balayage du servo, etc.)
		servo_set_angle(i);
		if(i>=180) sens = -1;
		if(i<=0) sens = 1;
		i += sens;
		angle = i;

		osDelay(10);   // p?riode de la boucle : 100 ms
	}
}

static void Thread_Capteur(void *argument)
{
	while(1){
		capteur_d_cm = distance_cm();
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
  configuration_overclock(); //pour pouvoir utiliser l'?cran LCD

	/**********************************
	D?but Initialisation p?riph?rique
	**********************************/
	Init_GPIOF();
	Init_GPIOI();
	Init_ADC3();
	Init_DAC();
	Init_TIM10();
	Init_TIM11();
	/**********************************
	Fin Initialisation p?riph?rique
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

  /* Infinite loop  (ne s'ex?cute pas avec le RTOS!!!)*/
  while (1);
}

void Init_GPIOI(void) {
    RCC->AHB1ENR |= (1 << 8);  // Activation de l'horloge du GPIO I
    RCC->APB2ENR |= (1 << 14); // Activation de l'horloge de SYSCFG

    // Configuration PI0 (Entr?e ECHO)
    GPIOI->MODER &= ~(3 << 0); // PIN PI0 en entr?e
    //GPIOI->PUPDR &= ~(3 << 0); // Force 00 (Flottant / No Pull)

    // Configuration PI2 (Sortie LED)
    GPIOI->MODER &= ~(3 << 4); // Nettoie les bits 5 et 4
    GPIOI->MODER |= (1 << 4);  // PIN PI2 en soritie (LED)

    // Configuration EXTI0 sur PI0
    SYSCFG->EXTICR[0] &= ~(0xF << 0); // Nettoie la s?lection pour EXTI0
    SYSCFG->EXTICR[0] |= (8 << 0);    // Relie PI0 ? EXTI0

    EXTI->RTSR |= (1 << 0);  // D?clenchement sur front montant
    EXTI->FTSR &= ~(1 << 0); // Ignore le front descendant
    EXTI->IMR |= (1 << 0);   // D?sactive le masque (active l'interruption)
    
    // Configuration NVIC
    NVIC_SetPriority(EXTI0_IRQn, 1); 
    NVIC_EnableIRQ(EXTI0_IRQn);
}

void Init_GPIOF(void){
	RCC->AHB1ENR |= (1<<5); // Activation GPIO F
	GPIOF->MODER |= (1<<14); // PIN PF7 en sortie (TRIG)
}

void Init_TIM10(void){
	RCC->APB2ENR |= (1<<17); // Activer TIM10
	
	GPIOF->MODER |= (2<<12); //PIN PF6 en alternate (PWM)
	GPIOF->AFR[0] |= (3<<24);
	
	TIM10->CCMR1 |= (6<<4);
	TIM10->CCER |= (1<<0);
	
	TIM10->PSC = 199;
	TIM10->ARR = 19999;
	
	TIM10->CR1 |= (1<<0); //D?marrage du timer
}

void Init_TIM11(void){
	  RCC->APB1ENR |= (1 << 0); // Activer TIM11

  TIM11->PSC = 199;          //200MHz/200 = 1 MHz (1 tick = 1 ?s)
  TIM11->ARR = 0xFFFFFFFF;
	
	NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 1);
  NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
}

void Init_ADC3(void){
	RCC->APB2ENR |= (1<<10);
	RCC->AHB1ENR |= (1<<5);
	
	GPIOF->MODER |= (3<<20);
	
	ADC3->SQR3 |= (8<<0);
	ADC3->CR2 |= (1<<0);
}

void Init_DAC(void){
	RCC->APB1ENR |= (1<<29); //Activer DAC
	RCC->AHB1ENR |= (1<<0);
}