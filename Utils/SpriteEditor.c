/* very rough sprite ed, will improve/rewrite as I move features into the engine. this is mainly
 * to bootstrap making fonts and simple sprites so I can work on text rendering and ui entirely
 * off my own tools */

#include "WeebCore.c"

char* EdHelpString() {
  return (
    "Controls:\n"
    "\n"
    "F1           toggle this help text\n"
    "Ctrl+S       save\n"
    "Ctrl+1       encode as 1bpp and save as .txt file containing a b64 string of the data.\n"
    "             if a selection is active, the output is cropped to the selection\n"
    "Middle Drag  pan around\n"
    "Wheel Up     zoom in\n"
    "Wheel Down   zoom out\n"
    "E            switch to pencil\n"
    "Left Click   (pencil) draw with col 1\n"
    "Right Click  (pencil) draw with col 2\n"
    "C            switch to col picker. press again to expand/collapse. expanded mode lets\n"
    "             you adjust cols with the ui, non-expanded picks from the pixels you click\n"
    "Left Click   (col picker) set col 1\n"
    "Right Click  (col picker) set col 2\n"
    "S            switch to selection mode\n"
    "Left Drag    (selection) select rectangle\n"
    "R            crop current selection to closest power-of-two size\n"
    "Shift+S      remove selection\n"
    "1            fill contents of selection with col 1\n"
    "2            fill contents of selection with col 2\n"
    "D            cut the contents of the selection. this will attach them to the selection\n"
    "Shift+D      undo cut. puts the contents of the cut back where they were\n"
    "T            (after cutting) paste the contents of the selection at the current location\n"
  );
}

#define ED_FLUSH_TIME 1.0f
#define ED_FLUSH_THRESHOLD 4096
#define ED_TEXSIZE 1024
#define ED_CHECKER_SIZE 8

#define ED_FDRAGGING (1<<1)
#define ED_FDRAWING (1<<2)
#define ED_FDIRTY (1<<3)
#define ED_FEXPAND_COLOR_PICKER (1<<4)
#define ED_FSELECT (1<<5)
#define ED_FALPHA_BLEND (1<<6)
#define ED_FIGNORE_SELECTION (1<<7)
#define ED_FHELP (1<<8)

enum {
  ED_PENCIL,
  ED_COLOR_PICKER,
  ED_SELECT,
  ED_LAST_TOOL
};

enum {
  ED_SELECT_NONE,
  ED_SELECT_MID,
  ED_SELECT_LEFT, /* TODO: resize selections */
  ED_SELECT_RIGHT,
  ED_SELECT_BOTTOM,
  ED_SELECT_TOP
};

typedef struct _EDUpd {
  int col;
  int x, y;
  int flags;
} EDUpd;

typedef struct _Ed {
  Wnd wnd;
  int flags;
  float oX, oY;
  float flushTimer; /* TODO: built in timer events */
  int scale;
  int cols[2];
  int colIndex;
  Tex checkerTex;
  Mesh mesh;
  Tex texture;
  int* textureData;
  EDUpd* updates;
  Trans trans;
  Mat mat, matOrtho;
  int tool;
  int width, height;
  int lastPutX, lastPutY;

  Ft font;
  Mesh helpTextMesh, helpBgMesh;

  float selBlinkTimer;
  Tex selBorderTex;
  float selRect[4];          /* raw rectangle between the cursor and the initial drag position */
  float effectiveSelRect[4]; /* normalized, snapped, etc selection rect */
  int selAnchor;

  Tex cutTex;
  int* cutData;
  float cutPotRect[4]; /* power of two size of the cut texture */
  Mesh cutMesh;
  Mat cutMat;
  Trans cutTrans;
  float cutSourceRect[4]; /* the location from where the cut data was taken */

  /* TODO: pull out col picker as a reusable widget */
  Mesh colPickerMesh;
  Trans colPickerTrans;
  Mat colPickerMat;
  int colPickerSize;
  int grey;
  float colX, colY, greyY, alphaY;
  char* filePath;
}* Ed;

void EdMapToTex(Ed ed, float* point);
void EdFlushUpds(Ed ed);
void EdClrSelection(Ed ed);
void EdUpdMesh(Ed ed);
void EdClrPanning(Ed ed);
void EdSelectedRect(Ed ed, float* rect);
void EdClrPutRateLimiter(Ed ed);
int EdUpdPutRateLimiter(Ed ed);
int EdSampleCheckerboard(float x, float y);
void EdPutPix(Ed ed, int x, int y, int col, int flags);

/* ---------------------------------------------------------------------------------------------- */

/* TODO: do not hardcode col picker coordinates this is so ugly */

#define CPICK_WIDTH 1.0f
#define CPICK_HEIGHT 1.0f
#define CPICK_MARGIN (1/32.0f)
#define CPICK_BAR_WIDTH (1/16.0f)
#define CPICK_BBAR_LEFT (CPICK_WIDTH + CPICK_MARGIN)
#define CPICK_BBAR_RIGHT (CPICK_BBAR_LEFT + CPICK_BAR_WIDTH)
#define CPICK_ABAR_LEFT (CPICK_BBAR_LEFT + CPICK_BAR_WIDTH + CPICK_MARGIN)
#define CPICK_ABAR_RIGHT (CPICK_ABAR_LEFT + CPICK_BAR_WIDTH)

int EdColPickerSize(Ed ed) {
  Wnd wnd = ed->wnd;
  int winSize = Min(WndWidth(wnd), WndHeight(wnd));
  int size = RoundUpToPowerOfTwo(winSize / 4);
  size = Max(size, 256);
  size = Min(size, winSize * 0.7);
  return size;
}

Mesh EdCreateColPickerMesh(int grey) {
  Mesh mesh = MkMesh();
  int cols[7] = { 0xff0000, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xff0000 };
  int brightness[3] = { 0xffffff, 0xff000000, 0x000000 };
  int brightnessSelect[2] = { 0xffffff, 0x000000 };
  int alphaSelect[2] = { 0x00ffffff, 0xff000000 };
  brightness[0] = grey;
  QuadGradH(mesh, 0, 0, CPICK_WIDTH, CPICK_HEIGHT, 7, cols);
  QuadGradV(mesh, 0, 0, CPICK_WIDTH, CPICK_HEIGHT, 3, brightness);
  QuadGradV(mesh, CPICK_BBAR_LEFT, 0, CPICK_BAR_WIDTH, CPICK_HEIGHT, 2, brightnessSelect);
  QuadGradV(mesh, CPICK_ABAR_LEFT, 0, CPICK_BAR_WIDTH, CPICK_HEIGHT, 2, alphaSelect);
  return mesh;
}

void EdPutColPicker(Ed ed) {
  float colsY = 0;
  Mesh mesh = MkMesh();
  if (ed->flags & ED_FEXPAND_COLOR_PICKER) {
    colsY = CPICK_HEIGHT + CPICK_MARGIN;
    Col(mesh, Mix(ed->grey, 0xff0000, 0.5));
    Quad(mesh, CPICK_BBAR_LEFT, ed->greyY - 1/128.0f, 1/16.0f, 1/64.0f);
    Col(mesh, 0x000000);
    Quad(mesh, CPICK_ABAR_LEFT, ed->alphaY - 1/128.0f, 1/16.0f, 1/64.0f);
    Col(mesh, ~ed->cols[ed->colIndex] & 0xffffff);
    Quad(mesh, ed->colX - 1/128.0f, ed->colY - 1/128.0f, 1/64.0f, 1/64.0f);
    Col(mesh, 0x000000);
    Quad(mesh, ed->colX - 1/256.0f, ed->colY - 1/256.0f, 1/128.0f, 1/128.0f);
    PutMesh(ed->colPickerMesh, ed->colPickerMat, 0);
  }
  Col(mesh, ed->cols[0]);
  Quad(mesh, 0, colsY         , 1/8.0f, 1/8.0f);
  Col(mesh, ed->cols[1]);
  Quad(mesh, 0, colsY + 1/8.0f, 1/8.0f, 1/8.0f);
  PutMesh(mesh, ed->colPickerMat, 0);
  RmMesh(mesh);
}

void EdSizeColPicker(Ed ed) {
  Trans trans = ed->colPickerTrans;
  ed->colPickerSize = EdColPickerSize(ed);
  ClrTrans(trans);
  SetPos(trans, 10, 10);
  SetScale1(trans, ed->colPickerSize);
  ed->colPickerMat = ToTmpMat(trans);
}

void EdUpdColPickerMesh(Ed ed) {
  RmMesh(ed->colPickerMesh);
  ed->colPickerMesh = EdCreateColPickerMesh(ed->grey);
}

void EdMixPickEdCol(Ed ed) {
  float x = ed->colX;
  float y = ed->colY;
  int cols[7] = { 0xff0000, 0xffff00, 0x00ff00,
    0x00ffff, 0x0000ff, 0xff00ff, 0xff0000 };
  int i = (int)(x * 6);
  int baseCol = Mix(cols[i], cols[i + 1], (x * 6) - i);
  ed->grey = 0x010101 * (int)((1 - ed->greyY) * 0xff);
  if (y <= 0.5f) {
    ed->cols[ed->colIndex] = Mix(ed->grey, baseCol, y * 2);
  } else {
    ed->cols[ed->colIndex] = Mix(baseCol, 0x000000, (y - 0.5f) * 2);
  }
  ed->cols[ed->colIndex] |= (int)(ed->alphaY * 0xff) << 24;
  EdUpdColPickerMesh(ed);
}

void EdPickCol(Ed ed) {
  Wnd wnd = ed->wnd;
  /* TODO: proper inverse mat so wnd can be moved around n shit */
  /* TODO: use generic rect intersect funcs */
  float x = (MouseX(wnd) - 10) / (float)ed->colPickerSize;
  float y = (MouseY(wnd) - 10) / (float)ed->colPickerSize;
  if (ed->flags & ED_FEXPAND_COLOR_PICKER) {
    /* clicked on the grey adjust bar */
    if (y >= 0 && y <= CPICK_HEIGHT && x >= CPICK_BBAR_LEFT && x < CPICK_BBAR_RIGHT) {
      ed->greyY = y;
    }
    /* clicked on the alpha adjust bar */
    else if (y >= 0 && y <= CPICK_HEIGHT && x >= CPICK_ABAR_LEFT && x < CPICK_ABAR_RIGHT) {
      ed->alphaY = y;
    }
    /* clicked on the col picker */
    else if (y >= 0 && y <= CPICK_HEIGHT && x >= 0 && x < CPICK_WIDTH) {
      ed->colX = x;
      ed->colY = y;
    }
    EdMixPickEdCol(ed);
  } else {
    float p[2];
    p[0] = MouseX(wnd);
    p[1] = MouseY(wnd);
    EdMapToTex(ed, p);
    if (x >= 0 && x < ed->width && y >= 0 && y < ed->height) {
      ed->cols[ed->colIndex] =
        ed->textureData[(int)p[1] * ed->width + (int)p[0]];
    }
  }
}

/* ---------------------------------------------------------------------------------------------- */

/* NOTE: this updates rect in place to the adjusted power-of-two size */
int* EdCreatePixsFromRegion(Ed ed, float* rect) {
  float extents[4];
  int* newData = 0;
  int x, y;

  /* commit any pending changes to the current image */
  if (ed->flags & ED_FDIRTY) {
    EdFlushUpds(ed);
  }

  /* snap crop rect to the closest power of two sizes */
  SetRectSize(rect, RoundUpToPowerOfTwo(RectWidth(rect)),
    RoundUpToPowerOfTwo(RectHeight(rect)));

  /* create cropped pixel data, fill any padding that goes out of the original image with transp */
  SetRect(extents, 0, ed->width, 0, ed->height);
  for (y = RectTop(rect); y < RectBot(rect); ++y) {
    for (x = RectLeft(rect); x < RectRight(rect); ++x) {
      if (PtInRect(extents, x, y)) {
        ArrCat(&newData, ed->textureData[y * ed->width + x]);
      } else {
        ArrCat(&newData, 0xff000000);
      }
    }
  }

  return newData;
}

void EdFillRect(Ed ed, float* rect, int col) {
  int x, y;
  if (EdUpdPutRateLimiter(ed)) {
    for (y = RectTop(rect); y < RectBot(rect); ++y) {
      for (x = RectLeft(rect); x < RectRight(rect); ++x) {
        EdPutPix(ed, x, y, col, 0);
      }
    }
  }
}

void EdCrop(Ed ed, float* rect) {
  float potRect[4];
  int* newData;

  MemCpy(potRect, rect, sizeof(potRect));
  newData = EdCreatePixsFromRegion(ed, potRect);

  /* refresh everything */
  RmArr(ed->textureData);
  ed->textureData = newData;
  ed->width = RectWidth(potRect);
  ed->height = RectHeight(potRect);
  EdClrSelection(ed);
  EdUpdMesh(ed);
  EdFlushUpds(ed);
  EdClrPanning(ed);
}

/* TODO: clean up this mess, maybe use a generic ui function to draw rectangles with borders */

/* the way this works is i have a 4x4 texture with a black square in the middle and a transparent
 * 1px border. by using texture coord offsets and messing with the repeat mode, I can reuse the
 * texture to draw all 4 sides of the dashed selection border */

void EdDashBorderH(Ed ed, Mesh mesh, float* rect, float off, int col) {
  float uSize = RectWidth(rect) * ed->scale;
  float vSize = RectHeight(rect) * ed->scale;
  Col(mesh, col);
  TexdQuad(mesh,
    RectX(rect)    , RectY(rect)     , 1 + off, 1,
    RectWidth(rect), RectHeight(rect), uSize  , vSize * 2
  );
  TexdQuad(mesh,
    RectX(rect)    , RectY(rect)     , 1 - off, -vSize * 2 + 2,
    RectWidth(rect), RectHeight(rect), uSize  ,  vSize * 2
  );
}

void EdDashBorderV(Ed ed, Mesh mesh, float* rect, float off, int col) {
  float uSize = RectWidth(rect) * ed->scale;
  float vSize = RectHeight(rect) * ed->scale;
  Col(mesh, col);
  TexdQuad(mesh,
    RectX(rect)    , RectY(rect)     , 1        , 1 - off,
    RectWidth(rect), RectHeight(rect), uSize * 2, vSize
  );
  TexdQuad(mesh,
    RectX(rect)    , RectY(rect)     , -uSize * 2 + 2, 1 + off,
    RectWidth(rect), RectHeight(rect),  uSize * 2    , vSize
  );
}

void EdUpdYank(Ed ed) {
  if (ed->cutMesh) {
    Trans trans = ed->cutTrans;
    float* rect = ed->effectiveSelRect;
    EdSelectedRect(ed, rect);
    ClrTrans(trans);
    /* TODO: instead of multiplying by scale everywhere, keep a separate mat for scale? */
    SetPos(trans, RectX(rect) * ed->scale, RectY(rect) * ed->scale);
    ed->cutMat = ToTmpMat(trans);
  }
}

void EdClrYank(Ed ed) {
  RmMesh(ed->cutMesh);
  ed->cutMesh = 0;
  RmArr(ed->cutData);
  ed->cutData = 0;
}

void EdClrSelection(Ed ed) {
  MemSet(ed->selRect, 0, sizeof(ed->selRect));
  ed->flags &= ~ED_FSELECT;
  EdClrYank(ed);
}

void EdYank(Ed ed) {
  float* potRect = ed->cutPotRect;
  float* rect = ed->effectiveSelRect;
  if (ed->cutMesh) {
    EdClrYank(ed);
  }
  MemCpy(ed->cutSourceRect, rect, sizeof(float) * 4);
  MemCpy(potRect, rect, sizeof(float) * 4);
  ed->cutData = EdCreatePixsFromRegion(ed, potRect);
  Pixs(ed->cutTex, RectWidth(potRect), RectHeight(potRect), ed->cutData);
  ed->cutMesh = MkMesh();
  Quad(ed->cutMesh, 0, 0, RectWidth(rect), RectHeight(rect));
  EdFillRect(ed, rect, 0xff000000);
  EdUpdYank(ed);
}

void EdPaste(Ed ed, float* rect, int flags) {
  if (ed->cutMesh) {
    int x, y;
    float* potRect = ed->cutPotRect;
    int stride = RectWidth(potRect);
    for (y = 0; y < Min(RectHeight(potRect), RectHeight(rect)); ++y) {
      for (x = 0; x < Min(RectWidth(potRect), RectWidth(rect)); ++x) {
        int dx = RectLeft(rect) + x;
        int dy = RectTop(rect) + y;
        int col = ed->cutData[y * stride + x];
        EdPutPix(ed, dx, dy, col, flags);
      }
    }
  }
}

void EdUnyank(Ed ed) {
  /* paste at start location. no alpha blending to ensure we restore that rect to exactly the state
   * it had when we yanked */
  EdPaste(ed, ed->cutSourceRect, ED_FIGNORE_SELECTION);
  EdClrYank(ed);
}

/* ---------------------------------------------------------------------------------------------- */

void EdBeginSelect(Ed ed) {
  switch (ed->selAnchor) {
    case ED_SELECT_NONE: {
      float p[2];
      EdClrSelection(ed);
      p[0] = MouseX(ed->wnd);
      p[1] = MouseY(ed->wnd);
      EdMapToTex(ed, p);
      SetRectLeft(ed->selRect, p[0]);
      SetRectTop(ed->selRect, p[1]);
      SetRectSize(ed->selRect, 0, 0);
      ed->flags |= ED_FSELECT;
      break;
    }
  }
}

void EdSelectedRect(Ed ed, float* rect) {
  float textureRect[4];
  float xoff = 1.5f, yoff = 1.5f;
  MemCpy(rect, ed->selRect, sizeof(float) * 4);
  if (RectWidth(rect) < 0) xoff *= -1;
  if (RectHeight(rect) < 0) yoff *= -1;
  SetRectSize(rect, (int)(RectWidth(rect) + xoff), (int)(RectHeight(rect) + yoff));
  NormRect(rect);
  SetRect(textureRect, 0, ed->width, 0, ed->height);
  ClampRect(rect, textureRect);
  FloorFlts(4, rect, rect);
}

void EdPutSelect(Ed ed) {
  Tex texture = ed->selBorderTex;
  float* rect = ed->effectiveSelRect;
  Mesh meshH = MkMesh();
  Mesh meshV = MkMesh();
  float off = ed->selBlinkTimer * 2;

  /* draw dashed border that alternates between white and black */
  EdDashBorderH(ed, meshH, rect, off + 0, 0xffffff);
  EdDashBorderH(ed, meshH, rect, off + 2, 0x000000);
  EdDashBorderV(ed, meshV, rect, off + 0, 0xffffff);
  EdDashBorderV(ed, meshV, rect, off + 2, 0x000000);

  SetTexWrapU(texture, REPEAT);
  SetTexWrapV(texture, CLAMP_TO_EDGE);
  PutMesh(meshH, ed->mat, texture);

  SetTexWrapU(texture, CLAMP_TO_EDGE);
  SetTexWrapV(texture, REPEAT);
  PutMesh(meshV, ed->mat, texture);

  RmMesh(meshH);
  RmMesh(meshV);
}

/* ---------------------------------------------------------------------------------------------- */

void EdBeginPuting(Ed ed) {
  switch (ed->tool) {
    case ED_SELECT: { EdBeginSelect(ed); break; }
  }
}

void EdPuting(Ed ed) {
  switch (ed->tool) {
    case ED_PENCIL: {
      int col = ed->cols[ed->colIndex];
      float point[2];
      point[0] = MouseX(ed->wnd);
      point[1] = MouseY(ed->wnd);
      EdMapToTex(ed, point);
      if (EdUpdPutRateLimiter(ed)) {
        /* alpha blend unless we're deleting the pixel with complete transparency */
        EdPutPix(ed, (int)point[0], (int)point[1], col,
          ((col & 0xff000000) == 0xff000000) ? 0 : ED_FALPHA_BLEND);
      }
      break;
    }
    case ED_COLOR_PICKER: {
      EdPickCol(ed);
      break;
    }
    /* TODO: not sure if select dragging belongs here */
    case ED_SELECT: {
      switch (ed->selAnchor) {
        case ED_SELECT_NONE: {
          float p[2];
          p[0] = MouseX(ed->wnd);
          p[1] = MouseY(ed->wnd);
          EdMapToTex(ed, p);
          SetRectRight(ed->selRect, p[0]);
          SetRectBot(ed->selRect, p[1]);
          break;
        }
        case ED_SELECT_MID: {
          float* rect = ed->selRect;
          SetRectPos(rect,
            RectX(rect) + MouseDX(ed->wnd) / (float)ed->scale,
            RectY(rect) + MouseDY(ed->wnd) / (float)ed->scale);
          if (ed->cutMesh) {
            EdUpdYank(ed);
          } else {
            EdSelectedRect(ed, ed->effectiveSelRect);
          }
          break;
        }
      }
    }
  }
}

void EdUpdTrans(Ed ed) {
  Trans trans = ed->trans;
  ClrTrans(trans);
  SetPos(trans, ed->oX, ed->oY);
  SetScale1(trans, ed->scale);
  ed->mat = ToTmpMat(trans);
  ed->matOrtho = ToTmpMatOrtho(trans);
  EdUpdYank(ed);
}

void EdClrPanning(Ed ed) {
  ed->oX = 100;
  ed->oY = 100;
  EdUpdTrans(ed);
}

void EdUpdMesh(Ed ed) {
  RmMesh(ed->mesh);
  ed->mesh = MkMesh();
  Quad(ed->mesh, 0, 0, ed->width, ed->height);
}

void EdMapToTex(Ed ed, float* point) {
  /* un-mat mouse coordinates so they are relative to the texture */
  InvTransPt(ed->matOrtho, point);
  point[0] /= ed->scale;
  point[1] /= ed->scale;
}

void EdChangeScale(Ed ed, int direction) {
  float p[2];
  float newScale = ed->scale + direction * (ed->scale / 8 + 1);
  newScale = Min(32, newScale);
  newScale = Max(1, newScale);
  p[0] = MouseX(ed->wnd);
  p[1] = MouseY(ed->wnd);
  EdMapToTex(ed, p);
  /* adjust panning so the pixel we're pointing stays under the cursor */
  ed->oX -= (int)((p[0] * newScale) - (p[0] * ed->scale));
  ed->oY -= (int)((p[1] * newScale) - (p[1] * ed->scale));
  ed->scale = newScale;
  EdUpdTrans(ed);
}

void EdSave(Ed ed) {
  char* spr;
  EdFlushUpds(ed);
  spr = ArgbToSprArr(ed->textureData, ed->width, ed->height);
  WriteFile(ed->filePath, spr, ArrLen(spr));
  RmArr(spr);
}

void EdSaveOneBpp(Ed ed) {
  char* data;
  char* b64Data;
  char* path = 0;
  if (ed->flags & ED_FSELECT) {
    int x, y;
    float* rect = ed->effectiveSelRect;
    int* crop = 0;
    for (y = RectTop(rect); y < RectBot(rect); ++y) {
      for (x = RectLeft(rect); x < RectRight(rect); ++x) {
        ArrCat(&crop, ed->textureData[y * ed->width + x]);
      }
    }
    data = ArgbArrToOneBpp(crop);
    RmArr(crop);
  } else {
    data = ArgbArrToOneBpp(ed->textureData);
  }
  b64Data = ArrToB64(data);
  RmArr(data);
  ArrStrCat(&path, ed->filePath);
  ArrStrCat(&path, ".txt");
  WriteFile(path, b64Data, StrLen(b64Data));
  RmArr(path);
  Free(b64Data);
}

void EdLoad(Ed ed) {
  Spr spr = MkSprFromFile(ed->filePath);
  if (spr) {
    EdFlushUpds(ed);
    RmArr(ed->textureData);
    ed->textureData = SprToArgbArr(spr);
    ed->width = SprWidth(spr);
    ed->height = SprHeight(spr);
    EdUpdMesh(ed);
    RmSpr(spr);
    EdFlushUpds(ed);
  }
}

void EdHandleKeyDown(Ed ed, int key, int state) {
  switch (key) {
    case MMID: { ed->flags |= ED_FDRAGGING; break; }
    case C: {
      if (ed->tool == ED_COLOR_PICKER) {
        ed->flags ^= ED_FEXPAND_COLOR_PICKER;
      } else {
        ed->tool = ED_COLOR_PICKER;
      }
      break;
    }
    case E: { ed->tool = ED_PENCIL; break; }
    case S: {
      if (state & FCTRL) {
        EdSave(ed);
      } else if (state & FSHIFT) {
        EdClrSelection(ed);
      } else {
        ed->tool = ED_SELECT;
      }
      break;
    }
    case R: {
      if (ed->flags & ED_FSELECT) {
        EdCrop(ed, ed->effectiveSelRect);
      }
      break;
    }
    case K1: {
      if (state & FCTRL) {
        EdSaveOneBpp(ed);
      } else {
        EdFillRect(ed, ed->effectiveSelRect, ed->cols[0]);
      }
      break;
    }
    case K2: { EdFillRect(ed, ed->effectiveSelRect, ed->cols[1]); break; }
    case D: {
      if (state & FSHIFT) {
        EdUnyank(ed);
      } else {
        EdYank(ed);
      }
      break;
    }
    case T: {
      if (EdUpdPutRateLimiter(ed)) {
        EdPaste(ed, ed->effectiveSelRect, ED_FALPHA_BLEND);
      }
      break;
    }
    case MWHEELUP: { EdChangeScale(ed, 1); break; }
    case MWHEELDOWN: { EdChangeScale(ed, -1); break; }
    case MLEFT:
    case MRIGHT: {
      EdClrPutRateLimiter(ed);
      ed->flags |= ED_FDRAWING;
      ed->colIndex = key == MRIGHT ? 1 : 0;
      ed->selAnchor = key == MRIGHT ? ED_SELECT_MID : ED_SELECT_NONE;
      EdBeginPuting(ed);
      EdPuting(ed);
      EdFlushUpds(ed);
      break;
    }
    case F1: {
      ed->flags ^= ED_FHELP;
      break;
    }
  }
}

void EdHandleKeyUp(Ed ed, int key) {
  switch (key) {
    case MMID: { ed->flags &= ~ED_FDRAGGING; break; }
    case MLEFT:
    case MRIGHT: { ed->flags &= ~ED_FDRAWING; break; }
  }
}

int EdHandleMsg(Ed ed) {
  Wnd wnd = ed->wnd;
  switch (MsgType(wnd)) {
    case SIZE: {
      EdSizeColPicker(ed);
      break;
    }
    case KEYDOWN: {
      EdHandleKeyDown(ed, Key(wnd), KeyState(ed->wnd));
      break;
    }
    case KEYUP: { EdHandleKeyUp(ed, Key(wnd)); break; }
    case MOTION: {
      int flags = ed->flags;
      if (flags & ED_FDRAGGING) {
        ed->oX += MouseDX(wnd);
        ed->oY += MouseDY(wnd);
        EdUpdTrans(ed);
      }
      if (flags & ED_FDRAWING) { EdPuting(ed); }
      break;
    }
    case QUIT: { return 0; }
  }
  return 1;
}

void EdUpd(Ed ed) {
  Wnd wnd = ed->wnd;
  if (ed->flags & ED_FDIRTY) {
    ed->flushTimer += Delta(wnd);
    /* flush updates periodically or if there's too many queued */
    if (ed->flushTimer > ED_FLUSH_TIME || ArrLen(ed->updates) > ED_FLUSH_THRESHOLD) {
      EdFlushUpds(ed);
      ed->flushTimer = Max(0, ed->flushTimer - ED_FLUSH_TIME);
    }
  } else {
    ed->flushTimer = 0;
  }

  ed->selBlinkTimer += Delta(ed->wnd);
  ed->selBlinkTimer = FltMod(ed->selBlinkTimer, 2);

  EdSelectedRect(ed, ed->effectiveSelRect);
}

void EdPutPix(Ed ed, int x, int y, int col, int flags) {
  float rect[4];
  int fcol = col;
  SetRect(rect, 0, ed->width, 0, ed->height);
  if ((ed->flags & ED_FSELECT) && !(flags & ED_FIGNORE_SELECTION)) {
    ClampRect(rect, ed->effectiveSelRect);
  }
  if (PtInRect(rect, x, y)) {
    int* px = &ed->textureData[y * ed->width + x];
    if (flags & ED_FALPHA_BLEND) {
      /* normal alpha blending with whatever is currently here */
      fcol = AlphaBlend(col, *px);
    }
    if (*px != fcol) {
      EDUpd* u;
      *px = fcol;
      u = ArrAlloc(&ed->updates, 1);
      u->x = x;
      u->y = y;
      u->col = col;
      u->flags = flags;
      ed->flags |= ED_FDIRTY;
    }
  }
}

void EdFlushUpds(Ed ed) {
  Pixs(ed->texture, ed->width, ed->height, ed->textureData);
  SetArrLen(ed->updates, 0);
  ed->flags &= ~ED_FDIRTY;
}

void EdClrPutRateLimiter(Ed ed) {
  ed->lastPutX = -1;
}

int EdUpdPutRateLimiter(Ed ed) {
  float p[2];
  int x, y;
  p[0] = MouseX(ed->wnd);
  p[1] = MouseY(ed->wnd);
  EdMapToTex(ed, p);
  x = (int)p[0];
  y = (int)p[1];
  if (x != ed->lastPutX || y != ed->lastPutY) {
    ed->lastPutX = x;
    ed->lastPutY = y;
    return 1;
  }
  return 0;
}

void EdPutUpds(Ed ed) {
  int i;
  Mesh mesh = MkMesh();
  EDUpd* updates = ed->updates;
  for (i = 0; i < ArrLen(updates); ++i) {
    EDUpd* u = &updates[i];
    if (u->flags & ED_FALPHA_BLEND) {
      /* normal alpha blending with whatever is currently here */
      Col(mesh, u->col);
    } else {
      /* no alpha blending, just overwrite the col.
       * because we can have pending transparent pixels we need to do some big brain custom alpha
       * blending with the checkerboard col at this location */
      int baseCol = EdSampleCheckerboard(u->x, u->y);
      float rgba[4];
      ColToFlts(u->col, rgba);
      Col(mesh, Mix(u->col & 0xffffff, baseCol, rgba[3]));
    }
    Quad(mesh, u->x, u->y, 1, 1);
  }
  PutMesh(mesh, ed->mat, 0);
  RmMesh(mesh);
}

/* cover rest of the area in grey to distinguish it from a transparent image */
void EdPutBorders(Ed ed) {
  /* TODO: only regenerate mesh when the mat changes */
  int winWidth = WndWidth(ed->wnd);
  int winHeight = WndHeight(ed->wnd);
  int right, bottom;
  int oX = (int)ed->oX, oY = (int)ed->oY;
  int scalEdWidth = ed->width * ed->scale;
  int scalEdHeight = ed->height * ed->scale;
  Mesh mesh = MkMesh();
  Col(mesh, 0x111111);
  if (oX > 0) {
    Quad(mesh, 0, 0, oX, winHeight);
  }
  right = oX + scalEdWidth;
  if (right < winWidth) {
    Quad(mesh, right, 0, winWidth - right, winHeight);
  }
  bottom = oY + scalEdHeight;
  if (oY > 0) {
    Quad(mesh, 0, 0, winWidth, oY);
  }
  if (bottom < winHeight) {
    Quad(mesh, oX, bottom, scalEdWidth, winHeight - bottom);
  }
  PutMesh(mesh, 0, 0);
  RmMesh(mesh);
}

void EdPut(Ed ed) {
  PutMesh(ed->mesh, ed->mat, ed->checkerTex);
  PutMesh(ed->mesh, ed->mat, ed->texture);
  EdPutBorders(ed);
  EdPutUpds(ed);
  if (ed->cutMesh) {
    Mat copy = DupMat(ed->cutMat);
    PutMesh(ed->cutMesh, MulMat(copy, ed->mat), ed->cutTex);
    RmMat(copy);
  }
  if (ed->flags & ED_FSELECT) {
    EdPutSelect(ed);
  }
  if (ed->tool == ED_COLOR_PICKER) {
    EdPutColPicker(ed);
  }
  if (ed->flags & ED_FHELP) {
    PutMesh(ed->helpBgMesh, 0, 0);
    PutMesh(ed->helpTextMesh, 0, FtTex(ed->font));
  }
  SwpBufs(ed->wnd);
}

Wnd EdCreateWnd() {
  Wnd wnd = MkWnd();
  SetWndClass(wnd, "WeebCoreSpriteEd");
  SetWndName(wnd, "WeebCore Sprite Ed");
  return wnd;
}

int EdSampleCheckerboard(float x, float y) {
  int data[4] = { 0xaaaaaa, 0x666666, 0x666666, 0xaaaaaa };
  x /= ED_CHECKER_SIZE;
  y /= ED_CHECKER_SIZE;
  return data[((int)y % 2) * 2 + ((int)x % 2)];
}

Tex EdCreateCheckerTex() {
  int x, y;
  int* data = 0;
  Tex texture = MkTex();
  for (y = 0; y < ED_CHECKER_SIZE * 2; ++y) {
    for (x = 0; x < ED_CHECKER_SIZE * 2; ++x) {
      ArrCat(&data, EdSampleCheckerboard(x, y));
    }
  }
  Pixs(texture, ED_CHECKER_SIZE * 2, ED_CHECKER_SIZE * 2, data);
  RmArr(data);
  return texture;
}

Tex EdCreateSelBorderTex() {
  Tex texture = MkTex();
  int data[4*4] = {
    0xff000000, 0xff000000, 0xff000000, 0xff000000,
    0xff000000, 0x00ffffff, 0x00ffffff, 0xff000000,
    0xff000000, 0x00ffffff, 0x00ffffff, 0xff000000,
    0xff000000, 0xff000000, 0xff000000, 0xff000000
  };
  return Pixs(texture, 4, 4, data);
}

int* EdCreatePixs(int fillCol, int width, int height) {
  int i;
  int* data = 0;
  for (i = 0; i < width * height; ++i) {
    ArrCat(&data, fillCol);
  }
  return data;
}

Mat EdCreateBgTrans() {
  Mat mat = MkMat();
  return Scale1(mat, ED_CHECKER_SIZE);
}

Ed EdCreate(char* filePath) {
  Ed ed = Alloc(sizeof(struct _Ed));
  ed->scale = 4;
  ed->wnd = EdCreateWnd();
  ed->checkerTex = EdCreateCheckerTex();
  ed->texture = MkTex();
  ed->cutTex = MkTex();
  ed->width = ED_TEXSIZE;
  ed->height = ED_TEXSIZE;
  ed->textureData = EdCreatePixs(0xff000000, ed->width, ed->height);
  ed->trans = MkTrans();
  ed->colPickerTrans = MkTrans();
  ed->cutTrans = MkTrans();
  ed->tool = ED_PENCIL;
  ed->grey = 0xffffff;
  ed->cols[1] = 0xff000000;
  ed->filePath = filePath ? filePath : "out.wbspr";
  ed->selBorderTex = EdCreateSelBorderTex();

  ed->flags |= ED_FHELP;
  ed->font = DefFt();
  /* TODO: scale with screen size */
  /* TODO: actual proper automatic ui layoutting */
  ed->helpTextMesh = MkMesh();
  Col(ed->helpTextMesh, 0xbebebe);
  FtMesh(ed->helpTextMesh, ed->font, 15, 15, EdHelpString());
  ed->helpBgMesh = MkMesh();
  Col(ed->helpBgMesh, 0x33000000);
  Quad(ed->helpBgMesh, 5, 5, 610, 290);

  EdClrSelection(ed);
  EdUpdColPickerMesh(ed);
  EdUpdMesh(ed);
  EdFlushUpds(ed);
  EdSizeColPicker(ed);
  EdClrPanning(ed);
  EdLoad(ed);

  return ed;
}

void RmEd(Ed ed) {
  if (ed) {
    EdClrYank(ed);
    EdClrSelection(ed);
    RmWnd(ed->wnd);
    RmMesh(ed->cutMesh);
    RmMesh(ed->mesh);
    RmMesh(ed->colPickerMesh);
    RmMesh(ed->helpTextMesh);
    RmMesh(ed->helpBgMesh);
    RmArr(ed->textureData);
    RmArr(ed->updates);
    RmTex(ed->texture);
    RmTex(ed->cutTex);
    RmTex(ed->checkerTex);
    RmTex(ed->selBorderTex);
    RmTrans(ed->trans);
    RmTrans(ed->colPickerTrans);
    RmTrans(ed->cutTrans);
    RmFt(ed->font);
  }
  Free(ed);
}

/* ---------------------------------------------------------------------------------------------- */



int main(int argc, char* argv[]) {
  Ed ed = EdCreate(argc > 1 ? argv[1] : 0);
  while (1) {
    while (NextMsg(ed->wnd)) {
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
