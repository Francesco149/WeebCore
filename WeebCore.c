#ifndef WEEBCORE_HEADER
#define WEEBCORE_HEADER

/* opaque handles */
typedef int ImgPtr;
typedef struct _Mesh* Mesh;
typedef struct _Img* Img;
typedef struct _Trans* Trans;
typedef struct _Packer* Packer;
typedef struct _Wnd* Wnd;
typedef struct _Mat* Mat;

/* ---------------------------------------------------------------------------------------------- */
/*                                          APP INTERFACE                                         */
/*                                                                                                */
/* the app interface is designed to minimize boilerplate when working with weebcore in C.         */
/* all you are required to do is define a void AppInit() function where you can bind msg handlers */
/* as you like.                                                                                   */
/*                                                                                                */
/* NOTE: when AppInit is called, nothing is initialized yet. you should do all your init from     */
/*       On(INIT, MyInitFunc) and only set app parameters in AppInit                              */
/* ---------------------------------------------------------------------------------------------- */

#ifndef WEEBCORE_LIB
void AppInit();
#endif

/* these funcs can be called in AppInit to set app parameters */

void SetAppName(char* name);
void SetAppClass(char* class);

/* pageSize is the size of a texture page for the texture allocator. there is a tradeoff between
 * runtime performance and load times overhead. a big pageSize will result in less texture switches
 * during rendering but it will slow down flushing img allocations significantly as it will have
 * to update a bigger img on the gpu */
void SetAppPageSize(int pageSize);

/* ---------------------------------------------------------------------------------------------- */

typedef void(* AppHandler)();

/* register handler to be called whenever msg happens. multiple handlers can be bound to same msg.
 * handlers are called in the order they are added.
 * scroll down to "ENUMS AND CONSTANTS" for a list of msgs, or check out the Examples */
void On(int msg, AppHandler handler);
void RmHandler(int msg, AppHandler handler);

/* time elapsed since the last frame, in seconds */
float Delta();

ImgPtr ImgFromSprFile(char* path);
void PutMesh(Mesh mesh, Mat mat, ImgPtr ptr);

Wnd AppWnd();
ImgPtr ImgAlloc(int width, int height);
void ImgFree(ImgPtr img);

/* discard EVERYTHING allocated on the img allocator. reinitialize built in textures such as the
 * default ft. this invalidates all ImgPtr's.
 * this is more optimal than freeing each ImgPtr individually if you are going to re-allocate new
 * stuff because freeing always leads to fragmentation over time */
void ClrImgs();

/* copy raw pixels dx, dy in the img. anything outside the img size is cropped out.
 * note that this replaces pixels. it doesn't do any kind of alpha blending */
void ImgCpyEx(ImgPtr ptr, int* pixs, int width, int height, int dx, int dy);

/* calls ImgCpyEx with dx, dy = 0, 0 */
void ImgCpy(ImgPtr ptr, int* pixs, int width, int height);

/* when you allocate img's they are not immediately sent to the gpu. they will temporarily be blank
 * until this is called. the engine calls this automatically every once in a while, but you can
 * explicitly call it to force refresh img's.
 * this is also automatically called after the INIT msg when using the App interface */
void FlushImgs();

int Argc();
char* Argv(int i);

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
void CpyRect(float* dst, float* src);
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

/* OpenGL-like post-multiplied mat. mat memory layout is row major */

Mat MkMat();
void RmMat(Mat mat);
Mat DupMat(Mat source);

/* these return mat for convienience. it's not actually a copy */
Mat SetIdentity(Mat mat);
Mat SetMat(Mat mat, float* matIn);
Mat GetMat(Mat mat, float* matOut);
Mat Scale(Mat mat, float x, float y);
Mat Scale1(Mat mat, float scale);
Mat Pos(Mat mat, float x, float y);
Mat Rot(Mat mat, float deg);
Mat MulMat(Mat mat, Mat other);
Mat MulMatFlt(Mat mat, float* matIn);

/* multiply a b and store the result in a new Mat */
Mat MkMulMat(Mat matA, Mat matB);
Mat MkMulMatFlt(Mat matA, float* matB);

/* trans 2D point in place */
void TransPt(Mat mat, float* point);

/* trans 2D point in place by inverse of mat. note that this only works if the mat is orthogonal */
void InvTransPt(Mat mat, float* point);

/* note: this is a DIRECT pointer to the matrix data so if you modify it it will affect it */
float* MatFlts(Mat mat);

/* ---------------------------------------------------------------------------------------------- */
/*                                          WBSPR FORMAT                                          */
/* this is meant as simple RLE compression for 2D sprites with a limited color palette            */
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


/* calls PutMeshRawEx with zero u/v offset */
void PutMeshRaw(Mesh mesh, Mat mat, Img img);

/* create a mat from scale, position, origin, rot applied in a fixed order */

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
 * Ortho version does not share the same mat as the non-Ortho so it doesnt invalidate it
 *
 * it's recommended to not hold onto these Mat pointers either way. if the Trans doesn't change,
 * the Mat will not be recalculated, so it's cheap to call these everywhere */
Mat ToTmpMat(Trans trans);
Mat ToTmpMatOrtho(Trans trans);

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
ImgPtr FtImg(Ft ft);

/* generate vertices and uv's for drawing string. must be rendered with the ft img from
 * FtImg or a img that has the same exact layout */
void FtMesh(Mesh mesh, Ft ft, int x, int y, char* string);

void PutFt(Ft ft, int col, int x, int y, char* string);

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
  ArrReserve((pArr), 1); \
  (*(pArr))[ArrLen(*(pArr))] = (x); \
  SetArrLen(*(pArr), ArrLen(*(pArr)) + 1); \
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
/*                                         RECT PACKER                                            */
/*                                                                                                */
/* attempts to efficiently pack rectangles inside a bigger rectangle. internally used to          */
/* automatically stitch together all the imags and not have the renderer swap img every time.     */
/* ---------------------------------------------------------------------------------------------- */

Packer MkPacker(int width, int height);
void RmPacker(Packer pak);

/* rect is an array of 4 floats (left, right, top, bottom) like in the Rect funcs
 *
 * NOTE: this adjusts rect in place and just returns it for convenience. make a copy if you don't
 * want to lose rect's original values.
 *
 * returns NULL if rect doesn't fit */
float* Pack(Packer pak, float* rect);

/* mark rect as a free area. this can be used to remove already packed rects */
void PackFree(Packer pak, float* rect);

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

int DecVarI32(char** pData);        /* increments *pData past the varint */
int EncVarI32(void* data, int x);   /* returns num of bytes written */
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

/* converts x to a string with the specified base and appends the characters to arr.
   base is clamped to 1-16 */
void ArrStrCatI32(char** arr, int x, int base);

/* like ArrStrCatI32 but creates an arr on the fly and null terminates it. must be rmd with RmArr */
char* I32ToArrStr(int x, int base);

/* ---------------------------------------------------------------------------------------------- */
/*                                      ENUMS AND CONSTANTS                                       */
/* ---------------------------------------------------------------------------------------------- */

#ifndef PI
#define PI 3.141592653589793238462643383279502884
#endif

/* message types returned by MsgType */
enum {
  QUIT_REQUEST = 1,
  KEYDOWN,
  KEYUP,
  MOTION,
  SIZE,
  INIT,
  FRAME,
  QUIT,
  LAST_MSG_TYPE
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

/* ---------------------------------------------------------------------------------------------- */
/*                            MISC DEBUG AND SEMI-INTERNAL INTERFACES                             */
/* ---------------------------------------------------------------------------------------------- */

/* these could be used to use the app interface from FFI (minus the handlers system unless your ffi
 * has simple ways to pass callbacks to C). see implementation of AppMain to see how you would do
 * it from FFI */

/* call the built in app handlers for the current msg */
int AppHandleMsg();

/* call the built in app handlers for the FRAME msg. this should be called at the start of every
 * tick of the game loop. note that this not call SwpBufs, you have to call it yourself */
void AppFrame();

/* true if no QUIT_REQUEST msg has been received */
int AppRunning();

/* (automatically called by AppMain) initialize the app globals. */
void MkApp(int argc, char* argv[]);

/* (automatically called by AppMain)
 * clean up the app globals (optional, as virtual memory cleans everything up when we exit) */
void RmApp();

/* (automatically called by the platform layer)
 * initializes app and runs the main loop.
 * this also takes care of calling pseudo msgs like INIT, FRAME and calling SwpBufs */
int AppMain(int argc, char* argv[]);

/* enable debug ui for the img allocator */
void DiagImgAlloc(int enabled);

/* ---------------------------------------------------------------------------------------------- */
/*                                        PLATFORM LAYER                                          */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample platform layers or write */
/* your own by defining these structs and funcs                                                   */
/* ---------------------------------------------------------------------------------------------- */

Wnd MkWnd(char* name, char* class);
void RmWnd(Wnd wnd);
void SetWndName(Wnd wnd, char* wndName);
void SetWndClass(Wnd wnd, char* className);
void SetWndSize(Wnd wnd, int width, int height);
int WndWidth(Wnd wnd);
int WndHeight(Wnd wnd);

/* get time elapsed in seconds since the last SwpBufs. guaranteed to return non-zero even if
 * no SwpBufs happened */
float WndDelta(Wnd wnd);

/* FPS limiter, 0 for unlimited. limiting happens in SwpBufs. note that unlimited fps still
 * waits for the minimum timer resolution for GetTime */
void SetWndFPS(Wnd wnd, int fps);

/* fetch one message. returns non-zero as long as there are more */
int NextMsg(Wnd wnd);

/* sends QUIT_REQUEST message to the wnd */
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

/* same as MemCpy but allows overlapping regions of memory */
void MemMv(void* dst, void* src, int n);

/* write data to disk. returns number of bytes written or < 0 for errors */
int WrFile(char* path, void* data, int dataLen);

/* read up to maxSize bytes from disk */
int RdFile(char* path, void* data, int maxSize);

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
/*                                           RENDERER                                             */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample renderers or write your  */
/* own by defining these structs and funcs                                                        */
/* ---------------------------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------------------------- */

/* Mesh is a collection of vertices that will be rendered using the same img and trans. try
 * to batch up as much stuff into a mesh as possible for optimal performance. */

Mesh MkMesh();
void RmMesh(Mesh mesh);

/* change current color. color is Argb 32-bit int (0xAARRGGBB). 255 alpha = completely transparent,
 * 0 alpha = completely opaque. the default color for a mesh should be 0xffffff */
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

/* this is internally used to do img atlases and map relative uv's to the bigger image */
void PutMeshRawEx(Mesh mesh, Mat mat, Img img, float uOffs, float vOffs);

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

/* to minimize duplication, I decided to have common flags shared by all internal stuff */
#define DIRTY (1<<0)
#define ORTHO_DIRTY (1<<1)
#define RUNNING (1<<2)

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

typedef struct { float r[4]; } PackerRect; /* left, right, top bottom */

struct _Packer {
  PackerRect* rects;
};

/* adds a free rect */
static void ArrCatRect(PackerRect** arr, float left, float right, float top, float bottom) {
  PackerRect* newRect = ArrAlloc(arr, 1);
  SetRect(newRect->r, left, right, top, bottom);
}

static void ArrCatRectFlts(PackerRect** arr, float* r) {
  PackerRect* newRect = ArrAlloc(arr, 1);
  MemCpy(newRect, r, sizeof(float) * 4);
}

Packer MkPacker(int width, int height) {
  Packer pak = Alloc(sizeof(struct _Packer));
  if (pak) {
    /* initialize area to be one big free rect */
    ArrCatRect(&pak->rects, 0, width, 0, height);
  }
  return pak;
}

void RmPacker(Packer pak) {
  if (pak) {
    RmArr(pak->rects);
  }
  Free(pak);
}

/* find the best fit free rectangle (smallest area that can fit the rect).
 * initially we will have 1 big free rect that takes the entire area */
static int PakFindFree(Packer pak, float* rect) {
  PackerRect* rects = pak->rects;
  int i, bestFit = -1;
  float bestFitArea = 2000000000;
  for (i = 0; i < ArrLen(rects); ++i) {
    float* r = rects[i].r;
    if (RectInRectArea(rect, r)) {
      float area = RectWidth(r) * RectHeight(r);
      if (area < bestFitArea) {
        bestFitArea = area;
        bestFit = i;
      }
    }
  }
  return bestFit;
}

/* once we have found a location for the rectangle, we need to split any free rectangles it
 * partially intersects with. this will generate two or more smaller rects */
static void PakSplit(Packer pak, float* rect) {
  PackerRect* rects = pak->rects;
  int i;
  PackerRect* newRects = 0;
  for (i = 0; i < ArrLen(rects); ++i) {
    float* r = rects[i].r;
    if (RectSect(rect, r)) {
      if (rect[0] > r[0]) { ArrCatRect(&newRects, r[0], rect[0], r[2], r[3]); /* left  */ }
      if (rect[1] < r[1]) { ArrCatRect(&newRects, rect[1], r[1], r[2], r[3]); /* right */ }
      if (rect[2] > r[2]) { ArrCatRect(&newRects, r[0], r[1], r[2], rect[2]); /* top   */ }
      if (rect[3] < r[3]) { ArrCatRect(&newRects, r[0], r[1], rect[3], r[3]); /* bott  */ }
    } else {
      ArrCatRectFlts(&newRects, r);
    }
  }
  RmArr(rects);
  pak->rects = newRects;
}

/* after the split step, there will be redundant rects because we create 1 rect for each side */
static void PakPrune(Packer pak) {
  int i, j;
  PackerRect* rects = pak->rects;
  PackerRect* newRects = 0;
  for (i = 0; i < ArrLen(rects); ++i) {
    for (j = 0; j < ArrLen(rects); ++j) {
      if (i == j) { continue; }
      if (RectInRect(rects[i].r, rects[j].r)) {
        rects[i].r[0] = rects[i].r[1] = 0; /* make it zero size to mark for removal */
      } else if (RectInRect(rects[j].r, rects[i].r)) {
        rects[j].r[0] = rects[j].r[1] = 0;
      }
    }
    if (RectWidth(rects[i].r) > 0) {
      ArrCatRectFlts(&newRects, rects[i].r);
    }
  }
  RmArr(rects);
  pak->rects = newRects;
}

float* Pack(Packer pak, float* rect) {
  float* free;
  int freeRect = PakFindFree(pak, rect);
  if (freeRect < 0) { /* full */
    return 0;
  }

  /* move rectangle to the top left of the free area */
  free = pak->rects[freeRect].r;
  SetRectPos(rect, free[0], free[2]);

  /* update free rects */
  PakSplit(pak, rect);
  PakPrune(pak);
  return rect;
}

void PackFree(Packer pak, float* rect) {
  /* TODO: figure out a good way to defragment the free rects */
  ArrCatRectFlts(&pak->rects, rect);
  PakPrune(pak);
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

char* I32ToArrStr(int x, int base) {
  char* res = 0;
  ArrStrCatI32(&res, x, base);
  ArrCat(&res, 0);
  return res;
}

void ArrStrCatI32(char** res, int x, int base) {
  static char* charset = "0123456789abcdef";
  int i, tmplen;
  char* tmp = 0;
  base = Min(16, Max(1, base));
  if (x < 0) {
    ArrCat(res, '-');
    x *= -1;
  }
  while (x) {
    ArrCat(&tmp, charset[x % base]);
    x /= base;
  }
  if (ArrLen(tmp) <= 0) {
    ArrCat(&tmp, '0');
  }
  tmplen = ArrLen(tmp);
  for (i = 0; i < tmplen; ++i) {
    ArrCat(res, tmp[tmplen - i - 1]);
  }
  RmArr(tmp);
}

/* ---------------------------------------------------------------------------------------------- */

void PutMeshRaw(Mesh mesh, Mat mat, Img img) {
  PutMeshRawEx(mesh, mat, img, 0, 0);
}

struct _Trans {
  float sX, sY;
  float x, y;
  float oX, oY;
  float deg;
  Mat tempMat, tempMatOrtho;
  int dirty;
};

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
  Pos(mat, trans->x, trans->y);
  Rot(mat, trans->deg);
  if (!ortho) {
    Scale(mat, trans->sX, trans->sY);
  }
  return Pos(mat, -trans->oX, -trans->oY);
}

Mat ToMat(Trans trans) { return CalcTrans(trans, MkMat(), 0); }
Mat ToMatOrtho(Trans trans) { return CalcTrans(trans, MkMat(), 1); }

Mat ToTmpMat(Trans trans) {
  if (trans->dirty & DIRTY) {
    SetIdentity(trans->tempMat);
    CalcTrans(trans, trans->tempMat, 0);
    trans->dirty &= ~DIRTY;
  }
  return trans->tempMat;
}

Mat ToTmpMatOrtho(Trans trans) {
  if (trans->dirty & ORTHO_DIRTY) {
    SetIdentity(trans->tempMatOrtho);
    CalcTrans(trans, trans->tempMatOrtho, 1);
    trans->dirty &= ~ORTHO_DIRTY;
  }
  return trans->tempMatOrtho;
}

/* ---------------------------------------------------------------------------------------------- */

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
  ImgPtr img;
  int charWidth, charHeight;
};

Ft MkFtFromSimpleGrid(int* pixs, int width, int height, int charWidth, int charHeight) {
  Ft ft = Alloc(sizeof(struct _Ft));
  if (ft) {
    int* decolored = 0;
    int i;
    ft->img = ImgAlloc(width, height);
    for (i = 0; i < width * height; ++i) {
      /* we want the base pixs to be white so it can be colored */
      ArrCat(&decolored, pixs[i] | 0xffffff);
    }
    ImgCpy(ft->img, decolored, width, height);
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
    ImgFree(ft->img);
  }
  Free(ft);
}

ImgPtr FtImg(Ft ft) { return ft->img; }

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

void PutFt(Ft ft, int col, int x, int y, char* string) {
  Mesh mesh = MkMesh();
  Col(mesh, col);
  FtMesh(mesh, ft, x, y, string);
  PutMesh(mesh, 0, FtImg(ft));
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

void CpyRect(float* dst, float* src) {
  SetRect(dst, src[0], src[1], src[2], src[3]);
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
    needle[0] >= haystack[0] && needle[1] <= haystack[1] &&
    needle[2] >= haystack[2] && needle[3] <= haystack[3]
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

/* memory layout:
 * float left_axis[4]; float up_axis[4]; float fwd_axis[4]; float translation[4]; */

struct _Mat { float m[16]; };

Mat MkMat() {
  Mat mat = Alloc(sizeof(struct _Mat));
  if (mat) {
    mat->m[0] = mat->m[5] = mat->m[10] = mat->m[15] = 1;
  }
  return mat;
}

void RmMat(Mat mat) {
  Free(mat);
}

Mat DupMat(Mat source) {
  return SetMat(MkMat(), source->m);
}

/* these return mat for convienience. it's not actually a copy */
Mat SetIdentity(Mat mat) {
  MemSet(mat->m, 0, sizeof(mat->m));
  mat->m[0] = mat->m[5] = mat->m[10] = mat->m[15] = 1;
  return mat;
}

Mat SetMat(Mat mat, float* matIn) {
  if (mat) {
    MemCpy(mat->m, matIn, sizeof(mat->m));
  }
  return mat;
}

Mat GetMat(Mat mat, float* matOut) {
  MemCpy(matOut, mat->m, sizeof(mat->m));
  return mat;
}

#define MAT_IDENT { \
    1, 0, 0, 0, \
    0, 1, 0, 0, \
    0, 0, 1, 0, \
    0, 0, 0, 1 \
  }

float* MatFlts(Mat mat) {
  static float identity[16] = MAT_IDENT;
  return mat ? mat->m : identity;
}

Mat Scale(Mat mat, float x, float y) {
  float m[16] = MAT_IDENT;
  m[0] = x;
  m[5] = y;
  return MulMatFlt(mat, m);
}

Mat Scale1(Mat mat, float scale) {
  return Scale(mat, scale, scale);
}

Mat Pos(Mat mat, float x, float y) {
  float m[16] = MAT_IDENT;
  m[12] = x;
  m[13] = y;
  return MulMatFlt(mat, m);
}

Mat Rot(Mat mat, float deg) {
  /* 2d rotation = z axis rotation. this means we rotate the left and up axes */
  float s = Sin(deg);
  float m[16] = MAT_IDENT;
  m[0] = m[5] = Cos(deg);
  m[1] = s;
  m[4] = -s;
  return MulMatFlt(mat, m);
}

Mat MulMat(Mat mat, Mat other) {
  return MulMatFlt(mat, other->m);
}

Mat MulMatFlt(Mat mat, float* matIn) {
  Mat tmp = MkMulMatFlt(mat, matIn);
  SetMat(mat, tmp->m);
  RmMat(tmp);
  return mat;
}

Mat MkMulMat(Mat matA, Mat matB) {
  return MkMulMatFlt(matA, matB->m);
}

Mat MkMulMatFlt(Mat matA, float* b) {
  Mat res = MkMat();
  float* a = matA->m;
  float* m = res->m;
  m[ 0] = b[ 0] * a[0] + b[ 1] * a[4] + b[ 2] * a[ 8] + b[ 3] * a[12];
  m[ 1] = b[ 0] * a[1] + b[ 1] * a[5] + b[ 2] * a[ 9] + b[ 3] * a[13];
  m[ 2] = b[ 0] * a[2] + b[ 1] * a[6] + b[ 2] * a[10] + b[ 3] * a[14];
  m[ 3] = b[ 0] * a[3] + b[ 1] * a[7] + b[ 2] * a[11] + b[ 3] * a[15];
  m[ 4] = b[ 4] * a[0] + b[ 5] * a[4] + b[ 6] * a[ 8] + b[ 7] * a[12];
  m[ 5] = b[ 4] * a[1] + b[ 5] * a[5] + b[ 6] * a[ 9] + b[ 7] * a[13];
  m[ 6] = b[ 4] * a[2] + b[ 5] * a[6] + b[ 6] * a[10] + b[ 7] * a[14];
  m[ 7] = b[ 4] * a[3] + b[ 5] * a[7] + b[ 6] * a[11] + b[ 7] * a[15];
  m[ 8] = b[ 8] * a[0] + b[ 9] * a[4] + b[10] * a[ 8] + b[11] * a[12];
  m[ 9] = b[ 8] * a[1] + b[ 9] * a[5] + b[10] * a[ 9] + b[11] * a[13];
  m[10] = b[ 8] * a[2] + b[ 9] * a[6] + b[10] * a[10] + b[11] * a[14];
  m[11] = b[ 8] * a[3] + b[ 9] * a[7] + b[10] * a[11] + b[11] * a[15];
  m[12] = b[12] * a[0] + b[13] * a[4] + b[14] * a[ 8] + b[15] * a[12];
  m[13] = b[12] * a[1] + b[13] * a[5] + b[14] * a[ 9] + b[15] * a[13];
  m[14] = b[12] * a[2] + b[13] * a[6] + b[14] * a[10] + b[15] * a[14];
  m[15] = b[12] * a[3] + b[13] * a[7] + b[14] * a[11] + b[15] * a[15];
  return res;
}

void TransPt(Mat mat, float* point) {
  float mPt[16];
  Mat tmp;
  MemSet(mPt, 0, sizeof(mPt));
  MemCpy(mPt, point, sizeof(float) * 2);
  mPt[3] = 1;
  tmp = MkMulMatFlt(mat, mPt);
  MemCpy(point, tmp->m, sizeof(float) * 2);
  RmMat(tmp);
}

void InvTransPt(Mat mat, float* point) {
  /* https://en.wikibooks.org/wiki/GLSL_Programming/Applying_Matrix_Transformations#Transforming_Pts_with_the_Inverse_Matrix */
  Mat tmp = DupMat(mat);
  float* m = tmp->m;
  point[0] -= m[12];
  point[1] -= m[13];
  m[12] = m[13] = m[14] = m[15] = m[3] = m[7] = m[11] = 0; /* equivalent of taking the 3x3 mat */
  TransPt(tmp, point);
  RmMat(tmp);
}

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

void CatSprHdr(char** pArr, int formatVersion, int width, int height, int* palette) {
  int i;
  ArrStrCat(pArr, "WBSP");
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
  if (MemCmp(p, "WBSP", 4)) {
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

/* ---------------------------------------------------------------------------------------------- */

typedef struct _ImgPage {
  Img img;
  int* pixs;
  Packer pak;
  int flags;
} ImgPage;

typedef struct _ImgRegion {
  int page;
  float r[4];
} ImgRegion;

static struct _Globals {
  /* params */
  char* name;
  char* class;
  int pageSize;

  int argc;
  char** argv;
  Wnd wnd;
  AppHandler* handlers[LAST_MSG_TYPE];
  int flags;
  Ft ft;

  /* img allocator */
  ImgPage* pages;
  ImgRegion* regions;
  float flushTimer;

  int diagPage;
} app;

Wnd AppWnd() { return app.wnd; }
float Delta() { return WndDelta(app.wnd); }
int Argc() { return app.argc; }
char* Argv(int i) { return app.argv[i]; }

void On(int msg, AppHandler handler) {
  if (msg >= 0 && msg < LAST_MSG_TYPE) {
    ArrCat(&app.handlers[msg], handler);
  }
}

static void Nop() { }

static void PruneHandlers() {
  if (app.flags & DIRTY) {
    int i, msg;
    for (msg = 0; msg < LAST_MSG_TYPE; ++msg) {
      AppHandler* handlers = app.handlers[msg];
      AppHandler* newHandlers = 0;
      for (i = 0; i < ArrLen(handlers); ++i) {
        if (handlers[i] != Nop) {
          ArrCat(&newHandlers, handlers[i]);
        }
      }
      RmArr(handlers);
      app.handlers[msg] = newHandlers;
    }
    app.flags &= ~DIRTY;
  }
}

/* we cannot change the size of handlers because this could be called while iterating them.
 * so we set a dirty flags that tells it to recompact the handlers array after we are done handling
 * this msg */
void RmHandler(int msg, AppHandler handler) {
  if (msg >= 0 && msg < LAST_MSG_TYPE) {
    int i;
    AppHandler* handlers = app.handlers[msg];
    for (i = 0; i < ArrLen(handlers); ++i) {
      if (handlers[i] == handler) {
        handlers[i] = Nop;
        app.flags |= DIRTY;
      }
    }
  }
}

void SetAppName(char* name) { app.name = name; }
void SetAppClass(char* class) { app.class = class; }
void SetAppPageSize(int pageSize) { app.pageSize = pageSize; }


static void AppHandle(int msg) {
  AppHandler* handlers = app.handlers[msg];
  int i;
  for (i = 0; i < ArrLen(handlers); ++i) {
    handlers[i]();
  }
  PruneHandlers();
}

typedef struct _FtData {
  int width, height, charWidth, charHeight;
  int* pixs;
} FtData;

static void DefFtData(FtData* ft) {
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
  int i;
  ft->pixs = OneBppArrToArgb(data);
  for (i = 0; i < ArrLen(ft->pixs); ++i) {
    ft->pixs[i] |= 0xffffff; /* make all pixs white */
  }
  ft->charWidth = 6;
  ft->charHeight = 11;
  ft->width = ft->charWidth * 0x20;
  ft->height = ft->charHeight * 3;
  RmArr(data);
}

static Ft MkDefFt() {
  FtData ftd;
  Ft ft;
  DefFtData(&ftd);
  ft = MkFtFromSimpleGrid(ftd.pixs, ftd.width, ftd.height, ftd.charWidth, ftd.charHeight);
  RmArr(ftd.pixs);
  return ft;
}

/* for when the ImgPtr is invalidated and we don't wanna invalidate ptrs to this Ft */
static void RefreshFt(Ft ft) {
  FtData ftd;
  DefFtData(&ftd);
  ft->img = ImgAlloc(ftd.width, ftd.height);
  ImgCpy(ft->img, ftd.pixs, ftd.width, ftd.height);
  RmArr(ftd.pixs);
}

Ft DefFt() { return app.ft; }

static void NukeImgs() {
  int i;
  for (i = 0; i < ArrLen(app.pages); ++i) {
    ImgPage* page = &app.pages[i];
    RmImg(page->img);
    RmPacker(page->pak);
    Free(page->pixs);
  }
  RmArr(app.pages);
  RmArr(app.regions);
  app.pages = 0;
  app.regions = 0;
}

void InitAppImgs() {
  if (!app.ft) {
    app.ft = MkDefFt();
  } else {
    RefreshFt(app.ft);
  }
}

void ClrImgs() {
  NukeImgs();
  InitAppImgs();
}

void MkApp(int argc, char* argv[]) {
  app.argc = argc;
  app.argv = argv;
  app.pageSize = app.pageSize ? RoundUpToPowerOfTwo(app.pageSize) : 1024;
  if (!app.name) { app.name = "WeebCore"; }
  if (!app.class) { app.class = "WeebCore"; }
  app.wnd = MkWnd(app.name, app.class);
  app.flags |= RUNNING;
  InitAppImgs();
  AppHandle(INIT);
  FlushImgs();
}

void RmApp() {
  int i;
  AppHandle(QUIT);
  NukeImgs();
  for (i = 0; i < LAST_MSG_TYPE; ++i) {
    RmArr(app.handlers[i]);
  }
}

int AppHandleMsg() {
  int msg = MsgType(app.wnd);
  AppHandle(msg);
  if (msg == QUIT_REQUEST) {
    app.flags &= ~RUNNING;
    return 0;
  }
  return 1;
}

void AppFrame() {
  AppHandle(FRAME);
  app.flushTimer += Delta();
  if (app.flushTimer >= 1) {
    FlushImgs();
    app.flushTimer = 0;
  }
}

int AppRunning() { return app.flags & RUNNING; }

int AppMain(int argc, char* argv[]) {
  MkApp(argc, argv);
  while (AppRunning()) {
    while (NextMsg(app.wnd) && AppHandleMsg());
    AppFrame();
    SwpBufs(app.wnd);
  }
  RmApp();
  return 0;
}

ImgPtr ImgAlloc(int width, int height) {
  ImgRegion* region = 0;
  float r[4];
  int i, pageIdx;
  ImgPage* page = 0;
  SetRect(r, 0, width, 0, height);
  for (i = 0; i < ArrLen(app.pages); ++i) {
    page = &app.pages[i];
    if (Pack(page->pak, r)) {
      break;
    }
  }
  pageIdx = i;
  if (pageIdx >= ArrLen(app.pages)) {
    /* no free pages, make a new page */
    page = ArrAlloc(&app.pages, 1);
    page->img = MkImg();
    page->pixs = Alloc(app.pageSize * app.pageSize * sizeof(int));
    page->pak = MkPacker(app.pageSize, app.pageSize);
    if (!Pack(page->pak, r)) {
      return 0;
    }
  }
  for (i = 0; i < ArrLen(app.regions); ++i) {
    if (app.regions[i].page < 0) {
      region = &app.regions[i];
    }
  }
  if (!region) {
    region = ArrAlloc(&app.regions, 1);
  }
  if (region) {
    CpyRect(region->r, r);
    region->page = pageIdx;
  }
  return region - app.regions + 1;
}

void ImgCpyEx(ImgPtr ptr, int* pixs, int width, int height, int dx, int dy) {
  if (ptr >= 1 && ptr <= ArrLen(app.regions)) {
    ImgRegion* region = &app.regions[ptr - 1];
    ImgPage* page = &app.pages[region->page];
    float* r = region->r;
    int pixsStride = width;
    int x, y;
    int left = 0, top = 0;
    int right = dx + width;
    int bot = dy + height;
    if (dx < 0) { left = -dx; }
    if (dy < 0) { top = -dy; }
    if (right > RectWidth(r)) {
      width -= right - (int)RectWidth(r);
    }
    if (bot > RectHeight(r)) {
      height -= bot - (int)RectHeight(r);
    }
    for (y = top; y < height; ++y) {
      for (x = left; x < width; ++x) {
        int dstx = RectX(r) + x + dx;
        int dsty = RectY(r) + y + dy;
        int* dstpix = &page->pixs[dsty * app.pageSize + dstx];
        int srcpix = pixs[y * pixsStride + x];
        if (*dstpix != srcpix) {
          *dstpix = srcpix;
          page->flags |= DIRTY;
        }
      }
    }
  }
}

void ImgCpy(ImgPtr ptr, int* pixs, int width, int height) {
  ImgCpyEx(ptr, pixs, width, height, 0, 0);
}

void FlushImgs() {
  int i;
  for (i = 0; i < ArrLen(app.pages); ++i) {
    ImgPage* page = &app.pages[i];
    if (page->flags & DIRTY) {
      Pixs(page->img, app.pageSize, app.pageSize, page->pixs);
      page->flags &= ~DIRTY;
    }
  }
}

ImgPtr ImgFromSprFile(char* path) {
  ImgPtr res = 0;
  Spr spr = MkSprFromFile(path);
  if (spr) {
    int* pixs = SprToArgbArr(spr);
    res = ImgAlloc(SprWidth(spr), SprHeight(spr));
    ImgCpy(res, pixs, SprWidth(spr), SprHeight(spr));
    RmArr(pixs);
  }
  RmSpr(spr);
  return res;
}

void ImgFree(ImgPtr img) {
  ImgRegion* region = &app.regions[img - 1];
  PackFree(app.pages[region->page].pak, region->r);
  region->page = -1;
}

void PutMesh(Mesh mesh, Mat mat, ImgPtr ptr) {
  if (ptr) {
    ImgRegion* region = &app.regions[ptr - 1];
    PutMeshRawEx(mesh, mat, app.pages[region->page].img, RectX(region->r), RectY(region->r));
  } else {
    PutMeshRaw(mesh, mat, 0);
  }
}

/* ---------------------------------------------------------------------------------------------- */

static void DiagImgAllocKeyDown() {
  Wnd wnd = AppWnd();
  switch (Key(wnd)) {
    case MWHEELUP: {
      app.diagPage = Max(0, Min(app.diagPage + 1, ArrLen(app.pages) - 1));
      break;
    }
    case MWHEELDOWN: {
      app.diagPage = Max(app.diagPage - 1, 0);
      break;
    }
  }
}

static void PutPageText() {
  char* pagestr = 0;
  ArrStrCat(&pagestr, "ImgAllocator Diag - page ");
  ArrStrCatI32(&pagestr, app.diagPage + 1, 10);
  ArrCat(&pagestr, '/');
  ArrStrCatI32(&pagestr, ArrLen(app.pages), 10);
  ArrStrCat(&pagestr, ", ");
  ArrStrCatI32(&pagestr, app.pageSize, 10);
  ArrCat(&pagestr, 'x');
  ArrStrCatI32(&pagestr, app.pageSize, 10);
  ArrCat(&pagestr, 0);
  PutFt(DefFt(), 0xbebebe, 10, 10, pagestr);
  RmArr(pagestr);
}

static void DiagImgAllocFrame() {
  if (ArrLen(app.pages)) {
    ImgPage* page = &app.pages[app.diagPage];
    int i;
    /* black background */
    Mesh mesh = MkMesh();
    Col(mesh, 0x000000);
    Quad(mesh, 0, 0, app.pageSize + 10, app.pageSize + 40);
    PutMesh(mesh, 0, 0);
    RmMesh(mesh);
    /* display entire page */
    mesh = MkMesh();
    Quad(mesh, 10, 30, app.pageSize, app.pageSize);
    PutMeshRaw(mesh, 0, page->img);
    RmMesh(mesh);
    /* rect packer region grid */
    mesh = MkMesh();
    Col(mesh, 0x00ff00);
    for (i = 0; i < ArrLen(page->pak->rects); ++i) {
      float* r = page->pak->rects[i].r;
      Quad(mesh, 10 + r[0], 30 + r[2], 1, RectHeight(r));
      Quad(mesh, 10 + r[1], 30 + r[2], 1, RectHeight(r));
      Quad(mesh, 10 + r[0], 30 + r[2], RectWidth(r), 1);
      Quad(mesh, 10 + r[0], 30 + r[3], RectWidth(r), 1);
    }
    PutMeshRaw(mesh, 0, 0);
    RmMesh(mesh);
  }
  PutPageText();
}

void DiagImgAlloc(int enabled) {
  if (enabled) {
    On(KEYDOWN, DiagImgAllocKeyDown);
    On(FRAME, DiagImgAllocFrame);
  } else {
    RmHandler(KEYDOWN, DiagImgAllocKeyDown);
    RmHandler(FRAME, DiagImgAllocFrame);
  }
}

#endif /* WEEBCORE_IMPLEMENTATION */
