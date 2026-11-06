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
File        : Resource.h
Purpose     : Generated file do NOT edit!
---------------------------END-OF-HEADER------------------------------
*/

#ifndef RESOURCE_H
#define RESOURCE_H

#include "AppWizard.h"

/*********************************************************************
*
*       Text
*/
#define ID_RTEXT_0 0

#define APPW_MANAGE_TEXT APPW_MANAGE_TEXT_EXT
extern GUI_CONST_STORAGE unsigned char acAPPW_Language_0[];

/*********************************************************************
*
*       Fonts
*/
extern GUI_CONST_STORAGE unsigned char acNettoOT_32_Normal_EXT_AA4[];

/*********************************************************************
*
*       Images
*/
extern GUI_CONST_STORAGE unsigned char aclogo_iut[];
extern GUI_CONST_STORAGE unsigned char acDARK_Switch_Body_H_On_55x20[];
extern GUI_CONST_STORAGE unsigned char acDARK_Switch_Body_H_Off_55x20[];
extern GUI_CONST_STORAGE unsigned char acDARK_Switch_Thumb_26x26[];
extern GUI_CONST_STORAGE unsigned char acDARK_Slider_Shaft_H_Left_Blue_30x16[];
extern GUI_CONST_STORAGE unsigned char acDARK_Slider_Shaft_H_Right_Gray_30x16[];
extern GUI_CONST_STORAGE unsigned char acDARK_Slider_Thumb_Up_22x22[];
extern GUI_CONST_STORAGE unsigned char acDARK_Slider_Thumb_Down_22x22[];
extern GUI_CONST_STORAGE unsigned char acDARK_Rotary_BG_Gray_80x80[];
extern GUI_CONST_STORAGE unsigned char acDARK_Rotary_Marker_Blue_16x5[];

/*********************************************************************
*
*       Variables
*/
#define ID_VAR_00 (GUI_ID_USER + 2048)

/*********************************************************************
*
*       Screens
*/
#define ID_SCREEN_00 (GUI_ID_USER + 4096)

extern APPW_ROOT_INFO ID_SCREEN_00_RootInfo;

#define APPW_INITIAL_SCREEN &ID_SCREEN_00_RootInfo

/*********************************************************************
*
*       Project path
*/
#define APPW_PROJECT_PATH "C:/Users/romain/Documents/projet_template/appwizard/TestAppwizard"

#endif  // RESOURCE_H

/*************************** End of file ****************************/
