
#include "cmsis_os2.h"
#include "GUI.h"
#include "rl_net.h"
#include "AppWizard.h"
#include <stdio.h>

/*----------------------------------------------------------------------------
 *      GUIThread: GUI Thread for Single-Task Execution Model
 *---------------------------------------------------------------------------*/
#define GUI_THREAD_STK_SZ    (4096U)

static osThreadId_t GUIThread_tid;                      /* thread id */
static uint64_t     GUIThread_stk[GUI_THREAD_STK_SZ/8]; /* thread stack */

static const osThreadAttr_t GUIThread_attr = {
  .stack_mem  = &GUIThread_stk[0],
  .stack_size = sizeof(GUIThread_stk),
  .priority   = osPriorityIdle 
};

static osThreadId_t CheckIP_Thread_tid;                      /* thread id */
static uint64_t     CheckIP_Thread_stk[GUI_THREAD_STK_SZ/8]; /* thread stack */

static const osThreadAttr_t CheckIP_Thread_attr = {
  .stack_mem  = &CheckIP_Thread_stk[0],
  .stack_size = sizeof(CheckIP_Thread_stk),
  .priority   = osPriorityIdle 
};

extern void MainTask(void);
void Check_IP(void);

int Init_GUIThread (void) {

  GUIThread_tid = osThreadNew((void *) MainTask, NULL, &GUIThread_attr);
  if (GUIThread_tid == NULL) {
    return(-1);
  }
  
  CheckIP_Thread_tid = osThreadNew((void *) Check_IP, NULL, &CheckIP_Thread_attr);
  if (GUIThread_tid == NULL) {
    return(-1);
  }

  return(0);
}

void Check_IP(void)
{
	uint8_t ip4_addr[NET_ADDR_IP4_LEN];
	WM_HWIN hItem;
	char texte[20];
	
	while(1)
	{
		osDelay(2000);	
		netIF_GetOption (NET_IF_CLASS_ETH | 0, netIF_OptionIP4_Address, ip4_addr, sizeof (ip4_addr));
		
		  hItem = WM_GetDialogItem(WM_GetActiveWindow(), GUI_ID_USER + 6);
		  sprintf(texte, "%d.%d.%d.%d", ip4_addr[0],ip4_addr[1],ip4_addr[2],ip4_addr[3]);
		  TEXT_SetText(hItem, texte);
	}
}

