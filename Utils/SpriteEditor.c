/* very rough sprite will improve/rewrite as I move features into the engine. this is mainly
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
    "N            (after selecting) anim preview. this moves the selection right by its\n"
    "             width until it finds a completely transparent frame\n"
    "Shift+Wheel  adjust animation speed\n"
    "Shift+N      cancel anim preview\n"
    "HJKL/Arrows  move cursor\n"
    "Shift (hold) move cursor faster when using HJKL/Arrows\n"
    "Q            left-click\n"
    "W            right-click\n"
    "A            middle-click\n"
    "[            wheel up\n"
    "]            wheel down\n"
  );
}

#define ED_FLUSH_TIME 1.0f
#define ED_FLUSH_THRESHOLD 4096
#define ED_IMGSIZE 1024
#define ED_CHECKER_SIZE 8

#define ED_FDRAGGING (1<<1)
#define ED_FPAINTING (1<<2)
#define ED_FDIRTY (1<<3)
#define ED_FEXPAND_COLOR_PICKER (1<<4)
#define ED_FSELECT (1<<5)
#define ED_FALPHA_BLEND (1<<6)
#define ED_FIGNORE_SELECTION (1<<7)
#define ED_FHELP (1<<8)
#define ED_FHOLDING_SHIFT (1<<9)
#define ED_FSHOW_CURSOR (1<<10)
#define ED_FANIM (1<<11)

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

Wnd wnd;

int flags;
float oX, oY;
float cX, cY;
int wishX, wishY;
float flushTimer; /* TODO: built in timer events */
int scale;
int cols[2];
int colIndex;
Img checkerImg;
Mesh mesh;
Img img;
int* imgData;
EDUpd* updates;
Trans trans;
int tool;
int width, height;
int lastPutX, lastPutY;

Ft font;
Mesh helpTextMesh, helpBgMesh;

float selBlinkTimer;
Img selBorderImg;
float selRect[4];          /* raw rectangle between the cursor and the initial drag position */
float effectiveSelRect[4]; /* normalizsnappetc selection rect */
int selAnchor;

float animTimer;
int animSpeed;
float animRect[4];
float animStartRect[4];

Img cutImg;
int* cutData;
float cutPotRect[4]; /* power of two size of the cut img */
Mesh cutMesh;
Trans cutTrans;
float cutSourceRect[4]; /* the location from where the cut data was taken */

/* TODO: pull out col picker as a reusable widget */
Mesh colPickerMesh;
Trans colPickerTrans;
int colPickerSize;
int grey;
float colX, colY, greyY, alphaY;
char* filePath;

void EdMapToImg(float* point);
void EdFlushUpds();
void EdClrSelection();
void EdUpdMesh();
void EdClrPanning();
void EdSelectedRect(float* rect);
void EdClrPaintRateLimiter();
int EdUpdPaintRateLimiter();
int EdSampleCheckerboard(float x, float y);
void EdPaintPix(int x, int y, int col, int flags);

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

int EdColPickerSize() {
  int winSize = Min(WndWidth(wnd), WndHeight(wnd));
  int size = RoundUpToPowerOfTwo(winSize / 4);
  size = Max(size, 256);
  size = Min(size, winSize * 0.7);
  return size;
}

Mesh EdCreateColPickerMesh() {
  Mesh mesh = MkMesh();
  int gradient[7] = { 0xff0000, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xff0000 };
  int brightness[3] = { 0xffffff, 0xff000000, 0x000000 };
  int brightnessSelect[2] = { 0xffffff, 0x000000 };
  int alphaSelect[2] = { 0x00ffffff, 0xff000000 };
  brightness[0] = grey;
  QuadGradH(mesh, 0, 0, CPICK_WIDTH, CPICK_HEIGHT, 7, gradient);
  QuadGradV(mesh, 0, 0, CPICK_WIDTH, CPICK_HEIGHT, 3, brightness);
  QuadGradV(mesh, CPICK_BBAR_LEFT, 0, CPICK_BAR_WIDTH, CPICK_HEIGHT, 2, brightnessSelect);
  QuadGradV(mesh, CPICK_ABAR_LEFT, 0, CPICK_BAR_WIDTH, CPICK_HEIGHT, 2, alphaSelect);
  return mesh;
}

void EdPutColPicker() {
  float colsY = 0;
  Mesh mesh = MkMesh();
  if (flags & ED_FEXPAND_COLOR_PICKER) {
    colsY = CPICK_HEIGHT + CPICK_MARGIN;
    Col(mesh, Mix(grey, 0xff0000, 0.5));
    Quad(mesh, CPICK_BBAR_LEFT, greyY - 1/128.0f, 1/16.0f, 1/64.0f);
    Col(mesh, 0x000000);
    Quad(mesh, CPICK_ABAR_LEFT, alphaY - 1/128.0f, 1/16.0f, 1/64.0f);
    Col(mesh, ~cols[colIndex] & 0xffffff);
    Quad(mesh, colX - 1/128.0f, colY - 1/128.0f, 1/64.0f, 1/64.0f);
    Col(mesh, 0x000000);
    Quad(mesh, colX - 1/256.0f, colY - 1/256.0f, 1/128.0f, 1/128.0f);
    PutMesh(colPickerMesh, ToTmpMat(colPickerTrans), 0);
  }
  Col(mesh, cols[0]);
  Quad(mesh, 0, colsY         , 1/8.0f, 1/8.0f);
  Col(mesh, cols[1]);
  Quad(mesh, 0, colsY + 1/8.0f, 1/8.0f, 1/8.0f);
  PutMesh(mesh, ToTmpMat(colPickerTrans), 0);
  RmMesh(mesh);
}

void EdSizeColPicker() {
  Trans trans = colPickerTrans;
  colPickerSize = EdColPickerSize();
  ClrTrans(trans);
  SetPos(trans, 10, 10);
  SetScale1(trans, colPickerSize);
}

void EdUpdColPickerMesh() {
  RmMesh(colPickerMesh);
  colPickerMesh = EdCreateColPickerMesh();
}

void EdMixPickedCol() {
  float x = colX;
  float y = colY;
  int gradient[7] = { 0xff0000, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xff0000 };
  int i = (int)(x * 6);
  int baseCol = Mix(gradient[i], gradient[i + 1], (x * 6) - i);
  grey = 0x010101 * (int)((1 - greyY) * 0xff);
  if (y <= 0.5f) {
    cols[colIndex] = Mix(grey, baseCol, y * 2);
  } else {
    cols[colIndex] = Mix(baseCol, 0x000000, (y - 0.5f) * 2);
  }
  cols[colIndex] |= (int)(alphaY * 0xff) << 24;
  EdUpdColPickerMesh();
}

void EdPickCol() {
  /* TODO: proper inverse mat so wnd can be moved around n shit */
  /* TODO: use generic rect intersect funcs */
  float x = (MouseX(wnd) - 10) / (float)colPickerSize;
  float y = (MouseY(wnd) - 10) / (float)colPickerSize;
  if (flags & ED_FEXPAND_COLOR_PICKER) {
    /* clicked on the grey adjust bar */
    if (y >= 0 && y <= CPICK_HEIGHT && x >= CPICK_BBAR_LEFT && x < CPICK_BBAR_RIGHT) {
      greyY = y;
    }
    /* clicked on the alpha adjust bar */
    else if (y >= 0 && y <= CPICK_HEIGHT && x >= CPICK_ABAR_LEFT && x < CPICK_ABAR_RIGHT) {
      alphaY = y;
    }
    /* clicked on the col picker */
    else if (y >= 0 && y <= CPICK_HEIGHT && x >= 0 && x < CPICK_WIDTH) {
      colX = x;
      colY = y;
    }
    EdMixPickedCol();
  } else {
    float p[2];
    p[0] = MouseX(wnd);
    p[1] = MouseY(wnd);
    EdMapToImg(p);
    if (x >= 0 && x < width && y >= 0 && y < height) {
      cols[colIndex] =
        imgData[(int)p[1] * width + (int)p[0]];
    }
  }
}

/* ---------------------------------------------------------------------------------------------- */

/* NOTE: this updates rect in place to the adjusted power-of-two size */
int* EdCreatePixsFromRegion(float* rect, int powerOfTwo) {
  float extents[4];
  int* newData = 0;
  int x, y;

  /* commit any pending changes to the current image */
  if (flags & ED_FDIRTY) {
    EdFlushUpds();
  }

  if (powerOfTwo) {
    /* snap crop rect to the closest power of two sizes */
    SetRectSize(rect, RoundUpToPowerOfTwo(RectWidth(rect)),
      RoundUpToPowerOfTwo(RectHeight(rect)));
  }

  /* create cropped pixel data, fill any padding that goes out of the original image with transp */
  SetRect(extents, 0, width, 0, height);
  for (y = RectTop(rect); y < RectBot(rect); ++y) {
    for (x = RectLeft(rect); x < RectRight(rect); ++x) {
      if (PtInRect(extents, x, y)) {
        ArrCat(&newData, imgData[y * width + x]);
      } else {
        ArrCat(&newData, 0xff000000);
      }
    }
  }

  return newData;
}

void EdFillRect(float* rect, int col) {
  int x, y;
  if (EdUpdPaintRateLimiter()) {
    for (y = RectTop(rect); y < RectBot(rect); ++y) {
      for (x = RectLeft(rect); x < RectRight(rect); ++x) {
        EdPaintPix(x, y, col, 0);
      }
    }
  }
}

void EdCrop(float* rect) {
  int* newData = EdCreatePixsFromRegion(rect, 0);

  /* refresh everything */
  RmArr(imgData);
  imgData = newData;
  width = RectWidth(rect);
  height = RectHeight(rect);
  EdClrSelection();
  EdUpdMesh();
  EdFlushUpds();
  EdClrPanning();
}

/* TODO: clean up this mess, maybe use a generic ui function to draw rectangles with borders */

/* the way this works is i have a 4x4 img with a black square in the middle and a transparent
 * 1px border. by using img coord offsets and messing with the repeat mode, I can reuse the
 * img to draw all 4 sides of the dashed selection border */

void EdDashBorderH(Mesh mesh, float* rect, float off, int col) {
  float uSize = RectWidth(rect) * scale;
  float vSize = RectHeight(rect) * scale;
  Col(mesh, col);
  ImgQuad(mesh,
    RectX(rect)    , RectY(rect)     , 1 + off, 1,
    RectWidth(rect), RectHeight(rect), uSize  , vSize * 2
  );
  ImgQuad(mesh,
    RectX(rect)    , RectY(rect)     , 1 - off, -vSize * 2 + 2,
    RectWidth(rect), RectHeight(rect), uSize  ,  vSize * 2
  );
}

void EdDashBorderV(Mesh mesh, float* rect, float off, int col) {
  float uSize = RectWidth(rect) * scale;
  float vSize = RectHeight(rect) * scale;
  Col(mesh, col);
  ImgQuad(mesh,
    RectX(rect)    , RectY(rect)     , 1        , 1 - off,
    RectWidth(rect), RectHeight(rect), uSize * 2, vSize
  );
  ImgQuad(mesh,
    RectX(rect)    , RectY(rect)     , -uSize * 2 + 2, 1 + off,
    RectWidth(rect), RectHeight(rect),  uSize * 2    , vSize
  );
}

void EdUpdYank() {
  if (cutMesh) {
    Trans trans = cutTrans;
    float* rect = effectiveSelRect;
    EdSelectedRect(rect);
    ClrTrans(trans);
    /* TODO: instead of multiplying by scale everywhere, keep a separate mat for scale? */
    SetPos(trans, RectX(rect) * scale, RectY(rect) * scale);
  }
}

void EdClrYank() {
  RmMesh(cutMesh);
  cutMesh = 0;
  RmArr(cutData);
  cutData = 0;
}

void EdClrSelection() {
  MemSet(selRect, 0, sizeof(selRect));
  flags &= ~ED_FSELECT;
  EdClrYank();
}

void EdYank() {
  float* potRect = cutPotRect;
  float* rect = effectiveSelRect;
  if (cutMesh) {
    EdClrYank();
  }
  MemCpy(cutSourceRect, rect, sizeof(float) * 4);
  MemCpy(potRect, rect, sizeof(float) * 4);
  cutData = EdCreatePixsFromRegion(potRect, 1);
  Pixs(cutImg, RectWidth(potRect), RectHeight(potRect), cutData);
  cutMesh = MkMesh();
  Quad(cutMesh, 0, 0, RectWidth(rect), RectHeight(rect));
  EdFillRect(rect, 0xff000000);
  EdUpdYank();
}

void EdPaste(float* rect, int flags) {
  if (cutMesh) {
    int x, y;
    float* potRect = cutPotRect;
    int stride = RectWidth(potRect);
    for (y = 0; y < Min(RectHeight(potRect), RectHeight(rect)); ++y) {
      for (x = 0; x < Min(RectWidth(potRect), RectWidth(rect)); ++x) {
        int dx = RectLeft(rect) + x;
        int dy = RectTop(rect) + y;
        int col = cutData[y * stride + x];
        EdPaintPix(dx, dy, col, flags);
      }
    }
  }
}

void EdUnyank() {
  /* paste at start location. no alpha blending to ensure we restore that rect to exactly the state
   * it had when we yanked */
  EdPaste(cutSourceRect, ED_FIGNORE_SELECTION);
  EdClrYank();
}

/* ---------------------------------------------------------------------------------------------- */

void EdBeginSelect() {
  switch (selAnchor) {
    case ED_SELECT_NONE: {
      float p[2];
      EdClrSelection();
      p[0] = MouseX(wnd);
      p[1] = MouseY(wnd);
      EdMapToImg(p);
      SetRectLeft(selRect, p[0]);
      SetRectTop(selRect, p[1]);
      SetRectSize(selRect, 0, 0);
      flags |= ED_FSELECT;
      break;
    }
  }
}

void EdSelectedRect(float* rect) {
  float imgRect[4];
  float xoff = 1.5f, yoff = 1.5f;
  MemCpy(rect, selRect, sizeof(float) * 4);
  if (RectWidth(rect) < 0) xoff *= -1;
  if (RectHeight(rect) < 0) yoff *= -1;
  SetRectSize(rect, (int)(RectWidth(rect) + xoff), (int)(RectHeight(rect) + yoff));
  NormRect(rect);
  SetRect(imgRect, 0, width, 0, height);
  ClampRect(rect, imgRect);
  FloorFlts(4, rect, rect);
}

void EdPutSelect() {
  Img img = selBorderImg;
  float* rect = effectiveSelRect;
  Mesh meshH = MkMesh();
  Mesh meshV = MkMesh();
  float off = selBlinkTimer * 2;

  /* draw dashed border that alternates between white and black */
  EdDashBorderH(meshH, rect, off + 0, 0xffffff);
  EdDashBorderH(meshH, rect, off + 2, 0x000000);
  EdDashBorderV(meshV, rect, off + 0, 0xffffff);
  EdDashBorderV(meshV, rect, off + 2, 0x000000);

  SetImgWrapU(img, REPEAT);
  SetImgWrapV(img, CLAMP_TO_EDGE);
  PutMeshRaw(meshH, ToTmpMat(trans), img);

  SetImgWrapU(img, CLAMP_TO_EDGE);
  SetImgWrapV(img, REPEAT);
  PutMeshRaw(meshV, ToTmpMat(trans), img);

  RmMesh(meshH);
  RmMesh(meshV);
}

/* ---------------------------------------------------------------------------------------------- */

void EdBeginPainting() {
  switch (tool) {
    case ED_SELECT: { EdBeginSelect(); break; }
  }
}

void EdPainting() {
  switch (tool) {
    case ED_PENCIL: {
      int col = cols[colIndex];
      float point[2];
      point[0] = (int)cX;
      point[1] = (int)cY;
      EdMapToImg(point);
      if (EdUpdPaintRateLimiter()) {
        /* alpha blend unless we're deleting the pixel with complete transparency */
        EdPaintPix((int)point[0], (int)point[1], col,
          ((col & 0xff000000) == 0xff000000) ? 0 : ED_FALPHA_BLEND);
      }
      break;
    }
    case ED_COLOR_PICKER: {
      EdPickCol();
      break;
    }
    /* TODO: not sure if select dragging belongs here */
    case ED_SELECT: {
      switch (selAnchor) {
        case ED_SELECT_NONE: {
          float p[2];
          p[0] = MouseX(wnd);
          p[1] = MouseY(wnd);
          EdMapToImg(p);
          SetRectRight(selRect, p[0]);
          SetRectBot(selRect, p[1]);
          break;
        }
        case ED_SELECT_MID: {
          float* rect = selRect;
          SetRectPos(rect,
            RectX(rect) + MouseDX(wnd) / (float)scale,
            RectY(rect) + MouseDY(wnd) / (float)scale);
          if (cutMesh) {
            EdUpdYank();
          } else {
            EdSelectedRect(effectiveSelRect);
          }
          break;
        }
      }
    }
  }
}

void EdUpdTrans() {
  ClrTrans(trans);
  SetPos(trans, (int)oX, (int)oY);
  SetScale1(trans, scale);
  EdUpdYank();
}

void EdClrPanning() {
  oX = 100;
  oY = 100;
  EdUpdTrans();
}

void EdUpdMesh() {
  RmMesh(mesh);
  mesh = MkMesh();
  Quad(mesh, 0, 0, width, height);
}

void EdMapToImg(float* point) {
  /* un-mat mouse coordinates so they are relative to the img */
  InvTransPt(ToTmpMatOrtho(trans), point);
  point[0] /= scale;
  point[1] /= scale;
}

void EdOnWheel(int direction) {
  if ((flags & ED_FHOLDING_SHIFT) && (flags & ED_FANIM)) {
    animSpeed = Max(1, Min(120, animSpeed + direction));
  } else {
    float p[2];
    float newScale = scale + direction * (scale / 8 + 1);
    newScale = Min(32, newScale);
    newScale = Max(1, newScale);
    p[0] = cX;
    p[1] = cY;
    EdMapToImg(p);
    /* adjust panning so the pixel we're pointing stays under the cursor */
    oX -= (int)((p[0] * newScale) - (p[0] * scale));
    oY -= (int)((p[1] * newScale) - (p[1] * scale));
    scale = newScale;
    EdUpdTrans();
  }
}

void EdSave() {
  char* spr;
  EdFlushUpds();
  spr = ArgbToSprArr(imgData, width, height);
  WrFile(filePath, spr, ArrLen(spr));
  RmArr(spr);
}

void EdSaveOneBpp() {
  char* data;
  char* b64Data;
  char* path = 0;
  if (flags & ED_FSELECT) {
    int x, y;
    float* rect = effectiveSelRect;
    int* crop = 0;
    for (y = RectTop(rect); y < RectBot(rect); ++y) {
      for (x = RectLeft(rect); x < RectRight(rect); ++x) {
        ArrCat(&crop, imgData[y * width + x]);
      }
    }
    data = ArgbArrToOneBpp(crop);
    RmArr(crop);
  } else {
    data = ArgbArrToOneBpp(imgData);
  }
  b64Data = ArrToB64(data);
  RmArr(data);
  ArrStrCat(&path, filePath);
  ArrStrCat(&path, ".txt");
  WrFile(path, b64Data, StrLen(b64Data));
  RmArr(path);
  Free(b64Data);
}

void EdLoad() {
  Spr spr = MkSprFromFile(filePath);
  if (spr) {
    EdFlushUpds();
    RmArr(imgData);
    imgData = SprToArgbArr(spr);
    width = SprWidth(spr);
    height = SprHeight(spr);
    EdUpdMesh();
    RmSpr(spr);
    EdFlushUpds();
  }
}

void EdOnMotion(float dx, float dy) {
  cX += dx;
  cY += dy;
  if (flags & ED_FDRAGGING) {
    oX += dx;
    oY += dy;
    EdUpdTrans();
  }
  if (flags & ED_FPAINTING) { EdPainting(); }
}

void EdHandleKeyDown(int key, int state) {
  switch (key) {
    case A:
    case MMID: { flags |= ED_FDRAGGING; break; }
    case C: {
      if (tool == ED_COLOR_PICKER) {
        flags ^= ED_FEXPAND_COLOR_PICKER;
      } else {
        tool = ED_COLOR_PICKER;
      }
      break;
    }
    case E: { tool = ED_PENCIL; break; }
    case S: {
      if (state & FCTRL) {
        EdSave();
      } else if (state & FSHIFT) {
        EdPaste(effectiveSelRect, ED_FALPHA_BLEND);
        EdClrSelection();
      } else {
        tool = ED_SELECT;
      }
      break;
    }
    case R: {
      if (flags & ED_FSELECT) {
        EdCrop(effectiveSelRect);
      }
      break;
    }
    case N: {
      if (state & FSHIFT) {
        flags &= ~ED_FANIM;
      } else if (flags & ED_FSELECT) {
        MemCpy(animRect, effectiveSelRect, sizeof(animRect));
        MemCpy(animStartRect, effectiveSelRect, sizeof(animStartRect));
        flags |= ED_FANIM;
      }
      break;
    }
    case K1: {
      if (state & FCTRL) {
        EdSaveOneBpp();
      } else {
        EdFillRect(effectiveSelRect, cols[0]);
      }
      break;
    }
    case K2: { EdFillRect(effectiveSelRect, cols[1]); break; }
    case D: {
      if (state & FSHIFT) {
        EdUnyank();
      } else {
        EdYank();
      }
      break;
    }
    case T: {
      if (EdUpdPaintRateLimiter()) {
        EdPaste(effectiveSelRect, ED_FALPHA_BLEND);
      }
      break;
    }
    case RIGHT_BRACKET:
    case MWHEELUP: { EdOnWheel(1); break; }
    case LEFT_BRACKET:
    case MWHEELDOWN: { EdOnWheel(-1); break; }
    case Q:
    case W:
    case MLEFT:
    case MRIGHT: {
      EdClrPaintRateLimiter();
      flags |= ED_FPAINTING;
      colIndex = key == MRIGHT || key == W ? 1 : 0;
      selAnchor = key == MRIGHT || key == W ? ED_SELECT_MID : ED_SELECT_NONE;
      EdBeginPainting();
      EdPainting();
      EdFlushUpds();
      break;
    }
    case F1: {
      flags ^= ED_FHELP;
      break;
    }
    case H: { wishX = -1; break; }
    case L: { wishX = 1; break; }
    case K: { wishY = -1; break; }
    case J: { wishY = 1; break; }
    case LEFT_SHIFT:
    case RIGHT_SHIFT: { flags |= ED_FHOLDING_SHIFT; break; }
  }
}

void EdHandleKeyUp(int key) {
  switch (key) {
    case A:
    case MMID: { flags &= ~ED_FDRAGGING; break; }
    case Q:
    case W:
    case MLEFT:
    case MRIGHT: { flags &= ~ED_FPAINTING; break; }
    case LEFT_SHIFT:
    case RIGHT_SHIFT: { flags &= ~ED_FHOLDING_SHIFT; break; }
    case H:
    case L: { wishX = 0; break; }
    case K:
    case J: { wishY = 0; break; }
  }
}

void EdUpd() {
  if (flags & ED_FDIRTY) {
    flushTimer += Delta(wnd);
    /* flush updates periodically or if there's too many queued */
    if (flushTimer > ED_FLUSH_TIME || ArrLen(updates) > ED_FLUSH_THRESHOLD) {
      EdFlushUpds();
      flushTimer = Max(0, flushTimer - ED_FLUSH_TIME);
    }
  } else {
    flushTimer = 0;
  }

  if (wishX || wishY) {
    float dx = wishX * (flags & ED_FHOLDING_SHIFT ? 300 : 100) * Delta();
    float dy = wishY * (flags & ED_FHOLDING_SHIFT ? 300 : 100) * Delta();
    flags |= ED_FSHOW_CURSOR;
    EdOnMotion(dx, dy);
  }

  selBlinkTimer += Delta();
  selBlinkTimer = FltMod(selBlinkTimer, 2);

  EdSelectedRect(effectiveSelRect);

  if (flags & ED_FANIM) {
    animTimer += Delta();
    while (animTimer >= 1.0f / animSpeed) {
      float extents[4];
      float* r = animRect;
      int x, y, empty = 1;
      animTimer -= 1.0f / animSpeed;
      SetRectPos(r, RectX(r) + RectWidth(r), RectY(r));
      SetRect(extents, 0, width, 0, height);
      if (RectInRect(r, extents)) {
        for (y = (int)RectTop(r); y < (int)RectBot(r); ++y) {
          for (x = (int)RectLeft(r); x < (int)RectRight(r); ++x) {
            if ((imgData[y * width + x] & 0xff000000) != 0xff000000) {
              empty = 0;
            }
          }
        }
      }
      if (empty) {
        MemCpy(r, animStartRect, sizeof(animRect));
      }
    }
  }
}

void EdPaintPix(int x, int y, int col, int paintFlags) {
  float rect[4];
  int fcol = col;
  SetRect(rect, 0, width, 0, height);
  if ((flags & ED_FSELECT) && !(flags & ED_FIGNORE_SELECTION)) {
    ClampRect(rect, effectiveSelRect);
  }
  if (PtInRect(rect, x, y)) {
    int* px = &imgData[y * width + x];
    if (paintFlags & ED_FALPHA_BLEND) {
      /* normal alpha blending with whatever is currently here */
      fcol = AlphaBlend(col, *px);
    }
    if (*px != fcol) {
      EDUpd* u;
      *px = fcol;
      u = ArrAlloc(&updates, 1);
      u->x = x;
      u->y = y;
      u->col = col;
      u->flags = flags;
      flags |= ED_FDIRTY;
    }
  }
}

void EdFlushUpds() {
  Pixs(img, width, height, imgData);
  SetArrLen(updates, 0);
  flags &= ~ED_FDIRTY;
}

void EdClrPaintRateLimiter() {
  lastPutX = -1;
}

int EdUpdPaintRateLimiter() {
  float p[2];
  int x, y;
  p[0] = cX;
  p[1] = cY;
  EdMapToImg(p);
  x = (int)p[0];
  y = (int)p[1];
  if (x != lastPutX || y != lastPutY) {
    lastPutX = x;
    lastPutY = y;
    return 1;
  }
  return 0;
}

void EdPutUpds() {
  int i;
  Mesh mesh = MkMesh();
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
  PutMesh(mesh, ToTmpMat(trans), 0);
  RmMesh(mesh);
}

/* cover rest of the area in grey to distinguish it from a transparent image */
void EdPutBorders() {
  /* TODO: only regenerate mesh when the mat changes */
  int winWidth = WndWidth(wnd);
  int winHeight = WndHeight(wnd);
  int right, bottom;
  int scalEdWidth = width * scale;
  int scalEdHeight = height * scale;
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

void EdPutCursor() {
  /* TODO: do not use a tmp mesh for the cursor */
  int x = (int)cX;
  int y = (int)cY;
  Mesh mesh = MkMesh();
  Col(mesh, 0xffffff);
  Tri(mesh, x, y, x, y + 20, x + 10, y + 17);
  Col(mesh, cols[colIndex]);
  Tri(mesh, x+1, y+3, x+1, y + 18, x + 8, y + 16);
  PutMesh(mesh, 0, 0);
  RmMesh(mesh);
}

void EdPut() {
  PutMeshRaw(mesh, ToTmpMat(trans), checkerImg);
  PutMeshRaw(mesh, ToTmpMat(trans), img);
  EdPutBorders();
  EdPutUpds();
  if (cutMesh) {
    Mat copy = ToMat(cutTrans);
    PutMeshRaw(cutMesh, MulMat(copy, ToTmpMat(trans)), cutImg);
    RmMat(copy);
  }
  if (flags & ED_FSELECT) {
    EdPutSelect();
  }
  if (tool == ED_COLOR_PICKER) {
    EdPutColPicker();
  }
  if (flags & ED_FSHOW_CURSOR) {
    EdPutCursor();
  }
  if (flags & ED_FHELP) {
    PutMesh(helpBgMesh, 0, 0);
    PutMesh(helpTextMesh, 0, FtImg(font));
  }
  if (flags & ED_FANIM) {
    Mesh mesh;
    char* str = 0;
    float* r = animRect;
    int x = (int)(WndWidth(wnd) - RectWidth(r) * scale);
    int w = RectWidth(r) * scale;
    int h = RectHeight(r) * scale;

    mesh = MkMesh();
    Col(mesh, 0x7f000000);
    Quad(mesh, x - 10, h + 10, w + 5, 20);
    PutMesh(mesh, 0, 0);
    RmMesh(mesh);

    mesh = MkMesh();
    ImgQuad(mesh, x, 5, RectLeft(r), RectTop(r), w, h, RectWidth(r), RectHeight(r));
    PutMeshRaw(mesh, 0, img);
    RmMesh(mesh);

    ArrStrCat(&str, "anim spd: ");
    ArrStrCatI32(&str, animSpeed, 10);
    ArrStrCat(&str, " fps");
    ArrCat(&str, 0);
    PutFt(font, 0xbebebe, x, (int)(RectHeight(r) * scale) + 15, str);
    RmArr(str);
  }
}

int EdSampleCheckerboard(float x, float y) {
  int data[4] = { 0xaaaaaa, 0x666666, 0x666666, 0xaaaaaa };
  x /= ED_CHECKER_SIZE;
  y /= ED_CHECKER_SIZE;
  return data[((int)y % 2) * 2 + ((int)x % 2)];
}

Img EdCreateCheckerImg() {
  int x, y;
  int* data = 0;
  Img img = MkImg();
  for (y = 0; y < ED_CHECKER_SIZE * 2; ++y) {
    for (x = 0; x < ED_CHECKER_SIZE * 2; ++x) {
      ArrCat(&data, EdSampleCheckerboard(x, y));
    }
  }
  Pixs(img, ED_CHECKER_SIZE * 2, ED_CHECKER_SIZE * 2, data);
  RmArr(data);
  return img;
}

Img EdCreateSelBorderImg() {
  Img img = MkImg();
  int data[4*4] = {
    0xff000000, 0xff000000, 0xff000000, 0xff000000,
    0xff000000, 0x00ffffff, 0x00ffffff, 0xff000000,
    0xff000000, 0x00ffffff, 0x00ffffff, 0xff000000,
    0xff000000, 0xff000000, 0xff000000, 0xff000000
  };
  return Pixs(img, 4, 4, data);
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

void Init() {
  wnd = AppWnd();
  filePath = Argc() > 1 ? Argv(1) : "out.wbspr";
  scale = 4;
  cX = WndWidth(wnd) / 2;
  cY = WndHeight(wnd) / 2;
  checkerImg = EdCreateCheckerImg();
  img = MkImg();
  cutImg = MkImg();
  width = ED_IMGSIZE;
  height = ED_IMGSIZE;
  imgData = EdCreatePixs(0xff000000, width, height);
  trans = MkTrans();
  colPickerTrans = MkTrans();
  cutTrans = MkTrans();
  tool = ED_PENCIL;
  grey = 0xffffff;
  cols[1] = 0xff000000;
  selBorderImg = EdCreateSelBorderImg();
  animSpeed = 5;

  flags |= ED_FHELP;
  font = DefFt();
  /* TODO: scale with screen size */
  /* TODO: actual proper automatic ui layoutting */
  helpTextMesh = MkMesh();
  Col(helpTextMesh, 0xbebebe);
  FtMesh(helpTextMesh, font, 15, 15, EdHelpString());
  helpBgMesh = MkMesh();
  Col(helpBgMesh, 0x33000000);
  Quad(helpBgMesh, 5, 5, 610, 405);

  EdClrSelection();
  EdUpdColPickerMesh();
  EdUpdMesh();
  EdFlushUpds();
  EdSizeColPicker();
  EdClrPanning();
  EdLoad();
}

void Quit() {
  EdClrYank();
  EdClrSelection();
  RmMesh(cutMesh);
  RmMesh(mesh);
  RmMesh(colPickerMesh);
  RmMesh(helpTextMesh);
  RmMesh(helpBgMesh);
  RmArr(imgData);
  RmArr(updates);
  RmImg(img);
  RmImg(cutImg);
  RmImg(checkerImg);
  RmImg(selBorderImg);
  RmTrans(trans);
  RmTrans(colPickerTrans);
  RmTrans(cutTrans);
  RmFt(font);
}

void Size() {
  EdSizeColPicker();
}

void KeyDown() {
  EdHandleKeyDown(Key(wnd), KeyState(wnd));
}

void KeyUp() {
  EdHandleKeyUp(Key(wnd));
}

void Motion() {
  cX = MouseX(wnd) - MouseDX(wnd);
  cY = MouseY(wnd) - MouseDY(wnd);
  flags &= ~ED_FSHOW_CURSOR;
  EdOnMotion(MouseDX(wnd), MouseDY(wnd));
}

void Frame() {
  EdUpd();
  EdPut();
}

void AppInit() {
  SetAppClass("WeebCoreSpriteEd");
  SetAppName("WeebCore Sprite Ed");
  On(INIT, Init);
  On(QUIT, Quit);
  On(SIZE, Size);
  On(KEYDOWN, KeyDown);
  On(KEYUP, KeyUp);
  On(MOTION, Motion);
  On(FRAME, Frame);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
