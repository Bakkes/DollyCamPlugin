#pragma once
#include "bakkesmod/wrappers/wrapperstructs.h"
#include "Models.h"
#include "IGameApplier.h"
#include "utils/io.h"
#include <fstream>

bool apply_frame(float replaySeconds, float currentTimeInMs);

savetype::iterator find_current_snapshot(float currentTime);

void reset_iterator();