// snoop.h
// (c)2026 nitrologic
// mit license

#pragma once

#include <cstddef>
#include <cstdint>

void initSnoop();
bool beginSnoop(uint32_t* buffer, size_t max_samples);
bool snoopComplete();
size_t getCapturedSampleCount();
void stopBusCapture();
