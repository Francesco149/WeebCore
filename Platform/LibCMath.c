#include <math.h>

float maFloatMod(float x, float y) {
  return (float)fmod(x, y);
}

float maSin(float degrees) {
  return (float)sin(wbToRadians(degrees));
}

float maCos(float degrees) {
  return (float)cos(wbToRadians(degrees));
}

int maCeil(float x) {
  return (int)ceil(x);
}

int maFloor(float x) {
  return (int)floor(x);
}

float maSqrt(float x) {
  return (float)sqrt(x);
}
