#include "stm32f7xx.h"                  // Device header

extern void configuration_overclock(void);
extern int Init_GUIThread (void);
extern void Init_Ethernet (void);
extern volatile uint8_t scan_en_cours;  // Défini dans UDP_Socket.c

#ifdef _RTE_
#include "RTE_Components.h"             // Component selection
#endif
#ifdef RTE_CMSIS_RTOS2                  // when RTE component CMSIS RTOS2 is used
#include "cmsis_os2.h"                  // ::CMSIS:RTOS2
#endif

// Utilise PB15 pour ECHO (commenter pour basculer sur PG6)
#define PB15_TEST

// Calibration PWM du servomoteur DM996 (en µs pour 50 Hz)
#define SERVO_MIN_US  500              // 0.5 ms -> 0°
#define SERVO_MAX_US  2400              // 2.0 ms -> 180°

void Init_GPIOB(void);
void Init_GPIOF(void);
void Init_GPIOG(void);
void Init_GPIOI(void);
void Init_TIM10(void);
void Init_TIM11(void);
void Init_ADC3(void);
void Init_DAC(void); 

float capteur_d_cm = 0;
 
osThreadId_t tid_Servo, tid_Capteur;

void servo_set_angle(int deg) {
	if (deg < 0)   deg = 0;
	if (deg > 180) deg = 180;

	uint32_t pulse_width = SERVO_MIN_US + (((SERVO_MAX_US - SERVO_MIN_US) * (uint32_t)deg) / 180);
	TIM10->CCR1 = pulse_width;
}

static inline uint32_t read_echo_pin(void) {
#ifdef PB15_TEST
	return (GPIOB->IDR & (1<<15));
#else
	return (GPIOG->IDR & (1<<6));
#endif
}
 
float distance_cm(void) {
	uint32_t t_start, t_end;

	// 1. Impulsion de déclenchement de 15 µs sur TRIG (PB14)
	GPIOB->ODR &= ~(1<<14);
	TIM11->CNT = 0;
	TIM11->CR1 |= (1<<0);
	while (TIM11->CNT < 5);

	GPIOB->ODR |= (1<<14);
	TIM11->CNT = 0;
	while (TIM11->CNT < 15);
	GPIOB->ODR &= ~(1<<14);

	// 2. Attente du front montant de ECHO (timeout 30 ms)
	TIM11->CNT = 0;
	while (read_echo_pin() == 0) {
		if (TIM11->CNT >= 30000) {
			TIM11->CR1 &= ~(1<<0);
			return -1.0f; // TIMEOUT : aucun front montant détecté
		}
	}
	t_start = TIM11->CNT;

	// 3. Mesure de la largeur de l'impulsion ECHO (timeout 30 ms)
	while (read_echo_pin() != 0) {
		if ((TIM11->CNT - t_start) >= 30000) {
			TIM11->CR1 &= ~(1<<0);
			return -1.0f; // TIMEOUT : écho trop long / aucun obstacle
		}
	}
	t_end = TIM11->CNT;
	TIM11->CR1 &= ~(1<<0);

	if (t_end <= t_start) return -1.0f;

	return (float)(t_end - t_start) / 58.0f;
}

static void Thread_Servo(void *argument) {
	(void)argument;
	int i = 0, sens = 1;

	while (1) {
		// servo_set_angle(i);
		if (i >= 180) sens = -1;
		if (i <= 0)   sens = 1;
		i += sens;
		
		osDelay(100);
	}
}

static void Thread_Capteur(void *argument) {
	(void)argument;
	while (1) {
		// Pause automatique pendant un balayage SCAN UDP
		if (!scan_en_cours) {
			capteur_d_cm = distance_cm();
		}
		osDelay(60);
	}
}

static int Init_Thread(void) {
	tid_Capteur = osThreadNew(Thread_Capteur, NULL, NULL);
	tid_Servo   = osThreadNew(Thread_Servo, NULL, NULL);
	if ((tid_Capteur == NULL) || (tid_Servo == NULL)) { 
		return -1;
	}
	return 0;
}

int main(void) {
	configuration_overclock();

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

void Init_GPIOB(void) {
	RCC->AHB1ENR |= (1<<1);  // Horloge GPIOB
	
	GPIOB->MODER &= ~(3<<28);
	GPIOB->MODER |=  (1<<28); // PB14 en sortie (TRIG)
	
	GPIOB->MODER &= ~(3<<30); // PB15 en entrée (ECHO)
	GPIOB->PUPDR &= ~(3<<30);
	GPIOB->PUPDR |=  (2<<30); // Pull-down sur PB15 pour éviter un niveau flottant
}

void Init_GPIOF(void) {
	RCC->AHB1ENR |= (1<<5);
	GPIOF->MODER |= (1<<14); // PF7 en sortie
}

void Init_GPIOG(void) {
	RCC->AHB1ENR |= (1<<6);  // Horloge GPIOG
	GPIOG->MODER &= ~(3<<12); // PG6 en entrée (ECHO)
}

void Init_GPIOI(void) {
	RCC->AHB1ENR |= (1<<8);
	GPIOI->MODER &= ~(3<<0); // PI0 en entrée
	GPIOI->MODER &= ~(3<<4);
	GPIOI->MODER |=  (1<<4); // PI2 en sortie (LED)
}

void Init_TIM10(void) {
	RCC->APB2ENR |= (1<<17);
	
	GPIOF->MODER &= ~(3<<12);
	GPIOF->MODER |=  (2<<12); // PF6 en Alternate Function (TIM10_CH1)
	GPIOF->AFR[0] &= ~(0xF<<24);
	GPIOF->AFR[0] |=  (3<<24);
	
	TIM10->CCMR1 |= (6<<4);
	TIM10->CCER  |= (1<<0);
	
	TIM10->PSC = 199;
	TIM10->ARR = 19999;
	TIM10->EGR = (1<<0);
	TIM10->SR  = 0;
	
	TIM10->CR1 |= (1<<0);
}

void Init_TIM11(void) {
	RCC->APB2ENR |= (1<<18);  // Horloge TIM11

	TIM11->PSC = 199;         // 200 MHz / 200 = 1 MHz (1 tick = 1 µs)
	TIM11->ARR = 0xFFFF;      // Compteur 16 bits max (65535 µs)
	TIM11->EGR = (1<<0);      // Charge le prescaler
	TIM11->SR  = 0;           // Acquitte le flag UIF
}

void Init_ADC3(void) {
	RCC->APB2ENR |= (1<<10);
	RCC->AHB1ENR |= (1<<5);
	
	GPIOF->MODER |= (3<<20);
	
	ADC3->SQR3 = 8;
	ADC3->CR2 |= (1<<0);
}

void Init_DAC(void) {
	RCC->APB1ENR |= (1<<29);
	RCC->AHB1ENR |= (1<<0);
}