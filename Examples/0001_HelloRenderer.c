#include "WeebCore.c"

float rot;
Mesh mesh;
Trans trans;

void Init() {
  trans = MkTrans();
  mesh = MkMesh();
  Col(mesh, 0xffffff);
  Quad(mesh, -100, -50, 200, 100);
  Col(mesh, 0xb26633);
  Quad(mesh, -90, -40, 180, 80);
}

void Quit() {
  RmMesh(mesh);
  RmTrans(trans);
}

void Frame() {
  /* just a random time based animation */
  rot = FltMod(rot + Delta() * 360, 360*2);
  SetRot(trans, rot);
  SetPos(trans, 320 + (int)(200 * Sin(rot / 2)), 240);
  SetScale1(trans, 0.25f + (1 + Sin(rot)) / 3);
  PutMesh(mesh, ToTmpMat(trans), 0);
}

void AppInit() {
  SetAppName("Hello WeebCore");
  SetAppClass("HelloWnd");
  On(INIT, Init);
  On(FRAME, Frame);
  On(QUIT, Quit);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
