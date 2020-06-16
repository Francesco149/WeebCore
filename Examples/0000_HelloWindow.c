#include "WeebCore.c"
#include <math.h>

int main() {
  Wnd wnd = MkWnd();
  SetWndClass(wnd, "HelloWnd");
  SetWndName(wnd, "Hello WeebCore");
  while (1) {
    while (NextMsg(wnd)) {
      switch (MsgType(wnd)) {
        case QUIT: {
          RmWnd(wnd);
          return 0;
        }
      }
    }
    SwpBufs(wnd);
  }
  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
