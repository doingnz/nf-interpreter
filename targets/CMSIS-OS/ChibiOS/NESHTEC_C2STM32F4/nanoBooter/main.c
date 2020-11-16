//
// Copyright (c) .NET Foundation and Contributors
// See LICENSE file in the project root for full license information.
//

#include <ch.h>
#include <hal.h>
#include <hal_nf_community.h>
#include <cmsis_os.h>

#include <usbcfg.h>
#include <swo.h>
#include <targetHAL.h>
#include <WireProtocol_ReceiverThread.h>
#include <nanoPAL_BlockStorage.h>
#include <LaunchCLR.h>

// need to declare the Receiver thread here
osThreadDef(ReceiverThread, osPriorityHigh, 2048, "ReceiverThread");

//  Application entry point.
int main(void)
{
    // HAL initialization, this also initializes the configured device drivers
    // and performs the board-specific initializations.
    halInit();

    // init boot clipboard
    InitBootClipboard();

// init SWO as soon as possible to make it available to output ASAP
#if (SWO_OUTPUT == TRUE)
    SwoInit();
#endif

    // The kernel is initialized but not started yet, this means that
    // main() is executing with absolute priority but interrupts are already enabled.
    osKernelInitialize();
    osDelay(20); // Let init stabilize

    // the following IF is not mandatory, it's just providing a way for a user to 'force'
    // the board to remain in nanoBooter and not launching nanoCLR

#if (HAL_NF_USE_STM32_CRC == TRUE)
    // startup crc
    crcStart(NULL);
#endif

    // check if there is a request to remain on nanoBooter
    if (!IsToRemainInBooter())
    {
        // if the USER button (blue one) is pressed, skip the check for a valid CLR image and remain in booter
        // the user button in this board has a pull-up resistor so the check has to be inverted
        //if (!palReadPad(GPIOA, GPIOA_BUTTON))
        {
            // we are checking for a valid image right after the configuration block
            if (CheckValidCLRImage((uint32_t)&__nanoConfig_end__))
            {
                // there seems to be a valid CLR image
                // launch nanoCLR
                LaunchCLR((uint32_t)&__nanoConfig_end__);
            }
        }
    }

    //  Initializes a serial-over-USB CDC driver.
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    // Activates the USB driver and then the USB bus pull-up on D+.
    // Note, a delay is inserted in order to not have to disconnect the cable after a reset.
    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(100);
    usbStart(serusbcfg.usbp, &usbcfg);
    usbConnectBus(serusbcfg.usbp);

    // create the receiver thread
    osThreadCreate(osThread(ReceiverThread), NULL);

    // start kernel, after this main() will behave like a thread with priority osPriorityNormal
    osKernelStart();

    // initialize block storage list and devices
    // in CLR this is called in nanoHAL_Initialize()
    // for nanoBooter we have to init it in order to provide the flash map for Monitor_FlashSectorMap command
    BlockStorageList_Initialize();
    BlockStorage_AddDevices();

    // report successfull nanoBooter execution
    ReportSuccessfullNanoBooter();

    //  Normal main() thread
    while (true)
    {
        palTogglePad(GPIOE, GPIOE_LD1);
        osDelay(250);
    }
}
