/* --COPYRIGHT--,FRAM-Utilities
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * This source code is part of FRAM Utilities for MSP430 FRAM Microcontrollers.
 * Visit http://www.ti.com/tool/msp-fram-utilities for software information and
 * download.
 * --/COPYRIGHT--*/
//******************************************************************************
//  nvs_ex2_config - Store application configuration
//
//  This example demonstrates how to store a complex structure in the non
//  volatile memory. The library function will assure to always retrieve a
//  complete set of variables regardless of asynchronous reset or power cycle
//  events. Even a compile and program update does not disrupt the configuration
//  as long as the NVS container stays the same.
//
//  Michael Zwerg, Brent Peterson
//  Texas Instruments Inc.
//  December 2016
//******************************************************************************
#include <msp430.h>

#include <stdint.h>
#include <string.h>

#include "nvs.h"
#include "main.h"


// Initial application configuration
const nvsVariables_t nvsVarInit = {
    .cwSpeed = 18,
    .qsk = 300,
    .paddleConfig = PADDLE_DAH_DIT
};

// NVS data handle
nvs_data_handle nvsHandle;

// Allocate NVS container inside INFO memory to store application configuration
#if defined(__TI_COMPILER_VERSION__)
//#pragma DATA_SECTION(nvsStorage, ".infoA")
#pragma PERSISTENT(nvsStorage)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma location="INFOA"
__no_init
#endif
uint8_t nvsStorage[NVS_DATA_STORAGE_SIZE(sizeof(nvsVariables_t))] = {0};

extern nvsVariables_t nvsVar;  // create structure nvsVar of type struct nvsVariables to hold nvs data

// This function will initialize the nvs memory if empty
void nvsVarInitialize(void)
{
    uint16_t status;

    // Check integrity of NVS container and initialize if required
    nvsHandle = nvs_data_init(nvsStorage, sizeof(nvsVariables_t));

    // Retrieve application configuration
    status = nvs_data_restore(nvsHandle, &nvsVar);

    switch (status) {
    case NVS_OK: break;
    case NVS_EMPTY:
        // Initialize local application configuration.
        memcpy(&nvsVar, &nvsVarInit, sizeof(nvsVariables_t));

        // Update NVS container with initial application configuration.
        status = nvs_data_commit(nvsHandle, &nvsVar);

        //
         // Status should never be not NVS_OK but if it happens trap execution.
         // Potential reason for NVS_NOK:
         //     1. nvsStorage not initialized
         //    2. nvsStorage got corrupted by other task (buffer overflow?)
         //
        if (status != NVS_OK) {
            while (1);
        }
        break;
    }
}

void nvsVarWrite(void)  // this will write the values of the variable in nvs
{
    uint16_t status;

    // Check integrity of NVS container and initialize if required
    nvsHandle = nvs_data_init(nvsStorage, sizeof(nvsVariables_t));


    /*
     * Update NVS container with application configuration. In case of a reset
     * the application will resume with this state.
     */
    status = nvs_data_commit(nvsHandle, &nvsVar);

    /*
     * Status should never be not NVS_OK but if it happens trap execution.
     * Potential reason for NVS_NOK:
     *     1. nvsStorage not initialized
     *     2. nvsStorage got corrupted by other task (buffer overflow?)
     */
    if (status != NVS_OK) {
        while (1);
    }
}


void nvsVarRead(void)  // this will read the values of the variable in nvs
{
    uint16_t status;

    // Check integrity of NVS container and initialize if required
    nvsHandle = nvs_data_init(nvsStorage, sizeof(nvsVariables_t));


    /*
     * Update NVS container with application configuration. In case of a reset
     * the application will resume with this state.
     */
    status = nvs_data_restore(nvsHandle, &nvsVar);

    /*
     * Status should never be not NVS_OK but if it happens trap execution.
     * Potential reason for NVS_NOK:
     *     1. nvsStorage not initialized
     *     2. nvsStorage got corrupted by other task (buffer overflow?)
     */
    if (status != NVS_OK) {
        while (1);
    }
}

