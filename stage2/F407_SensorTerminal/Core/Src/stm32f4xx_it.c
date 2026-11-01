#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

void SysTick_Handler(void)
{
    HAL_IncTick();

    /* HAL starts SysTick before the FreeRTOS scheduler exists. */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}
