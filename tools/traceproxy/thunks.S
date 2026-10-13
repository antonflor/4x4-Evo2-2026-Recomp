.intel_syntax noprefix
.text
.globl _T_APIDLLGetDisplayContext
_T_APIDLLGetDisplayContext:
  pushad
  lea eax,[esp+36]
  push eax
  push 0
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+0]
.globl _T_APIDLLInformation
_T_APIDLLInformation:
  pushad
  lea eax,[esp+36]
  push eax
  push 1
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+4]
.globl _T_APIDLLReleaseDisplayContext
_T_APIDLLReleaseDisplayContext:
  pushad
  lea eax,[esp+36]
  push eax
  push 2
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+8]
.globl _T_APIDLLadd3dLine
_T_APIDLLadd3dLine:
  pushad
  lea eax,[esp+36]
  push eax
  push 3
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+12]
.globl _T_APIDLLaddParticle
_T_APIDLLaddParticle:
  pushad
  lea eax,[esp+36]
  push eax
  push 4
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+16]
.globl _T_APIDLLbeginScene
_T_APIDLLbeginScene:
  pushad
  lea eax,[esp+36]
  push eax
  push 5
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+20]
.globl _T_APIDLLbuildCardList
_T_APIDLLbuildCardList:
  pushad
  lea eax,[esp+36]
  push eax
  push 6
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+24]
.globl _T_APIDLLclear
_T_APIDLLclear:
  pushad
  lea eax,[esp+36]
  push eax
  push 7
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+28]
.globl _T_APIDLLclearZBox
_T_APIDLLclearZBox:
  pushad
  lea eax,[esp+36]
  push eax
  push 8
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+32]
.globl _T_APIDLLclearZBuffer
_T_APIDLLclearZBuffer:
  pushad
  lea eax,[esp+36]
  push eax
  push 9
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+36]
.globl _T_APIDLLdrawPolyList
_T_APIDLLdrawPolyList:
  pushad
  lea eax,[esp+36]
  push eax
  push 10
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+40]
.globl _T_APIDLLdrawPolyList2
_T_APIDLLdrawPolyList2:
  pushad
  lea eax,[esp+36]
  push eax
  push 11
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+44]
.globl _T_APIDLLdrawPolyList3
_T_APIDLLdrawPolyList3:
  pushad
  lea eax,[esp+36]
  push eax
  push 12
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+48]
.globl _T_APIDLLdrawPolygon
_T_APIDLLdrawPolygon:
  pushad
  lea eax,[esp+36]
  push eax
  push 13
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+52]
.globl _T_APIDLLdrawPolygon2
_T_APIDLLdrawPolygon2:
  pushad
  lea eax,[esp+36]
  push eax
  push 14
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+56]
.globl _T_APIDLLenableClipping
_T_APIDLLenableClipping:
  pushad
  lea eax,[esp+36]
  push eax
  push 15
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+60]
.globl _T_APIDLLenableCulling
_T_APIDLLenableCulling:
  pushad
  lea eax,[esp+36]
  push eax
  push 16
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+64]
.globl _T_APIDLLendScene
_T_APIDLLendScene:
  pushad
  lea eax,[esp+36]
  push eax
  push 17
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+68]
.globl _T_APIDLLflushLineList
_T_APIDLLflushLineList:
  pushad
  lea eax,[esp+36]
  push eax
  push 18
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+72]
.globl _T_APIDLLflushParticleList
_T_APIDLLflushParticleList:
  pushad
  lea eax,[esp+36]
  push eax
  push 19
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+76]
.globl _T_APIDLLgetTextureHandle
_T_APIDLLgetTextureHandle:
  pushad
  lea eax,[esp+36]
  push eax
  push 20
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+80]
.globl _T_APIDLLgetTextureInfo
_T_APIDLLgetTextureInfo:
  pushad
  lea eax,[esp+36]
  push eax
  push 21
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+84]
.globl _T_APIDLLgetVideoMemory
_T_APIDLLgetVideoMemory:
  pushad
  lea eax,[esp+36]
  push eax
  push 22
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+88]
.globl _T_APIDLLinit
_T_APIDLLinit:
  pushad
  lea eax,[esp+36]
  push eax
  push 23
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+92]
.globl _T_APIDLLkill
_T_APIDLLkill:
  pushad
  lea eax,[esp+36]
  push eax
  push 24
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+96]
.globl _T_APIDLLlineListD3DL
_T_APIDLLlineListD3DL:
  pushad
  lea eax,[esp+36]
  push eax
  push 25
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+100]
.globl _T_APIDLLlineListD3DTL
_T_APIDLLlineListD3DTL:
  pushad
  lea eax,[esp+36]
  push eax
  push 26
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+104]
.globl _T_APIDLLlockFrame
_T_APIDLLlockFrame:
  pushad
  lea eax,[esp+36]
  push eax
  push 27
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+108]
.globl _T_APIDLLmasterZBuffer
_T_APIDLLmasterZBuffer:
  pushad
  lea eax,[esp+36]
  push eax
  push 28
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+112]
.globl _T_APIDLLpolyListD3DL
_T_APIDLLpolyListD3DL:
  pushad
  lea eax,[esp+36]
  push eax
  push 29
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+116]
.globl _T_APIDLLpolyListL
_T_APIDLLpolyListL:
  pushad
  lea eax,[esp+36]
  push eax
  push 30
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+120]
.globl _T_APIDLLpolyListTL
_T_APIDLLpolyListTL:
  pushad
  lea eax,[esp+36]
  push eax
  push 31
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+124]
.globl _T_APIDLLresetTextureCache
_T_APIDLLresetTextureCache:
  pushad
  lea eax,[esp+36]
  push eax
  push 32
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+128]
.globl _T_APIDLLrestoreVideoMode
_T_APIDLLrestoreVideoMode:
  pushad
  lea eax,[esp+36]
  push eax
  push 33
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+132]
.globl _T_APIDLLrestoreZBuffer
_T_APIDLLrestoreZBuffer:
  pushad
  lea eax,[esp+36]
  push eax
  push 34
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+136]
.globl _T_APIDLLselectCard
_T_APIDLLselectCard:
  pushad
  lea eax,[esp+36]
  push eax
  push 35
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+140]
.globl _T_APIDLLselectTexture
_T_APIDLLselectTexture:
  pushad
  lea eax,[esp+36]
  push eax
  push 36
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+144]
.globl _T_APIDLLselectTextureBGRA
_T_APIDLLselectTextureBGRA:
  pushad
  lea eax,[esp+36]
  push eax
  push 37
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+148]
.globl _T_APIDLLsetColorTable16
_T_APIDLLsetColorTable16:
  pushad
  lea eax,[esp+36]
  push eax
  push 38
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+152]
.globl _T_APIDLLsetFog
_T_APIDLLsetFog:
  pushad
  lea eax,[esp+36]
  push eax
  push 39
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+156]
.globl _T_APIDLLsetFogColor
_T_APIDLLsetFogColor:
  pushad
  lea eax,[esp+36]
  push eax
  push 40
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+160]
.globl _T_APIDLLsetMipMapLevel
_T_APIDLLsetMipMapLevel:
  pushad
  lea eax,[esp+36]
  push eax
  push 41
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+164]
.globl _T_APIDLLsetTextureClamp
_T_APIDLLsetTextureClamp:
  pushad
  lea eax,[esp+36]
  push eax
  push 42
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+168]
.globl _T_APIDLLsetTransform
_T_APIDLLsetTransform:
  pushad
  lea eax,[esp+36]
  push eax
  push 43
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+172]
.globl _T_APIDLLsetVideoMode
_T_APIDLLsetVideoMode:
  pushad
  lea eax,[esp+36]
  push eax
  push 44
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+176]
.globl _T_APIDLLsetVideoMode2
_T_APIDLLsetVideoMode2:
  pushad
  lea eax,[esp+36]
  push eax
  push 45
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+180]
.globl _T_APIDLLsetViewport
_T_APIDLLsetViewport:
  pushad
  lea eax,[esp+36]
  push eax
  push 46
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+184]
.globl _T_APIDLLsync
_T_APIDLLsync:
  pushad
  lea eax,[esp+36]
  push eax
  push 47
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+188]
.globl _T_APIDLLtoggle
_T_APIDLLtoggle:
  pushad
  lea eax,[esp+36]
  push eax
  push 48
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+192]
.globl _T_APIDLLunlockFrame
_T_APIDLLunlockFrame:
  pushad
  lea eax,[esp+36]
  push eax
  push 49
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+196]
.globl _T_APIDLLupdateTexture
_T_APIDLLupdateTexture:
  pushad
  lea eax,[esp+36]
  push eax
  push 50
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+200]
.globl _T_APIDLLupdateTextureBGRA
_T_APIDLLupdateTextureBGRA:
  pushad
  lea eax,[esp+36]
  push eax
  push 51
  call _LogCall
  add esp,8
  popad
  jmp dword ptr [_R+204]
