#include "WeebCore.c"

int main(int argc, char* argv[]) {
  GMesh mesh;
  OSWindow window = osCreateWindow();
  GFont font = gDefaultFont();
  GTransform transform = gCreateTransform();
  osSetWindowClass(window, "HelloWindow");
  osSetWindowName(window, "Hello Font");

  gTranslate(transform, 0, 120);
  gScale1(transform, 2);
  mesh = gCreateMesh();
  gColor(mesh, 0xbebebe);
  gFont(mesh, font, 10, 10, "Hello, this is a bitmap font!\nWow! 123450123ABC\n\n"
    "  Illegal1 = O0;\n"
    "  int* p = &Illegal1;\n\n"
    "  int main(int argc, char* argv[]) {\n"
    "    puts(\"Hello World!\");\n"
    "  }");

  while (1) {
    while (osNextMessage(window)) {
      switch (osMessageType(window)) {
        case OS_QUIT: {
          gDestroyMesh(mesh);
          gDestroyFont(font);
          gDestroyTransform(transform);
          osDestroyWindow(window);
          return 0;
        }
      }
    }

    gDrawMesh(mesh, 0, gFontTexture(font));
    gDrawMesh(mesh, transform, gFontTexture(font));
    gSwapBuffers(window);
  }

  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
