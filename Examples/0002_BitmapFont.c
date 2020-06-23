#include "WeebCore.c"

Mesh mesh;
Trans trans;
Ft ft;

void Init() {
  ft = DefFt();

  trans = MkTrans();
  SetPos(trans, 0, 120);
  SetScale1(trans, 2);

  mesh = MkMesh();
  Col(mesh, 0xbebebe);
  FtMesh(mesh, ft, 10, 10,
    "Hello, this is a bitmap ft!\nWow! 123450123ABC\n\n"
    "  Illegal1 = O0;\n"
    "  int* p = &Illegal1;\n\n"
    "  int main(int argc, char* argv[]) {\n"
    "    puts(\"Hello World!\");\n"
    "  }");
}

void Quit() {
  RmMesh(mesh);
  RmFt(ft);
  RmTrans(trans);
}

void Frame() {
  PutMeshRaw(mesh, 0, FtImg(ft));
  PutMeshRaw(mesh, ToTmpMat(trans), FtImg(ft));
}

void AppInit() {
  SetAppClass("HelloWnd");
  SetAppName("Hello Font");
  On(INIT, Init);
  On(QUIT, Quit);
  On(FRAME, Frame);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
