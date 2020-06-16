#include "WeebCore.c"

Trans MkBigTextTrans() {
  Trans trans = MkTrans();
  SetPos(trans, 0, 120);
  SetScale1(trans, 2);
  return trans;
}

int main(int argc, char* argv[]) {
  Mesh mesh;
  Wnd wnd = MkWnd();
  Ft ft = DefFt();
  Trans trans = MkBigTextTrans();
  SetWndClass(wnd, "HelloWnd");
  SetWndName(wnd, "Hello Font");

  mesh = MkMesh();
  Col(mesh, 0xbebebe);
  FtMesh(mesh, ft, 10, 10,
    "Hello, this is a bitmap ft!\nWow! 123450123ABC\n\n"
    "  Illegal1 = O0;\n"
    "  int* p = &Illegal1;\n\n"
    "  int main(int argc, char* argv[]) {\n"
    "    puts(\"Hello World!\");\n"
    "  }");

  while (1) {
    while (NextMsg(wnd)) {
      switch (MsgType(wnd)) {
        case QUIT: {
          RmMesh(mesh);
          RmFt(ft);
          RmTrans(trans);
          RmWnd(wnd);
          return 0;
        }
      }
    }

    PutMesh(mesh, 0, FtTex(ft));
    PutMesh(mesh, ToTmpMat(trans), FtTex(ft));
    SwpBufs(wnd);
  }

  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
