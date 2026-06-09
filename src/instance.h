#pragma once

#include <functional>

// Primary instance: bind socket and return true.
// Secondary instance: notify running instance to show, return false.
bool AcquireInstance();

void PollInstanceServer(const std::function<void()>& on_show);
void ReleaseInstance();
