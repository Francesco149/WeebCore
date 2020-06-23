#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"

Mesh mesh;

void Init() {
  mesh = MkMesh();
  Col(mesh, 0xff3333);
  Quad(mesh, 10, 10, 200, 100);
}

void Frame() {
  PutMesh(mesh, 0, 0);
}

void AppInit() {
  On(INIT, Init);
  On(FRAME, Frame);
}
