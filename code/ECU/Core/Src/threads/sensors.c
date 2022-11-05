/**
 * @file sensors.c
 * @author Joseph Telaak
 * @brief 
 * @version 0.1
 * @date 2022-11-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */

// Header includes
#include "sensors.h"

// Init function
void sensors_init(void) {
    allocator = rcl_get_default_allocator();
    rclc_support_init(&support, 0, NULL, &allocator);

}