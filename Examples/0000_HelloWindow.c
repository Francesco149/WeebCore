#include "WeebCore.c"
#include <math.h>

int main() {
  OSWindow window = osCreateWindow();
  osSetWindowClass(window, "HelloWindow");
  osSetWindowName(window, "Hello WeebCore");
  while (1) {
    while (osNextMessage(window)) {
      switch (osMessageType(window)) {
        case OS_QUIT: {
          osDestroyWindow(window);
          return 0;
        }
      }
    }
    gSwapBuffers(window);
  }

  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
