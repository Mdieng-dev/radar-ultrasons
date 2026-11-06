/*********************************************************************
*                     SEGGER Microcontroller GmbH                    *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996 - 2025  SEGGER Microcontroller GmbH                *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File        : ID_SCREEN_00_Slots.c
Purpose     : AppWizard managed file, function content could be changed
---------------------------END-OF-HEADER------------------------------
*/

#include "Application.h"
#include "../Generated/Resource.h"
#include "../Generated/ID_SCREEN_00.h"

/*** Begin of user code area ***/
#include "stm32f7xx.h"                  // Device header

/*** End of user code area ***/

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       cbID_SCREEN_00
*/
void cbID_SCREEN_00(WM_MESSAGE * pMsg) {
  GUI_USE_PARA(pMsg);
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_SWITCH_00__WM_NOTIFICATION_VALUE_CHANGED
*/
void ID_SCREEN_00__ID_SWITCH_00__WM_NOTIFICATION_VALUE_CHANGED(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  int Val;
  Val = APPW_GetValue(ID_SCREEN_00, ID_SWITCH_00, NULL);
  if(Val==1) GPIOI->ODR |= (1<<2);
	else GPIOI->ODR &= ~(1<<2);
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_SLIDER_00__WM_NOTIFICATION_VALUE_CHANGED
*/
void ID_SCREEN_00__ID_SLIDER_00__WM_NOTIFICATION_VALUE_CHANGED(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  int Val;
  Val = APPW_GetValue(ID_SCREEN_00, ID_SLIDER_00, NULL);
  //Val vaut une valeur entre 0 et 100
  DAC->DHR12R1 = Val*4095.0/100;
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_ROTARY_00__WM_NOTIFICATION_VALUE_CHANGED
*/
void ID_SCREEN_00__ID_ROTARY_00__WM_NOTIFICATION_VALUE_CHANGED(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  int Val;
	double rc;
//  Val = APPW_GetValue(ID_SCREEN_00, ID_ROTARY_00, NULL);
//  //Val vaut une valeur entre -300 et 300
//  rc = (Val+300)*100/600;
//	TIM10->CCR1 = (int)rc*43199;
}

/*********************************************************************
*
*       ID_SCREEN_00__APPW_NOTIFICATION_INITDIALOG__ID_TIMER_00__APPW_JOB_START
*/
void ID_SCREEN_00__APPW_NOTIFICATION_INITDIALOG__ID_TIMER_00__APPW_JOB_START(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  GUI_USE_PARA(pAction);
  GUI_USE_PARA(hScreen);
  GUI_USE_PARA(pMsg);
  GUI_USE_PARA(pResult);
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_TIMER_00__APPW_NOTIFICATION_TIMER
*/
void ID_SCREEN_00__ID_TIMER_00__APPW_NOTIFICATION_TIMER(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
	APPW_SetValue(ID_SCREEN_00, ID_GAUGE_00, ADC3->DR);
	ADC3->CR2 |= (1<<30);
}

/*************************** End of file ****************************/
