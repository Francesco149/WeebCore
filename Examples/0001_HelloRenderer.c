#include "WeebCore.c"

int main() {
  float rotate = 0;
  GMesh mesh;
  GTransformBuilder transform = gCreateTransformBuilder();
  OSWindow window = osCreateWindow();
  osSetWindowClass(window, "HelloWindow");
  osSetWindowName(window, "Hello WeebCore");

  mesh = gCreateMesh();
  gColor(mesh, 0xffffff);
  gQuad(mesh, -100, -50, 200, 100);
  gColor(mesh, 0xb26633);
  gQuad(mesh, -90, -40, 180, 80);

  while (1) {
    while (osNextMessage(window)) {
      switch (osMessageType(window)) {
        case OS_QUIT: {
          osDestroyWindow(window);
          gDestroyMesh(mesh);
          gDestroyTransformBuilder(transform);
          return 0;
        }
      }
    }

    /* just a random time based animation */
    rotate = maFloatMod(rotate + osDeltaTime(window) * 360, 360*2);
    gSetRotation(transform, rotate);
    gSetPosition(transform, 320 + (int)(200 * maSin(rotate / 2)), 240);
    gSetScale1(transform, 0.25f + (1 + maSin(rotate)) / 3);

    gDrawMesh(mesh, gBuildTempTransform(transform), 0);
    gSwapBuffers(window);
  }

  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
