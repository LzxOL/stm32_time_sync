#ifndef __TIME_SYNC_CONFIG_H
#define __TIME_SYNC_CONFIG_H

/* Protocol mcu_marker_v1. The ROS decoder must use the same constants.
 * These divider values require a 72 MHz APB1 timer clock.
 * The civil date is only an encoding of a relative clock, not wall time. */
#define TIME_SYNC_RELATIVE_CLOCK     1
#define CAMERA_START_PPS             10U
/* Change only this value for the normal camera rate (1..60 Hz).
 * The startup marker timings remain fixed. */
#define CAMERA_TRIGGER_HZ            2U
#if CAMERA_TRIGGER_HZ < 1U || CAMERA_TRIGGER_HZ > 60U
#error CAMERA_TRIGGER_HZ must be between 1 and 60
#endif
#define CAMERA_TIMER_PRESCALER       (4800U - 1U)
#define CAMERA_TIMER_TICK_HZ         15000U
/* Round to the nearest timer tick for frequencies without an exact divisor. */
#define CAMERA_TRIGGER_PERIOD       ((CAMERA_TIMER_TICK_HZ + CAMERA_TRIGGER_HZ / 2U) / CAMERA_TRIGGER_HZ - 1U)
#define CAMERA_TRIGGER_PULSE         150U
#define PPS_TIMER_PRESCALER          (7200U - 1U)
#define PPS_PERIOD                   (10000U - 1U)
#define PPS_PULSE                    1000U

#endif

