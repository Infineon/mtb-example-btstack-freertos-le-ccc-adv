/*******************************************************************************
 * File Name: app_bt_event_handler.c
 *
 * Description: This file contains the bluetooth event handler that processes
 *              the bluetooth events from host stack.
 *
 * Related Document: See README.md
 *
 *
 *******************************************************************************
 * Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 ******************************************************************************/

/*******************************************************************************
 * Header Files
 ******************************************************************************/

#include "inttypes.h"
#include <FreeRTOS.h>
#include <task.h>
#include "timers.h"
#include "cyhal_gpio.h"
#include "cycfg_gap.h"
#include "app_bt_utils.h"
#include "app_hw_device.h"
#include "app_bt_event_handler.h"
#include "app_bt_utils.h"
#include "wiced_bt_stack.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_ble.h"
#ifdef ENABLE_BT_SPY_LOG
#include "cybt_debug_uart.h"
#endif

/*******************************************************************************
 * Variable Definitions
 ******************************************************************************/

/* Holds the global state of the ccc vehicle application */
ccc_vehicle_state_t ccc_vehicle_state;

/*******************************************************************************
 * Function Definitions
 ******************************************************************************/
/**
 * Function Name: app_bt_management_callback
 *
 * Function Description:
 *   @brief This is a Bluetooth stack event handler function to receive
 *   management events from the LE stack and process as per the application.
 *
 *   @param wiced_bt_management_evt_t event             : LE event code of
 *          one byte length
 *   @param wiced_bt_management_evt_data_t *p_event_data: Pointer to LE
 *          management event structures
 *
 *   @return wiced_result_t                             : Error code from
 *           WICED_RESULT_LIST or BT_RESULT_LIST
 *
 */
wiced_result_t
app_bt_management_callback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_result_t result = WICED_BT_SUCCESS;
    const wiced_bt_ble_advert_mode_t *p_mode;
    wiced_bt_device_address_t local_bda = {0x00, 0xA0, 0x50, 0x11, 0x44, 0x55};
    printf("Event:%s\n", get_btm_event_name(event));
    switch (event)
    {
        case BTM_ENABLED_EVT:
            /* Bluetooth Controller and Host Stack Enabled */
            if (WICED_BT_SUCCESS == p_event_data->enabled.status)
            {
                wiced_bt_set_local_bdaddr(local_bda, BLE_ADDR_PUBLIC);
                wiced_bt_dev_read_local_addr(local_bda);
                printf("Local Bluetooth Address:");
                print_bd_address(local_bda);
                /* Perform application-specific initialization */
                app_bt_application_init();
            }
            else
            {
                printf("Bluetooth enable failed\n");
            }
            break;

        case BTM_DISABLED_EVT:
            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
            p_mode = &p_event_data->ble_advert_state_changed;
            printf("Adv State Change:%s\n",get_bt_advert_mode_name(*p_mode));
            if (*p_mode == BTM_BLE_ADVERT_OFF)
            {
                ccc_vehicle_state.start_adv = 0;
            }
            break;

        default:
            printf("Unhandled Bluetooth Management Event:0x%x,%s\n",
                    event, get_btm_event_name(event));
            break;
    }
    return result;
}

/**
 * Function Name: hci_trace_cback
 *
 * Function Description:
 *   @brief This callback routes HCI packets to debug uart.
 *
 *   @param wiced_bt_hci_trace_type_t type : HCI trace type
 *   @param uint16_t length : length of p_data
 *   @param uint8_t* p_data : pointer to data
 *
 *   @return None
 *
 */
#ifdef ENABLE_BT_SPY_LOG
void hci_trace_cback(wiced_bt_hci_trace_type_t type,
                     uint16_t length, uint8_t* p_data)
{
    cybt_debug_uart_send_hci_trace(type, length, p_data);
}
#endif

/**
 * Function Name: app_bt_application_init
 *
 * Function Description:
 *   @brief This function handles application level initialization tasks and is
 *   called from the BT management callback once the LE stack enabled event
 *   (BTM_ENABLED_EVT) is triggered. This function is executed in the
 *   BTM_ENABLED_EVT management callback.
 *
 *   @param None
 *
 *   @return None
 *
 */
void app_bt_application_init(void)
{
    wiced_result_t result = WICED_BT_SUCCESS;

    #ifdef ENABLE_BT_SPY_LOG
    wiced_bt_dev_register_hci_trace(hci_trace_cback);
    #endif

    printf("\nCCC Adv application init\n");

    app_bt_interrupt_config();

    app_bt_hw_init();

    /* Set Advertisement Data */
    wiced_bt_ble_set_raw_advertisement_data(CY_BT_ADV_PACKET_DATA_SIZE, cy_bt_adv_packet_data);
    /* Start Undirected LE Advertisements on device startup.
     * The corresponding parameters are contained in 'app_bt_cfg.c' */
    result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
    /* Failed to start advertisement. Stop program execution */
    if (WICED_BT_SUCCESS != result)
    {
        printf("Start Adv failed:%d\n", result);
        CY_ASSERT(0);
    }
    else
    {
        ccc_vehicle_state.start_adv = 1;
        printf("Start Adv:%d\n", result);
    }
}

/* END OF FILE [] */
