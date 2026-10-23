# Camera trigger frequency

Edit only `CAMERA_TRIGGER_HZ` in `Hardware/TimeSync_Config.h`, then rebuild and flash `Project.uvprojx`:

```c
#define CAMERA_TRIGGER_HZ 5U
```

Supported values are integer frequencies from 1 to 60 Hz. Both PA0 and PA1 use the same rate and keep a 10 ms high pulse. The OLED labels follow this parameter automatically. Values outside this range cause a compilation error.

The fixed 15 kHz timer clock rounds each period to the nearest tick. The actual rate is `15000 / round(15000 / CAMERA_TRIGGER_HZ)` Hz. For 5 Hz, the period is exactly 3000 ticks (200 ms). Rates without an exact divisor have a small rounding error.

The PPS 10 startup and the 200/300/400 ms marker intervals remain fixed. Normal acquisition follows the marker sequence at the configured rate. PB5 continues to output 1 Hz PPS. If a host/ROS decoder assumes a 15 Hz normal acquisition rate, update that assumption to the configured rate as well.
