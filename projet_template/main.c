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

void Init_GPIOB(void);
void Init_GPIOF(void);
void Init_GPIOG(void);
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

void EXTI9_5_IRQHandler(void){
	if (EXTI->PR & (1 << 6)) { // Vérification que l'interruption vient bien de PG6
		EXTI->PR = (1 << 6); // Acquittement de l'interruption 6
		osThreadFlagsSet((osThreadId_t)tid_Capteur, (1<<1));
	}
}

void EXTI15_10_IRQHandler(void){
	if (EXTI->PR & (1 << 15)) { // Vérification que l'interruption vient bien de PB15
		EXTI->PR = (1 << 15); // Acquittement de l'interruption 15
		osThreadFlagsSet((osThreadId_t)tid_Capteur, (1<<1));
	}
}

void servo_set_angle(int deg){
	double pulse_width;
	pulse_width = 500+((2000*deg)/180);// 2 ms = 180° & 0.5 ms = 0°
	TIM10->CCR1 = (int)pulse_width;
}
 
double distance_cm(void){
	uint32_t t1 = 0, t2 = 0, flag;
	double dt;
	
	// TRIG -> PB14
	// ECHO -> PG6 (temp PB15)

	// 1. Envoi de l'impulsion de 10 us sur TRIG (PB14)
	GPIOB->ODR &= ~(1<<14); // TRIG à 0
	
	GPIOB->ODR |= (1<<14); // TRIG à 1
	TIM11->PSC = 9;
	TIM11->ARR = 199;
	TIM11->CNT = 0; // Remise à zéro indispensable pour temporiser 10us exactes
	TIM11->CR1 |= (1<<0); // Démarrage du timer
	
	osThreadFlagsWait((1<<0), osFlagsWaitAll, osWaitForever); // Attente de 10us
	GPIOB->ODR &= ~(1<<14); // TRIG à 0
	TIM11->CR1 &= ~(1<<0); // Arrêt du timer

	// 2. Mesure du temps de retour de l'Echo
	TIM11->PSC = 199; // 1 microseconde par incrément
	TIM11->ARR = 0xFFFF;
	TIM11->CNT = 0; // Remise à zéro pour que les chronos (t1, t2) soient propres
	TIM11->CR1 |= (1<<0); // Démarrage du timer en mode compteur libre
	
	// Vidage préventif des flags pour purger une éventuelle interruption fantôme
	osThreadFlagsClear((1<<1));

	// Attente du front montant de l'echo sur PG6 via EXTI6 avec timeout de 30ms
	flag = osThreadFlagsWait((1<<1), osFlagsWaitAll, 30); 
	if(flag & osFlagsError) return -1.0f; // Vérifie s'il y a eu un timeout
	
	if((GPIOG->IDR & (1<<6)) == (1<<6)) t1 = TIM11->CNT; // Lecture sur IDR !
	
	// Attente du front déscendant de l'echo sur PG6 via EXTI6 avec timeout de 30ms
	flag = osThreadFlagsWait((1<<1), osFlagsWaitAll, 30); 
	if(flag & osFlagsError) return -1.0f;
	
	if((GPIOG->IDR & (1<<6)) == 0) t2 = TIM11->CNT; // Lecture sur IDR !
	
	TIM11->CR1 &= ~(1<<0); // Arrêt du timer
	
	dt = (double)(t2 - t1); // Durée de l'impulsion en us
	
	return dt / 58.0; // Conversion us -> cm selon la datasheet
}

static void Thread_Servo (void *argument)
{
	int i = 0, sens = 1; // Correction pour empêcher des plantages mémoire

	while (1)
	{
		//servo_set_angle(i);
		if(i>=180) sens = -1;
		if(i<=0) sens = 1;
		i += sens;
		angle = i;

		osDelay(10);   // période de la boucle : 10 ms
	}
}

static void Thread_Capteur(void *argument)
{
	while(1){
		capteur_d_cm = distance_cm();
		osDelay(60); // Pause obligatoire minimale de 60ms entre 2 requêtes
	}
}

static int Init_Thread (void)
{
  tid_Capteur = osThreadNew(Thread_Capteur, NULL, NULL);
  tid_Servo = osThreadNew(Thread_Servo, NULL, NULL);
  if ((tid_Capteur == NULL) || (tid_Servo == NULL)) { 
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
	Init_GPIOB(); // AJOUT
	Init_GPIOF();
	Init_GPIOG(); // AJOUT
	Init_GPIOI();
	Init_ADC3();
	Init_DAC();
	Init_TIM10();
	Init_TIM11();
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
  while (1);
}

void Init_GPIOB(void){
	RCC->AHB1ENR |= (1<<1); // Activation de l'horloge du GPIO B
	
	GPIOB->MODER |= (1<<28); // PIN PB14 en sortie (TRIG)
	GPIOB->MODER &= ~(3<<30); // PIN PB15 en entrée (temp ECHO)
	
	//interruption pour ECHO sur PB15 (uniquemetn pour tests, sinon PG6)
	EXTI->RTSR |= (1 << 15);  // Déclenchement sur front montant (EXTI15)
	EXTI->FTSR |= (1 << 15);  // Déclenchement sur front descendant (EXTI5)
	EXTI->IMR |= (1 << 15);   // Active l'interruption pour EXTI15
	
	NVIC_SetPriority(EXTI15_10_IRQn, 1); 
	NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Init_GPIOF(void){
	RCC->AHB1ENR |= (1<<5); // Activation de l'horloge du GPIO F
	GPIOF->MODER |= (1<<14); // PIN PF7 en sortie
}

void Init_GPIOG(void){
	RCC->AHB1ENR |= (1<<6); // Activation de l'horloge du GPIO G
	RCC->APB2ENR |= (1 << 14); // Activation de l'horloge de SYSCFG
	
	GPIOG->MODER &= ~(3<<12); // PIN PG6 en entrée (ECHO) - Masque 00 sur bits 13:12
	
	// Configuration EXTI sur PG6
	SYSCFG->EXTICR[1] &= ~(0xF<<8); // L'EXTI6 est géré par EXTICR[1], décalage de 8 bits
	SYSCFG->EXTICR[1] |= (6<<8);    // Relie le port G (valeur 6) à l'EXTI6

	EXTI->RTSR |= (1 << 6);  // Déclenchement sur front montant (EXTI6)
	EXTI->FTSR |= (1 << 6);  // Déclenchement sur front descendant (EXTI6)
	EXTI->IMR |= (1 << 6);   // Active l'interruption pour EXTI6
	
	// Configuration NVIC
	NVIC_SetPriority(EXTI9_5_IRQn, 1); 
	NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void Init_GPIOI(void) {
    RCC->AHB1ENR |= (1 << 8);  // Activation de l'horloge du GPIO I

    // Configuration PI0 
    GPIOI->MODER &= ~(3 << 0); // PIN PI0 en entrée

    // Configuration PI2 (Sortie LED)
    GPIOI->MODER &= ~(3 << 4); // Nettoie les bits 5 et 4
    GPIOI->MODER |= (1 << 4);  // PIN PI2 en sortie (LED)
}

void Init_TIM10(void){
	RCC->APB2ENR |= (1<<17); // Activer TIM10
	
	GPIOF->MODER |= (2<<12); //PIN PF6 en alternate (PWM)
	GPIOF->AFR[0] |= (3<<24);
	
	TIM10->CCMR1 |= (6<<4);
	TIM10->CCER |= (1<<0);
	
	TIM10->PSC = 199;
	TIM10->ARR = 19999;
	
	TIM10->CR1 |= (1<<0); //Démarrage du timer
}

void Init_TIM11(void){
  RCC->APB1ENR |= (1 << 0); // Activer TIM11

  TIM11->PSC = 199;          // 200MHz/200 = 1 MHz (1 tick = 1 us)
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