/* very early draft of the sprite editor
 * middle click drag to pan
 * left click drag to draw
 * mouse wheel to zoom
 *
 * to avoid updating the texture for every new pixel, the pixels are batched up and temporarily
 * rendered as individual quads until they are flushed to the real texture */

#include "WeebCore.c"

GTexture CreateCheckerTexture() {
  GTexture texture;
  int data[4] = { 0xaaaaaa, 0x666666, 0x666666, 0xaaaaaa };
  texture = gCreateTexture();
  gPixels(texture, 2, 2, data);
  return texture;
}

int* CreatePixels(int fillColor, int width, int height) {
  int i;
  int* data = 0;
  for (i = 0; i < width * height; ++i) {
    wbArrayAppend(&data, fillColor);
  }
  return data;
}

#define TEXSIZE 1024
#define CHECKER_SIZE 64

#define DRAGGING (1<<1)
#define DRAWING (1<<2)
#define DIRTY (1<<3)

typedef struct _EDUpdate {
  int color;
  int x, y;
} EDUpdate;

typedef struct _Editor {
  OSWindow window;
  int flags;
  float oX, oY;
  float flushTimer;
  int scale;
  int color;
  GMesh backgroundMesh;
  GTexture checkerTexture;
  GMesh mesh;
  GTexture texture;
  int* textureData;
  EDUpdate* updates;
  GTransformBuilder transformBuilder;
  GTransform transform, transformOrtho, backgroundTransform;
}* Editor;

OSWindow edCreateWindow() {
  OSWindow window = osCreateWindow();
  osSetWindowClass(window, "HelloWindow");
  osSetWindowName(window, "Hello WeebCore");
  return window;
}

void edFlushUpdates(Editor editor) {
  gPixels(editor->texture, TEXSIZE, TEXSIZE, editor->textureData);
  wbSetArrayLen(editor->updates, 0);
  editor->flags &= ~DIRTY;
}

void edUpdateBackgroundMesh(Editor editor) {
  gDestroyMesh(editor->backgroundMesh);
  editor->backgroundMesh = gCreateMesh();
  gQuad(editor->backgroundMesh, 0, 0,
    maCeil(osWindowWidth(editor->window)  / (float)CHECKER_SIZE),
    maCeil(osWindowHeight(editor->window) / (float)CHECKER_SIZE)
  );
}

GTransform edCreateBackgroundTransform() {
  GTransform transform = gCreateTransform();
  gScale1(transform, CHECKER_SIZE);
  return transform;
}

void edMapToTexture(Editor editor, float* point) {
  /* un-transform mouse coordinates so they are relative to the texture */
  gInverseTransformPoint(editor->transformOrtho, point);
  point[0] /= editor->scale;
  point[1] /= editor->scale;
}

void edUpdateTransform(Editor editor) {
  GTransformBuilder tbuilder = editor->transformBuilder;
  gResetTransform(tbuilder);
  gSetPosition(tbuilder, editor->oX, editor->oY);
  gSetScale1(tbuilder, editor->scale);
  editor->transform = gBuildTempTransform(tbuilder);
  editor->transformOrtho = gBuildTempTransformOrtho(tbuilder);
}

Editor edCreate() {
  Editor editor = osAlloc(sizeof(struct _Editor));
  editor->scale = 4;
  editor->oX = 100;
  editor->oY = 100;
  editor->window = edCreateWindow();
  editor->checkerTexture = CreateCheckerTexture();
  editor->texture = gCreateTexture();
  editor->textureData = CreatePixels(0xffffff, TEXSIZE, TEXSIZE);
  editor->transformBuilder = gCreateTransformBuilder();
  editor->backgroundTransform = edCreateBackgroundTransform();

  editor->mesh = gCreateMesh();
  gQuad(editor->mesh, 0, 0, TEXSIZE, TEXSIZE);

  edFlushUpdates(editor);
  edUpdateBackgroundMesh(editor);
  edUpdateTransform(editor);

  return editor;
}

void edDestroy(Editor editor) {
  if (editor) {
    osDestroyWindow(editor->window);
    gDestroyMesh(editor->mesh);
    gDestroyMesh(editor->backgroundMesh);
    wbDestroyArray(editor->textureData);
    wbDestroyArray(editor->updates);
    gDestroyTexture(editor->texture);
    gDestroyTexture(editor->checkerTexture);
    gDestroyTransformBuilder(editor->transformBuilder);
    gDestroyTransform(editor->backgroundTransform);
  }
  osFree(editor);
}

void edDrawPixel(Editor editor) {
  int x, y;
  float point[2];
  point[0] = osMouseX(editor->window);
  point[1] = osMouseY(editor->window);
  edMapToTexture(editor, point);
  x = (int)point[0];
  y = (int)point[1];
  if (x >= 0 && x < TEXSIZE && y >= 0 && y < TEXSIZE) {
    int* px = &editor->textureData[y * TEXSIZE + x];
    if (*px != editor->color) {
      EDUpdate* u;
      *px = editor->color;
      u = wbArrayAlloc(&editor->updates, 1);
      u->x = x;
      u->y = y;
      u->color = editor->color;
      editor->flags |= DIRTY;
    }
  }
}

void edChangeScale(Editor editor, int direction) {
  float p[2];
  float newScale = editor->scale + direction * (editor->scale / 8 + 1);
  newScale = wbMin(32, newScale);
  newScale = wbMax(1, newScale);
  p[0] = osMouseX(editor->window);
  p[1] = osMouseY(editor->window);
  edMapToTexture(editor, p);
  /* adjust panning so the pixel we're pointing stays under the cursor */
  editor->oX -= (int)((p[0] * newScale) - (p[0] * editor->scale) + 0.5f);
  editor->oY -= (int)((p[1] * newScale) - (p[1] * editor->scale) + 0.5f);
  editor->scale = newScale;
  edUpdateTransform(editor);
}

void edHandleKeyDown(Editor editor, int key) {
  switch (key) {
    case OS_MMID: { editor->flags |= DRAGGING; break; }
    case OS_MWHEELUP: { edChangeScale(editor, 1); break; }
    case OS_MWHEELDOWN: { edChangeScale(editor, -1); break; }
    case OS_MLEFT: {
      editor->flags |= DRAWING;
      edDrawPixel(editor);
      edFlushUpdates(editor);
      break;
    }
  }
}

void edHandleKeyUp(Editor editor, int key) {
  switch (key) {
    case OS_MMID: { editor->flags &= ~DRAGGING; break; }
    case OS_MLEFT: { editor->flags &= ~DRAWING; break; }
  }
}

int edHandleMessage(Editor editor) {
  OSWindow window = editor->window;
  switch (osMessageType(window)) {
    case OS_SIZE: { edUpdateBackgroundMesh(editor); break; }
    case OS_KEYDOWN: { edHandleKeyDown(editor, osKey(window)); break; }
    case OS_KEYUP: { edHandleKeyUp(editor, osKey(window)); break; }
    case OS_MOTION: {
      int flags = editor->flags;
      if (flags & DRAGGING) {
        editor->oX += osMouseDX(window);
        editor->oY += osMouseDY(window);
        edUpdateTransform(editor);
      }
      if (flags & DRAWING) {
        edDrawPixel(editor);
      }
      break;
    }
    case OS_QUIT: { return 0; }
  }
  return 1;
}

void edUpdate(Editor editor) {
  OSWindow window = editor->window;
  editor->flushTimer += osDeltaTime(window);
  if ((editor->flags & DIRTY) && editor->flushTimer > 1.0f) {
    edFlushUpdates(editor);
    editor->flushTimer -= 1.0f;
  }
}

void edDrawUpdates(Editor editor) {
  int i;
  GMesh mesh = gCreateMesh();
  EDUpdate* updates = editor->updates;
  for (i = 0; i < wbArrayLen(updates); ++i) {
    EDUpdate* u = &updates[i];
    gColor(mesh, u->color);
    gQuad(mesh, u->x, u->y, 1, 1);
  }
  gDrawMesh(mesh, editor->transform, 0);
  gDestroyMesh(mesh);
}

void edDraw(Editor editor) {
  gDrawMesh(editor->backgroundMesh, editor->backgroundTransform, editor->checkerTexture);
  gDrawMesh(editor->mesh, editor->transform, editor->texture);
  edDrawUpdates(editor);
  gSwapBuffers(editor->window);
}

int main() {
  Editor editor = edCreate();
  while (1) {
    while (osNextMessage(editor->window)) {
      if (!edHandleMessage(editor)) {
        edDestroy(editor);
        return 0;
      }
    }
    edUpdate(editor);
    edDraw(editor);
  }
  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
