#!/usr/bin/env python

# run from WeebCore root dir with
# ./build.sh lib && vblank_mode=0 LD_LIBRARY_PATH="$(pwd)" Examples/0006_ffi.py

from ctypes import CDLL, c_void_p, c_int, c_float, CFUNCTYPE

weeb = CDLL('WeebCore.so')  # on windows you would change this to dll
weeb.MkMesh.restype = weeb.AppWnd.restype = weeb.MkMesh.restype = c_void_p
weeb.Col.argtypes = [c_void_p, c_int]
weeb.Quad.argtypes = [c_void_p] + [c_float] * 4
weeb.PutMesh.argtypes = [c_void_p] * 3

INIT = 6
FRAME = 7


class Game:
  def __init__(self):
    self.mesh = None


G = Game()


@CFUNCTYPE(None)
def init():
  G.mesh = weeb.MkMesh()
  weeb.Col(G.mesh, 0xff3333)
  weeb.Quad(G.mesh, 10, 10, 200, 100)


@CFUNCTYPE(None)
def frame():
  weeb.PutMesh(G.mesh, None, None)


weeb.On(INIT, init)
weeb.On(FRAME, frame)
weeb.AppMain(0, None)
