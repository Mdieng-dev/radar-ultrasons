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
File        : ID_SCREEN_00.c
Purpose     : Generated file do NOT edit!
---------------------------END-OF-HEADER------------------------------
*/

#include "Resource.h"
#include "ID_SCREEN_00.h"

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
/*********************************************************************
*
*       _aCreate
*/
static APPW_CREATE_ITEM _aCreate[] = {
  { WM_OBJECT_WINDOW_Create,
    ID_SCREEN_00, 0,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
      },
      0, 0, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_BOX_Create,
    ID_BOX_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
      },
      0, 0, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_IMAGE_Create,
    ID_IMAGE_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      162, 88, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_TEXT_Create,
    ID_TEXT_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 177, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 28, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      294, 32, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_SWITCH_Create,
    ID_SWITCH_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 22, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 123, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      55, 26, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_SLIDER_Create,
    ID_SLIDER_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 140, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 205, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      200, 22, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_ROTARY_Create,
    ID_ROTARY_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 147, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 96, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      80, 80, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_TEXT_Create,
    ID_TEXT_01, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 238, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      290, 32, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_GAUGE_Create,
    ID_GAUGE_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 280, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 81, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      200, 105, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_TIMER_Create,
    ID_TIMER_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 480, 0, 0 },
        { DISPOSE_MODE_NULL, 272, 0, 0 },
      },
      1, 1, 0, 0, 0, 0
    },
    { 0, 0 }
  },
};

/*********************************************************************
*
*       _aSetup
*/
static GUI_CONST_STORAGE APPW_SETUP_ITEM _aSetup[] = {
  { ID_BOX_00,    APPW_SET_PROP_COLOR,        { ARG_V(0xffffe873) } },
  { ID_IMAGE_00,  APPW_SET_PROP_SBITMAP,      { ARG_VP(0, aclogo_iut),
                                                ARG_V(9051), } },
  { ID_IMAGE_00,  APPW_SET_PROP_COLOR,        { ARG_V(GUI_INVALID_COLOR) } },
  { ID_IMAGE_00,  APPW_SET_PROP_SCALE,        { ARG_V(500) } },
  { ID_IMAGE_00,  APPW_SET_PROP_ALIGNBITMAP,  { ARG_V(GUI_ALIGN_HCENTER | GUI_ALIGN_VCENTER),
                                                ARG_V(0),
                                                ARG_V(0) } },
  { ID_TEXT_00,   APPW_SET_PROP_COLOR,        { ARG_V(0xff1802c0) } },
  { ID_TEXT_00,   APPW_SET_PROP_ALIGNTEXT,    { ARG_V(GUI_ALIGN_HCENTER | GUI_ALIGN_VCENTER),
                                                ARG_V(0),
                                                ARG_V(0) } },
  { ID_TEXT_00,   APPW_SET_PROP_FONT,         { ARG_VP(0, acNettoOT_32_Normal_EXT_AA4) } },
  { ID_TEXT_00,   APPW_SET_PROP_TEXTID,       { ARG_V(ID_RTEXT_0) } },
  { ID_SWITCH_00, APPW_SET_PROP_SBITMAPS,     { ARG_VP(0, acDARK_Switch_Body_H_On_55x20),
                                                ARG_VP(0, acDARK_Switch_Body_H_Off_55x20),
                                                ARG_VP(0, NULL),
                                                ARG_VP(0, acDARK_Switch_Thumb_26x26),
                                                ARG_VP(0, acDARK_Switch_Thumb_26x26), } },
  { ID_SWITCH_00, APPW_SET_PROP_FOCUSABLE,    { ARG_V(0) } },
  { ID_SWITCH_00, APPW_SET_PROP_SWITCHOFF,    { ARG_V(0) } },
  { ID_SWITCH_00, APPW_SET_PROP_FADE,         { ARG_V(0) } },
  { ID_SLIDER_00, APPW_SET_PROP_SBITMAPS,     { ARG_VP(0, acDARK_Slider_Shaft_H_Left_Blue_30x16),
                                                ARG_VP(0, acDARK_Slider_Shaft_H_Right_Gray_30x16),
                                                ARG_VP(0, acDARK_Slider_Thumb_Up_22x22),
                                                ARG_VP(0, acDARK_Slider_Thumb_Down_22x22), } },
  { ID_SLIDER_00, APPW_SET_PROP_RANGE,        { ARG_V(0),
                                                ARG_V(100) } },
  { ID_ROTARY_00, APPW_SET_PROP_SBITMAPS,     { ARG_VP(0, acDARK_Rotary_BG_Gray_80x80),
                                                ARG_VP(0, acDARK_Rotary_Marker_Blue_16x5), } },
  { ID_ROTARY_00, APPW_SET_PROP_POS,          { ARG_V(25) } },
  { ID_ROTARY_00, APPW_SET_PROP_ROTATE,       { ARG_V(0) } },
  { ID_ROTARY_00, APPW_SET_PROP_VALUE,        { ARG_V(0) } },
  { ID_ROTARY_00, APPW_SET_PROP_RANGE,        { ARG_V(300),
                                                ARG_V(300) } },
  { ID_ROTARY_00, APPW_SET_PROP_OFFSET,       { ARG_V(900) } },
  { ID_ROTARY_00, APPW_SET_PROP_ALIGNBITMAP,  { ARG_V(GUI_ALIGN_HCENTER | GUI_ALIGN_VCENTER),
                                                ARG_V(0),
                                                ARG_V(0) } },
  { ID_ROTARY_00, APPW_SET_PROP_PERIOD,       { ARG_V(0) } },
  { ID_ROTARY_00, APPW_SET_PROP_LQ,           { ARG_V(0) } },
  { ID_TEXT_01,   APPW_SET_PROP_COLOR,        { ARG_V(GUI_BLUE) } },
  { ID_TEXT_01,   APPW_SET_PROP_ALIGNTEXT,    { ARG_V(GUI_ALIGN_LEFT | GUI_ALIGN_VCENTER),
                                                ARG_V(0),
                                                ARG_V(0) } },
  { ID_TEXT_01,   APPW_SET_PROP_FONT,         { ARG_VP(0, acNettoOT_32_Normal_EXT_AA4) } },
  { ID_TEXT_01,   APPW_SET_PROP_UNTOUCHABLE,  { ARG_V(0) } },
  { ID_GAUGE_00,  APPW_SET_PROP_ALIGNBITMAP,  { ARG_V(GUI_ALIGN_HCENTER | GUI_ALIGN_BOTTOM),
                                                ARG_V(0),
                                                ARG_V(4294967286) } },
  { ID_GAUGE_00,  APPW_SET_PROP_RANGE,        { ARG_V(0),
                                                ARG_V(1800) } },
  { ID_GAUGE_00,  APPW_SET_PROP_SPAN,         { ARG_V(0),
                                                ARG_V(4095) } },
  { ID_GAUGE_00,  APPW_SET_PROP_RADIUS,       { ARG_V(80) } },
  { ID_GAUGE_00,  APPW_SET_PROP_COLORS,       { ARG_V(0xff2c2c30),
                                                ARG_V(0xff2777b3),
                                                ARG_V(GUI_INVALID_COLOR) } },
  { ID_GAUGE_00,  APPW_SET_PROP_VALUES,       { ARG_V(19),
                                                ARG_V(13) } },
  { ID_GAUGE_00,  APPW_SET_PROP_ROUNDEDVAL,   { ARG_V(0) } },
  { ID_GAUGE_00,  APPW_SET_PROP_ROUNDEDEND,   { ARG_V(0) } },
  { ID_GAUGE_00,  APPW_SET_PROP_VALUE,        { ARG_V(2048) } },
  { ID_GAUGE_00,  APPW_SET_PROP_UNTOUCHABLE,  { ARG_V(0) } },
  { ID_TIMER_00,  APPW_SET_PROP_PERIOD,       { ARG_V(100) } },
  { ID_TIMER_00,  APPW_SET_PROP_AUTORESTART,  { ARG_V(0) } },
};

/*********************************************************************
*
*       _aAction
*/
static GUI_CONST_STORAGE APPW_ACTION_ITEM _aAction[] = {
  { ID_SWITCH_00, WM_NOTIFICATION_VALUE_CHANGED,    0,            APPW_JOB_NULL,           ID_SCREEN_00__ID_SWITCH_00__WM_NOTIFICATION_VALUE_CHANGED,
  },
  { ID_SLIDER_00, WM_NOTIFICATION_VALUE_CHANGED,    0,            APPW_JOB_NULL,           ID_SCREEN_00__ID_SLIDER_00__WM_NOTIFICATION_VALUE_CHANGED,
  },
  { ID_ROTARY_00, WM_NOTIFICATION_VALUE_CHANGED,    0,            APPW_JOB_NULL,           ID_SCREEN_00__ID_ROTARY_00__WM_NOTIFICATION_VALUE_CHANGED,
  },
  { ID_SCREEN_00, APPW_NOTIFICATION_INITDIALOG,     ID_TIMER_00,  APPW_JOB_START,          ID_SCREEN_00__APPW_NOTIFICATION_INITDIALOG__ID_TIMER_00__APPW_JOB_START,
  },
  { ID_TIMER_00,  APPW_NOTIFICATION_TIMER,          0,            APPW_JOB_NULL,           ID_SCREEN_00__ID_TIMER_00__APPW_NOTIFICATION_TIMER,
  },
};

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
/*********************************************************************
*
*       ID_SCREEN_00_RootInfo
*/
APPW_ROOT_INFO ID_SCREEN_00_RootInfo = {
  ID_SCREEN_00,
  _aCreate, GUI_COUNTOF(_aCreate),
  _aSetup,  GUI_COUNTOF(_aSetup),
  _aAction, GUI_COUNTOF(_aAction),
  cbID_SCREEN_00,
  0
};

/*************************** End of file ****************************/
