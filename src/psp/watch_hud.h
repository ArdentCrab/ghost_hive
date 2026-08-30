#ifndef WATCH_HUD_H
#define WATCH_HUD_H

#include "ghost_output.h"

bool watch_bind_ok();
bool watch_danger_headline(const GhostOutput::WatchSrc& src);
bool watch_headline_alarm(const GhostOutput::WatchSrc& src);
void watch_fill_alert(const GhostOutput::WatchSrc& src, char* dst);

#endif
