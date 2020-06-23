#include "WeebCore.c"

void AppInit() {
  SetAppName("Hello WeebCore");
  SetAppClass("HelloWnd");
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
