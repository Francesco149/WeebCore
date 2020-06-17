/* very early draft of the sprite ed
 * middle click drag to pan
 * left click drag to draw
 * mouse wheel to zoom
 *
 * to avoid updating the img for every new pix, the pixs are batched up and temporarily
 * rendered as individual quads until they are flushed to the real img */

#include "WeebCore.c"

Img MkCheckerImg() {
  Img img;
  int data[4] = { 0xaaaaaa, 0x666666, 0x666666, 0xaaaaaa };
  img = MkImg();
  return Pixs(img, 2, 2, data);
}

int* MkPixs(int fillCol, int width, int height) {
  int i;
  int* data = 0;
  for (i = 0; i < width * height; ++i) {
    ArrCat(&data, fillCol);
  }
  return data;
}

#define IMGSIZE 1024
#define CHECKER_SIZE 64

#define DRAGGING (1<<1)
#define PAINTING (1<<2)
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
  Img checkerImg;
  Mesh mesh;
  Img img;
  int* imgData;
  EDUpd* updates;
  Trans trans;
  Mat bgMat;
}* Editor;

Wnd MkEdWnd() {
  Wnd window = MkWnd();
  SetWndClass(window, "HelloWnd");
  SetWndName(window, "Hello WeebCore");
  return window;
}

void EdFlushUpds(Editor ed) {
  Pixs(ed->img, IMGSIZE, IMGSIZE, ed->imgData);
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

Mat MkEdBgMat() {
  Mat mat = MkMat();
  return Scale1(mat, CHECKER_SIZE);
}

void EdMapToImg(Editor ed, float* point) {
  /* un-trans mouse coordinates so they are relative to the img */
  InvTransPt(ToTmpMatOrtho(ed->trans), point);
  point[0] /= ed->scale;
  point[1] /= ed->scale;
}

void EdUpdTrans(Editor ed) {
  Trans trans = ed->trans;
  ClrTrans(trans);
  SetPos(trans, ed->oX, ed->oY);
  SetScale1(trans, ed->scale);
}

Editor MkEd() {
  Editor ed = Alloc(sizeof(struct _Editor));
  ed->scale = 4;
  ed->oX = 100;
  ed->oY = 100;
  ed->window = MkEdWnd();
  ed->checkerImg = MkCheckerImg();
  ed->img = MkImg();
  ed->imgData = MkPixs(0xffffff, IMGSIZE, IMGSIZE);
  ed->trans = MkTrans();
  ed->bgMat = MkEdBgMat();

  ed->mesh = MkMesh();
  Quad(ed->mesh, 0, 0, IMGSIZE, IMGSIZE);

  EdFlushUpds(ed);
  EdUpdBMesh(ed);
  EdUpdTrans(ed);

  return ed;
}

void RmEd(Editor ed) {
  if (ed) {
    RmWnd(ed->window);
    RmMesh(ed->mesh);
    RmMesh(ed->bgMesh);
    RmArr(ed->imgData);
    RmArr(ed->updates);
    RmImg(ed->img);
    RmImg(ed->checkerImg);
    RmTrans(ed->trans);
    RmMat(ed->bgMat);
  }
  Free(ed);
}

void EdPaintPix(Editor ed) {
  int x, y;
  float point[2];
  point[0] = MouseX(ed->window);
  point[1] = MouseY(ed->window);
  EdMapToImg(ed, point);
  x = (int)point[0];
  y = (int)point[1];
  if (x >= 0 && x < IMGSIZE && y >= 0 && y < IMGSIZE) {
    int* px = &ed->imgData[y * IMGSIZE + x];
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
  EdMapToImg(ed, p);
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
      ed->flags |= PAINTING;
      EdPaintPix(ed);
      EdFlushUpds(ed);
      break;
    }
  }
}

void EdHandleKeyUp(Editor ed, int key) {
  switch (key) {
    case MMID: { ed->flags &= ~DRAGGING; break; }
    case MLEFT: { ed->flags &= ~PAINTING; break; }
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
      if (flags & PAINTING) {
        EdPaintPix(ed);
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
  PutMesh(mesh, ToTmpMat(ed->trans), 0);
  RmMesh(mesh);
}

void EdPut(Editor ed) {
  PutMesh(ed->bgMesh, ed->bgMat, ed->checkerImg);
  PutMesh(ed->mesh, ToTmpMat(ed->trans), ed->img);
  EdPutUpds(ed);
  SwpBufs(ed->window);
}

int main() {
  Editor ed = MkEd();
  while (1) {
    while (NextMsg(ed->window)) {
      if (!EdHandleMsg(ed)) {
        RmEd(ed);
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
