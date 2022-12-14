/**
 * @file brake.c
 * @author Joseph Telaak
 * @brief Monitors the brake pedal position, controls the brake lights, and sets the brake pressure.
 * @version 0.1
 * @date 2022-11-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */

// Includes
#include "brake_node.h"

// Inirialize the brake node
void brake_node_init(void) {
    // Init the node
    rclc_node_init_default(&brake_node, BRAKE_NODE_NAME, "", &support);

    // Messages
    std_msgs__msg__Bool emergency_msg;
    std_msgs__msg__Float32 brake_actuator_l_msg;
    std_msgs__msg__Float32 brake_actuator_r_msg;

    // Create subscribers
    rclc_subscription_init_default(&emergency_brake_sub, &brake_node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool), EMERGEBCY_BRAKE_TOPIC);
    rclc_subscription_init_default(&brake_actuator_l_sub, &brake_node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), BRAKE_ACTUATOR_L_TOPIC);
    rclc_subscription_init_default(&brake_actuator_r_sub, &brake_node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), BRAKE_ACTUATOR_R_TOPIC);

    // Create publishers
    rclc_publisher_init_default(&brake_pedal_pressed_pub, &brake_node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool), BRAKE_PEDAL_PRESSED_TOPIC);
    rclc_publisher_init_default(&brake_pedal_position_pub, &brake_node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), BRAKE_PEDAL_POSITION_TOPIC);
    rclc_publisher_init_default(&brake_actuator_active_pub, &brake_node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool), BRAKE_ACTUATOR_ACTIVE_TOPIC);
    rclc_publisher_init_default(&brake_act_position_pub, &brake_node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), BRAKE_ACTUATOR_POSITION_TOPIC);

    // Create timers
    rclc_timer_init_default(&brake_timer, &support, RCL_MS_TO_NS(BRAKE_UPDATE_FREQUENCY), brake_timer_callback);

    // Initialize executor
    rclc_executor_init(&brake_executor, &support.context, 5, &support.allocator);

    // Add subscriptions to executor
    rclc_executor_add_subscription(&brake_executor, &emergency_brake_sub, &emergency_msg, &emergency_brake_callback, ON_NEW_DATA);
    rclc_executor_add_subscription(&brake_executor, &brake_actuator_l_sub, &brake_actuator_l_msg, &brake_actuator_l_callback, ON_NEW_DATA);
    rclc_executor_add_subscription(&brake_executor, &brake_actuator_r_sub, &brake_actuator_r_msg, &brake_actuator_r_callback, ON_NEW_DATA);

    // Add timers to executor
    rclc_executor_add_timer(&brake_executor, &brake_timer);

}

// Brake timer callback
void brake_timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
    // Create messages
    std_msgs__msg__Bool brake_pedal_pressed_msg;
    std_msgs__msg__Float32 brake_pedal_position_msg;
    std_msgs__msg__Float32 brake_act_position_msg;
    std_msgs__msg__Bool brake_actuator_active_msg;
    // Get the brake actuator position
    brake_act_position_msg.data = get_brake_actuator_position();

    // Check if the brake actuator is active
    brake_actuator_active_msg.data = brake_actuator_active();

    // Check if the brake pedal is pressed
    if (brake_is_pressed()) {
        brake_pedal_pressed_msg.data = true;
        brake_pedal_position_msg.data = get_brake_pedal_position();

    } else {
        brake_pedal_pressed_msg.data = false;
        brake_pedal_position_msg.data = 0.0;

    }

    // Publish the messages
    rclc_publish(&brake_pedal_pressed_pub, (const void*)&brake_pedal_pressed_msg);
    rclc_publish(&brake_pedal_position_pub, (const void*)&brake_pedal_position_msg);
    rclc_publish(&brake_act_position_pub, (const void*)&brake_act_position_msg);
    rclc_publish(&brake_actuator_active_pub, (const void*)&brake_actuator_active_msg);

}

// Spin function
void brake_node_spin(void) {
    rclc_executor_spin(&brake_executor);
}

// Is brake pressed
bool brake_is_pressed(void) {
    // Check if the brake pedal is pressed
    if (read_brake_pedal_position() > BRAKE_PEDAL_THRESHOLD) {
        // Activate the brake detect LED
        brake_detect_led(true);

        return true;

    } else {
        // Deactivate the brake detect LED
        brake_detect_led(false);

        return false;

    }
}

// Read brake pedal position
float read_brake_pedal_position(void) {
    // Read the brake pedal position from the correct ADC channel
    SELECT_BRAKE_PEDAL_CHANNEL();
    HAL_ADC_Start(BRAKE_PEDAL_ADC);
    HAL_ADC_PollForConversion(BRAKE_PEDAL_ADC, 100);
    uint32_t brake_pedal_position = HAL_ADC_GetValue(BRAKE_PEDAL_ADC);
    HAL_ADC_Stop(BRAKE_PEDAL_ADC);

    // Return the brake actuator position
    return brake_pedal_position;

}

// Brake detect led
void brake_detect_led(bool state) {
    // Check if the brake pedal is pressed
    if (state) {
        // Turn on the brake detect led
        HAL_GPIO_WritePin(Brake_Detect_LED_GPIO_Port, Brake_Detect_LED_Pin, GPIO_PIN_SET);

    } else {
        // Turn off the brake detect led
        HAL_GPIO_WritePin(Brake_Detect_LED_GPIO_Port, Brake_Detect_LED_Pin, GPIO_PIN_RESET);

    }

}

// Brake motor control
void brake_motor_control(float left_pwm, bool left_enable, float right_pwm, bool right_enable) {
    // The brake motor uses the htim3 timer
    // The left motor uses channel 4
    // The right motor uses channel 3

    // Set the left motor
    if (left_enable) {
        // Set the pwm
        __HAL_TIM_SET_COMPARE(BRAKE_ACTUATOR_MOTOR_CONTROLLER_TIMER, BRAKE_ACTUATOR_MOTOR_CONTROLLER_L, left_pwm);
        // Enable the motor
        HAL_GPIO_WritePin(Brake_L_EN_GPIO_Port, Brake_L_EN_Pin, GPIO_PIN_SET);

        #ifdef BRAKE_INVERTED
            // Send the brake actuator active message
            send_brake_actuator_active(true);
        #else
            // Send the brake actuator active message
            send_brake_actuator_active(false);
        #endif

    } else {
        // Disable the motor
        HAL_GPIO_WritePin(Brake_L_EN_GPIO_Port, Brake_L_EN_Pin, GPIO_PIN_RESET);

    }

    // Set the right motor
    if (right_enable) {
        // Set the pwm
        __HAL_TIM_SET_COMPARE(BRAKE_ACTUATOR_MOTOR_CONTROLLER_TIMER, BRAKE_ACTUATOR_MOTOR_CONTROLLER_R, right_pwm);
        // Enable the motor
        HAL_GPIO_WritePin(Brake_R_EN_GPIO_Port, Brake_R_EN_Pin, GPIO_PIN_SET);

        #ifdef BRAKE_INVERTED
            // Send the brake actuator active message
            send_brake_actuator_active(false);
        #else
            // Send the brake actuator active message
            send_brake_actuator_active(true);
        #endif

    } else {
        // Disable the motor
        HAL_GPIO_WritePin(Brake_R_EN_GPIO_Port, Brake_R_EN_Pin, GPIO_PIN_RESET);

    }

}

// Brake actuator left callback
void brake_actuator_l_callback(const void * msgin) {
    // Get the message
    const std_msgs__msg__Float32 * msg = (const std_msgs__msg__Float32 *)msgin;

    // Check if the value is greater than 0
    if (msg->data > 0) {
        brake_motor_control(msg->data, true, 0, false);
        
    } else {
        brake_motor_control(0, false, 0, false);

    }
}

// Brake actuator right callback
void brake_actuator_r_callback(const void * msgin) {
    // Get the message
    const std_msgs__msg__Float32 * msg = (const std_msgs__msg__Float32 *)msgin;

    // Check if the value is greater than 0
    if (msg->data > 0) {
        brake_motor_control(0, false, msg->data, true);
        
    } else {
        brake_motor_control(0, false, 0, false);

    }
}

// Read brake actuator position
float brake_get_actuator_pos(void) {
    // Read the brake actuator position from the correct ADC channel
    SELECT_BRAKE_ACTUATOR_CHANNEL();
    HAL_ADC_Start(BRAKE_ACTUATOR_ADC);
    HAL_ADC_PollForConversion(BRAKE_ACTUATOR_ADC, 100);
    uint32_t brake_actuator_position = HAL_ADC_GetValue(BRAKE_ACTUATOR_ADC);
    HAL_ADC_Stop(BRAKE_ACTUATOR_ADC);

    // Return the brake actuator position
    return brake_actuator_position;

}

// Emergency brake callback
void emergency_brake_callback(const void * msgin) {
    // Get the message
    const std_msgs__msg__Bool * msg = (const std_msgs__msg__Bool *)msgin;

    // Set the emergency brake
    #ifdef BRAKE_INVERTED
        brake_motor_control(255, true, 0, false);
    #else
        brake_motor_control(0, false, 255, true);
    #endif

}

// Is brake actuator active
bool brake_actuator_is_active(void) {
    // Check if the brake actuator is active
    return HAL_GPIO_ReadPin(Brake_L_EN_GPIO_Port, Brake_L_EN_Pin) == GPIO_PIN_SET || 
            HAL_GPIO_ReadPin(Brake_R_EN_GPIO_Port, Brake_R_EN_Pin) == GPIO_PIN_SET;

}

// Send brake actuator active message
void send_brake_actuator_active_message(bool state) {
    // Set the brake actuator active message
    std_msgs__msg__Bool brake_actuator_active_msg;
    brake_actuator_active_msg.data = state;

    // Publish the brake actuator active message
    rclc_publish(&brake_actuator_active_pub, (const void*)&brake_actuator_active_msg);

}
