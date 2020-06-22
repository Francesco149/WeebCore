/* NOTE: this intentionally uses OpenGL 1.x for maximum compatibility. do not try to use modern
 * opengl features in here. make a new platform layer for optimization with modern APIs instead. */

#include <GL/gl.h>

typedef struct _VertData {
  float x, y;
  int color;
  float u, v;
} VertData;

struct _Mesh {
  int color;
  VertData* verts;
  int* indices;
  int istart;
};

struct _Img { GLuint handle; int width, height; };

/* convert a 0xAARRGGBB integer to OpenGL's 0xAABBGGRR.
 * also flip alpha because we use 0 = opaque */

#define COL(argb) \
  ((((argb) & 0x00FF0000) >> 16) | \
    ((argb) & 0x0000FF00) | \
   (((argb) & 0x000000FF) << 16) | \
   (0xFF000000 - ((argb) & 0xFF000000)))

static void GrInit() {
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

/* ---------------------------------------------------------------------------------------------- */

Mesh MkMesh() {
  Mesh mesh = Alloc(sizeof(struct _Mesh));
  Col(mesh, 0xffffff);
  return mesh;
}

void RmMesh(Mesh mesh) {
  if (mesh) {
    RmArr(mesh->verts);
    RmArr(mesh->indices);
  }
  Free(mesh);
}

void Col(Mesh mesh, int color) {
  mesh->color = COL(color);
}

void Begin(Mesh mesh) {
  if (mesh->istart == -1) {
    mesh->istart = ArrLen(mesh->verts);
  }
}

void End(Mesh mesh) {
  mesh->istart = -1;
}

void Vert(Mesh mesh, float x, float y) {
  VertData* vertex = ArrAlloc(&mesh->verts, 1);
  vertex->x = x;
  vertex->y = y;
  vertex->color = mesh->color;
}

void ImgCoord(Mesh mesh, float u, float v) {
  int len = ArrLen(mesh->verts);
  if (len) {
    VertData* vertex = &mesh->verts[len - 1];
    vertex->u = u;
    vertex->v = v;
  }
}

void Face(Mesh mesh, int i1, int i2, int i3) {
  int* indices = ArrAlloc(&mesh->indices, 3);
  indices[0] = mesh->istart + i1;
  indices[1] = mesh->istart + i2;
  indices[2] = mesh->istart + i3;
}

void PutMeshEx(Mesh mesh, Mat mat, Img img, float uOffs, float vOffs) {
  int stride = sizeof(VertData);
  GLuint gltex, curImg;

  if (!ArrLen(mesh->indices)) {
    return;
  }

  gltex = img ? img->handle : 0;
  glVertexPointer(2, GL_FLOAT, stride, &mesh->verts[0].x);
  glColorPointer(4, GL_UNSIGNED_BYTE, stride, &mesh->verts[0].color);
  if (gltex) {
    glTexCoordPointer(2, GL_FLOAT, stride, &mesh->verts[0].u);
    /* scale img coords so we can use pixs instead of texels */
    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(1.0f / img->width, 1.0f / img->height, 1.0f);
    glTranslatef(uOffs, vOffs, 0);
    glPopAttrib();
  }

  glGetIntegerv(GL_TEXTURE_BINDING_2D, (void*)&curImg);
  if (gltex != curImg) {
    glBindTexture(GL_TEXTURE_2D, gltex);
  }

  glLoadMatrixf(MatFlts(mat));
  glDrawElements(GL_TRIANGLES, ArrLen(mesh->indices), GL_UNSIGNED_INT, mesh->indices);
}

/* ---------------------------------------------------------------------------------------------- */

Img MkImg() {
  Img img = Alloc(sizeof(struct _Img));
  glGenTextures(1, &img->handle);
  SetImgWrapU(img, REPEAT);
  SetImgWrapV(img, REPEAT);
  SetImgMinFilter(img, NEAREST);
  SetImgMagFilter(img, NEAREST);
  return img;
}

void RmImg(Img img) {
  if (img) {
    glDeleteTextures(1, &img->handle);
  }
  Free(img);
}

static int ConvImgWrap(int mode) {
  switch (mode) {
    case CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
    case MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
    case REPEAT: return GL_REPEAT;
  }
  return REPEAT;
}

static int ConvImgFilter(int filter) {
  switch (filter) {
    case NEAREST: return GL_NEAREST;
    case LINEAR: return GL_LINEAR;
  }
  return NEAREST;
}

static void SetImgParam(Img img, int param, int value) {
  glBindTexture(GL_TEXTURE_2D, img->handle);
  glTexParameteri(GL_TEXTURE_2D, param, value);
}

void SetImgWrapU(Img img, int mode) {
  SetImgParam(img, GL_TEXTURE_WRAP_S, ConvImgWrap(mode));
}

void SetImgWrapV(Img img, int mode) {
  SetImgParam(img, GL_TEXTURE_WRAP_T, ConvImgWrap(mode));
}

void SetImgMinFilter(Img img, int filter) {
  SetImgParam(img, GL_TEXTURE_MIN_FILTER, ConvImgFilter(filter));
}

void SetImgMagFilter(Img img, int filter) {
  SetImgParam(img, GL_TEXTURE_MAG_FILTER, ConvImgFilter(filter));
}

Img Pixs(Img img, int width, int height, int* data) {
  return PixsEx(img, width, height, data, width);
}

Img PixsEx(Img img, int width, int height, int* data, int stride) {
  int* rgba = 0;
  int x, y;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      ArrCat(&rgba, COL(data[y * stride + x]));
    }
  }
  glBindTexture(GL_TEXTURE_2D, img->handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  RmArr(rgba);
  img->width = width;
  img->height = height;
  return img;
}

/* ---------------------------------------------------------------------------------------------- */

void Viewport(Wnd window, int x, int y, int width, int height) {
  /* OpenGL uses a Y axis that starts from the bottom of the window.
   * I flip it because I find it most intuitive that way */
  glViewport(x, WndHeight(window) - y - height, width, height);
  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, 0, 1);
  glPopAttrib();
}

void ClsCol(int color) {
  float rgba[4];
  ColToFlts(color, rgba);
  glClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void SwpBufs(Wnd window) {
  OsSwpBufs(window);
  /* this is to avoid having to have an extra function to call at the beginning of the frame */
  glClear(GL_COLOR_BUFFER_BIT);
}
