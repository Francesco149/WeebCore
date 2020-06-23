#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

struct _OsTime { struct timespec t; };

float FltMod(float x, float y) { return (float)fmod(x, y); }
float Sin(float degrees) { return (float)sin(ToRad(degrees)); }
float Cos(float degrees) { return (float)cos(ToRad(degrees)); }
int Ceil(float x) { return (int)ceil(x); }
int Floor(float x) { return (int)floor(x); }
float Sqrt(float x) { return (float)sqrt(x); }

static void Die(char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vprintf(fmt, va);
  va_end(va);
  putchar('\n');
  exit(1);
}

void* Alloc(int n) {
  void* p = malloc(n);
  MemSet(p, 0, n);
  return p;
}

void* Realloc(void* p, int n) {
  return realloc(p, n);
}

void Free(void* p) {
  free(p);
}

void MemSet(void* p, unsigned char val, int n) {
  memset(p, val, n);
}

void MemCpy(void* dst, void* src, int n) {
  memcpy(dst, src, n);
}

void MemMv(void* dst, void* src, int n) {
  memmove(dst, src, n);
}

int WrFile(char* path, void* data, int dataLen) {
  FILE* f = fopen(path, "wb");
  int res;
  if (!f) {
    return -1;
  }
  res = (int)fwrite(data, 1, dataLen, f);
  fclose(f);
  return res;
}

int RdFile(char* path, void* data, int maxSize) {
  FILE* f = fopen(path, "rb");
  int res;
  if (!f) {
    return -1;
  }
  res = (int)fread(data, 1, maxSize, f);
  fclose(f);
  return res;
}

static OsTime MkOsTime() {
  return Alloc(sizeof(struct _OsTime));
}

static void RmOsTime(OsTime t) {
  Free(t);
}

static void OsTimeCpy(OsTime dst, OsTime src) {
  MemCpy(&dst->t, &src->t, sizeof(dst->t));
}

static void OsTimeNow(OsTime dst) {
  if (clock_gettime(CLOCK_MONOTONIC, &dst->t) == -1) {
    Die("clock_gettime failed: %s\n", strerror(errno));
  }
}

static void TSpecDelta(struct timespec* delta, struct timespec* a, struct timespec* b) {
  if (a->tv_nsec > b->tv_nsec) {
    delta->tv_sec = b->tv_sec - a->tv_sec - 1;
    delta->tv_nsec = 1000000000 + b->tv_nsec - a->tv_nsec;
  } else {
    delta->tv_sec = b->tv_sec - a->tv_sec;
    delta->tv_nsec = b->tv_nsec - a->tv_nsec;
  }
}

static float OsTimeDelta(OsTime before, OsTime now) {
  struct timespec delta;
  TSpecDelta(&delta, &before->t, &now->t);
  return delta.tv_sec + delta.tv_nsec * 1e-9f;
}
