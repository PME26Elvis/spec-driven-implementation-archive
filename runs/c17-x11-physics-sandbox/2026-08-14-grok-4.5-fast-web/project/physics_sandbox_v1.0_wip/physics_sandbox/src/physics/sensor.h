#ifndef PS_SENSOR_H
#define PS_SENSOR_H
#include "world.h"

typedef struct {
    ps_vec2 min, max;
    int overlap_count;
    int enabled;
} ps_sensor;

void ps_sensor_init(ps_sensor *s, ps_vec2 min, ps_vec2 max);
void ps_sensor_update(ps_sensor *s, ps_world *w);

#endif
