#ifndef PS_SCENE_IO_H
#define PS_SCENE_IO_H
#include "../physics/world.h"

int ps_scene_save_json(const ps_world *w, const char *path);
int ps_scene_load_json(ps_world *w, const char *path);

#endif
