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

// Commenter la ligne suivante pour utiliser PG6 au lieu de PB15
#define USE_TEST_PB15

#define FLAG_ECHO  (1<<1)

void Init_GPIOB(void);
void Init_GPIOF(void);
void Init_GPIOG(void);
void Init_GPIOI(void);
void Init_TIM10(void);
void Init_TIM11(void);
void Init_ADC3(void);
void Init_DAC(void); 

int angle = 0;
float capteur_d_cm = 0.0;
 
osThreadId_t tid_Servo, tid_Capteur;

#ifdef USE_TEST_PB15
	void EXTI15_10_IRQHandler(void){
		if (EXTI->PR & (1 << 15)) {
			EXTI->PR = (1 << 15); // Acquittement PB15
			if (tid_Capteur != NULL) {
				osThreadFlagsSet(tid_Capteur, FLAG_ECHO);
			}
		}
	}
#else
	void EXTI9_5_IRQHandler(void){
		if (EXTI->PR & (1 << 6)) {
			EXTI->PR = (1 << 6); // Acquittement PG6
			if (tid_Capteur != NULL) {
				osThreadFlagsSet(tid_Capteur, FLAG_ECHO);
			}
		}
	}
#endif


void servo_set_angle(int deg){
	float pulse_width;
	pulse_width = 500 + ((2000 * deg) / 180); // 2 ms = 180° & 0.5 ms = 0°
	TIM10->CCR1 = (int)pulse_width;
}
 
float distance_cm(void){
	uint32_t t1 = 0, t2 = 0, flag;
	float dt;
	
	// Nettoyage d'éventuels flags résiduels avant de lancer une mesure
	osThreadFlagsClear(FLAG_ECHO);

	// 1. Envoi de l'impulsion de 10 us sur TRIG (PB14)
	GPIOB->ODR &= ~(1<<14); // TRIG à 0
	TIM11->CNT = 0;
	TIM11->CR1 |= (1U << 0); // Démarrage du timer
	while (TIM11->CNT < 2);

	GPIOB->ODR |= (1<<14); // TRIG à 1
	TIM11->CNT = 0;
	while (TIM11->CNT < 10);  // Attente précise de 10 us
	GPIOB->ODR &= ~(1<<14); // TRIG à 0

	// 2. Mesure du temps de retour de l'Echo
	TIM11->CNT = 0; // RAZ du chrono

	// Attente du front montant de l'Echo avec timeout de 30 ms
	flag = osThreadFlagsWait(FLAG_ECHO, osFlagsWaitAll, 30);
	if (flag & osFlagsError) {
		TIM11->CR1 &= ~(1<<0); // Arrêt propre du timer
		return 400.0;
	}
	
#ifdef USE_TEST_PB15
	if ((GPIOB->IDR & (1<<15)) != 0) t1 = TIM11->CNT;
#else
	if ((GPIOG->IDR & (1<<6)) != 0)  t1 = TIM11->CNT;
#endif
	
	// Attente du front descendant de l'Echo avec timeout de 30 ms
	flag = osThreadFlagsWait(FLAG_ECHO, osFlagsWaitAll, 30);
	if (flag & osFlagsError) {
		TIM11->CR1 &= ~(1<<0);
		return 400.0;
	}
	
#ifdef USE_TEST_PB15
	if ((GPIOB->IDR & (1<<15)) == 0) t2 = TIM11->CNT;
#else
	if ((GPIOG->IDR & (1<<6)) == 0)  t2 = TIM11->CNT;
#endif
	
	TIM11->CR1 &= ~(1<<0); // Arrêt du timer
	
	if (t2 <= t1) return -1.0f; // Sécurité en cas de front manqué
	
	dt = (float)(t2 - t1); // Durée de l'impulsion en us
	return dt / 58.0f;     // Conversion us -> cm
}

static void Thread_Servo (void *argument)
{
	int i = 0, sens = 1;

	while (1)
	{
		//servo_set_angle(i);
		if (i >= 180) sens = -1;
		if (i <= 0)   sens = 1;
		i += sens;
		angle = i;

		osDelay(10); // période de la boucle : 10 ms
	}
}

static void Thread_Capteur(void *argument)
{
	while (1) {
		capteur_d_cm = (float)distance_cm();
		osDelay(60); // Pause obligatoire minimale de 60 ms entre 2 requêtes
	}
}

static int Init_Thread (void)
{
  tid_Capteur = osThreadNew(Thread_Capteur, NULL, NULL);
  tid_Servo   = osThreadNew(Thread_Servo, NULL, NULL);
  if ((tid_Capteur == NULL) || (tid_Servo == NULL)) { 
    return -1;
  }
  return 0;
}

int main(void)
{
  configuration_overclock(); // Configuration horloge (200 MHz)

	/**********************************
	Début Initialisation périphérique
	**********************************/
	Init_GPIOB();
	Init_GPIOF();
	Init_GPIOG();
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
	  osKernelInitialize();

		Init_Ethernet();
		Init_GUIThread();
		Init_Thread();

	  osKernelStart();
	#endif

  while (1);
}

void Init_GPIOB(void){
	RCC->AHB1ENR |= (1<<1);  // Horloge GPIOB
	RCC->APB2ENR |= (1<<14); // Horloge SYSCFG
	
	GPIOB->MODER &= ~(3<<28);
	GPIOB->MODER |=  (1<<28); // PB14 en sortie (TRIG)
	GPIOB->MODER &= ~(3<<30); // PB15 en entrée (test ECHO)
	
#ifdef USE_TEST_PB15
	SYSCFG->EXTICR[3] &= ~(0xF<<12);
	SYSCFG->EXTICR[3] |=  (1<<12); // Port B sur EXTI15
	EXTI->RTSR |= (1<<15);
	EXTI->FTSR |= (1<<15);
	EXTI->IMR  |= (1<<15);
	
	NVIC_SetPriority(EXTI15_10_IRQn, 5); // Priorité compatible RTX5 (>= 5)
	NVIC_EnableIRQ(EXTI15_10_IRQn);
#endif
}

void Init_GPIOF(void){
	RCC->AHB1ENR |= (1<<5);
	GPIOF->MODER |= (1<<14); // PF7 en sortie
}

void Init_GPIOG(void){
	RCC->AHB1ENR |= (1<<6);  // Horloge GPIOG
	RCC->APB2ENR |= (1<<14); // Horloge SYSCFG
	
	GPIOG->MODER &= ~(3<<12); // PG6 en entrée (ECHO)
	
#ifndef USE_TEST_PB15
	SYSCFG->EXTICR[1] &= ~(0xF<<8);
	SYSCFG->EXTICR[1] |=  (6<<8); // Port G sur EXTI6

	EXTI->RTSR |= (1<<6);
	EXTI->FTSR |= (1<<6);
	EXTI->IMR  |= (1<<6);
	
	NVIC_SetPriority(EXTI9_5_IRQn, 5); // Priorité compatible RTX5 (>= 5)
	NVIC_EnableIRQ(EXTI9_5_IRQn);
#endif
}

void Init_GPIOI(void) {
    RCC->AHB1ENR |= (1<<8);
    GPIOI->MODER &= ~(3<<0); // PI0 en entrée
    GPIOI->MODER &= ~(3<<4);
    GPIOI->MODER |=  (1<<4); // PI2 en sortie (LED)
}

void Init_TIM10(void){
	RCC->APB2ENR |= (1<<17);
	
	GPIOF->MODER &= ~(3<<12);
	GPIOF->MODER |=  (2<<12); // PF6 en Alternate Function
	GPIOF->AFR[0] &= ~(0xF<<24);
	GPIOF->AFR[0] |=  (3<<24);
	
	TIM10->CCMR1 |= (6<<4);
	TIM10->CCER  |= (1<<0);
	
	TIM10->PSC = 199;
	TIM10->ARR = 19999;
	TIM10->EGR = (1<<0);
	
	TIM10->CR1 |= (1<<0);
}

void Init_TIM11(void){
  RCC->APB2ENR |= (1<<18); // Horloge TIM11 (APB2 bit 18)

  TIM11->PSC = 199;           // 200 MHz / 200 = 1 MHz (1 tick = 1 us)
  TIM11->ARR = 0xFFFF;        // Compteur 16 bits max (65535 us)
  TIM11->EGR = (1<<0);     // Force la prise en compte immédiate du PSC
}

void Init_ADC3(void){
	RCC->APB2ENR |= (1U << 10);
	RCC->AHB1ENR |= (1U << 5);
	
	GPIOF->MODER |= (3U << 20);
	
	ADC3->SQR3 = 8;
	ADC3->CR2 |= (1U << 0);
}

void Init_DAC(void){
	RCC->APB1ENR |= (1U << 29);
	RCC->AHB1ENR |= (1U << 0);
}