#include "WeebCore.c"

int main() {
  float rot = 0;
  Mesh mesh;
  Trans trans = MkTrans();
  Wnd wnd = MkWnd();
  SetWndClass(wnd, "HelloWnd");
  SetWndName(wnd, "Hello WeebCore");

  mesh = MkMesh();
  Col(mesh, 0xffffff);
  Quad(mesh, -100, -50, 200, 100);
  Col(mesh, 0xb26633);
  Quad(mesh, -90, -40, 180, 80);

  while (1) {
    while (NextMsg(wnd)) {
      switch (MsgType(wnd)) {
        case QUIT: {
          RmWnd(wnd);
          RmMesh(mesh);
          RmTrans(trans);
          return 0;
        }
      }
    }

    /* just a random time based animation */
    rot = FltMod(rot + Delta(wnd) * 360, 360*2);
    SetRot(trans, rot);
    SetPos(trans, 320 + (int)(200 * Sin(rot / 2)), 240);
    SetScale1(trans, 0.25f + (1 + Sin(rot)) / 3);

    PutMesh(mesh, ToTmpMat(trans), 0);
    SwpBufs(wnd);
  }

  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
