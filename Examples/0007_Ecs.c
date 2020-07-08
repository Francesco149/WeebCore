#include "WeebCore.c"

/* prototype of the ECS that will be built into WeebCore
 *
 * Ents with the same Comps are grouped together in memory as buckets.
 *
 * buckets are laid out as structs of arrays. for example, a bucket with 10 Ents that have
 * a Transform and a Mesh will be laid out as
 *
 * struct {
 *   TransformData transforms[10];
 *   MeshData meshes[10];
 * }
 */

/*

- Ents by themselves don't contain any data. they're just a unique integer handle
- Ents are assigned to a bucket which contains the data of all the entities as an array
- buckets are identified by an arbitrarily long bitmask represented as a int array. this bitmask
  represents which Comps the entities have. these bitmasks are also interned to save memory. that
  way the entity handle only needs to have a pointer to the only instance of the bitmask array
- Ent handles are Ent's themselves. code that deals with handles assumes the Ent is in the bucket
  for Ents with only a Handle Comp. the Handle comp just contains the bitmask ptr and index within
  the bucket
- we can simply AND bitmasks with the components we are looking for to see which buckets match and
  call systems on matching ones

TODO:
- we want some hooks on comp creation/destruction and stuff like that to be able to cleanup
- for hierarchies of entities, we just create a unique Comp for each parent entity which is used
  to tag child entities, that way they are grouped together in a separate bucket. we will also have
  a built-in Orphan Comp for root Ent's. we also probably want a generic Parent comp which just
  contains the mask for the parent Comp bits
- run a func on all Ents that have a System comp which will recursively run the System's func on all
  Ents that match (SystemComps | ChildOf<ParentEnt>), where ParentEnt starts as Orphan.
  also implement a Priority comp that lets us give an execution order. reserve negative priorities
  for builtin stuff that must run before everything else
- these primitives should be enough to build arbitrarily complex systems on top

*/

/* utils for performing bit operations on Arrs of int's */

#define DefArrOp(name, op) \
void Arr##name(int** dst, int* src1, int* src2) { \
  int i, len = Min(ArrLen(src1), ArrLen(src2)); \
  SetArrLen(*dst, 0); \
  for (i = 0; i < len; ++i) { \
    ArrCat(dst, src1[i] op src2[i]); \
  } \
}

DefArrOp(Or, |)
DefArrOp(And, &)

void ArrNot(int** dst, int* arr) {
  int i, len = ArrLen(*dst);
  SetArrLen(*dst, 0);
  for (i = 0; i < len; ++i) {
    ArrCat(dst, ~arr[i]);
  }
}

/* find first bit that is equal to bit (either 1 or 0) */
int ArrBitScan(int* arr, int bit) {
  int i, j;
  for (i = 0; i < ArrLen(arr); ++i) {
    for (j = 0; j < 32; ++j) {
      if ((arr[i] & (1 << j)) == (bit << j)) {
        return i * 32 + j;
      }
    }
  }
  return -1;
}

/* count number of bits that are equal to bit (either 1 or 0) */
int ArrBitCount(int* arr, int bit) {
  int i, j, res = 0;
  for (i = 0; i < ArrLen(arr); ++i) {
    for (j = 0; j < 32; ++j) {
      if ((arr[i] & (1 << j)) == (bit << j)) {
        res += 1;
      }
    }
  }
  return res;
}

/* returns non-zero if (arr & bits) == bits */
int ArrHasBits(int* arr, int* bits) {
  int* and = 0;
  int res;
  ArrAnd(&and, arr, bits);
  res = ArrMemCmp(and, bits);
  RmArr(and);
  return res == 0;
}

/* ---------------------------------------------------------------------------------------------- */

typedef int Ent;

typedef struct _EcsBucket {
  int* mask;     /* interned mask so we don't store copies */
  int* comps;    /* comp id's */
  int compsLen;  /* total number of bytes occupied by 1 instance of each comp */
  Map offs;      /* map of comp id -> offset into data where the comp arr starts */
  int* occupied; /* bitmask of occupied indices */
  char* data;
} EcsBucket;

typedef struct _Comp {
  char* name;
  int id, len;
} Comp;

typedef struct _HandleComp {
  int* mask; /* ptr to interned mask */
  int index;
} HandleComp;

typedef struct _NameComp {
  char* name;
} NameComp;

typedef void SystemFunc(int* mask);

typedef struct _SystemComp {
  int prio;
  int* mask;
  SystemFunc* func;
} SystemComp;

Arena ecsArena;
Hash ecsBuckets;
Map compsById;
Hash compsByName;
int nextCompId;

/* hardcoded archetypes for bootstrap comps */
#define HANDLE_COMP_ID 0

int* nullBucket;
int* handleCompBucket;

static Comp* CompByName(char* name) {
  return HashGet(compsByName, name);
}

static Comp* CompById(int id) {
  return MapGet(compsById, id);
}

/* converts an Arr of Comp id's to an arbitrarily long bitmask (little endian). returns an Arr */
static int* CompsToMask(int* comps) {
  int* mask = 0;
  int i;
  for (i = 0; i < ArrLen(comps); ++i) {
    Comp* comp = CompById(comps[i]);
    while (comp->id / 32 >= ArrLen(mask)) {
      ArrCat(&mask, 0);
    }
    mask[comp->id / 32] |= 1 << (comp->id % 32);
  }
  return mask;
}

/* convert a string such as Comp1|Comp2 into an arr of Comp id's. spaces are ignored.
 * invalid comps are silently ignored */
static int* CompsStrToArr(char* s) {
  char* name = 0;
  int* comps = 0;
  int parsing = 1;
  for (; parsing; ++s) {
    switch (*s) {
      case ' ':
      case '\t':
      case '\n':
      case '\v':
      case '\f':
      case '\r': {
        continue;
      }
      case 0: {
        parsing = 0;
      }
      case '|': {
        Comp* comp;
        ArrCat(&name, 0);
        comp = CompByName(name);
        SetArrLen(name, 0);
        if (comp) {
          ArrCat(&comps, comp->id);
        }
        break;
      }
      default: {
        ArrCat(&name, *s);
        break;
      }
    }
  }
  RmArr(name);
  return comps;
}

/* like CompsStrToArr but returns the bitmask for the Comps */
static int* CompsStrToMask(char* s) {
  int* comps = CompsStrToArr(s);
  int* mask = CompsToMask(comps);
  RmArr(comps);
  return mask;
}

/* converts a bitmask of Comps to an Arr of comp id's */
static int* MaskToCompsArr(int* mask) {
  int i, j;
  int* res = 0;
  for (i = 0; i < ArrLen(mask); ++i) {
    int c = mask[i];
    for (j = 0; j < 32; ++j) {
      if (c & (1<<j)) {
        ArrCat(&res, i * 32 + j);
      }
    }
  }
  return res;
}

void MkCompEx(char* name, int len) {
  Comp* comp = ArenaAlloc(ecsArena, sizeof(Comp));
  comp->id = nextCompId++;
  comp->len = len;
  comp->name = name;
  MapSet(compsById, comp->id, comp);
  HashSet(compsByName, name, comp);
}

#define MkComp(T) MkCompEx(#T, sizeof(T))

static EcsBucket* MkEcsBucket(int* mask) {
  EcsBucket* bucket = ArenaAlloc(ecsArena, sizeof(EcsBucket));
  if (bucket) {
    int i, off;
    bucket->mask = ArrDup(mask);
    bucket->comps = MaskToCompsArr(mask);
    bucket->data = 0;
    bucket->offs = MkMap();
    for (off = 0, i = 0; i < ArrLen(bucket->comps); ++i) {
      Comp* comp = CompById(bucket->comps[i]);
      off += comp->len;
    }
    bucket->compsLen = off;
  }
  return bucket;
}

static void RmEcsBucket(EcsBucket* bucket) {
  if (bucket) {
    RmArr(bucket->comps);
    RmArr(bucket->data);
    RmMap(bucket->offs);
    RmArr(bucket->mask);
    RmArr(bucket->occupied);
  }
}

static void SetEcsBucket(int* mask, EcsBucket* bucket) {
  HashSetb(ecsBuckets, (char*)mask, ArrLen(mask) * 4, bucket);
}

static EcsBucket* GetEcsBucket(int* mask) {
  EcsBucket* bucket = HashGetb(ecsBuckets, (char*)mask, ArrLen(mask) * 4);
  if (!bucket) {
    bucket = MkEcsBucket(mask);
    SetEcsBucket(mask, bucket);
  }
  return bucket;
}

static int EcsBucketCap(EcsBucket* bucket) {
  if (bucket) {
    return bucket->compsLen ? ArrCap(bucket->data) / bucket->compsLen : 0;
  }
  return 0;
}

static int EcsBucketOccupancy(EcsBucket* bucket) {
  return ArrBitCount(bucket->occupied, 1);
}

static int EcsBucketOccupied(EcsBucket* bucket, int index) {
  return bucket->occupied[index / 32] & (1 << (index % 32));
}

static void* GetCompsInternal(EcsBucket* bucket, int compId) {
  if (bucket) {
    int off = (int)MapGet(bucket->offs, compId);
    return &bucket->data[off];
  }
  return 0;
}

static void* GetCompInternal(EcsBucket* bucket, int compId, int index) {
  char* comps = GetCompsInternal(bucket, compId);
  Comp* comp = CompById(compId);
  if (comps && comp) {
    return comps + comp->len * index;
  }
  return 0;
}

static HandleComp* EntHandle(Ent ent) {
  EcsBucket* bucket = GetEcsBucket(handleCompBucket);
  return GetCompInternal(bucket, HANDLE_COMP_ID, ent);
}

void* GetComps(int* mask, char* compName) {
  EcsBucket* bucket = GetEcsBucket(mask);
  Comp* comp = CompByName(compName);
  if (comp) {
    return GetCompsInternal(bucket, comp->id);
  }
  return 0;
}

void* GetComp(Ent ent, char* name) {
  HandleComp* handle = EntHandle(ent);
  EcsBucket* bucket = GetEcsBucket(handle->mask);
  Comp* comp = CompByName(name);
  return GetCompInternal(bucket, comp->id, handle->index);
}

/* we use a occupancy bitmask to avoid having to relocate all the memory every time we remove */

static int EcsBucketAlloc(int* mask) {
  EcsBucket* bucket = GetEcsBucket(mask);
  char* data = bucket->data;
  int cap = EcsBucketCap(bucket);
  int len = EcsBucketOccupancy(bucket);
  int compsLen = bucket->compsLen;
  Map offs = bucket->offs;
  int* comps = bucket->comps;
  int index, i;
  /* rebuild entire array to relen it (it's laid out as struct of arrays so we reloc everything) */
  if (cap - len <= 0) {
    char* newData = 0;
    int off, newOff;
    int newCap = Max(cap * 2, 32);
    ArrAlloc(&newData, compsLen * newCap);
    for (off = 0, newOff = 0, i = 0; i < ArrLen(comps); ++i) {
      Comp* comp = CompById(comps[i]);
      if (data) {
        MemCpy(&newData[newOff], &data[off], comp->len * len);
      }
      MapSet(offs, comps[i], (void*)newOff);
      newOff += newCap * comp->len;
      off += cap * comp->len;
    }
    RmArr(data);
    bucket->data = data = newData;
    while (ArrLen(bucket->occupied) < newCap) {
      ArrCat(&bucket->occupied, 0);
    }
    cap = newCap;
  }
  index = ArrBitScan(bucket->occupied, 0);
  bucket->occupied[index / 32] |= 1 << (index % 32);
  for (i = 0; i < ArrLen(comps); ++i) {
    Comp* comp = CompById(comps[i]);
    MemSet(GetCompInternal(bucket, comp->id, index), 0, comp->len);
  }
  return index;
}

void RmEnt(Ent ent) {
  HandleComp* handle = EntHandle(ent);
  EcsBucket* bucket = GetEcsBucket(handle->mask);
  bucket->occupied[handle->index / 32] &= ~(1 << (handle->index % 32));
}

static Ent MkEntHandle(int* mask, int index) {
  EcsBucket* bucket = GetEcsBucket(mask);
  Ent res = EcsBucketAlloc(handleCompBucket);
  HandleComp* handle = EntHandle(res);
  handle->index = index;
  handle->mask = bucket->mask;
  return res;
}

static Ent MkEntInBucket(int* mask) {
  return MkEntHandle(mask, EcsBucketAlloc(mask));
}

Ent MkEnt() {
  return MkEntInBucket(nullBucket);
}

int EntValid(Ent ent) {
  EcsBucket* bucket = GetEcsBucket(handleCompBucket);
  return EcsBucketOccupied(bucket, ent);
}

int NextEnt(int* mask, int index) {
  EcsBucket* bucket = GetEcsBucket(mask);
  for (++index; index < EcsBucketCap(bucket) && !EcsBucketOccupied(bucket, index); ++index);
  if (index >= EcsBucketCap(bucket)) {
    return -1;
  }
  return index;
}

int FirstEnt(int* mask) { return NextEnt(mask, -1); }

static void RelocateEnt(Ent ent, int* newMask) {
  HandleComp* handle = EntHandle(ent);
  if (ArrMemCmp(newMask, handle->mask)) {
    EcsBucket* oldBucket = GetEcsBucket(handle->mask);
    EcsBucket* bucket = GetEcsBucket(newMask);
    int index = EcsBucketAlloc(newMask);
    int* comps = MaskToCompsArr(handle->mask);
    int i;
    for (i = 0; i < ArrLen(comps); ++i) {
      Comp* comp = CompById(comps[i]);
      void* old = GetCompInternal(oldBucket, comp->id, handle->index);
      void* new = GetCompInternal(bucket, comp->id, index);
      MemCpy(new, old, comp->len);
    }
    RmEnt(ent);
    handle->mask = bucket->mask;
    handle->index = index;
  }
}

void* AddComp(Ent ent, char* name) {
  HandleComp* handle = EntHandle(ent);
  int* newMask = CompsStrToMask(name);
  ArrOr(&newMask, newMask, handle->mask);
  RelocateEnt(ent, newMask);
  RmArr(newMask);
  return GetComp(ent, name);
}

void RmComp(Ent ent, char* name) {
  HandleComp* handle = EntHandle(ent);
  int* newMask = CompsStrToMask(name);
  ArrNot(&newMask, newMask);
  ArrAnd(&newMask, newMask, handle->mask);
  RelocateEnt(ent, newMask);
  RmArr(newMask);
}

void MkEcs() {
  /* initialize hardcoded bucket masks */
  ArrCat(&nullBucket, 0);
  ArrCat(&handleCompBucket, 1<<HANDLE_COMP_ID);

  ecsArena = MkArena();
  ecsBuckets = MkHash();
  compsById = MkMap();
  compsByName = MkHash();

  MkComp(HandleComp);
  MkComp(NameComp);
  MkComp(SystemComp);
}

void RmEcs() {
  int i;
  for (i = 0; i < HashNumKeys(ecsBuckets); ++i) {
    EcsBucket* bucket = HashGetb(ecsBuckets, HashKey(ecsBuckets, i), HashKeyLen(ecsBuckets, i));
    if (bucket) {
      RmEcsBucket(bucket);
    }
  }
  RmMap(compsById);
  RmHash(compsByName);
  RmHash(ecsBuckets);
  RmArena(ecsArena);
  RmArr(nullBucket);
  RmArr(handleCompBucket);
}

static EcsBucket** FindEcsBucketsByMask(int* mask) {
  EcsBucket** buckets = 0;
  int i;
  for (i = 0; i < HashNumKeys(ecsBuckets); ++i) {
    EcsBucket* bucket = HashGetb(ecsBuckets, HashKey(ecsBuckets, i), HashKeyLen(ecsBuckets, i));
    if (ArrHasBits(bucket->mask, mask)) {
      ArrCat(&buckets, bucket);
    }
  }
  return buckets;
}

static EcsBucket** FindEcsBuckets(char* compsStr) {
  int* mask = CompsStrToMask(compsStr);
  EcsBucket** buckets = FindEcsBucketsByMask(mask);
  RmArr(mask);
  return buckets;
}

Ent MkSystemEx(int prio, char* compsStr, SystemFunc func, char* name) {
  Ent ent = MkEnt();
  SystemComp* sys;
  NameComp* nameComp;
  sys = AddComp(ent, "SystemComp");
  sys->prio = prio;
  sys->mask = CompsStrToMask(compsStr);
  sys->func = func;
  nameComp = AddComp(ent, "NameComp");
  nameComp->name = name;
  sys = GetComp(ent, "SystemComp");
  return ent;
}

#define MkSystem(prio, compsStr, func) MkSystemEx(prio, compsStr, func, #func)

static int CmpSystems(SystemComp* a, SystemComp* b) {
  if (a->prio < b->prio) {
    return -1;
  } else if (a->prio > b->prio) {
    return 1;
  }
  return 0;
}

void EcsFrame() {
  /* TODO: be more efficient than an array of ptrs? */
  SystemComp** systems = 0;
  EcsBucket** buckets = FindEcsBuckets("SystemComp");
  int i, j;
  Comp* comp = CompByName("SystemComp");
  for (i = 0; i < ArrLen(buckets); ++i) {
    EcsBucket* bucket = buckets[i];
    SystemComp* comps = GetCompsInternal(bucket, comp->id);
    for (j = 0; j < EcsBucketCap(bucket); ++j) {
      if (EcsBucketOccupied(bucket, j)) {
        ArrCat(&systems, &comps[j]);
      }
    }
  }
  RmArr(buckets);
  Qsort((void**)systems, ArrLen(systems), (QsortCmp*)CmpSystems);
  for (i = 0; i < ArrLen(systems); ++i) {
    SystemComp* sys = systems[i];
    buckets = FindEcsBucketsByMask(sys->mask);
    for (j = 0; j < ArrLen(buckets); ++j) {
      sys->func(buckets[j]->mask);
    }
    RmArr(buckets);
  }
  RmArr(systems);
}

void EcsInit() {
  On(INIT, MkEcs);
  On(QUIT, RmEcs);
  On(FRAME, EcsFrame);
}

void DiagEcsFrame() {
  int i;
  char* lines = 0;
  EcsBucket* handles = GetEcsBucket(handleCompBucket);
  for (i = 0; i < HashNumKeys(ecsBuckets); ++i) {
    EcsBucket* bucket = HashGetb(ecsBuckets, HashKey(ecsBuckets, i), HashKeyLen(ecsBuckets, i));
    int* comps = bucket->comps;
    if (EcsBucketOccupancy(bucket)) {
      int j, start, n = 0;
      char* s = 0;
      int* nameCompMask = CompsStrToMask("NameComp");
      int hasName = ArrHasBits(bucket->mask, nameCompMask);
      for (j = 0; j < ArrLen(comps); ++j) {
        Comp* comp = CompById(comps[j]);
        if (j != 0) { ArrStrCat(&s, "|"); }
        ArrStrCat(&s, comp->name);
      }
      if (!s) {
        ArrStrCat(&s, "(no comps)");
      }
      ArrStrCat(&s, ": ");
      ArrStrCatI32(&s, EcsBucketOccupancy(bucket), 10);
      ArrStrCat(&s, "/");
      ArrStrCatI32(&s, EcsBucketCap(bucket), 10);
      ArrStrCat(&s, " ents (");
      ArrStrCatI32(&s, bucket->compsLen, 10);
      ArrStrCat(&s, "b per ent)");
      ArrStrCat(&s, "\n  ");
      start = ArrLen(s);
      if (hasName) {
        for (j = 0; j < EcsBucketCap(handles); ++j) {
          if (EntValid(j)) {
            HandleComp* handle = EntHandle(j);
            NameComp* name;
            if (handle->mask != bucket->mask) { continue; }
            ++n;
            if (j) { ArrStrCat(&s, " "); }
            ArrStrCatI32(&s, j, 16);
            ArrStrCat(&s, "/");
            ArrStrCatI32(&s, handle->index, 16);
            name = GetComp(j, "NameComp");
            ArrStrCat(&s, "'");
            ArrStrCat(&s, name->name);
            ArrStrCat(&s, "'");
            if (j < EcsBucketCap(handles) && ArrLen(s) - start >= 90) {
              ArrStrCat(&s, "\n  ");
              start = ArrLen(s);
            }
          }
        }
      }
      if (n) {
        ArrStrCat(&s, "\n  ");
      }
      ArrCat(&s, 0);
      ArrStrCat(&lines, s);
      ArrStrCat(&lines, "\n");
      RmArr(s);
      RmArr(nameCompMask);
    }
  }
  ArrCat(&lines, 0);
  PutFt(DefFt(), 0xbebebe, 10, 30, lines);
  RmArr(lines);
}

void DiagEcs(int enabled) {
  if (enabled) {
    On(FRAME, DiagEcsFrame);
  } else {
    RmHandler(FRAME, DiagEcsFrame);
  }
}

/* ---------------------------------------------------------------------------------------------- */

/* TODO: actually embedding these structs in the Comp's would make more sense */
typedef struct _MeshComp {
  Mesh mesh;
} MeshComp;

typedef struct _TransComp {
  Trans trans;
} TransComp;

typedef struct _MovementComp {
  float x, y;
  float vx, vy;
} MovementComp;

Ent MkParticle(int x, int y) {
  Ent ent = MkEnt();
  MovementComp* mov;
  MeshComp* mesh;
  TransComp* trans;

  mov = AddComp(ent, "MovementComp");
  mov->x = x;
  mov->y = y;

  mesh = AddComp(ent, "MeshComp");
  mesh->mesh = MkMesh();
  Col(mesh->mesh, 0x993333);
  Quad(mesh->mesh, -2, -2, 4, 4);

  trans = AddComp(ent, "TransComp");
  trans->trans = MkTrans();
  return ent;
}

void MovementSystem(int* mask) {
  MovementComp* mov = GetComps(mask, "MovementComp");
  TransComp* trans = GetComps(mask, "TransComp");
  int i;
  Wnd wnd = AppWnd();
  for (i = FirstEnt(mask); i >= 0; i = NextEnt(mask, i)) {
    MovementComp* m = &mov[i];
    float dx = m->x - MouseX(wnd);
    float dy = m->y - MouseY(wnd);
    float mag = Sqrt(dx * dx + dy * dy);
    float force = 1 - Min(1, mag / 100); /* mouse repels */
    m->vx += force * dx * 30 * Delta();
    m->vy += force * dy * 30 * Delta();
    m->vx -= m->vx * Delta(); /* friction */
    m->vy -= m->vy * Delta();
    m->vx -= Min(100, Max(0, m->x - WndWidth(wnd)) * Delta() * 10000); /* edges of screen repel */
    m->vy -= Min(100, Max(0, m->y - WndHeight(wnd)) * Delta() * 10000);
    m->vx += Min(100, Max(0, -m->x) * Delta() * 10000);
    m->vy += Min(100, Max(0, -m->y) * Delta() * 10000);
    m->x += m->vx * Delta();
    m->y += m->vy * Delta();
    SetPos(trans[i].trans, (int)m->x, (int)m->y);
  }
}

void RenderSystem(int* mask) {
  MeshComp* mesh = GetComps(mask, "MeshComp");
  TransComp* trans = GetComps(mask, "TransComp");
  int i;
  for (i = FirstEnt(mask); i >= 0; i = NextEnt(mask, i)) {
    PutMesh(mesh[i].mesh, ToTmpMat(trans[i].trans), 0);
  }
}

void InitEcs() {
  MkComp(MovementComp);
  MkComp(MeshComp);
  MkComp(TransComp);
  MkSystem(0, "MovementComp|TransComp", MovementSystem);
  MkSystem(1, "MeshComp|TransComp", RenderSystem);
}

/* ---------------------------------------------------------------------------------------------- */

#define DEMO_DRAGGING 1
#define DEMO_DIAG 2
int flags = DEMO_DIAG;
float spawnTimer, fpsTimer;
int frames;
Mesh fpsMesh; /* TODO: move fps counter to ecs */
Ft ft;

void Init() {
  ft = DefFt();
  DiagEcs(flags & DEMO_DIAG);
  InitEcs();
}

void Quit() {
  RmMesh(fpsMesh);
}

void KeyDown() {
  Wnd wnd = AppWnd();
  switch (Key(wnd)) {
    case MLEFT:
      flags |= DEMO_DRAGGING;
      break;
    case F1:
      flags ^= DEMO_DIAG;
      DiagEcs(flags & DEMO_DIAG);
      break;
  }
}

void KeyUp() {
  Wnd wnd = AppWnd();
  if (Key(wnd) == MLEFT) {
    flags &= ~DEMO_DRAGGING;
  }
}

void Motion() {
  if (flags & DEMO_DRAGGING) {
    Wnd wnd = AppWnd();
    spawnTimer += Delta();
    while (spawnTimer >= 0.01f) {
      MkParticle(MouseX(wnd), MouseY(wnd));
      spawnTimer -= 0.01f;
    }
  }
}

void Frame() {
  ++frames;
  fpsTimer += Delta();
  while (!fpsMesh || fpsTimer >= 1) {
    char* s = 0;
    ArrStrCat(&s, "fps: ");
    if (fpsMesh) {
      ArrStrCatI32(&s, frames, 10);
    }
    ArrCat(&s, 0);
    RmMesh(fpsMesh);
    fpsMesh = MkMesh();
    Col(fpsMesh, 0xbebebe);
    FtMesh(fpsMesh, ft, 10, 10, s);
    RmArr(s);
    frames = 0;
    fpsTimer -= 1;
  }
  PutMesh(fpsMesh, 0, FtImg(ft));
}

void AppInit() {
  SetAppName("WeebCore - ECS Demo");
  SetAppClass("WeebCoreEcsDemo");
  EcsInit();
  On(INIT, Init);
  On(QUIT, Quit);
  On(MOTION, Motion);
  On(KEYDOWN, KeyDown);
  On(KEYUP, KeyUp);
  On(FRAME, Frame);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
