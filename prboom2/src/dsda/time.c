//
// Copyright(C) 2020 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA Time
//

#include <time.h>

#include "i_system.h"

#include "time.h"

// [XA] MSVC hack: need to implement a handful of functions here.
// this was yoinked from https://stackoverflow.com/a/31335254 so
// no guarantees on if this stuff is accurate/kosher/whatever,
// so maybe don't cut an official release with MSVC just yet.
// it's enough to hack it into compiling for me, at least. ;)

#ifdef _MSC_VER

#include <windows.h>

#define CLOCK_MONOTONIC -1
#define exp7           10000000i64 //1E+7
#define exp9         1000000000i64 //1E+9
#define w2ux 116444736000000000i64 //1.jan1601 to 1.jan1970

void unix_time(struct timespec *spec) {
  __int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime);
  wintime -= w2ux;  spec->tv_sec = wintime / exp7;
  spec->tv_nsec = wintime % exp7 * 100;
}

int clock_gettime(int _skip, struct timespec *spec)
{
  static struct timespec startspec;
  static double ticks2nano;
  static __int64 startticks, tps = 0;
  __int64 tmp, curticks;

  QueryPerformanceFrequency((LARGE_INTEGER*)&tmp);

  if (tps != tmp) {
    tps = tmp;
    QueryPerformanceCounter((LARGE_INTEGER*)&startticks);
    unix_time(&startspec); ticks2nano = (double)exp9 / tps;
  }

  QueryPerformanceCounter((LARGE_INTEGER*)&curticks); curticks -= startticks;
  spec->tv_sec = startspec.tv_sec + (curticks / tps);
  spec->tv_nsec = startspec.tv_nsec + (double)(curticks % tps) * ticks2nano;
  if (!(spec->tv_nsec < exp9)) { spec->tv_sec++; spec->tv_nsec -= exp9; }
  return 0;
}

#endif //_MSC_VER

// [XA] END HACK

static struct timespec dsda_time[DSDA_TIMER_COUNT];

void dsda_StartTimer(int timer) {
  clock_gettime(CLOCK_MONOTONIC, &dsda_time[timer]);
}

unsigned long long dsda_ElapsedTime(int timer) {
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);

  return (now.tv_nsec - dsda_time[timer].tv_nsec) / 1000 +
         (now.tv_sec - dsda_time[timer].tv_sec) * 1000000;
}

unsigned long long dsda_ElapsedTimeMS(int timer) {
  return dsda_ElapsedTime(timer) / 1000;
}

static void dsda_Throttle(int timer, unsigned long long target_time) {
  unsigned long long elapsed_time;
  unsigned long long remaining_time;

  while (1) {
    elapsed_time = dsda_ElapsedTime(timer);

    if (elapsed_time >= target_time) {
      dsda_StartTimer(timer);
      return;
    }

    // Sleeping doesn't have high accuracy
    remaining_time = target_time - elapsed_time;
    if (remaining_time > 1000)
      I_uSleep(remaining_time - 1000);
  }
}

int dsda_fps_limit;

void dsda_LimitFPS(void) {
  extern int movement_smooth;

  if (movement_smooth && dsda_fps_limit) {
    unsigned long long target_time;

    target_time = 1000000 / dsda_fps_limit;

    dsda_Throttle(dsda_timer_fps, target_time);
  }
}

#define TICRATE 35

int dsda_RealticClockRate(void);

static unsigned long long dsda_RealTime(void) {
  static dboolean started = false;

  if (!started)
  {
    started = true;
    dsda_StartTimer(dsda_timer_realtime);
  }

  return dsda_ElapsedTime(dsda_timer_realtime);
}

static unsigned long long dsda_ScaledTime(void) {
  return dsda_RealTime() * dsda_RealticClockRate() / 100;
}

extern int ms_to_next_tick;

// During a fast demo, each call yields a new tick
static int dsda_GetTickFastDemo(void)
{
  static int tick;
  return tick++;
}

int dsda_GetTickRealTime(void) {
  int i;
  unsigned long long t;

  t = dsda_RealTime();

  i = t * TICRATE / 1000000;
  ms_to_next_tick = (i + 1) * 1000 / TICRATE - t / 1000;
  if (ms_to_next_tick > 1000 / TICRATE) ms_to_next_tick = 1;
  if (ms_to_next_tick < 1) ms_to_next_tick = 0;
  return i;
}

static int dsda_TickMS(int n) {
  return n * 1000 * 100 / dsda_RealticClockRate() / TICRATE;
}

static int dsda_GetTickScaledTime(void) {
  int i;
  unsigned long long t;

  t = dsda_RealTime();

  i = t * TICRATE * dsda_RealticClockRate() / 100 / 1000000;
  ms_to_next_tick = dsda_TickMS(i + 1) - t / 1000;
  if (ms_to_next_tick > dsda_TickMS(1)) ms_to_next_tick = 1;
  if (ms_to_next_tick < 1) ms_to_next_tick = 0;
  return i;
}

// During a fast demo, no time elapses in between ticks
static unsigned long long dsda_TickElapsedTimeFastDemo(void) {
  return 0;
}

static unsigned long long dsda_TickElapsedRealTime(void) {
  int tick = dsda_GetTick();

  return dsda_RealTime() - (unsigned long long) tick * 1000000 / TICRATE;
}

static unsigned long long dsda_TickElapsedScaledTime(void) {
  int tick = dsda_GetTick();

  return dsda_ScaledTime() - (unsigned long long) tick * 1000000 / TICRATE;
}

int (*dsda_GetTick)(void) = dsda_GetTickRealTime;
unsigned long long (*dsda_TickElapsedTime)(void) = dsda_TickElapsedRealTime;

void dsda_ResetTimeFunctions(int fastdemo) {
  if (fastdemo) {
    dsda_GetTick = dsda_GetTickFastDemo;
    dsda_TickElapsedTime = dsda_TickElapsedTimeFastDemo;
  }
  else if (dsda_RealticClockRate() != 100) {
    dsda_GetTick = dsda_GetTickScaledTime;
    dsda_TickElapsedTime = dsda_TickElapsedScaledTime;
  }
  else {
    dsda_GetTick = dsda_GetTickRealTime;
    dsda_TickElapsedTime = dsda_TickElapsedRealTime;
  }
}
