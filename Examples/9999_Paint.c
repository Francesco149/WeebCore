/* very early draft of the sprite ed
 * middle click drag to pan
 * left click drag to draw
 * mouse wheel to zoom
 *
 * to avoid updating the tex for every new pix, the pixs are batched up and temporarily
 * rendered as individual quads until they are flushed to the real tex */

#include "WeebCore.c"

Tex MkCheckerTex() {
  Tex tex;
  int data[4] = { 0xaaaaaa, 0x666666, 0x666666, 0xaaaaaa };
  tex = MkTex();
  Pixs(tex, 2, 2, data);
  return tex;
}

int* MkPixs(int fillCol, int width, int height) {
  int i;
  int* data = 0;
  for (i = 0; i < width * height; ++i) {
    ArrCat(&data, fillCol);
  }
  return data;
}

#define TEXSIZE 1024
#define CHECKER_SIZE 64

#define DRAGGING (1<<1)
#define DRAWING (1<<2)
#define DIRTY (1<<3)

typedef struct _EDUpd {
  int color;
  int x, y;
} EDUpd;

typedef struct _Editor {
  Wnd window;
  int flags;
  float oX, oY;
  float flushTimer;
  int scale;
  int color;
  Mesh bgMesh;
  Tex checkerTex;
  Mesh mesh;
  Tex tex;
  int* texData;
  EDUpd* updates;
  Trans trans;
  Mat mat, matOrtho, bgMat;
}* Editor;

Wnd EdMkWnd() {
  Wnd window = MkWnd();
  SetWndClass(window, "HelloWnd");
  SetWndName(window, "Hello WeebCore");
  return window;
}

void EdFlushUpds(Editor ed) {
  Pixs(ed->tex, TEXSIZE, TEXSIZE, ed->texData);
  SetArrLen(ed->updates, 0);
  ed->flags &= ~DIRTY;
}

void EdUpdBMesh(Editor ed) {
  RmMesh(ed->bgMesh);
  ed->bgMesh = MkMesh();
  Quad(ed->bgMesh, 0, 0,
    Ceil(WndWidth(ed->window)  / (float)CHECKER_SIZE),
    Ceil(WndHeight(ed->window) / (float)CHECKER_SIZE)
  );
}

Mat EdMkBgMat() {
  Mat mat = MkMat();
  return Scale1(mat, CHECKER_SIZE);
}

void EdMapToTex(Editor ed, float* point) {
  /* un-trans mouse coordinates so they are relative to the tex */
  InvTransPt(ed->matOrtho, point);
  point[0] /= ed->scale;
  point[1] /= ed->scale;
}

void EdUpdTrans(Editor ed) {
  Trans trans = ed->trans;
  ClrTrans(trans);
  SetPos(trans, ed->oX, ed->oY);
  SetScale1(trans, ed->scale);
  ed->mat = ToTmpMat(trans);
  ed->matOrtho = ToTmpMatOrtho(trans);
}

Editor EdMk() {
  Editor ed = Alloc(sizeof(struct _Editor));
  ed->scale = 4;
  ed->oX = 100;
  ed->oY = 100;
  ed->window = EdMkWnd();
  ed->checkerTex = MkCheckerTex();
  ed->tex = MkTex();
  ed->texData = MkPixs(0xffffff, TEXSIZE, TEXSIZE);
  ed->trans = MkTrans();
  ed->bgMat = EdMkBgMat();

  ed->mesh = MkMesh();
  Quad(ed->mesh, 0, 0, TEXSIZE, TEXSIZE);

  EdFlushUpds(ed);
  EdUpdBMesh(ed);
  EdUpdTrans(ed);

  return ed;
}

void EdRm(Editor ed) {
  if (ed) {
    RmWnd(ed->window);
    RmMesh(ed->mesh);
    RmMesh(ed->bgMesh);
    RmArr(ed->texData);
    RmArr(ed->updates);
    RmTex(ed->tex);
    RmTex(ed->checkerTex);
    RmTrans(ed->trans);
    RmMat(ed->bgMat);
  }
  Free(ed);
}

void EdPutPix(Editor ed) {
  int x, y;
  float point[2];
  point[0] = MouseX(ed->window);
  point[1] = MouseY(ed->window);
  EdMapToTex(ed, point);
  x = (int)point[0];
  y = (int)point[1];
  if (x >= 0 && x < TEXSIZE && y >= 0 && y < TEXSIZE) {
    int* px = &ed->texData[y * TEXSIZE + x];
    if (*px != ed->color) {
      EDUpd* u;
      *px = ed->color;
      u = ArrAlloc(&ed->updates, 1);
      u->x = x;
      u->y = y;
      u->color = ed->color;
      ed->flags |= DIRTY;
    }
  }
}

void EdChangeScale(Editor ed, int direction) {
  float p[2];
  float newScale = ed->scale + direction * (ed->scale / 8 + 1);
  newScale = Min(32, newScale);
  newScale = Max(1, newScale);
  p[0] = MouseX(ed->window);
  p[1] = MouseY(ed->window);
  EdMapToTex(ed, p);
  /* adjust panning so the pix we're pointing stays under the cursor */
  ed->oX -= (int)((p[0] * newScale) - (p[0] * ed->scale) + 0.5f);
  ed->oY -= (int)((p[1] * newScale) - (p[1] * ed->scale) + 0.5f);
  ed->scale = newScale;
  EdUpdTrans(ed);
}

void EdHandleKeyDown(Editor ed, int key) {
  switch (key) {
    case MMID: { ed->flags |= DRAGGING; break; }
    case MWHEELUP: { EdChangeScale(ed, 1); break; }
    case MWHEELDOWN: { EdChangeScale(ed, -1); break; }
    case MLEFT: {
      ed->flags |= DRAWING;
      EdPutPix(ed);
      EdFlushUpds(ed);
      break;
    }
  }
}

void EdHandleKeyUp(Editor ed, int key) {
  switch (key) {
    case MMID: { ed->flags &= ~DRAGGING; break; }
    case MLEFT: { ed->flags &= ~DRAWING; break; }
  }
}

int EdHandleMsg(Editor ed) {
  Wnd window = ed->window;
  switch (MsgType(window)) {
    case SIZE: { EdUpdBMesh(ed); break; }
    case KEYDOWN: { EdHandleKeyDown(ed, Key(window)); break; }
    case KEYUP: { EdHandleKeyUp(ed, Key(window)); break; }
    case MOTION: {
      int flags = ed->flags;
      if (flags & DRAGGING) {
        ed->oX += MouseDX(window);
        ed->oY += MouseDY(window);
        EdUpdTrans(ed);
      }
      if (flags & DRAWING) {
        EdPutPix(ed);
      }
      break;
    }
    case QUIT: { return 0; }
  }
  return 1;
}

void EdUpd(Editor ed) {
  Wnd window = ed->window;
  ed->flushTimer += Delta(window);
  if ((ed->flags & DIRTY) && ed->flushTimer > 1.0f) {
    EdFlushUpds(ed);
    ed->flushTimer -= 1.0f;
  }
}

void EdPutUpds(Editor ed) {
  int i;
  Mesh mesh = MkMesh();
  EDUpd* updates = ed->updates;
  for (i = 0; i < ArrLen(updates); ++i) {
    EDUpd* u = &updates[i];
    Col(mesh, u->color);
    Quad(mesh, u->x, u->y, 1, 1);
  }
  PutMesh(mesh, ed->mat, 0);
  RmMesh(mesh);
}

void EdPut(Editor ed) {
  PutMesh(ed->bgMesh, ed->bgMat, ed->checkerTex);
  PutMesh(ed->mesh, ed->mat, ed->tex);
  EdPutUpds(ed);
  SwpBufs(ed->window);
}

int main() {
  Editor ed = EdMk();
  while (1) {
    while (NextMsg(ed->window)) {
      if (!EdHandleMsg(ed)) {
        EdRm(ed);
        return 0;
      }
    }
    EdUpd(ed);
    EdPut(ed);
  }
  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
