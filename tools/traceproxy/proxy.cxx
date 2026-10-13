// Auto-generated trace proxy: logs every renderer call, forwards to REAL_DLL.
#include <windows.h>
#include <stdio.h>
static const char* N[]={"APIDLLGetDisplayContext","APIDLLInformation","APIDLLReleaseDisplayContext","APIDLLadd3dLine","APIDLLaddParticle","APIDLLbeginScene","APIDLLbuildCardList","APIDLLclear","APIDLLclearZBox","APIDLLclearZBuffer","APIDLLdrawPolyList","APIDLLdrawPolyList2","APIDLLdrawPolyList3","APIDLLdrawPolygon","APIDLLdrawPolygon2","APIDLLenableClipping","APIDLLenableCulling","APIDLLendScene","APIDLLflushLineList","APIDLLflushParticleList","APIDLLgetTextureHandle","APIDLLgetTextureInfo","APIDLLgetVideoMemory","APIDLLinit","APIDLLkill","APIDLLlineListD3DL","APIDLLlineListD3DTL","APIDLLlockFrame","APIDLLmasterZBuffer","APIDLLpolyListD3DL","APIDLLpolyListL","APIDLLpolyListTL","APIDLLresetTextureCache","APIDLLrestoreVideoMode","APIDLLrestoreZBuffer","APIDLLselectCard","APIDLLselectTexture","APIDLLselectTextureBGRA","APIDLLsetColorTable16","APIDLLsetFog","APIDLLsetFogColor","APIDLLsetMipMapLevel","APIDLLsetTextureClamp","APIDLLsetTransform","APIDLLsetVideoMode","APIDLLsetVideoMode2","APIDLLsetViewport","APIDLLsync","APIDLLtoggle","APIDLLunlockFrame","APIDLLupdateTexture","APIDLLupdateTextureBGRA"};
extern "C" { void* R[52]; }
static FILE* F; static unsigned long C;
extern "C" void __cdecl LogCall(int i, unsigned* a){ if(!F){F=fopen(".\\system\\trace_proxy.log","w");} if(F && C++<60000){fprintf(F,"%s(%08x,%08x,%08x,%08x)\n",N[i],a[0],a[1],a[2],a[3]);fflush(F);} }
extern "C" void __cdecl Load(){ HMODULE h=LoadLibraryA(REAL_DLL); for(int i=0;i<52;i++) R[i]=(void*)GetProcAddress(h,N[i]); }
BOOL WINAPI DllMain(HINSTANCE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) Load(); return TRUE; }
