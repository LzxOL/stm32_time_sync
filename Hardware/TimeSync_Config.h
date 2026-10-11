#ifndef __TIME_SYNC_CONFIG_H
#define __TIME_SYNC_CONFIG_H

/* Protocol mcu_marker_v1. The ROS decoder must use the same constants.
 * These divider values require a 72 MHz APB1 timer clock.
 * The civil date is only an encoding of a relative clock, not wall time. */
#define TIME_SYNC_RELATIVE_CLOCK     1
#define CAMERA_START_PPS             10U
#define CAMERA_TIMER_PRESCALER       (4800U - 1U)
#define CAMERA_TIMER_TICK_HZ         15000U
#define CAMERA_TRIGGER_PERIOD       (1000U - 1U)
#define CAMERA_TRIGGER_PULSE         150U
#define PPS_TIMER_PRESCALER          (7200U - 1U)
#define PPS_PERIOD                   (10000U - 1U)
#define PPS_PULSE                    1000U

#endif
