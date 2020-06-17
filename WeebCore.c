#ifndef WEEBCORE_HEADER
#define WEEBCORE_HEADER

/* ---------------------------------------------------------------------------------------------- */
/*                                        PLATFORM LAYER                                          */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample platform layers or write */
/* your own by defining these structs and funcs                                                   */
/* ---------------------------------------------------------------------------------------------- */

/* opaque handles */
typedef struct _Wnd* Wnd;
typedef struct _Clk* Clk;

Wnd MkWnd();
void RmWnd(Wnd wnd);
void SetWndName(Wnd wnd, char* wndName);
void SetWndClass(Wnd wnd, char* className);
void SetWndSize(Wnd wnd, int width, int height);
int WndWidth(Wnd wnd);
int WndHeight(Wnd wnd);

/* get time elapsed in seconds since the last SwpBufs. guaranteed to return non-zero even if
 * no SwpBufs happened */
float Delta(Wnd wnd);

/* FPS limiter, 0 for unlimited. limiting happens in SwpBufs. note that unlimited fps still
 * waits for the minimum timer resolution for GetTime */
void SetWndFPS(Wnd wnd, int fps);

/* fetch one message. returns non-zero as long as there are more */
int NextMsg(Wnd wnd);

/* sends QUIT message to the wnd */
void PostQuitMsg(Wnd wnd);

/* these funcs get data from the last message fetched by NextMsg */
int MsgType(Wnd wnd);
int Key(Wnd wnd);
int KeyState(Wnd wnd);
int MouseX(Wnd wnd);
int MouseY(Wnd wnd);
int MouseDX(Wnd wnd);
int MouseDY(Wnd wnd);

/* allocates n bytes and initializes memory to zero */
void* Alloc(int n);

/* reallocate p to new size n. memory that wasn't initialized is not guaranteed to be zero */
void* Realloc(void* p, int n);

void Free(void* p);
void MemSet(void* p, unsigned char val, int n);
void MemCpy(void* dst, void* src, int n);

/* write data to disk. returns number of bytes written or < 0 for errors */
int WrFile(char* path, void* data, int dataLen);

/* read up to maxSize bytes from disk */
int RdFile(char* path, void* data, int maxSize);

/* ---------------------------------------------------------------------------------------------- */
/*                                           RENDERER                                             */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample renderers or write your  */
/* own by defining these structs and funcs                                                        */
/* ---------------------------------------------------------------------------------------------- */

/* opaque handles */
typedef struct _Mesh* Mesh;
typedef struct _Img* Img;
typedef struct _Mat* Mat;
typedef struct _Trans* Trans;

/* ---------------------------------------------------------------------------------------------- */

/* Mesh is a collection of vertices that will be rendered using the same img and trans. try
 * to batch up as much stuff into a mesh as possible for optimal performance. */

Mesh MkMesh();
void RmMesh(Mesh mesh);

/* change current color. color is Argb 32-bit int (0xAARRGGBB). 255 alpha = completely transparent,
 * 0 alpha = completely opaque */
void Col(Mesh mesh, int color);

/* these are for custom meshes.
 * wrap Vert and Face calls between Begin and End.
 * indices start at zero and we are indexing the vertices submitted between Begin and End. see the
 * implementations of Quad and Tri for examples */
void Begin(Mesh mesh);
void End(Mesh mesh);
void Vert(Mesh mesh, float x, float y);
void ImgCoord(Mesh mesh, float u, float v);
void Face(Mesh mesh, int i1, int i2, int i3);
void PutMesh(Mesh mesh, Mat mat, Img img);

/* ---------------------------------------------------------------------------------------------- */

/* OpenGL-like post-multiplied trans. mat memory layout is row major */

Mat MkMat();
void RmMat(Mat mat);
Mat DupMat(Mat source);

/* these return mat for convienience. it's not actually a copy */
Mat SetIdentity(Mat mat);
Mat SetMat(Mat mat, float* matIn);
Mat GetMat(Mat mat, float* matOut);
Mat Scale(Mat mat, float x, float y);
Mat Scale1(Mat mat, float scale);
Mat Move(Mat mat, float x, float y);
Mat Rot(Mat mat, float deg);
Mat MulMat(Mat mat, Mat other);
Mat MulMatFlt(Mat mat, float* matIn);

/* ---------------------------------------------------------------------------------------------- */

Img MkImg();
void RmImg(Img img);

/* set wrap mode for img coordinates. default is REPEAT */
void SetImgWrapU(Img img, int mode);
void SetImgWrapV(Img img, int mode);

/* set min/mag filter for img. default is NEAREST */
void SetImgMinFilter(Img img, int filter);
void SetImgMagFilter(Img img, int filter);

/* set the img's pix data. must be an array of 0xAARRGGBB colors as explained in Col.
 * pixs are laid out row major - for example a 4x4 image would be:
 * { px00, px10, px01, px11 }
 * note that this is usually an expensive call. only update the img data when it's actually
 * changing.
 * return img for convenience */
Img Pixs(Img img, int width, int height, int* data);

/* same as Pixs but you can specify stride which is how many bytes are between the beginning
 * of each row, for cases when you have extra padding or when you're submitting a sub-region of a
 * bigger image */
Img PixsEx(Img img, int width, int height, int* data, int stride);

/* ---------------------------------------------------------------------------------------------- */

/* flush all rendered geometry to the screen */
void SwpBufs(Wnd wnd);

/* set rendering rectangle from the top left of the wnd, in pixs. this is automatically called
 * when the wnd is resized. it can also be called manually to render to a subregion of the
 * wnd. all coordinates passed to other rendering functions start from the top left corner of
 * this rectangle. */
void Viewport(Wnd wnd, int x, int y, int width, int height);

/* the initial color of a blank frame */
void ClsCol(int color);

/* ---------------------------------------------------------------------------------------------- */
/*                                         MATH FUNCTIONS                                         */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample implementations or write */
/* your own by defining these funcs                                                               */
/* ---------------------------------------------------------------------------------------------- */

float FltMod(float x, float y); /* returns x modulo y */
float Sin(float deg);
float Cos(float deg);
int Ceil(float x);
int Floor(float x);
float Sqrt(float x);

/* ---------------------------------------------------------------------------------------------- */
/*                                    BUILT-IN MATH FUNCTIONS                                     */
/* ---------------------------------------------------------------------------------------------- */

float Lerp(float a, float b, float amount);
void LerpFlts(int n, float* a, float* b, float* result, float amount);
void MulFltsScalar(int n, float* floats, float* result, float scalar);
void FloorFlts(int n, float* floats, float* result);
void ClampFlts(int n, float* floats, float* result, float min, float max);
void AddFlts(int n, float* a, float* b, float* result);

/* these funcs operate on a rectangle represented as an array of 4 floats (left, top, right, bot) */
void SetRect(float* rect, float left, float right, float top, float bot);
void SetRectPos(float* rect, float x, float y);
void SetRectSize(float* rect, float width, float height);
void SetRectLeft(float* rect, float left);
void SetRectRight(float* rect, float right);
void SetRectTop(float* rect, float top);
void SetRectBot(float* rect, float bot);
void NormRect(float* rect); /* swaps values around so width/height aren't negative */
void ClampRect(float* rect, float* other); /* clamp rect to be inside of other rect */
float RectWidth(float* rect);
float RectHeight(float* rect);
float RectX(float* rect);
float RectY(float* rect);
float RectLeft(float* rect);
float RectRight(float* rect);
float RectTop(float* rect);
float RectBot(float* rect);
int PtInRect(float* rect, float x, float y); /* check if xy lies in rect. must be normalized. */
int RectSect(float* a, float* b);

/* check that needle is entirely inside of haystack */
int RectInRect(float* needle, float* haystack);

/* check that needle's area can entirely fit inside of haystack (ignores position) */
int RectInRectArea(float* needle, float* haystack);

/* ---------------------------------------------------------------------------------------------- */
/*                                          SPR FORMAT                                          */
/* this is meant as simple RLE compression for 2D spriteswith a limited color palette             */
/* ---------------------------------------------------------------------------------------------- */

typedef struct _Spr* Spr;

Spr MkSpr(char* data, int length);
Spr MkSprFromArr(char* data);
Spr MkSprFromFile(char* filePath);
void RmSpr(Spr spr);
int SprWidth(Spr spr);
int SprHeight(Spr spr);
void SprToArgb(Spr spr, int* argb);
int* SprToArgbArr(Spr spr);

/* returns a resizable array that must be freed with RmArr */
char* ArgbToSprArr(int* argb, int width, int height);

/* Format Details:
 *
 * char[4] "SP"
 * varint formatVersion
 * varint width, height
 * varint paletteLength
 * int32 palette[paletteLength]
 * Blob {
 *   varint type (repeat/data)
 *   varint size
 *   varint paletteIndices[size] <- 1 varint for repeat
 * }
 * ...
 *
 * varint's are protobuf style encoded integers. see EncVarI32 */

/* ---------------------------------------------------------------------------------------------- */
/*                                          RENDER UTILS                                          */
/* ---------------------------------------------------------------------------------------------- */

/* create a trans from scale, position, origin, rot applied in a fixed order */

Trans MkTrans();
void RmTrans(Trans trans);
void ClrTrans(Trans trans);

/* these return trans for convenience. it's not actually a copy */
Trans SetScale(Trans trans, float x, float y);
Trans SetScale1(Trans trans, float scale);
Trans SetPos(Trans trans, float x, float y);
Trans SetOrig(Trans trans, float x, float y);
Trans SetRot(Trans trans, float deg);

Mat ToMat(Trans trans);

/* the ortho version of To* functions produce a mat that is orthogonal (doesn't apply scale
 * among other things) this is useful for trivial inversion */
Mat ToMatOrtho(Trans trans);

/* the mat returned by this does not need to be destroyed. it will be automatically destroyed
 * when RmTrans is called.
 *
 * note that subsequent calls to this function will invalidate the previously generated mat
 *
 * Ortho version does not share the same trans as the non-Ortho so it doesnt invalidate it
 *
 * it's recommended to not hold onto these Mat pointers either way. if the Trans doesn't change,
 * the Mat will not be recalculated, so it's cheap to call these everywhere */
Mat ToTmpMat(Trans trans);
Mat ToTmpMatOrtho(Trans trans);

/* trans 2D point in place */
void TransPt(Mat mat, float* point);

/* trans 2D point in place by inverse of trans note that this only works if the mat is
 * orthogonal, which means that it cannot have scale */
void InvTransPt(Mat mat, float* point);

/* add a rectangle to mesh */
void Quad(Mesh mesh, float x, float y, float width, float height);

/* add a textured rectangle to mesh. note that the u/v coordinates are in pixels, since we usually
 * want to work pixel-perfect in 2d */
void ImgQuad(Mesh mesh,
  float x, float y, float u, float v,
  float width, float height, float uWidth, float vHeight
);

/* add a triangle to mesh */
void Tri(Mesh mesh, float x1, float y1, float x2, float y2, float x3, float y3);
void ImgTri(Mesh mesh,
  float x1, float y1, float u1, float v1,
  float x2, float y2, float u2, float v2,
  float x3, float y3, float u3, float v3
);

/* draw gradients using vertex color interpolation */

void QuadGradH(Mesh mesh, float x, float y, float width, float height, int n, int* colors);
void QuadGradV(Mesh mesh, float x, float y, float width, float height, int n, int* colors);

/* convert Argb color to { r, g, b, a } (0.0-1.0) */
void ColToFlts(int color, float* floats);

/* convert { r, g, b, a } (0.0-1.0) to Argb color */
int FltsToCol(float* f);

/* linearly interpolate between colors a and b */
int Mix(int a, int b, float amount);

/* multiply color's rgb values by scalar (does not touch alpha) */
int MulScalar(int color, float scalar);

/* add colors together (clamps values to avoid overflow) */
int Add(int a, int b);

/* alpha blend between color src and dst. alpha is not premultiplied */
int AlphaBlend(int src, int dst);

/* same as alpha blend but blends in place into dst */
void AlphaBlendp(int* dst, int src);

/* convert rgba pixs to 1bpp. all non-transparent colors become a 1 and transparency becomes 0.
 * the data is tightly packed (8 pixs per byte).
 * the returned data is an array and must be freed with ArrFree */
char* ArgbToOneBpp(int* pixs, int numPixs);
char* ArgbArrToOneBpp(int* pixs);
int* OneBppToArgb(char* data, int numBytes);
int* OneBppArrToArgb(char* data);

/* ---------------------------------------------------------------------------------------------- */
/*                                        BITMAP FONTS                                            */
/* ---------------------------------------------------------------------------------------------- */

typedef struct _Ft* Ft;

/* simplest form of bitmap ft.
 *
 * this assumes pixs is a tightly packed grid of 32x3 characters that cover ascii 0x20-0x7f
 * (from space to the end of the ascii table).
 *
 * while this is the fastest way for the renderer to look up characters, it should only be used in
 * cases where you have a hardcoded ft that's never gonna change. it was created to bootstrap
 * the built-in ft */
Ft MkFtFromSimpleGrid(int* pixs, int width, int height, int charWidth, int charHeight);
Ft MkFtFromSimpleGridFile(char* filePath, int charWidth, int charHeight);

/* basic built in bitmap ft */
Ft DefFt();

void RmFt(Ft ft);
Img FtImg(Ft ft);

/* generate vertices and uv's for drawing string. must be rendered with the ft img from
 * FtImg or a img that has the same exact layout */
void FtMesh(Mesh mesh, Ft ft, int x, int y, char* string);

void PutFt(Ft ft, int x, int y, char* string);

/* ---------------------------------------------------------------------------------------------- */
/*                                       RESIZABLE ARRAYS                                         */
/*                                                                                                */
/* make sure your initial pointer is initialized to NULL which counts as an empty array.          */
/* these are special fat pointers and must be freed with RmArr                                    */
/* ---------------------------------------------------------------------------------------------- */

void RmArr(void* array);
int ArrLen(void* array);
void SetArrLen(void* array, int len);

/* shorthand macro to append a single element to the array */
#define ArrCat(pArr, x) { \
  int len = ArrLen(*(pArr)); \
  ArrReserve((pArr), 1); \
  (*(pArr))[len] = (x); \
  SetArrLen(*(pArr), len + 1); \
}

/* reserve memory for at least numElements extra elements.
 * returns a pointer to the end of the array */
#define ArrReserve(pArr, numElements) \
  ArrReserveEx((void**)(pArr), sizeof((*pArr)[0]), numElements)

/* reserve memory for at least numElements extra elements and set the
 * array length to current length + numElements.
 * returns a pointer to the beginning of the new elements */
#define ArrAlloc(pArr, numElements) \
  ArrAllocEx((void**)(pArr), sizeof((*pArr)[0]), numElements)

void ArrStrCat(char** pArr, char* str);

void* ArrReserveEx(void** pArr, int elementSize, int numElements);
void* ArrAllocEx(void** pArr, int elementSize, int numElements);

/* ---------------------------------------------------------------------------------------------- */
/*                                   MISC UTILS AND MACROS                                        */
/* ---------------------------------------------------------------------------------------------- */

#define Min(x, y) ((x) < (y) ? (x) : (y))
#define Max(x, y) ((x) > (y) ? (x) : (y))
#define Clamp(x, min, max) Max(min, Min(max, x))

/* (a3, b3) = (a1, b1) - (a2, b2) */
#define Sub2(a1, b1, a2, b2, a3, b3) \
  ((a3) = (a1) - (a2)), \
  ((b3) = (b1) - (b2))

/* cross product between (x1, y1) and (x2, y2) */
#define Cross(x1, y1, x2, y2) ((x1) * (y2) - (y1) * (x2))

#define ToRad(deg) ((deg) / 360 * PI * 2)

/* return the closest power of two that is higher than x */
int RoundUpToPowerOfTwo(int x);

/* null-terminated string utils */
int StrLen(char* s);
void StrCpy(char* dst, char* src);

/* mem utils */
int MemCmp(void* a, void* b, int n);

/* protobuf style varints. encoded in base 128. each byte contains 7 bits of the integer and the
 * msb is set if there's more. byte order is little endian */

int DecVarI32(char** pData);          /* increments *pData past the varint */
int EncVarI32(void* data, int x);     /* returns num of bytes written */
void CatVarI32(char** pArr, int x); /* append to resizable arr */

/* encode integers in little endian */
int EncI32(void* data, int x);
int DecI32(char** pData);
void CatI32(char** pArr, int x);

void SwpFlts(float* a, float* b);

/* encode data to a b64 string. returned string must be freed with Free */
char* ToB64(void* data, int dataSize);
char* ArrToB64(char* data);
char* ArrFromB64(char* b64Data);

/* ---------------------------------------------------------------------------------------------- */
/*                                      ENUMS AND CONSTANTS                                       */
/* ---------------------------------------------------------------------------------------------- */

#ifndef PI
#define PI 3.141592653589793238462643383279502884
#endif

/* message types returned by MsgType */
enum {
  QUIT = 1,
  KEYDOWN,
  KEYUP,
  MOTION,
  SIZE,
  LAST_EVENT_TYPE
};

/* flags for the bitfield returned by MsgKeyState */
enum {
  FSHIFT     = 1<<1,
  FCONTROL   = 1<<2,
  FALT       = 1<<3,
  FSUPER     = 1<<4,
  FCAPS_LOCK = 1<<5,
  FNUM_LOCK  = 1<<6,
  FREPEAT    = 1<<7,
  FLAST_STATE
};

#define FCTRL FCONTROL

#define MLEFT MOUSE1
#define MMID MOUSE2
#define MRIGHT MOUSE3
#define MWHEELUP MOUSE4
#define MWHEELDOWN MOUSE5

/* keys returned by MsgKey */
enum {
  MOUSE1          =   1,
  MOUSE2          =   2,
  MOUSE3          =   3,
  MOUSE4          =   4,
  MOUSE5          =   5,
  SPACE           =  32,
  APOSTROPHE      =  39,
  COMMA           =  44,
  MINUS           =  45,
  PERIOD          =  46,
  SLASH           =  47,
  K0              =  48,
  K1              =  49,
  K2              =  50,
  K3              =  51,
  K4              =  52,
  K5              =  53,
  K6              =  54,
  K7              =  55,
  K8              =  56,
  K9              =  57,
  SEMICOLON       =  59,
  EQUAL           =  61,
  A               =  65,
  B               =  66,
  C               =  67,
  D               =  68,
  E               =  69,
  F               =  70,
  G               =  71,
  H               =  72,
  I               =  73,
  J               =  74,
  K               =  75,
  L               =  76,
  M               =  77,
  N               =  78,
  O               =  79,
  P               =  80,
  Q               =  81,
  R               =  82,
  S               =  83,
  T               =  84,
  U               =  85,
  V               =  86,
  W               =  87,
  X               =  88,
  Y               =  89,
  Z               =  90,
  LEFT_BRACKET    =  91,
  BACKSLASH       =  92,
  RIGHT_BRACKET   =  93,
  GRAVE_ACCENT    =  96,
  WORLD_1         = 161,
  WORLD_2         = 162,
  ESCAPE          = 256,
  ENTER           = 257,
  TAB             = 258,
  BACKSPACE       = 259,
  INSERT          = 260,
  DELETE          = 261,
  RIGHT           = 262,
  LEFT            = 263,
  DOWN            = 264,
  UP              = 265,
  PAGE_UP         = 266,
  PAGE_DOWN       = 267,
  HOME            = 268,
  END             = 269,
  CAPS_LOCK       = 280,
  SCROLL_LOCK     = 281,
  NUM_LOCK        = 282,
  PRINT_SCREEN    = 283,
  PAUSE           = 284,
  F1              = 290,
  F2              = 291,
  F3              = 292,
  F4              = 293,
  F5              = 294,
  F6              = 295,
  F7              = 296,
  F8              = 297,
  F9              = 298,
  F10             = 299,
  F11             = 300,
  F12             = 301,
  F13             = 302,
  F14             = 303,
  F15             = 304,
  F16             = 305,
  F17             = 306,
  F18             = 307,
  F19             = 308,
  F20             = 309,
  F21             = 310,
  F22             = 311,
  F23             = 312,
  F24             = 313,
  F25             = 314,
  KP_0            = 320,
  KP_1            = 321,
  KP_2            = 322,
  KP_3            = 323,
  KP_4            = 324,
  KP_5            = 325,
  KP_6            = 326,
  KP_7            = 327,
  KP_8            = 328,
  KP_9            = 329,
  KP_DECIMAL      = 330,
  KP_DIVIDE       = 331,
  KP_MULTIPLY     = 332,
  KP_SUBTRACT     = 333,
  KP_ADD          = 334,
  KP_ENTER        = 335,
  KP_EQUAL        = 336,
  LEFT_SHIFT      = 340,
  LEFT_CONTROL    = 341,
  LEFT_ALT        = 342,
  LEFT_SUPER      = 343,
  RIGHT_SHIFT     = 344,
  RIGHT_CONTROL   = 345,
  RIGHT_ALT       = 346,
  RIGHT_SUPER     = 347,
  MENU            = 348,
  LAST_KEY
};

/* img wrap modes */
enum {
  CLAMP_TO_EDGE,
  MIRRORED_REPEAT,
  REPEAT,
  LAST_IMG_WRAP
};

/* img filters */
enum {
  NEAREST,
  LINEAR,
  LAST_IMG_FILTER
};

#endif /* WEEBCORE_HEADER */

/* ############################################################################################## */
/* ############################################################################################## */
/* ############################################################################################## */
/*                                                                                                */
/*        Hdr part ends here. This is all you need to know to use it. Implementation below.       */
/*                                                                                                */
/* ############################################################################################## */
/* ############################################################################################## */
/* ############################################################################################## */

#if defined(WEEBCORE_IMPLEMENTATION) && !defined(WEEBCORE_OVERRIDE_MONOLITHIC_BUILD)

/* ---------------------------------------------------------------------------------------------- */

typedef struct _ArrHdr {
  int capacity;
  int length;
} ArrHdr;

static ArrHdr* GetArrHdr(void* array) {
  if (!array) return 0;
  return (ArrHdr*)array - 1;
}

void RmArr(void* array) {
  Free(GetArrHdr(array));
}

int ArrLen(void* array) {
  if (!array) return 0;
  return GetArrHdr(array)->length;
}

void SetArrLen(void* array, int length) {
  if (array) {
    GetArrHdr(array)->length = length;
  }
}

void ArrStrCat(char** pArr, char* str) {
  for (; *str; ++str) {
    ArrCat(pArr, *str);
  }
}

void* ArrReserveEx(void** pArr, int elementSize, int numElements) {
  ArrHdr* header;
  if (!*pArr) {
    int capacity = RoundUpToPowerOfTwo(Max(numElements, 16));
    header = Alloc(sizeof(ArrHdr) + elementSize * capacity);
    header->capacity = capacity;
    *pArr = header + 1;
  } else {
    int minCapacity;
    header = GetArrHdr(*pArr);
    minCapacity = header->length + numElements;
    if (header->capacity < minCapacity) {
      int newSize;
      while (header->capacity < minCapacity) {
        header->capacity *= 2;
      }
      newSize = sizeof(ArrHdr) + elementSize * header->capacity;
      header = Realloc(header, newSize);
      *pArr = header + 1;
    }
  }
  return (char*)*pArr + ArrLen(*pArr) * elementSize;
}

void* ArrAllocEx(void** pArr, int elementSize, int numElements) {
  void* res = ArrReserveEx(pArr, elementSize, numElements);
  SetArrLen(*pArr, ArrLen(*pArr) + numElements);
  return res;
}

/* ---------------------------------------------------------------------------------------------- */

/* https://graphics.stanford.edu/~seander/bithacks.html */
int RoundUpToPowerOfTwo(int x) {
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return ++x;
}

int StrLen(char* s) {
  char* p;
  for (p = s; *p; ++p);
  return p - s;
}

void StrCpy(char* dst, char* src) {
  while (*src) {
    *dst++ = *src++;
  }
}

int DecVarI32(char** pData) {
  unsigned char* p;
  int res = 0, shift = 0, more = 1;
  for (p = (unsigned char*)*pData; more; ++p) {
    more = *p & 0x80;
    res |= (*p & 0x7F) << shift;
    shift += 7;
  }
  *pData = (char*)p;
  return res;
}

int EncVarI32(void* data, int x) {
  unsigned char* p = data;
  unsigned u = (unsigned)x;
  while (1) {
    unsigned char* byte = p++;
    *byte = u & 0x7F;
    u >>= 7;
    if (u) {
      *byte |= 0x80;
    } else {
      break;
    }
  }
  return p - (unsigned char*)data;
}

void CatVarI32(char** pArr, int x) {
  char* p = ArrReserve(pArr, 4);
  int n = EncVarI32(p, x);
  SetArrLen(*pArr, ArrLen(*pArr) + n);
}

int EncI32(void* data, int x) {
  unsigned char* p = data;
  p[0] = (x & 0x000000ff) >>  0;
  p[1] = (x & 0x0000ff00) >>  8;
  p[2] = (x & 0x00ff0000) >> 16;
  p[3] = (x & 0xff000000) >> 24;
  return 4;
}

int DecI32(char** pData) {
  unsigned char* p = (unsigned char*)*pData;
  *pData += 4;
  return (p[0] <<  0) | (p[1] <<  8) | (p[2] << 16) | (p[3] << 24);
}

void CatI32(char** pArr, int x) {
  char* p = ArrAlloc(pArr, 4);
  EncI32(p, x);
}

int MemCmp(void* a, void* b, int n) {
  unsigned char* p1 = a;
  unsigned char* p2 = b;
  int i;
  for (i = 0; i < n; ++i) {
    if (*p1 < *p2) {
      return -1;
    } else if (*p1 > *p2) {
      return 1;
    }
  }
  return 0;
}

void SwpFlts(float* a, float* b) {
  float tmp = *a;
  *a = *b;
  *b = tmp;
}

/* ---------------------------------------------------------------------------------------------- */

struct _Trans {
  float sX, sY;
  float x, y;
  float oX, oY;
  float deg;
  Mat tempMat, tempMatOrtho;
  int dirty;
};

#define MAT_DIRTY (1<<0)
#define MAT_ORTHO_DIRTY (1<<1)

Trans MkTrans() {
  Trans trans = Alloc(sizeof(struct _Trans));
  trans->tempMat = MkMat();
  trans->tempMatOrtho = MkMat();
  ClrTrans(trans);
  return trans;
}

void RmTrans(Trans trans) {
  RmMat(trans->tempMat);
  RmMat(trans->tempMatOrtho);
  Free(trans);
}

void ClrTrans(Trans trans) {
  trans->sX = 1;
  trans->sY = 1;
  trans->x = trans->y =
  trans->oX = trans->oY =
  trans->deg = 0;
  trans->dirty = ~0;
}

Trans SetScale(Trans trans, float x, float y) {
  trans->sX = x;
  trans->sY = y;
  trans->dirty = ~0;
  return trans;
}

Trans SetScale1(Trans trans, float scale) {
  return SetScale(trans, scale, scale);
}

Trans SetPos(Trans trans, float x, float y) {
  trans->x = x;
  trans->y = y;
  trans->dirty = ~0;
  return trans;
}

Trans SetOrig(Trans trans, float x, float y) {
  trans->oX = x;
  trans->oY = y;
  trans->dirty = ~0;
  return trans;
}

Trans SetRot(Trans trans, float deg) {
  trans->deg = deg;
  trans->dirty = ~0;
  return trans;
}

static Mat CalcTrans(Trans trans, Mat mat, int ortho) {
  Move(mat, trans->x, trans->y);
  Rot(mat, trans->deg);
  if (!ortho) {
    Scale(mat, trans->sX, trans->sY);
  }
  return Move(mat, -trans->oX, -trans->oY);
}

Mat ToMat(Trans trans) { return CalcTrans(trans, MkMat(), 0); }
Mat ToMatOrtho(Trans trans) { return CalcTrans(trans, MkMat(), 1); }

Mat ToTmpMat(Trans trans) {
  if (trans->dirty & MAT_DIRTY) {
    SetIdentity(trans->tempMat);
    CalcTrans(trans, trans->tempMat, 0);
    trans->dirty &= ~MAT_DIRTY;
  }
  return trans->tempMat;
}

Mat ToTmpMatOrtho(Trans trans) {
  if (trans->dirty & MAT_ORTHO_DIRTY) {
    SetIdentity(trans->tempMatOrtho);
    CalcTrans(trans, trans->tempMatOrtho, 1);
    trans->dirty &= ~MAT_ORTHO_DIRTY;
  }
  return trans->tempMatOrtho;
}

/* ---------------------------------------------------------------------------------------------- */

void TransPt(Mat mat, float* point) {
  Mat copy = DupMat(mat);
  float mPt[16];
  MemSet(mPt, 0, sizeof(mPt));
  MemCpy(mPt, point, sizeof(float) * 2);
  mPt[3] = 1;
  MulMatFlt(copy, mPt);
  GetMat(copy, mPt);
  MemCpy(point, mPt, sizeof(float) * 2);
  RmMat(copy);
}

void InvTransPt(Mat mat, float* point) {
  /* https://en.wikibooks.org/wiki/GLSL_Programming/Applying_Matrix_Transformations#Transforming_Pts_with_the_Inverse_Matrix */
  Mat copy = MkMat();
  float m[16];
  GetMat(mat, m);
  point[0] -= m[12];
  point[1] -= m[13];
  m[12] = m[13] = m[14] = m[15] = m[3] = m[7] = m[11] = 0; /* equivalent of taking the 3x3 mat */
  TransPt(SetMat(copy, m), point);
  RmMat(copy);
}

void ImgTri(Mesh mesh,
  float x1, float y1, float u1, float v1,
  float x2, float y2, float u2, float v2,
  float x3, float y3, float u3, float v3
) {
  /* ensure vertex order is counter-clockwise so we can cull backfaces */
  float v1x, v1y, v2x, v2y;
  float i2 = 1, i3 = 2;
  Sub2(x2, y2, x1, y1, v1x, v1y);
  Sub2(x3, y3, x1, y1, v2x, v2y);
  if (Cross(v1x, v1y, v2x, v2y) > 0) {
    i2 = 2, i3 = 1;
  }
  Begin(mesh);
  Vert(mesh, x1, y1); ImgCoord(mesh, u1, v1);
  Vert(mesh, x2, y2); ImgCoord(mesh, u2, v2);
  Vert(mesh, x3, y3); ImgCoord(mesh, u3, v3);
  Face(mesh, 0, i2, i3);
  End(mesh);
}

void Tri(Mesh mesh, float x1, float y1, float x2, float y2, float x3, float y3) {
  ImgTri(mesh, x1, y1, 0, 0, x2, y2, 0, 0, x3, y3, 0, 0);
}

void ImgQuad(Mesh mesh, float x, float y, float u, float v,
  float width, float height, float uWidth, float vHeight)
{
  Begin(mesh);
  Vert(mesh, x        , y         ); ImgCoord(mesh, u         , v          );
  Vert(mesh, x        , y + height); ImgCoord(mesh, u         , v + vHeight);
  Vert(mesh, x + width, y + height); ImgCoord(mesh, u + uWidth, v + vHeight);
  Vert(mesh, x + width, y         ); ImgCoord(mesh, u + uWidth, v          );
  Face(mesh, 0, 1, 2);
  Face(mesh, 0, 2, 3);
  End(mesh);
}


void Quad(Mesh mesh, float x, float y, float width, float height) {
  ImgQuad(mesh, x, y, 0, 0, width, height, width, height);
}


void QuadGradH(Mesh mesh, float x, float y, float width, float height, int n, int* colors) {
  int i;
  float off;
  Begin(mesh);
  for (i = 0; i < n; ++i) {
    int s = i * 2;
    off = (float)i / (n - 1) * width;
    Col(mesh, colors[i]);
    Vert(mesh, x + off, y);
    Vert(mesh, x + off, y + height);
    if (i < n - 1) {
      Face(mesh, s + 0, s + 1, s + 2);
      Face(mesh, s + 1, s + 3, s + 2);
    }
  }
  End(mesh);
}

void QuadGradV(Mesh mesh, float x, float y, float width, float height, int n, int* colors) {
  int i;
  float off;
  Begin(mesh);
  for (i = 0; i < n; ++i) {
    int s = i * 2;
    off = (float)i / (n - 1) * height;
    Col(mesh, colors[i]);
    Vert(mesh, x + width, y + off);
    Vert(mesh,         x, y + off);
    if (i < n - 1) {
      Face(mesh, s + 0, s + 1, s + 2);
      Face(mesh, s + 1, s + 3, s + 2);
    }
  }
  End(mesh);
}

void ColToFlts(int c, float* floats) {
  floats[0] = ((c & 0x00ff0000) >> 16) / 255.0f;
  floats[1] = ((c & 0x0000ff00) >>  8) / 255.0f;
  floats[2] = ((c & 0x000000ff) >>  0) / 255.0f;
  floats[3] = ((c & 0xff000000) >> 24) / 255.0f;
}

int FltsToCol(float* f) {
  int color = 0;
  color |= (int)(f[0] * 255.0f) << 16;
  color |= (int)(f[1] * 255.0f) <<  8;
  color |= (int)(f[2] * 255.0f) <<  0;
  color |= (int)(f[3] * 255.0f) << 24;
  return color;
}

char* ToB64(void* vdata, int dataSize) {
  static char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int i, len = (dataSize / 3 + 1) * 4 + 1;
  char* result = Alloc(len);
  char* p = result;
  unsigned char* data = vdata;
  for (i = 0; i < dataSize; i += 3) {
    p = &result[i / 3 * 4];
    switch (dataSize - i) {
      default: /* abusing fallthrough because it's fun */
      case 3:
        p[3] = charset[data[i + 2] & 0x3f];
        p[2] = ((data[i + 1] & 0x0f) << 2) | ((data[i + 2] & 0xc0) >> 6);
      case 2:
        p[2] = charset[p[2] | (data[i + 1] & 0x0f) << 2];
        p[1] = ((data[i] & 0x03) << 4) | ((data[i + 1] & 0xf0) >> 4);
      case 1:
        p[1] = charset[p[1] | (data[i] & 0x03) << 4];
        p[0] = charset[(data[i] & 0xfc) >> 2];
    }
  }
  if (!p[2]) p[2] = '=';
  if (!p[3]) p[3] = '=';
  return result;
}

#define NLETTERS ('Z' - 'A' + 1)

static char DecB64(char c) {
  if (c >= 'A' && c <= 'Z') {
    c -= 'A';
  } else if (c >= 'a' && c <= 'z') {
    c = c - 'a' + NLETTERS;
  } else if (c >= '0' && c <= '9') {
    c = c - '0' + NLETTERS * 2;
  } else if (c == '+') {
    c = NLETTERS * 2 + 10;
  } else if (c == '/') {
    c = NLETTERS * 2 + 10 + 1;
  } else {
    c = 0;
  }
  return c;
}

char* ArrFromB64(char* data) {
  char* result = 0;
  int len = StrLen(data), i;
  for (i = 0; i < len; i += 4) {
    int chunkLen = Min(3, len - i);
    char* p = ArrAlloc(&result, chunkLen);
    MemSet(p, 0, chunkLen);
    switch (len - i) {
      default:
      case 4:
        p[2] |= DecB64(data[i + 3]);        /* 6 bits */
      case 3:
        p[2] |= (DecB64(data[i + 2]) << 6); /* 2 bits */
        p[1] |= DecB64(data[i + 2]) >> 2;   /* 4 bits */
      case 2:
        p[1] |= (DecB64(data[i + 1]) << 4); /* 4 bits */
        p[0] |= DecB64(data[i + 1]) >> 4;   /* 2 bits */
      case 1:
        p[0] |= DecB64(data[i]) << 2;       /* 6 bits */
    }
  }
  return result;
}

char* ArrToB64(char* data) {
  return ToB64(data, ArrLen(data));
}

char* ArgbToOneBpp(int* pixs, int numPixs) {
  char* data = 0;
  int i, j;
  for (i = 0; i < numPixs;) {
    char b = 0;
    for (j = 0; j < 8 && i < numPixs; ++j, ++i) {
      if ((pixs[i] & 0xff000000) != 0xff000000) {
        b |= 1 << (7 - j);
      }
    }
    ArrCat(&data, b);
  }
  return data;
}

char* ArgbArrToOneBpp(int* pixs) {
  return ArgbToOneBpp(pixs, ArrLen(pixs));
}

int* OneBppToArgb(char* data, int numBytes) {
  int* result = 0;
  int i, j;
  for (i = 0; i < numBytes; ++i) {
    for (j = 0; j < 8; ++j) {
      if (data[i] & (1 << (7 - j))) {
        ArrCat(&result, 0x00000000);
      } else {
        ArrCat(&result, 0xff000000);
      }
    }
  }
  return result;
}

int* OneBppArrToArgb(char* data) {
  return OneBppToArgb(data, ArrLen(data));
}

int Mix(int a, int b, float amount) {
  float fa[4], fb[4];
  ColToFlts(a, fa);
  ColToFlts(b, fb);
  LerpFlts(4, fa, fb, fa, amount);
  ClampFlts(4, fa, fa, 0, 1);
  return FltsToCol(fa);
}

static int MulScalarUnchecked(int color, float scalar) {
  float rgba[4];
  ColToFlts(color, rgba);
  MulFltsScalar(3, rgba, rgba, scalar);
  return FltsToCol(rgba);
}

int MulScalar(int color, float scalar) {
  float rgba[4];
  ColToFlts(color, rgba);
  MulFltsScalar(3, rgba, rgba, scalar);
  ClampFlts(3, rgba, rgba, 0, 1);
  return FltsToCol(rgba);
}

int Add(int a, int b) {
  float fa[4], fb[4];
  ColToFlts(a, fa);
  ColToFlts(b, fb);
  AddFlts(4, fa, fb, fa);
  ClampFlts(4, fa, fa, 0, 1);
  return FltsToCol(fa);
}

int AlphaBlend(int src, int dst) {
  int result;
  float d[4], s[4], da, sa;
  float outAlpha;
  ColToFlts(dst, d);
  ColToFlts(src, s);
  sa = s[3] = 1 - s[3]; /* engine uses inverted alpha, but here we need the real alpha */
  da = d[3] = 1 - d[3];
  /*
   * https://en.wikipedia.org/wiki/Alpha_compositing
   *
   * outAlpha = srcAlpha + dstAlpha * (1 - srcAlpha)
   * finalRGB = (srcRGB * srcAlpha + dstRGB * dstAlpha * (1 - srcAlpha)) / outAlpha
   */
  outAlpha = sa + da * (1 - sa);
  src = MulScalarUnchecked(src & 0xffffff, sa);
  dst = MulScalarUnchecked(dst & 0xffffff, da);
  result = src + MulScalarUnchecked(dst, 1 - sa);
  result = MulScalarUnchecked(result, 1.0f / outAlpha);
  result |= (int)((1 - outAlpha) * 255) << 24; /* invert alpha back */
  return result;
}

void AlphaBlendp(int* dst, int src) {
  *dst = AlphaBlend(src, *dst);
}

/* ---------------------------------------------------------------------------------------------- */

struct _Ft {
  Img img;
  int charWidth, charHeight;
};

Ft MkFtFromSimpleGrid(int* pixs, int width, int height, int charWidth, int charHeight) {
  Ft ft = Alloc(sizeof(struct _Ft));
  if (ft) {
    int* decolored = 0;
    int i;
    ft->img = MkImg();
    for (i = 0; i < width * height; ++i) {
      /* we want the base pixs to be white so it can be colored */
      ArrCat(&decolored, pixs[i] | 0xffffff);
    }
    Pixs(ft->img, width, height, decolored);
    RmArr(decolored);
    ft->charWidth = charWidth;
    ft->charHeight = charHeight;
  }
  return ft;
}

Ft MkFtFromSimpleGridFile(char* filePath, int charWidth, int charHeight) {
  Spr spr = MkSprFromFile(filePath);
  if (spr) {
    Ft ft;
    int* imgData = SprToArgbArr(spr);
    int width = SprWidth(spr), height = SprHeight(spr);
    ft = MkFtFromSimpleGrid(imgData, width, height, charWidth, charHeight);
    RmArr(imgData);
    RmSpr(spr);
    return ft;
  }
  return 0;
}

void RmFt(Ft ft) {
  if (ft) {
    RmImg(ft->img);
  }
  Free(ft);
}

Img FtImg(Ft ft) { return ft->img; }

static int* PadPixs(int* pixs, int width, int height, int newWidth, int newHeight) {
  int* newPixs = 0;
  int x, y;
  for (y = 0; y < newHeight; ++y) {
    if (y < height) {
      for (x = 0; x < newWidth; ++x) {
        if (x < width) {
          ArrCat(&newPixs, pixs[y * width + x]);
        } else {
          ArrCat(&newPixs, 0xff000000);
        }
      }
    } else {
      ArrCat(&newPixs, 0xff000000);
    }
  }
  return newPixs;
}

Ft DefFt() {
  /* generated from ft.wbspr using the SpriteEditor */
  static char* b64Data =
    "AIUQIQYIIIAAAAAEcIccE+c+ccAAAAAcAIUUcokIQEIIAAAEiYiiMgiCiiIIAAAiAIU0oSoAQEqIAAAImICCUggEiiAAEA"
    "QCAIAecMQAQEc+AcAIqIEMk88EceAAIcIEAIA0KQqAQEqIAAAQyIICkCiIiCAAQAEIAIAe8kkAQEIIAAAQiIQC+CiIiCAA"
    "IcIIAAAUIKkAQEAAEAIgiIgiECiQiiIIEAQAAIAEIEaAIIAAIAAgcc+cE8cQccAQAAAIAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAcc8e8++cicOigiic8c8e+iii"
    "ii+MQYIAmiigiggiiIEig2yiiiigIiiiiiCIQIUAqiigigggiIEkgqqiiiigIiiiUUEIIIiAq+8gi88u+IE4gimi8i8cIi"
    "iqIIIIIIAAsiigiggiiIEkgiiigqiCIiiqUIQIIIAAiiigiggiiIEigiiigmiCIii2iIgIEIAAiiigiggiiIkigiiigiiC"
    "IiUiiIgIEIAAci8e8+gcicYi+iicgci8IcIiiI+MEYA+AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAgACAMAgIIgYAAAAAAAQAAAAAAEIIAAQAgACAQAgAAgIAAA"
    "AAAAQAAAAAAIIEAAIc8eecQe8YYkI08c8e8e8iiiii+IIEAAACigii8iiIIkIqiiiiigQiiiUiEIIEYAAeigi+QiiII4Iq"
    "iiiigcQiiiIiIQICqIAiigigQiiIIkIqiiiigCQiiqUiQIIEMAAiigigQiiIIiIiiiiigCQiUqiigIIEAAAe8eeeQeicIi"
    "ciic8eg8MeIUie+IIEAAAAAAAAACAAIAAAAAgCAAAAAAACAEIIAAAAAAAAAcAAQAAAAAgCAAAAAAAcAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAA";
  char* data = ArrFromB64(b64Data);
  int* pixs = OneBppArrToArgb(data);
  int charWidth = 6;
  int charHeight = 11;
  int width = charWidth * 0x20;
  int height = charHeight * 3;
  /* temporarily pad it to power-of-two size until I have an actual rectangle packer */
  int potWidth = RoundUpToPowerOfTwo(width);
  int potHeight = RoundUpToPowerOfTwo(height);
  int* potPixs = PadPixs(pixs, width, height, potWidth, potHeight);
  Ft ft = MkFtFromSimpleGrid(potPixs, potWidth, potHeight, charWidth, charHeight);
  RmArr(data);
  RmArr(pixs);
  RmArr(potPixs);
  return ft;
}

void FtMesh(Mesh mesh, Ft ft, int x, int y, char* string) {
  char c;
  int left = x;
  int width = ft->charWidth;
  int height = ft->charHeight;
  for (; (c = *string); ++string) {
    if (c >= 0x20) {
      int u = ((c - 0x20) % 0x20) * width;
      int v = ((c - 0x20) / 0x20) * height;
      ImgQuad(mesh, x, y, u, v, width, height, width, height);
      x += width;
    } else if (c == '\n') {
      y += height;
      x = left;
    }
  }
}

void PutFt(Ft ft, int x, int y, char* string) {
  Trans trans = MkTrans();
  Mesh mesh = MkMesh();
  FtMesh(mesh, ft, x, y, string);
  SetPos(trans, x, y);
  PutMesh(mesh, ToTmpMat(trans), FtImg(ft));
  RmMesh(mesh);
}

/* ---------------------------------------------------------------------------------------------- */

float Lerp(float a, float b, float amount) {
  return a * (1 - amount) + b * amount;
}

void LerpFlts(int n, float* a, float* b, float* c, float amount) {
  int i;
  for (i = 0; i < n; ++i) {
    c[i] = Lerp(a[i], b[i], amount);
  }
}

void MulFltsScalar(int n, float* floats, float* result, float scalar) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = floats[i] * scalar;
  }
}

void FloorFlts(int n, float* floats, float* result) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = Floor(floats[i]);
  }
}

void ClampFlts(int n, float* floats, float* result, float min, float max) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = Clamp(floats[i], min, max);
  }
}

void AddFlts(int n, float* a, float* b, float* result) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = a[i] + b[i];
  }
}

void SetRect(float* rect, float left, float right, float top, float bot) {
  rect[0] = left;
  rect[1] = right;
  rect[2] = top;
  rect[3] = bot;
}

void SetRectPos(float* rect, float x, float y) {
  rect[1] = x + RectWidth(rect);
  rect[3] = y + RectHeight(rect);
  rect[0] = x;
  rect[2] = y;
}

void SetRectSize(float* rect, float width, float height) {
  rect[1] = RectX(rect) + width;
  rect[3] = RectY(rect) + height;
}

void NormRect(float* rect) {
  if (rect[0] > rect[1]) {
    SwpFlts(&rect[0], &rect[1]);
  }
  if (rect[2] > rect[3]) {
    SwpFlts(&rect[2], &rect[3]);
  }
}

void ClampRect(float* rect, float* other) {
  rect[0] = Min(Max(rect[0], other[0]), other[1]);
  rect[1] = Min(Max(rect[1], other[0]), other[1]);
  rect[2] = Min(Max(rect[2], other[2]), other[3]);
  rect[3] = Min(Max(rect[3], other[2]), other[3]);
}

int PtInRect(float* rect, float x, float y) {
  return x >= rect[0] && x < rect[1] && y >= rect[2] && y < rect[3];
}

int RectSect(float* a, float* b) {
  return a[0] < b[1] && a[1] >= b[0] && a[2] < b[3] && a[3] >= b[2];
}

int RectInRect(float* needle, float* haystack) {
  return (
    needle[0] >= haystack[0] && needle[1] < haystack[1] &&
    needle[2] >= haystack[2] && needle[3] < haystack[3]
  );
}

int RectInRectArea(float* needle, float* haystack) {
  return (
    RectWidth(needle) <= RectWidth(haystack) &&
    RectHeight(needle) <= RectHeight(haystack)
  );
}

void SetRectLeft(float* rect, float left)   { rect[0] = left; }
void SetRectRight(float* rect, float right) { rect[1] = right; }
void SetRectTop(float* rect, float top) { rect[2] = top; }
void SetRectBot(float* rect, float bot) { rect[3] = bot; }
float RectWidth(float* rect)  { return rect[1] - rect[0]; }
float RectHeight(float* rect) { return rect[3] - rect[2]; }
float RectX(float* rect) { return rect[0]; }
float RectY(float* rect) { return rect[2]; }
float RectLeft(float* rect)  { return rect[0]; }
float RectRight(float* rect) { return rect[1]; }
float RectTop(float* rect) { return rect[2]; }
float RectBot(float* rect) { return rect[3]; }

/* ---------------------------------------------------------------------------------------------- */

struct _Spr {
  int width, height;
  int* palette;
  char* data;
};

#define SPR_DATA 0xd
#define SPR_REPEAT 0xe

/* TODO: maybe use hashmap to make palette lookup faster */
static int PaletteIndex(int** palette, int color) {
  int i;
  for (i = 0; i < ArrLen(*palette); ++i) {
    if ((*palette)[i] == color) { return i; }
  }
  ArrCat(palette, color);
  return ArrLen(*palette) - 1;
}

void CatSprHdr(char** pArr, int formatVersion, int width,
  int height, int* palette)
{
  int i;
  char* p = ArrAlloc(pArr, 4);
  StrCpy(p, "SP");
  CatVarI32(pArr, formatVersion);
  CatVarI32(pArr, width);
  CatVarI32(pArr, height);
  CatVarI32(pArr, ArrLen(palette));
  for (i = 0; i < ArrLen(palette); ++i) {
    CatI32(pArr, palette[i]);
  }
}

char* ArgbToSprArr(int* argb, int width, int height) {
  char* spr = 0;
  int* palette = 0;
  int i;
  for (i = 0; i < width * height; ++i) {
    PaletteIndex(&palette, argb[i]);
  }
  CatSprHdr(&spr, 1, width, height, palette);
  for (i = 0; i < width * height;) {
    int start;
    for (start = i; i < width * height && argb[i] == argb[start]; ++i);
    if (i - start > 1) {
      CatVarI32(&spr, SPR_REPEAT);
      CatVarI32(&spr, i - start);
      CatVarI32(&spr, PaletteIndex(&palette, argb[start]));
    } else {
      i = start + 1;
      for (; i < width * height && argb[i] != argb[i - 1]; ++i);
      CatVarI32(&spr, SPR_DATA);
      CatVarI32(&spr, i - start);
      for (; start < i; ++start) {
        CatVarI32(&spr, PaletteIndex(&palette, argb[start]));
      }
    }
  }
  RmArr(palette);
  return spr;
}

int SprWidth(Spr spr) {
  return spr->width;
}

int SprHeight(Spr spr) {
  return spr->height;
}

Spr MkSpr(char* data, int length) {
  char* p = data;
  int paletteSize, i, dataLen;
  Spr spr = Alloc(sizeof(struct _Spr));
  if (MemCmp(p, "SP", 4)) {
    /* TODO: error codes or something */
    return 0;
  }
  p += 4;
  DecVarI32(&p); /* format version */
  spr->width = DecVarI32(&p);
  spr->height = DecVarI32(&p);
  paletteSize = DecVarI32(&p);
  for (i = 0; i < paletteSize; ++i) {
    ArrCat(&spr->palette, DecI32(&p));
  }
  dataLen = length - (p - data);
  ArrAlloc(&spr->data, dataLen);
  MemCpy(spr->data, p, dataLen);
  return spr;
}

Spr MkSprFromArr(char* data) {
  return MkSpr(data, ArrLen(data));
}

Spr MkSprFromFile(char* filePath) {
  int len = 1024000; /* TODO: handle bigger files */
  char* data = Alloc(len);
  Spr res;
  len = RdFile(filePath, data, len);
  res = len >= 0 ? MkSpr(data, len) : 0;
  Free(data);
  return res;
}

void SprToArgb(Spr spr, int* argb) {
  char* p = spr->data;
  int* pix = argb;
  while (p - spr->data < ArrLen(spr->data)) {
    int i;
    int type = DecVarI32(&p);
    int length = DecVarI32(&p);
    switch (type) {
      case SPR_DATA: {
        for (i = 0; i < length; ++i) {
          *pix++ = spr->palette[DecVarI32(&p)];
        }
        break;
      }
      case SPR_REPEAT: {
        int color = spr->palette[DecVarI32(&p)];
        for (i = 0; i < length; ++i) {
          *pix++ = color;
        }
      }
    }
  }
}

int* SprToArgbArr(Spr spr) {
  int* res = 0;
  ArrAlloc(&res, spr->width * spr->height);
  SprToArgb(spr, res);
  return res;
}

void RmSpr(Spr spr) {
  if (spr) {
    RmArr(spr->palette);
    RmArr(spr->data);
  }
  Free(spr);
}

#endif /* WEEBCORE_IMPLEMENTATION */
