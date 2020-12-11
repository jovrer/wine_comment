/*
 * Direct3D wine internal public interface file
 *
 * Copyright 2002-2003 The wine-d3d team
 * Copyright 2002-2003 Raphael Junqueira
 * Copyright 2005 Oliver Stieber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __WINE_WINED3D_INTERFACE_H
#define __WINE_WINED3D_INTERFACE_H

#if !defined( __WINE_CONFIG_H )
# error You must include config.h to use this header
#endif

#if !defined( __WINE_D3D8_H ) && !defined( __WINE_D3D9_H )
# error You must include d3d8.h or d3d9.h header to use this header
#endif

#include "wined3d_types.h"
/*****************************************************************
 * THIS FILE MUST NOT CONTAIN X11 or MESA DEFINES 
 * PLEASE USE wine/wined3d_gl.h INSTEAD
 */


 /*****************************************************************************
 * Predeclare the interfaces
 */

struct IWineD3D;
struct IWineD3DDevice;
struct IWineD3DResource;
struct IWineD3DVertexBuffer;
struct IWineD3DIndexBuffer;
struct IWineD3DBaseTexture;
struct IWineD3DTexture;
struct IWineD3DCubeTexture;
struct IWineD3DVolumeTexture;
struct IWineD3DStateBlock;
struct IWineD3DSurface;
struct IWineD3DVolume;
struct IWineD3DVertexDeclaration;
struct IWineD3DVertexShader;
struct IWineD3DPixelShader;
struct IWineD3DQuery;
struct IWineD3DSwapChain;


/* {108F9C44-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3D, 
0x108f9c44, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {108F9C44-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DDevice, 
0x108f9c44, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);


/* {1F3BFB34-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DResource, 
0x1f3bfb34, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {217F671E-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DVertexBuffer, 
0x217f671e, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {24769ED8-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DVolume, 
0x24769ed8, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);


/* {34D01B10-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DSwapChain, 
0x34d01b10, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {37CD5526-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DSurface, 
0x37cd5526, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);


/* {3A02A54E-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DIndexBuffer, 
0x3a02a54e, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {3C2AEBF6-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DBaseTexture, 
0x3c2aebf6, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {3E72CC1C-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DTexture, 
0x3e72cc1c, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {41752900-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DCubeTexture, 
0x41752900, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {7B39470C-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DVolumeTexture, 
0x7b39470c, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {7CD55BE6-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DVertexDeclaration, 
0x7cd55be6, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {7F7A2B60-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DVertexShader, 
0x7f7a2b60, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {818503DA-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DPixelShader, 
0x818503da, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {83B073CE-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DStateBlock, 
0x83b073ce, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);

/* {905DDBAC-6F30-11d9-C687-00046142C14F} */
DEFINE_GUID(IID_IWineD3DQuery, 
0x905ddbac, 0x6f30, 0x11d9, 0xc6, 0x87, 0x0, 0x4, 0x61, 0x42, 0xc1, 0x4f);


/* The following have differing names, but actually are the same layout. */
/* Also, D3DCAPS8 is a subset of D3DCAPS9 so can be typecase as long as
     none of the 9 fields are accessed when the device is d3d8           */
/* TODO: remove the d3d8/d3d9 dependencies by making a all inclusive WINED3D version */
#if defined( __WINE_D3D8_H )
 /* Identical: */ 
# define WINED3DLIGHT           D3DLIGHT8
# define WINED3DCLIPSTATUS      D3DCLIPSTATUS8
# define WINED3DMATERIAL        D3DMATERIAL8
# define WINED3DVIEWPORT        D3DVIEWPORT8

#else
 /* Identical: */ 
# define WINED3DLIGHT           D3DLIGHT9
# define WINED3DCLIPSTATUS      D3DCLIPSTATUS9
# define WINED3DMATERIAL        D3DMATERIAL9
# define WINED3DVIEWPORT        D3DVIEWPORT9

#endif

/*****************************************************************************
 * Callback functions required for predefining surfaces / stencils
 */
typedef HRESULT WINAPI (*D3DCB_CREATERENDERTARGETFN) (IUnknown *pDevice,
                                               UINT       Width, 
                                               UINT       Height, 
                                               WINED3DFORMAT  Format, 
                                               D3DMULTISAMPLE_TYPE MultiSample, 
                                               DWORD      MultisampleQuality, 
                                               BOOL       Lockable, 
                                               struct IWineD3DSurface **ppSurface,
                                               HANDLE    *pSharedHandle);

typedef HRESULT WINAPI (*D3DCB_CREATESURFACEFN) (IUnknown *pDevice,
                                               UINT       Width, 
                                               UINT       Height,
                                               WINED3DFORMAT Format,
                                               DWORD      Usage,
                                               D3DPOOL    Pool,            
                                               UINT       Level,
                                               struct IWineD3DSurface **ppSurface,
                                               HANDLE    *pSharedHandle);

typedef HRESULT WINAPI (*D3DCB_CREATEDEPTHSTENCILSURFACEFN) (IUnknown *pDevice,
                                               UINT       Width, 
                                               UINT       Height, 
                                               WINED3DFORMAT  Format, 
                                               D3DMULTISAMPLE_TYPE MultiSample, 
                                               DWORD      MultisampleQuality, 
                                               BOOL       Discard, 
                                               struct IWineD3DSurface **ppSurface,
                                               HANDLE    *pSharedHandle);


typedef HRESULT WINAPI (*D3DCB_CREATEVOLUMEFN) (IUnknown *pDevice,
                                               UINT       Width, 
                                               UINT       Height, 
                                               UINT       Depth, 
                                               WINED3DFORMAT  Format, 
                                               D3DPOOL    Pool,
                                               DWORD      Usage,
                                               struct IWineD3DVolume **ppVolume,
                                               HANDLE    *pSharedHandle);

typedef HRESULT WINAPI (*D3DCB_CREATEADDITIONALSWAPCHAIN) (IUnknown *pDevice,
                                               WINED3DPRESENT_PARAMETERS *pPresentationParameters,
                                               struct IWineD3DSwapChain **pSwapChain
                                               );

/*****************************************************************************
 * IWineD3D interface 
 */

#define INTERFACE IWineD3D
DECLARE_INTERFACE_(IWineD3D,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3D methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD_(UINT,GetAdapterCount)(THIS) PURE;
    STDMETHOD(RegisterSoftwareDevice)(THIS_ void * pInitializeFunction) PURE;
    STDMETHOD_(HMONITOR,GetAdapterMonitor)(THIS_ UINT Adapter) PURE;
    STDMETHOD_(UINT,GetAdapterModeCount)(THIS_ UINT Adapter, WINED3DFORMAT Format) PURE;
    STDMETHOD(EnumAdapterModes)(THIS_ UINT  Adapter, UINT  Mode, WINED3DFORMAT Format, D3DDISPLAYMODE * pMode) PURE;
    STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT  Adapter, D3DDISPLAYMODE *pMode) PURE;
    STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter, DWORD Flags, WINED3DADAPTER_IDENTIFIER* pIdentifier) PURE;
    STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, WINED3DFORMAT  SurfaceFormat, BOOL  Windowed, D3DMULTISAMPLE_TYPE  MultiSampleType, DWORD *pQuality) PURE;
    STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, WINED3DFORMAT  AdapterFormat, WINED3DFORMAT  RenderTargetFormat, WINED3DFORMAT  DepthStencilFormat) PURE;
    STDMETHOD(CheckDeviceType)(THIS_ UINT  Adapter, D3DDEVTYPE  CheckType, WINED3DFORMAT  DisplayFormat, WINED3DFORMAT  BackBufferFormat, BOOL  Windowed) PURE;
    STDMETHOD(CheckDeviceFormat)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, WINED3DFORMAT  AdapterFormat, DWORD  Usage, D3DRESOURCETYPE  RType, WINED3DFORMAT  CheckFormat) PURE;
    STDMETHOD(CheckDeviceFormatConversion)(THIS_ UINT Adapter, D3DDEVTYPE DeviceType, WINED3DFORMAT SourceFormat, WINED3DFORMAT TargetFormat) PURE;
    STDMETHOD(GetDeviceCaps)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, WINED3DCAPS *pCaps) PURE;
    STDMETHOD(CreateDevice)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType,HWND  hFocusWindow, DWORD  BehaviorFlags, WINED3DPRESENT_PARAMETERS *pPresentationParameters, struct IWineD3DDevice **ppReturnedDeviceInterface, IUnknown *parent, D3DCB_CREATEADDITIONALSWAPCHAIN pFn3) PURE;

};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3D_QueryInterface(p,a,b)                    (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3D_AddRef(p)                                (p)->lpVtbl->AddRef(p)
#define IWineD3D_Release(p)                               (p)->lpVtbl->Release(p)
/*** IWineD3D methods ***/
#define IWineD3D_GetParent(p,a)                           (p)->lpVtbl->GetParent(p,a)
#define IWineD3D_GetAdapterCount(p)                       (p)->lpVtbl->GetAdapterCount(p)
#define IWineD3D_RegisterSoftwareDevice(p,a)              (p)->lpVtbl->RegisterSoftwareDevice(p,a)
#define IWineD3D_GetAdapterMonitor(p,a)                   (p)->lpVtbl->GetAdapterMonitor(p,a)
#define IWineD3D_GetAdapterModeCount(p,a,b)               (p)->lpVtbl->GetAdapterModeCount(p,a,b)
#define IWineD3D_EnumAdapterModes(p,a,b,c,d)              (p)->lpVtbl->EnumAdapterModes(p,a,b,c,d)
#define IWineD3D_GetAdapterDisplayMode(p,a,b)             (p)->lpVtbl->GetAdapterDisplayMode(p,a,b)
#define IWineD3D_GetAdapterIdentifier(p,a,b,c)            (p)->lpVtbl->GetAdapterIdentifier(p,a,b,c)
#define IWineD3D_CheckDeviceMultiSampleType(p,a,b,c,d,e,f) (p)->lpVtbl->CheckDeviceMultiSampleType(p,a,b,c,d,e,f)
#define IWineD3D_CheckDepthStencilMatch(p,a,b,c,d,e)      (p)->lpVtbl->CheckDepthStencilMatch(p,a,b,c,d,e)
#define IWineD3D_CheckDeviceType(p,a,b,c,d,e)             (p)->lpVtbl->CheckDeviceType(p,a,b,c,d,e)
#define IWineD3D_CheckDeviceFormat(p,a,b,c,d,e,f)         (p)->lpVtbl->CheckDeviceFormat(p,a,b,c,d,e,f)
#define IWineD3D_CheckDeviceFormatConversion(p,a,b,c,d)   (p)->lpVtbl->CheckDeviceFormatConversion(p,a,b,c,d)
#define IWineD3D_GetDeviceCaps(p,a,b,c)                   (p)->lpVtbl->GetDeviceCaps(p,a,b,c)
#define IWineD3D_CreateDevice(p,a,b,c,d,e,f,g,h)          (p)->lpVtbl->CreateDevice(p,a,b,c,d,e,f,g,h)
#endif

/* Define the main WineD3D entrypoint */
IWineD3D* WINAPI WineDirect3DCreate(UINT SDKVersion, UINT dxVersion, IUnknown *parent);

/*****************************************************************************
 * IWineD3DDevice interface 
 */
#define INTERFACE IWineD3DDevice
DECLARE_INTERFACE_(IWineD3DDevice,IUnknown) 
{ 
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DDevice methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(CreateVertexBuffer)(THIS_ UINT  Length,DWORD  Usage,DWORD  FVF,D3DPOOL  Pool,struct IWineD3DVertexBuffer **ppVertexBuffer, HANDLE *sharedHandle, IUnknown *parent) PURE;
    STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length, DWORD Usage, WINED3DFORMAT Format, D3DPOOL Pool, struct IWineD3DIndexBuffer** ppIndexBuffer, HANDLE* pSharedHandle, IUnknown *parent) PURE;
    STDMETHOD(CreateStateBlock)(THIS_ WINED3DSTATEBLOCKTYPE Type, struct IWineD3DStateBlock **ppStateBlock, IUnknown *parent) PURE;
    STDMETHOD(CreateSurface)(THIS_ UINT Width, UINT Height, WINED3DFORMAT Format,  BOOL Lockable, BOOL Discard, UINT Level,  struct IWineD3DSurface** ppSurface, D3DRESOURCETYPE Type, DWORD Usage, D3DPOOL Pool, D3DMULTISAMPLE_TYPE MultiSample ,DWORD MultisampleQuality,  HANDLE* pSharedHandle, IUnknown *parent) PURE;    
    STDMETHOD(CreateTexture)(THIS_ UINT Width, UINT Height, UINT Levels, DWORD Usage, WINED3DFORMAT Format, D3DPOOL Pool, struct IWineD3DTexture** ppTexture, HANDLE* pSharedHandle, IUnknown *parent, D3DCB_CREATESURFACEFN pFn) PURE;
    STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, WINED3DFORMAT Format, D3DPOOL Pool, struct IWineD3DVolumeTexture** ppVolumeTexture, HANDLE* pSharedHandle, IUnknown *parent, D3DCB_CREATEVOLUMEFN pFn) PURE;
    STDMETHOD(CreateVolume)(THIS_ UINT Width, UINT Height, UINT Depth, DWORD Usage, WINED3DFORMAT Format, D3DPOOL Pool, struct IWineD3DVolume** ppVolumeTexture, HANDLE* pSharedHandle, IUnknown *parent) PURE;
    STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength, UINT Levels, DWORD Usage, WINED3DFORMAT Format, D3DPOOL Pool, struct IWineD3DCubeTexture** ppCubeTexture, HANDLE* pSharedHandle, IUnknown *parent, D3DCB_CREATESURFACEFN pFn) PURE;
    STDMETHOD(CreateQuery)(THIS_ WINED3DQUERYTYPE Type, struct IWineD3DQuery **ppQuery, IUnknown *pParent);
    STDMETHOD(CreateAdditionalSwapChain)(THIS_ WINED3DPRESENT_PARAMETERS *pPresentationParameters, struct IWineD3DSwapChain **pSwapChain, IUnknown *pParent, D3DCB_CREATERENDERTARGETFN pFn, D3DCB_CREATEDEPTHSTENCILSURFACEFN pFn2);
    STDMETHOD(CreateVertexDeclaration)(THIS_ CONST VOID* pDeclaration, struct IWineD3DVertexDeclaration** ppDecl, IUnknown* pParent) PURE;
    STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD* pFunction, struct IWineD3DVertexShader** ppShader, IUnknown *pParent) PURE;
    STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD* pFunction, struct IWineD3DPixelShader** ppShader, IUnknown *pParent) PURE;
    STDMETHOD(EvictManagedResources)(THIS) PURE;
    STDMETHOD_(UINT, GetAvailableTextureMem)(THIS) PURE;
    STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain, UINT BackBuffer, D3DBACKBUFFER_TYPE, struct IWineD3DSurface** ppBackBuffer) PURE;
    STDMETHOD(GetCreationParameters)(THIS_ D3DDEVICE_CREATION_PARAMETERS *pParameters) PURE;
    STDMETHOD(GetDeviceCaps)(THIS_ WINED3DCAPS* pCaps) PURE;
    STDMETHOD(GetDirect3D)(THIS_ IWineD3D** ppD3D) PURE;
    STDMETHOD(GetDisplayMode)(THIS_ UINT iSwapChain, D3DDISPLAYMODE* pMode) PURE;
    STDMETHOD_(UINT, GetNumberOfSwapChains)(THIS) PURE;
    STDMETHOD(GetRasterStatus)(THIS_ UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) PURE;
    STDMETHOD(GetSwapChain)(THIS_ UINT iSwapChain, struct IWineD3DSwapChain **pSwapChain) PURE;
    STDMETHOD(Reset)(THIS_ WINED3DPRESENT_PARAMETERS* pPresentationParameters) PURE;
    STDMETHOD(SetDialogBoxMode)(THIS_ BOOL bEnableDialogs) PURE;
    STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot, UINT YHotSpot, struct IWineD3DSurface* pCursorBitmap) PURE;
    STDMETHOD_(void, SetCursorPosition)(THIS_ int XScreenSpace, int YScreenSpace, DWORD Flags) PURE;
    STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow) PURE;
    STDMETHOD(TestCooperativeLevel)(THIS) PURE;
    STDMETHOD(SetClipPlane)(THIS_ DWORD  Index,CONST float * pPlane) PURE;
    STDMETHOD(GetClipPlane)(THIS_ DWORD  Index,float * pPlane) PURE;
    STDMETHOD(SetClipStatus)(THIS_ CONST WINED3DCLIPSTATUS * pClipStatus) PURE;
    STDMETHOD(GetClipStatus)(THIS_ WINED3DCLIPSTATUS * pClipStatus) PURE;
    STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber) PURE;
    STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT *PaletteNumber) PURE;
    STDMETHOD(SetDepthStencilSurface)(THIS_ struct IWineD3DSurface* pNewZStencil) PURE;
    STDMETHOD(GetDepthStencilSurface)(THIS_ struct IWineD3DSurface** ppZStencilSurface) PURE;
    STDMETHOD(SetFVF)(THIS_ DWORD  fvf) PURE;
    STDMETHOD(GetFVF)(THIS_ DWORD * pfvf) PURE;
    STDMETHOD_(void, SetGammaRamp)(THIS_ UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) PURE;
    STDMETHOD_(void, GetGammaRamp)(THIS_ UINT iSwapChain, D3DGAMMARAMP* pRamp) PURE;
    STDMETHOD(SetIndices)(THIS_ struct IWineD3DIndexBuffer * pIndexData,UINT  BaseVertexIndex) PURE;
    STDMETHOD(GetIndices)(THIS_ struct IWineD3DIndexBuffer ** ppIndexData,UINT * pBaseVertexIndex) PURE;
    STDMETHOD(SetLight)(THIS_ DWORD  Index,CONST WINED3DLIGHT * pLight) PURE;
    STDMETHOD(GetLight)(THIS_ DWORD  Index,WINED3DLIGHT * pLight) PURE;
    STDMETHOD(SetLightEnable)(THIS_ DWORD  Index,BOOL  Enable) PURE;
    STDMETHOD(GetLightEnable)(THIS_ DWORD  Index,BOOL * pEnable) PURE;
    STDMETHOD(SetMaterial)(THIS_ CONST WINED3DMATERIAL * pMaterial) PURE;
    STDMETHOD(GetMaterial)(THIS_ WINED3DMATERIAL *pMaterial) PURE;
    STDMETHOD(SetNPatchMode)(THIS_ float nSegments) PURE;
    STDMETHOD_(float, GetNPatchMode)(THIS) PURE;
    STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber, CONST PALETTEENTRY* pEntries) PURE;
    STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber,PALETTEENTRY* pEntries) PURE;
    STDMETHOD(SetPixelShader)(THIS_ struct IWineD3DPixelShader  *pShader) PURE;
    STDMETHOD(GetPixelShader)(THIS_ struct IWineD3DPixelShader **ppShader) PURE;
    STDMETHOD(SetPixelShaderConstantB)(THIS_ UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount) PURE;
    STDMETHOD(GetPixelShaderConstantB)(THIS_ UINT StartRegister, BOOL* pConstantData, UINT BoolCount) PURE;
    STDMETHOD(SetPixelShaderConstantI)(THIS_ UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) PURE;
    STDMETHOD(GetPixelShaderConstantI)(THIS_ UINT StartRegister, int* pConstantData, UINT Vector4iCount) PURE;
    STDMETHOD(SetPixelShaderConstantF)(THIS_ UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) PURE;
    STDMETHOD(GetPixelShaderConstantF)(THIS_ UINT StartRegister, float* pConstantData, UINT Vector4fCount) PURE;
    STDMETHOD(SetRenderState)(THIS_ D3DRENDERSTATETYPE  State,DWORD  Value) PURE;
    STDMETHOD(GetRenderState)(THIS_ D3DRENDERSTATETYPE  State,DWORD * pValue) PURE;
    STDMETHOD(SetRenderTarget)(THIS_ DWORD RenderTargetIndex, struct IWineD3DSurface* pRenderTarget) PURE;
    STDMETHOD(GetRenderTarget)(THIS_ DWORD RenderTargetIndex, struct IWineD3DSurface** ppRenderTarget) PURE;
    STDMETHOD(SetSamplerState)(THIS_ DWORD Sampler, WINED3DSAMPLERSTATETYPE Type, DWORD Value) PURE;
    STDMETHOD(GetSamplerState)(THIS_ DWORD Sampler, WINED3DSAMPLERSTATETYPE Type, DWORD* Value) PURE;
    STDMETHOD(SetScissorRect)(THIS_ CONST RECT* pRect) PURE;
    STDMETHOD(GetScissorRect)(THIS_ RECT* pRect) PURE;
    STDMETHOD(SetSoftwareVertexProcessing)(THIS_ BOOL bSoftware) PURE;
    STDMETHOD_(BOOL, GetSoftwareVertexProcessing)(THIS) PURE;
    STDMETHOD(SetStreamSource)(THIS_ UINT  StreamNumber,struct IWineD3DVertexBuffer * pStreamData,UINT Offset,UINT  Stride) PURE;
    STDMETHOD(GetStreamSource)(THIS_ UINT  StreamNumber,struct IWineD3DVertexBuffer ** ppStreamData,UINT *pOffset, UINT * pStride) PURE;
    STDMETHOD(SetStreamSourceFreq)(THIS_ UINT StreamNumber, UINT Divider) PURE;
    STDMETHOD(GetStreamSourceFreq)(THIS_ UINT StreamNumber, UINT* Divider) PURE;
    STDMETHOD(SetTexture)(THIS_ DWORD Stage, struct IWineD3DBaseTexture* pTexture) PURE;
    STDMETHOD(GetTexture)(THIS_ DWORD Stage, struct IWineD3DBaseTexture** ppTexture) PURE;
    STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage, WINED3DTEXTURESTAGESTATETYPE Type,DWORD Value) PURE;
    STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage, WINED3DTEXTURESTAGESTATETYPE Type,DWORD *pValue) PURE;
    STDMETHOD(SetTransform)(THIS_ D3DTRANSFORMSTATETYPE  State,CONST D3DMATRIX * pMatrix) PURE;
    STDMETHOD(GetTransform)(THIS_ D3DTRANSFORMSTATETYPE  State,D3DMATRIX * pMatrix) PURE;
    STDMETHOD(SetVertexDeclaration)(THIS_ struct IWineD3DVertexDeclaration* pDecl) PURE;
    STDMETHOD(GetVertexDeclaration)(THIS_ struct IWineD3DVertexDeclaration** ppDecl) PURE;
    STDMETHOD(SetVertexShader)(THIS_ struct IWineD3DVertexShader* pShader) PURE;
    STDMETHOD(GetVertexShader)(THIS_ struct IWineD3DVertexShader** ppShader) PURE;
    STDMETHOD(SetVertexShaderConstantB)(THIS_ UINT StartRegister, CONST BOOL*  pConstantData, UINT BoolCount) PURE;
    STDMETHOD(GetVertexShaderConstantB)(THIS_ UINT StartRegister, BOOL*        pConstantData, UINT BoolCount) PURE;
    STDMETHOD(SetVertexShaderConstantI)(THIS_ UINT StartRegister, CONST int*   pConstantData, UINT Vector4iCount) PURE;
    STDMETHOD(GetVertexShaderConstantI)(THIS_ UINT StartRegister, int*         pConstantData, UINT Vector4iCount) PURE;
    STDMETHOD(SetVertexShaderConstantF)(THIS_ UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) PURE;
    STDMETHOD(GetVertexShaderConstantF)(THIS_ UINT StartRegister, float*       pConstantData, UINT Vector4fCount) PURE;
    STDMETHOD(SetViewport)(THIS_ CONST WINED3DVIEWPORT * pViewport) PURE;
    STDMETHOD(GetViewport)(THIS_ WINED3DVIEWPORT * pViewport) PURE;
    STDMETHOD(MultiplyTransform)(THIS_ D3DTRANSFORMSTATETYPE  State, CONST D3DMATRIX * pMatrix) PURE;
    STDMETHOD(ValidateDevice)(THIS_ DWORD* pNumPasses) PURE;
    STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, struct IWineD3DVertexBuffer* pDestBuffer, struct IWineD3DVertexBuffer* pVertexDecl, DWORD Flags) PURE;
    STDMETHOD(BeginStateBlock)(THIS) PURE;
    STDMETHOD(EndStateBlock)(THIS_ struct IWineD3DStateBlock** ppStateBlock) PURE;
    STDMETHOD(BeginScene)(THIS) PURE;
    STDMETHOD(EndScene)(THIS) PURE;
    STDMETHOD(Present)(THIS_ CONST RECT * pSourceRect,CONST RECT * pDestRect,HWND  hDestWindowOverride,CONST RGNDATA * pDirtyRegion) PURE;
    STDMETHOD(Clear)(THIS_ DWORD  Count,CONST D3DRECT * pRects,DWORD  Flags,D3DCOLOR  Color,float  Z,DWORD  Stencil) PURE;
    STDMETHOD(DrawPrimitive)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,UINT  StartVertex,UINT  PrimitiveCount) PURE;
    STDMETHOD(DrawIndexedPrimitive)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,INT baseVIdx, UINT  minIndex,UINT  NumVertices,UINT  startIndex,UINT  primCount) PURE;
    STDMETHOD(DrawPrimitiveUP)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,UINT  PrimitiveCount,CONST void * pVertexStreamZeroData,UINT  VertexStreamZeroStride) PURE;
    STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,UINT  MinVertexIndex,UINT  NumVertexIndices,UINT  PrimitiveCount,CONST void * pIndexData,WINED3DFORMAT  IndexDataFormat,CONST void * pVertexStreamZeroData,UINT  VertexStreamZeroStride) PURE;
    STDMETHOD(DrawRectPatch)(THIS_ UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo) PURE;
    STDMETHOD(DrawTriPatch)(THIS_ UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) PURE;
    STDMETHOD(DeletePatch)(THIS_ UINT Handle) PURE;
    STDMETHOD(ColorFill)(THIS_ struct IWineD3DSurface* pSurface, CONST D3DRECT* pRect, D3DCOLOR color) PURE;
    STDMETHOD(UpdateTexture)(THIS_ struct IWineD3DBaseTexture *pSourceTexture, struct IWineD3DBaseTexture *pDestinationTexture) PURE;
    STDMETHOD(UpdateSurface)(THIS_ struct IWineD3DSurface* pSourceSurface, CONST RECT* pSourceRect, struct IWineD3DSurface* pDestinationSurface, CONST POINT* pDestPoint) PURE;
    STDMETHOD(StretchRect)(THIS_ struct IWineD3DSurface* pSourceSurface, CONST RECT* pSourceRect, struct IWineD3DSurface* pDestinationSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) PURE;
    STDMETHOD(GetRenderTargetData)(THIS_ struct IWineD3DSurface* pRenderTarget, struct IWineD3DSurface* pSurface) PURE;
    STDMETHOD(GetFrontBufferData)(THIS_ UINT iSwapChain,struct IWineD3DSurface* pSurface) PURE;
    /*** Internal use IWineD3Device methods ***/
    STDMETHOD_(void, SetupTextureStates)(THIS_ DWORD Stage, DWORD Flags);
    STDMETHOD_(void, SwapChainReleased)(THIS_ struct IWineD3DSwapChain *swapChain);
    /*** object tracking ***/
    STDMETHOD_(void, ResourceReleased)(THIS_ struct IWineD3DResource *resource);
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DDevice_QueryInterface(p,a,b)             (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DDevice_AddRef(p)                         (p)->lpVtbl->AddRef(p)
#define IWineD3DDevice_Release(p)                        (p)->lpVtbl->Release(p)
/*** IWineD3DDevice methods ***/
#define IWineD3DDevice_GetParent(p,a)                           (p)->lpVtbl->GetParent(p,a)
#define IWineD3DDevice_CreateVertexBuffer(p,a,b,c,d,e,f,g)      (p)->lpVtbl->CreateVertexBuffer(p,a,b,c,d,e,f,g)
#define IWineD3DDevice_CreateIndexBuffer(p,a,b,c,d,e,f,g)       (p)->lpVtbl->CreateIndexBuffer(p,a,b,c,d,e,f,g)
#define IWineD3DDevice_CreateStateBlock(p,a,b,c)                (p)->lpVtbl->CreateStateBlock(p,a,b,c)
#define IWineD3DDevice_CreateSurface(p,a,b,c,d,e,f,g,h,i,j,k,l,m,n)      (p)->lpVtbl->CreateSurface(p,a,b,c,d,e,f,g,h,i,j,k,l,m,n)
#define IWineD3DDevice_CreateTexture(p,a,b,c,d,e,f,g,h,i,j)     (p)->lpVtbl->CreateTexture(p,a,b,c,d,e,f,g,h,i,j)
#define IWineD3DDevice_CreateVolumeTexture(p,a,b,c,d,e,f,g,h,i,j,k)    (p)->lpVtbl->CreateVolumeTexture(p,a,b,c,d,e,f,g,h,i,j,k)
#define IWineD3DDevice_CreateVolume(p,a,b,c,d,e,f,g,h,i)        (p)->lpVtbl->CreateVolume(p,a,b,c,d,e,f,g,h,i)
#define IWineD3DDevice_CreateCubeTexture(p,a,b,c,d,e,f,g,h,i)   (p)->lpVtbl->CreateCubeTexture(p,a,b,c,d,e,f,g,h,i)
#define IWineD3DDevice_CreateQuery(p,a,b,c)                     (p)->lpVtbl->CreateQuery(p,a,b,c)
#define IWineD3DDevice_CreateAdditionalSwapChain(p,a,b,c,d,e)   (p)->lpVtbl->CreateAdditionalSwapChain(p,a,b,c,d,e)
#define IWineD3DDevice_CreateVertexDeclaration(p,b,c,d)         (p)->lpVtbl->CreateVertexDeclaration(p,b,c,d)
#define IWineD3DDevice_CreateVertexShader(p,a,b,c)              (p)->lpVtbl->CreateVertexShader(p,a,b,c)
#define IWineD3DDevice_CreatePixelShader(p,a,b,c)               (p)->lpVtbl->CreatePixelShader(p,a,b,c)
#define IWineD3DDevice_EvictManagedResources(p)                 (p)->lpVtbl->EvictManagedResources(p)
#define IWineD3DDevice_GetAvailableTextureMem(p)                (p)->lpVtbl->GetAvailableTextureMem(p)
#define IWineD3DDevice_GetBackBuffer(p,a,b,c,d)                 (p)->lpVtbl->GetBackBuffer(p,a,b,c,d)
#define IWineD3DDevice_GetCreationParameters(p,a)               (p)->lpVtbl->GetCreationParameters(p,a)
#define IWineD3DDevice_GetDeviceCaps(p,a)                       (p)->lpVtbl->GetDeviceCaps(p,a)
#define IWineD3DDevice_GetDirect3D(p,a)                         (p)->lpVtbl->GetDirect3D(p,a)
#define IWineD3DDevice_GetDisplayMode(p,a,b)                    (p)->lpVtbl->GetDisplayMode(p,a,b)
#define IWineD3DDevice_GetNumberOfSwapChains(p)                 (p)->lpVtbl->GetNumberOfSwapChains(p)
#define IWineD3DDevice_Reset(p,a)                               (p)->lpVtbl->Reset(p,a)
#define IWineD3DDevice_SetDialogBoxMode(p,a)                    (p)->lpVtbl->SetDialogBoxMode(p,a)
#define IWineD3DDevice_SetCursorProperties(p,a,b,c)             (p)->lpVtbl->SetCursorProperties(p,a,b,c)
#define IWineD3DDevice_SetCursorPosition(p,a,b,c)               (p)->lpVtbl->SetCursorPosition(p,a,b,c)
#define IWineD3DDevice_ShowCursor(p,a)                          (p)->lpVtbl->ShowCursor(p,a)
#define IWineD3DDevice_TestCooperativeLevel(p)                  (p)->lpVtbl->TestCooperativeLevel(p)
#define IWineD3DDevice_SetFVF(p,a)                              (p)->lpVtbl->SetFVF(p,a)
#define IWineD3DDevice_GetFVF(p,a)                              (p)->lpVtbl->GetFVF(p,a)
#define IWineD3DDevice_SetClipPlane(p,a,b)                      (p)->lpVtbl->SetClipPlane(p,a,b)
#define IWineD3DDevice_GetClipPlane(p,a,b)                      (p)->lpVtbl->GetClipPlane(p,a,b)
#define IWineD3DDevice_SetClipStatus(p,a)                       (p)->lpVtbl->SetClipStatus(p,a)
#define IWineD3DDevice_GetClipStatus(p,a)                       (p)->lpVtbl->GetClipStatus(p,a)
#define IWineD3DDevice_SetCurrentTexturePalette(p,a)            (p)->lpVtbl->SetCurrentTexturePalette(p,a)
#define IWineD3DDevice_GetCurrentTexturePalette(p,a)            (p)->lpVtbl->GetCurrentTexturePalette(p,a)
#define IWineD3DDevice_SetDepthStencilSurface(p,a)              (p)->lpVtbl->SetDepthStencilSurface(p,a)
#define IWineD3DDevice_GetDepthStencilSurface(p,a)              (p)->lpVtbl->GetDepthStencilSurface(p,a)
#define IWineD3DDevice_SetGammaRamp(p,a,b,c)                    (p)->lpVtbl->SetGammaRamp(p,a,b,c)
#define IWineD3DDevice_GetGammaRamp(p,a,b)                      (p)->lpVtbl->GetGammaRamp(p,a,b)
#define IWineD3DDevice_SetIndices(p,a,b)                        (p)->lpVtbl->SetIndices(p,a,b)
#define IWineD3DDevice_GetIndices(p,a,b)                        (p)->lpVtbl->GetIndices(p,a,b)
#define IWineD3DDevice_SetLight(p,a,b)                          (p)->lpVtbl->SetLight(p,a,b)
#define IWineD3DDevice_GetLight(p,a,b)                          (p)->lpVtbl->GetLight(p,a,b)
#define IWineD3DDevice_SetLightEnable(p,a,b)                    (p)->lpVtbl->SetLightEnable(p,a,b)
#define IWineD3DDevice_GetLightEnable(p,a,b)                    (p)->lpVtbl->GetLightEnable(p,a,b)
#define IWineD3DDevice_SetMaterial(p,a)                         (p)->lpVtbl->SetMaterial(p,a)
#define IWineD3DDevice_GetMaterial(p,a)                         (p)->lpVtbl->GetMaterial(p,a)
#define IWineD3DDevice_SetNPatchMode(p,a)                       (p)->lpVtbl->SetNPatchMode(p,a)
#define IWineD3DDevice_GetNPatchMode(p)                         (p)->lpVtbl->GetNPatchMode(p)
#define IWineD3DDevice_SetPaletteEntries(p,a,b)                 (p)->lpVtbl->SetPaletteEntries(p,a,b)
#define IWineD3DDevice_GetPaletteEntries(p,a,b)                 (p)->lpVtbl->GetPaletteEntries(p,a,b)
#define IWineD3DDevice_SetPixelShader(p,a)                      (p)->lpVtbl->SetPixelShader(p,a)
#define IWineD3DDevice_GetPixelShader(p,a)                      (p)->lpVtbl->GetPixelShader(p,a)
#define IWineD3DDevice_SetPixelShaderConstantB(p,a,b,c)         (p)->lpVtbl->SetPixelShaderConstantB(p,a,b,c)
#define IWineD3DDevice_GetPixelShaderConstantB(p,a,b,c)         (p)->lpVtbl->GetPixelShaderConstantB(p,a,b,c)
#define IWineD3DDevice_SetPixelShaderConstantI(p,a,b,c)         (p)->lpVtbl->SetPixelShaderConstantI(p,a,b,c)
#define IWineD3DDevice_GetPixelShaderConstantI(p,a,b,c)         (p)->lpVtbl->GetPixelShaderConstantI(p,a,b,c)
#define IWineD3DDevice_SetPixelShaderConstantF(p,a,b,c)         (p)->lpVtbl->SetPixelShaderConstantF(p,a,b,c)
#define IWineD3DDevice_GetPixelShaderConstantF(p,a,b,c)         (p)->lpVtbl->GetPixelShaderConstantF(p,a,b,c)
#define IWineD3DDevice_GetRasterStatus(p,a,b)                   (p)->lpVtbl->GetRasterStatus(p,a,b)
#define IWineD3DDevice_SetRenderState(p,a,b)                    (p)->lpVtbl->SetRenderState(p,a,b)
#define IWineD3DDevice_GetRenderState(p,a,b)                    (p)->lpVtbl->GetRenderState(p,a,b)
#define IWineD3DDevice_SetRenderTarget(p,a,b)                   (p)->lpVtbl->SetRenderTarget(p,a,b)
#define IWineD3DDevice_GetRenderTarget(p,a,b)                   (p)->lpVtbl->GetRenderTarget(p,a,b)
#define IWineD3DDevice_SetSamplerState(p,a,b,c)                 (p)->lpVtbl->SetSamplerState(p,a,b,c)
#define IWineD3DDevice_GetSamplerState(p,a,b,c)                 (p)->lpVtbl->GetSamplerState(p,a,b,c)
#define IWineD3DDevice_SetScissorRect(p,a)                      (p)->lpVtbl->SetScissorRect(p,a)
#define IWineD3DDevice_GetScissorRect(p,a)                      (p)->lpVtbl->GetScissorRect(p,a)
#define IWineD3DDevice_SetSoftwareVertexProcessing(p,a)         (p)->lpVtbl->SetSoftwareVertexProcessing(p,a)
#define IWineD3DDevice_GetSoftwareVertexProcessing(p)           (p)->lpVtbl->GetSoftwareVertexProcessing(p)
#define IWineD3DDevice_SetStreamSource(p,a,b,c,d)               (p)->lpVtbl->SetStreamSource(p,a,b,c,d)
#define IWineD3DDevice_GetStreamSource(p,a,b,c,d)               (p)->lpVtbl->GetStreamSource(p,a,b,c,d)
#define IWineD3DDevice_SetStreamSourceFreq(p,a,b)               (p)->lpVtbl->SetStreamSourceFreq(p,a,b)
#define IWineD3DDevice_GetStreamSourceFreq(p,a,b)               (p)->lpVtbl->GetStreamSourceFreq(p,a,b)
#define IWineD3DDevice_GetSwapChain(p,a,b)                      (p)->lpVtbl->GetSwapChain(p,a,b)
#define IWineD3DDevice_SetTextureStageState(p,a,b,c)            (p)->lpVtbl->SetTextureStageState(p,a,b,c)
#define IWineD3DDevice_GetTextureStageState(p,a,b,c)            (p)->lpVtbl->GetTextureStageState(p,a,b,c)
#define IWineD3DDevice_SetTexture(p,a,b)                        (p)->lpVtbl->SetTexture(p,a,b)
#define IWineD3DDevice_GetTexture(p,a,b)                        (p)->lpVtbl->GetTexture(p,a,b)
#define IWineD3DDevice_SetTransform(p,a,b)                      (p)->lpVtbl->SetTransform(p,a,b)
#define IWineD3DDevice_GetTransform(p,a,b)                      (p)->lpVtbl->GetTransform(p,a,b)
#define IWineD3DDevice_SetVertexDeclaration(p,a)                (p)->lpVtbl->SetVertexDeclaration(p,a)
#define IWineD3DDevice_GetVertexDeclaration(p,a)                (p)->lpVtbl->GetVertexDeclaration(p,a)
#define IWineD3DDevice_SetVertexShader(p,a)                     (p)->lpVtbl->SetVertexShader(p,a)
#define IWineD3DDevice_GetVertexShader(p,a)                     (p)->lpVtbl->GetVertexShader(p,a)
#define IWineD3DDevice_SetVertexShaderConstantB(p,a,b,c)        (p)->lpVtbl->SetVertexShaderConstantB(p,a,b,c)
#define IWineD3DDevice_GetVertexShaderConstantB(p,a,b,c)        (p)->lpVtbl->GetVertexShaderConstantB(p,a,b,c)
#define IWineD3DDevice_SetVertexShaderConstantI(p,a,b,c)        (p)->lpVtbl->SetVertexShaderConstantI(p,a,b,c)
#define IWineD3DDevice_GetVertexShaderConstantI(p,a,b,c)        (p)->lpVtbl->GetVertexShaderConstantI(p,a,b,c)
#define IWineD3DDevice_SetVertexShaderConstantF(p,a,b,c)        (p)->lpVtbl->SetVertexShaderConstantF(p,a,b,c)
#define IWineD3DDevice_GetVertexShaderConstantF(p,a,b,c)        (p)->lpVtbl->GetVertexShaderConstantF(p,a,b,c)
#define IWineD3DDevice_SetViewport(p,a)                         (p)->lpVtbl->SetViewport(p,a)
#define IWineD3DDevice_GetViewport(p,a)                         (p)->lpVtbl->GetViewport(p,a)
#define IWineD3DDevice_MultiplyTransform(p,a,b)                 (p)->lpVtbl->MultiplyTransform(p,a,b)
#define IWineD3DDevice_ValidateDevice(p,a)                      (p)->lpVtbl->ValidateDevice(p,a)
#define IWineD3DDevice_ProcessVertices(p,a,b,c,d,e,f)           (p)->lpVtbl->ProcessVertices(p,a,b,c,d,e,f)
#define IWineD3DDevice_BeginStateBlock(p)                       (p)->lpVtbl->BeginStateBlock(p)
#define IWineD3DDevice_EndStateBlock(p,a)                       (p)->lpVtbl->EndStateBlock(p,a)
#define IWineD3DDevice_BeginScene(p)                            (p)->lpVtbl->BeginScene(p)
#define IWineD3DDevice_EndScene(p)                              (p)->lpVtbl->EndScene(p)
#define IWineD3DDevice_Present(p,a,b,c,d)                       (p)->lpVtbl->Present(p,a,b,c,d)
#define IWineD3DDevice_Clear(p,a,b,c,d,e,f)                     (p)->lpVtbl->Clear(p,a,b,c,d,e,f)
#define IWineD3DDevice_DrawPrimitive(p,a,b,c)                   (p)->lpVtbl->DrawPrimitive(p,a,b,c)
#define IWineD3DDevice_DrawIndexedPrimitive(p,a,b,c,d,e,f)      (p)->lpVtbl->DrawIndexedPrimitive(p,a,b,c,d,e,f)
#define IWineD3DDevice_DrawPrimitiveUP(p,a,b,c,d)               (p)->lpVtbl->DrawPrimitiveUP(p,a,b,c,d)
#define IWineD3DDevice_DrawIndexedPrimitiveUP(p,a,b,c,d,e,f,g,h) (p)->lpVtbl->DrawIndexedPrimitiveUP(p,a,b,c,d,e,f,g,h)
#define IWineD3DDevice_DrawRectPatch(p,a,b,c)                   (p)->lpVtbl->DrawRectPatch(p,a,b,c)
#define IWineD3DDevice_DrawTriPatch(p,a,b,c)                    (p)->lpVtbl->DrawTriPatch(p,a,b,c)
#define IWineD3DDevice_DeletePatch(p,a)                         (p)->lpVtbl->DeletePatch(p,a)
#define IWineD3DDevice_ColorFill(p,a,b,c)                       (p)->lpVtbl->ColorFill(p,a,b,c)
#define IWineD3DDevice_UpdateTexture(p,a,b)                     (p)->lpVtbl->UpdateTexture(p,a,b)
#define IWineD3DDevice_UpdateSurface(p,a,b,c,d)                 (p)->lpVtbl->UpdateSurface(p,a,b,c,d)
#define IWineD3DDevice_StretchRect(p,a,b,c,d,e)                 (p)->lpVtbl->StretchRect(p,a,b,c,d,e)
#define IWineD3DDevice_GetRenderTargetData(p,a,b)               (p)->lpVtbl->GetRenderTargetData(p,a,b)
#define IWineD3DDevice_GetFrontBufferData(p,a,b)                (p)->lpVtbl->GetFrontBufferData(p,a,b)
#define IWineD3DDevice_SetupTextureStates(p,a,b)                (p)->lpVtbl->SetupTextureStates(p,a,b)
#define IWineD3DDevice_SwapChainReleased(p,a)                   (p)->lpVtbl->SwapChainReleased(p,a)
#define IWineD3DDevice_ResourceReleased(p,a)                    (p)->lpVtbl->ResourceReleased(p,a)
#endif

/*****************************************************************************
 * WineD3DResource interface 
 */
#define INTERFACE IWineD3DResource
DECLARE_INTERFACE_(IWineD3DResource,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DResource_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DResource_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DResource_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DResource_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DResource_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DResource_SetPrivateData(p,a,b,c,d)    (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DResource_GetPrivateData(p,a,b,c)      (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DResource_FreePrivateData(p,a)         (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DResource_SetPriority(p,a)             (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DResource_GetPriority(p)               (p)->lpVtbl->GetPriority(p)
#define IWineD3DResource_PreLoad(p)                   (p)->lpVtbl->PreLoad(p)
#define IWineD3DResource_GetType(p)                   (p)->lpVtbl->GetType(p)
#endif

/*****************************************************************************
 * WineD3DVertexBuffer interface 
 */
#define INTERFACE IWineD3DVertexBuffer
DECLARE_INTERFACE_(IWineD3DVertexBuffer,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DVertexBuffer methods ***/
    STDMETHOD(Lock)(THIS_ UINT  OffsetToLock, UINT  SizeToLock, BYTE ** ppbData, DWORD  Flags) PURE;
    STDMETHOD(Unlock)(THIS) PURE;
    STDMETHOD(GetDesc)(THIS_ D3DVERTEXBUFFER_DESC  * pDesc) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DVertexBuffer_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DVertexBuffer_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DVertexBuffer_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DVertexBuffer_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DVertexBuffer_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DVertexBuffer_SetPrivateData(p,a,b,c,d)    (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DVertexBuffer_GetPrivateData(p,a,b,c)      (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DVertexBuffer_FreePrivateData(p,a)         (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DVertexBuffer_SetPriority(p,a)             (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DVertexBuffer_GetPriority(p)               (p)->lpVtbl->GetPriority(p)
#define IWineD3DVertexBuffer_PreLoad(p)                   (p)->lpVtbl->PreLoad(p)
#define IWineD3DVertexBuffer_GetType(p)                   (p)->lpVtbl->GetType(p)
/*** IWineD3DVertexBuffer methods ***/
#define IWineD3DVertexBuffer_Lock(p,a,b,c,d)              (p)->lpVtbl->Lock(p,a,b,c,d)
#define IWineD3DVertexBuffer_Unlock(p)                    (p)->lpVtbl->Unlock(p)
#define IWineD3DVertexBuffer_GetDesc(p,a)                 (p)->lpVtbl->GetDesc(p,a)
#endif

/*****************************************************************************
 * WineD3DIndexBuffer interface 
 */
#define INTERFACE IWineD3DIndexBuffer
DECLARE_INTERFACE_(IWineD3DIndexBuffer,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DIndexBuffer methods ***/
    STDMETHOD(Lock)(THIS_ UINT  OffsetToLock, UINT  SizeToLock, BYTE ** ppbData, DWORD  Flags) PURE;
    STDMETHOD(Unlock)(THIS) PURE;
    STDMETHOD(GetDesc)(THIS_ D3DINDEXBUFFER_DESC  * pDesc) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DIndexBuffer_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DIndexBuffer_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DIndexBuffer_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DIndexBuffer_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DIndexBuffer_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DIndexBuffer_SetPrivateData(p,a,b,c,d)    (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DIndexBuffer_GetPrivateData(p,a,b,c)      (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DIndexBuffer_FreePrivateData(p,a)         (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DIndexBuffer_SetPriority(p,a)             (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DIndexBuffer_GetPriority(p)               (p)->lpVtbl->GetPriority(p)
#define IWineD3DIndexBuffer_PreLoad(p)                   (p)->lpVtbl->PreLoad(p)
#define IWineD3DIndexBuffer_GetType(p)                   (p)->lpVtbl->GetType(p)
/*** IWineD3DIndexBuffer methods ***/
#define IWineD3DIndexBuffer_Lock(p,a,b,c,d)              (p)->lpVtbl->Lock(p,a,b,c,d)
#define IWineD3DIndexBuffer_Unlock(p)                    (p)->lpVtbl->Unlock(p)
#define IWineD3DIndexBuffer_GetDesc(p,a)                 (p)->lpVtbl->GetDesc(p,a)
#endif

/*****************************************************************************
 * IWineD3DBaseTexture interface
 *   Note at d3d8 this does NOT extend Resource, but at d3d9 it does
 *     since most functions are common anyway, it makes sense to extend it
 */
#define INTERFACE IWineD3DBaseTexture
DECLARE_INTERFACE_(IWineD3DBaseTexture,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DBaseTexture methods ***/
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew) PURE;
    STDMETHOD_(DWORD, GetLOD)(THIS) PURE;
    STDMETHOD_(DWORD, GetLevelCount)(THIS) PURE;
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType) PURE;
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS) PURE;
    STDMETHOD_(void, GenerateMipSubLevels)(THIS) PURE;
    STDMETHOD_(BOOL, SetDirty)(THIS_ BOOL) PURE;
    STDMETHOD_(BOOL, GetDirty)(THIS) PURE;
    STDMETHOD(BindTexture)(THIS) PURE;
    STDMETHOD(UnBindTexture)(THIS) PURE;
    STDMETHOD_(UINT, GetTextureDimensions)(THIS) PURE;
    STDMETHOD_(void, ApplyStateChanges)(THIS_ const DWORD textureStates[WINED3D_HIGHEST_TEXTURE_STATE + 1], const DWORD samplerStates[WINED3D_HIGHEST_SAMPLER_STATE + 1]) PURE;

};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DBaseTexture_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DBaseTexture_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DBaseTexture_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DBaseTexture methods: IWineD3DResource ***/
#define IWineD3DBaseTexture_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DBaseTexture_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DBaseTexture_SetPrivateData(p,a,b,c,d)  (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DBaseTexture_GetPrivateData(p,a,b,c)    (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DBaseTexture_FreePrivateData(p,a)       (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DBaseTexture_SetPriority(p,a)           (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DBaseTexture_GetPriority(p)             (p)->lpVtbl->GetPriority(p)
#define IWineD3DBaseTexture_PreLoad(p)                 (p)->lpVtbl->PreLoad(p)
#define IWineD3DBaseTexture_GetType(p)                 (p)->lpVtbl->GetType(p)
/*** IWineD3DBaseTexture methods ***/
#define IWineD3DBaseTexture_SetLOD(p,a)                (p)->lpVtbl->SetLOD(p,a)
#define IWineD3DBaseTexture_GetLOD(p)                  (p)->lpVtbl->GetLOD(p)
#define IWineD3DBaseTexture_GetLevelCount(p)           (p)->lpVtbl->GetLevelCount(p)
#define IWineD3DBaseTexture_SetAutoGenFilterType(p,a)  (p)->lpVtbl->SetAutoGenFilterType(p,a)
#define IWineD3DBaseTexture_GetAutoGenFilterType(p)    (p)->lpVtbl->GetAutoGenFilterType(p)
#define IWineD3DBaseTexture_GenerateMipSubLevels(p)    (p)->lpVtbl->GenerateMipSubLevels(p)
#define IWineD3DBaseTexture_SetDirty(p,a)              (p)->lpVtbl->SetDirty(p,a)
#define IWineD3DBaseTexture_GetDirty(p)                (p)->lpVtbl->GetDirty(p)
/*** internal methods ***/
#define IWineD3DBaseTexture_BindTexture(p)             (p)->lpVtbl->BindTexture(p)
#define IWineD3DBaseTexture_UnBindTexture(p)           (p)->lpVtbl->UnBindTexture(p)
#define IWineD3DBaseTexture_GetTextureDimensions(p)    (p)->lpVtbl->GetTextureDimensions(p)
#define IWineD3DBaseTexture_ApplyStateChanges(p,a,b)   (p)->lpVtbl->ApplyStateChanges(p,a,b)
#endif

/*****************************************************************************
 * IWineD3DTexture interface
 */
#define INTERFACE IWineD3DTexture
DECLARE_INTERFACE_(IWineD3DTexture,IWineD3DBaseTexture)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DBaseTexture methods ***/
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew) PURE;
    STDMETHOD_(DWORD, GetLOD)(THIS) PURE;
    STDMETHOD_(DWORD, GetLevelCount)(THIS) PURE;
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType) PURE;
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS) PURE;
    STDMETHOD_(void, GenerateMipSubLevels)(THIS) PURE;
    STDMETHOD_(BOOL, SetDirty)(THIS_ BOOL) PURE;
    STDMETHOD_(BOOL, GetDirty)(THIS) PURE;
    STDMETHOD(BindTexture)(THIS) PURE;
    STDMETHOD(UnBindTexture)(THIS) PURE;
    STDMETHOD_(UINT, GetTextureDimensions)(THIS) PURE;
    STDMETHOD_(void, ApplyStateChanges)(THIS_ const DWORD textureStates[WINED3D_HIGHEST_TEXTURE_STATE + 1], const DWORD samplerStates[WINED3D_HIGHEST_SAMPLER_STATE + 1]) PURE;
    /*** IWineD3DTexture methods ***/
    STDMETHOD(GetLevelDesc)(THIS_ UINT Level, WINED3DSURFACE_DESC* pDesc) PURE;
    STDMETHOD(GetSurfaceLevel)(THIS_ UINT Level, struct IWineD3DSurface** ppSurfaceLevel) PURE;
    STDMETHOD(LockRect)(THIS_ UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) PURE;
    STDMETHOD(UnlockRect)(THIS_ UINT Level) PURE;
    STDMETHOD(AddDirtyRect)(THIS_ CONST RECT* pDirtyRect) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DTexture_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DTexture_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DTexture_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DTexture methods: IWineD3DResource ***/
#define IWineD3DTexture_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DTexture_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DTexture_SetPrivateData(p,a,b,c,d)  (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DTexture_GetPrivateData(p,a,b,c)    (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DTexture_FreePrivateData(p,a)       (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DTexture_SetPriority(p,a)           (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DTexture_GetPriority(p)             (p)->lpVtbl->GetPriority(p)
#define IWineD3DTexture_PreLoad(p)                 (p)->lpVtbl->PreLoad(p)
#define IWineD3DTexture_GetType(p)                 (p)->lpVtbl->GetType(p)
/*** IWineD3DTexture methods: IWineD3DBaseTexture ***/
#define IWineD3DTexture_SetLOD(p,a)                (p)->lpVtbl->SetLOD(p,a)
#define IWineD3DTexture_GetLOD(p)                  (p)->lpVtbl->GetLOD(p)
#define IWineD3DTexture_GetLevelCount(p)           (p)->lpVtbl->GetLevelCount(p)
#define IWineD3DTexture_SetAutoGenFilterType(p,a)  (p)->lpVtbl->SetAutoGenFilterType(p,a)
#define IWineD3DTexture_GetAutoGenFilterType(p)    (p)->lpVtbl->GetAutoGenFilterType(p)
#define IWineD3DTexture_GenerateMipSubLevels(p)    (p)->lpVtbl->GenerateMipSubLevels(p)
#define IWineD3DTexture_SetDirty(p,a)              (p)->lpVtbl->SetDirty(p,a)
#define IWineD3DTexture_GetDirty(p)                (p)->lpVtbl->GetDirty(p)
#define IWineD3DTexture_BindTexture(p)             (p)->lpVtbl->BindTexture(p)
#define IWineD3DTexture_UnBindTexture(p)           (p)->lpVtbl->UnBindTexture(p)
#define IWineD3DTexture_GetTextureDimensions(p)    (p)->lpVtbl->GetTextureDimensions(p)
#define IWineD3DTexture_ApplyStateChanges(p,a,b)   (p)->lpVtbl->ApplyStateChanges(p,a,b)
/*** IWineD3DTexture methods ***/
#define IWineD3DTexture_GetLevelDesc(p,a,b)        (p)->lpVtbl->GetLevelDesc(p,a,b)
#define IWineD3DTexture_GetSurfaceLevel(p,a,b)     (p)->lpVtbl->GetSurfaceLevel(p,a,b)
#define IWineD3DTexture_LockRect(p,a,b,c,d)        (p)->lpVtbl->LockRect(p,a,b,c,d)
#define IWineD3DTexture_UnlockRect(p,a)            (p)->lpVtbl->UnlockRect(p,a)
#define IWineD3DTexture_AddDirtyRect(p,a)          (p)->lpVtbl->AddDirtyRect(p,a)
#endif

/*****************************************************************************
 * IWineD3DCubeTexture interface
 */
#define INTERFACE IWineD3DCubeTexture
DECLARE_INTERFACE_(IWineD3DCubeTexture,IWineD3DBaseTexture)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DBaseTexture methods ***/
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew) PURE;
    STDMETHOD_(DWORD, GetLOD)(THIS) PURE;
    STDMETHOD_(DWORD, GetLevelCount)(THIS) PURE;
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType) PURE;
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS) PURE;
    STDMETHOD_(void, GenerateMipSubLevels)(THIS) PURE;
    STDMETHOD_(BOOL, SetDirty)(THIS_ BOOL) PURE;
    STDMETHOD_(BOOL, GetDirty)(THIS) PURE;
    STDMETHOD(BindTexture)(THIS) PURE;
    STDMETHOD(UnBindTexture)(THIS) PURE;
    STDMETHOD_(UINT, GetTextureDimensions)(THIS) PURE;
    STDMETHOD_(void, ApplyStateChanges)(THIS_ DWORD const textureStates[WINED3D_HIGHEST_TEXTURE_STATE + 1], const DWORD samplerStates[WINED3D_HIGHEST_SAMPLER_STATE + 1]) PURE;
    /*** IWineD3DCubeTexture methods ***/
    STDMETHOD(GetLevelDesc)(THIS_ UINT Level,WINED3DSURFACE_DESC* pDesc) PURE;
    STDMETHOD(GetCubeMapSurface)(THIS_ D3DCUBEMAP_FACES FaceType, UINT Level, struct IWineD3DSurface** ppCubeMapSurface) PURE;
    STDMETHOD(LockRect)(THIS_ D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) PURE;
    STDMETHOD(UnlockRect)(THIS_ D3DCUBEMAP_FACES FaceType, UINT Level) PURE;
    STDMETHOD(AddDirtyRect)(THIS_ D3DCUBEMAP_FACES FaceType, CONST RECT* pDirtyRect) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DCubeTexture_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DCubeTexture_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DCubeTexture_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DCubeTexture methods: IWineD3DResource ***/
#define IWineD3DCubeTexture_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DCubeTexture_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DCubeTexture_SetPrivateData(p,a,b,c,d)  (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DCubeTexture_GetPrivateData(p,a,b,c)    (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DCubeTexture_FreePrivateData(p,a)       (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DCubeTexture_SetPriority(p,a)           (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DCubeTexture_GetPriority(p)             (p)->lpVtbl->GetPriority(p)
#define IWineD3DCubeTexture_PreLoad(p)                 (p)->lpVtbl->PreLoad(p)
#define IWineD3DCubeTexture_GetType(p)                 (p)->lpVtbl->GetType(p)
/*** IWineD3DCubeTexture methods: IWineD3DBaseTexture ***/
#define IWineD3DCubeTexture_SetLOD(p,a)                (p)->lpVtbl->SetLOD(p,a)
#define IWineD3DCubeTexture_GetLOD(p)                  (p)->lpVtbl->GetLOD(p)
#define IWineD3DCubeTexture_GetLevelCount(p)           (p)->lpVtbl->GetLevelCount(p)
#define IWineD3DCubeTexture_SetAutoGenFilterType(p,a)  (p)->lpVtbl->SetAutoGenFilterType(p,a)
#define IWineD3DCubeTexture_GetAutoGenFilterType(p)    (p)->lpVtbl->GetAutoGenFilterType(p)
#define IWineD3DCubeTexture_GenerateMipSubLevels(p)    (p)->lpVtbl->GenerateMipSubLevels(p)
#define IWineD3DCubeTexture_SetDirty(p,a)              (p)->lpVtbl->SetDirty(p,a)
#define IWineD3DCubeTexture_GetDirty(p)                (p)->lpVtbl->GetDirty(p)
#define IWineD3DCubeTexture_BindTexture(p)              (p)->lpVtbl->BindTexture(p)
#define IWineD3DCubeTexture_UnBindTexture(p)            (p)->lpVtbl->UnBindTexture(p)
#define IWineD3DCubeTexture_GetTextureDimensions(p)     (p)->lpVtbl->GetTextureDimensions(p)
#define IWineD3DCubeTexture_ApplyStateChanges(p,a,b)   (p)->lpVtbl->ApplyStateChanges(p,a,b)
/*** IWineD3DCubeTexture methods ***/
#define IWineD3DCubeTexture_GetLevelDesc(p,a,b)        (p)->lpVtbl->GetLevelDesc(p,a,b)
#define IWineD3DCubeTexture_GetCubeMapSurface(p,a,b,c) (p)->lpVtbl->GetCubeMapSurface(p,a,b,c)
#define IWineD3DCubeTexture_LockRect(p,a,b,c,d,e)      (p)->lpVtbl->LockRect(p,a,b,c,d,e)
#define IWineD3DCubeTexture_UnlockRect(p,a,b)          (p)->lpVtbl->UnlockRect(p,a,b)
#define IWineD3DCubeTexture_AddDirtyRect(p,a,b)        (p)->lpVtbl->AddDirtyRect(p,a,b)
#endif


/*****************************************************************************
 * IWineD3DVolumeTexture interface
 */
#define INTERFACE IWineD3DVolumeTexture
DECLARE_INTERFACE_(IWineD3DVolumeTexture,IWineD3DBaseTexture)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DBaseTexture methods ***/
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew) PURE;
    STDMETHOD_(DWORD, GetLOD)(THIS) PURE;
    STDMETHOD_(DWORD, GetLevelCount)(THIS) PURE;
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType) PURE;
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS) PURE;
    STDMETHOD_(void, GenerateMipSubLevels)(THIS) PURE;
    STDMETHOD_(BOOL, SetDirty)(THIS_ BOOL) PURE;
    STDMETHOD_(BOOL, GetDirty)(THIS) PURE;
    STDMETHOD(BindTexture)(THIS) PURE;
    STDMETHOD(UnBindTexture)(THIS) PURE;
    STDMETHOD_(UINT, GetTextureDimensions)(THIS) PURE;
    STDMETHOD_(void, ApplyStateChanges)(THIS_ const DWORD textureStates[WINED3D_HIGHEST_TEXTURE_STATE + 1], const DWORD samplerStates[WINED3D_HIGHEST_SAMPLER_STATE + 1]) PURE;
    /*** IWineD3DVolumeTexture methods ***/
    STDMETHOD(GetLevelDesc)(THIS_ UINT Level, WINED3DVOLUME_DESC *pDesc) PURE;
    STDMETHOD(GetVolumeLevel)(THIS_ UINT Level, struct IWineD3DVolume** ppVolumeLevel) PURE;
    STDMETHOD(LockBox)(THIS_ UINT Level, D3DLOCKED_BOX* pLockedVolume, CONST D3DBOX* pBox, DWORD Flags) PURE;
    STDMETHOD(UnlockBox)(THIS_ UINT Level) PURE;
    STDMETHOD(AddDirtyBox)(THIS_ CONST D3DBOX* pDirtyBox) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DVolumeTexture_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DVolumeTexture_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DVolumeTexture_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DVolumeTexture methods: IWineD3DResource ***/
#define IWineD3DVolumeTexture_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DVolumeTexture_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DVolumeTexture_SetPrivateData(p,a,b,c,d)  (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DVolumeTexture_GetPrivateData(p,a,b,c)    (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DVolumeTexture_FreePrivateData(p,a)       (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DVolumeTexture_SetPriority(p,a)           (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DVolumeTexture_GetPriority(p)             (p)->lpVtbl->GetPriority(p)
#define IWineD3DVolumeTexture_PreLoad(p)                 (p)->lpVtbl->PreLoad(p)
#define IWineD3DVolumeTexture_GetType(p)                 (p)->lpVtbl->GetType(p)
/*** IWineD3DVolumeTexture methods: IWineD3DBaseTexture ***/
#define IWineD3DVolumeTexture_SetLOD(p,a)                (p)->lpVtbl->SetLOD(p,a)
#define IWineD3DVolumeTexture_GetLOD(p)                  (p)->lpVtbl->GetLOD(p)
#define IWineD3DVolumeTexture_GetLevelCount(p)           (p)->lpVtbl->GetLevelCount(p)
#define IWineD3DVolumeTexture_SetAutoGenFilterType(p,a)  (p)->lpVtbl->SetAutoGenFilterType(p,a)
#define IWineD3DVolumeTexture_GetAutoGenFilterType(p)    (p)->lpVtbl->GetAutoGenFilterType(p)
#define IWineD3DVolumeTexture_GenerateMipSubLevels(p)    (p)->lpVtbl->GenerateMipSubLevels(p)
#define IWineD3DVolumeTexture_SetDirty(p,a)              (p)->lpVtbl->SetDirty(p,a)
#define IWineD3DVolumeTexture_GetDirty(p)                (p)->lpVtbl->GetDirty(p)
#define IWineD3DVolumeTexture_BindTexture(p)              (p)->lpVtbl->BindTexture(p)
#define IWineD3DVolumeTexture_UnBindTexture(p)            (p)->lpVtbl->UnBindTexture(p)
#define IWineD3DVolumeTexture_GetTextureDimensions(p)     (p)->lpVtbl->GetTextureDimensions(p)
#define IWineD3DVolumeTexture_ApplyStateChanges(p,a,b)   (p)->lpVtbl->ApplyStateChanges(p,a,b)
/*** IWineD3DVolumeTexture methods ***/
#define IWineD3DVolumeTexture_GetLevelDesc(p,a,b)        (p)->lpVtbl->GetLevelDesc(p,a,b)
#define IWineD3DVolumeTexture_GetVolumeLevel(p,a,b)      (p)->lpVtbl->GetVolumeLevel(p,a,b)
#define IWineD3DVolumeTexture_LockBox(p,a,b,c,d)         (p)->lpVtbl->LockBox(p,a,b,c,d)
#define IWineD3DVolumeTexture_UnlockBox(p,a)             (p)->lpVtbl->UnlockBox(p,a)
#define IWineD3DVolumeTexture_AddDirtyBox(p,a)           (p)->lpVtbl->AddDirtyBox(p,a)
#endif

/*****************************************************************************
 * IWineD3DSurface interface
 */
#define INTERFACE IWineD3DSurface
DECLARE_INTERFACE_(IWineD3DSurface,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DSurface methods ***/
    STDMETHOD(GetContainer)(THIS_ REFIID  riid, void ** ppContainer) PURE;
    STDMETHOD(GetDesc)(THIS_ WINED3DSURFACE_DESC * pDesc) PURE;
    STDMETHOD(LockRect)(THIS_ D3DLOCKED_RECT * pLockedRect, CONST RECT * pRect,DWORD  Flags) PURE;
    STDMETHOD(UnlockRect)(THIS) PURE;
    STDMETHOD(GetDC)(THIS_ HDC *pHdc) PURE;
    STDMETHOD(ReleaseDC)(THIS_ HDC hdc) PURE;
    /* Internally used methods */
    STDMETHOD(CleanDirtyRect)(THIS) PURE;
    STDMETHOD(AddDirtyRect)(THIS_ CONST RECT* pRect) PURE;
    STDMETHOD(LoadTexture)(THIS) PURE;
    STDMETHOD(SaveSnapshot)(THIS_ const char *filename) PURE;
    STDMETHOD(SetContainer)(THIS_ IUnknown *container) PURE;
    STDMETHOD(SetPBufferState)(THIS_ BOOL inPBuffer, BOOL  inTexture) PURE;
    STDMETHOD_(void,SetGlTextureDesc)(THIS_ UINT textureName, int target) PURE;
    STDMETHOD_(void,GetGlDesc)(THIS_ glDescriptor **glDescription) PURE;
    STDMETHOD_(CONST void *, GetData)(THIS) PURE;
    STDMETHOD(SetFormat)(THIS_ WINED3DFORMAT format) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DSurface_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DSurface_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DSurface_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DSurface_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DSurface_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DSurface_SetPrivateData(p,a,b,c,d)    (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DSurface_GetPrivateData(p,a,b,c)      (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DSurface_FreePrivateData(p,a)         (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DSurface_SetPriority(p,a)             (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DSurface_GetPriority(p)               (p)->lpVtbl->GetPriority(p)
#define IWineD3DSurface_PreLoad(p)                   (p)->lpVtbl->PreLoad(p)
#define IWineD3DSurface_GetType(p)                   (p)->lpVtbl->GetType(p)
/*** IWineD3DSurface methods ***/
#define IWineD3DSurface_GetContainer(p,a,b)          (p)->lpVtbl->GetContainer(p,a,b)
#define IWineD3DSurface_GetDesc(p,a)                 (p)->lpVtbl->GetDesc(p,a)
#define IWineD3DSurface_LockRect(p,a,b,c)            (p)->lpVtbl->LockRect(p,a,b,c)
#define IWineD3DSurface_UnlockRect(p)                (p)->lpVtbl->UnlockRect(p)
#define IWineD3DSurface_GetDC(p,a)                   (p)->lpVtbl->GetDC(p,a)
#define IWineD3DSurface_ReleaseDC(p,a)               (p)->lpVtbl->ReleaseDC(p,a)
/*** IWineD3DSurface (Internal, no d3d mapping) methods ***/
#define IWineD3DSurface_CleanDirtyRect(p)            (p)->lpVtbl->CleanDirtyRect(p)
#define IWineD3DSurface_AddDirtyRect(p,a)            (p)->lpVtbl->AddDirtyRect(p,a)
#define IWineD3DSurface_LoadTexture(p)               (p)->lpVtbl->LoadTexture(p)
#define IWineD3DSurface_SaveSnapshot(p,a)            (p)->lpVtbl->SaveSnapshot(p,a)
#define IWineD3DSurface_SetContainer(p,a)            (p)->lpVtbl->SetContainer(p,a)
#define IWineD3DSurface_SetPBufferState(p,a,b)       (p)->lpVtbl->SetPBufferState(p,a,b)
#define IWineD3DSurface_SetGlTextureDesc(p,a,b)      (p)->lpVtbl->SetGlTextureDesc(p,a,b)
#define IWineD3DSurface_GetGlDesc(p,a)               (p)->lpVtbl->GetGlDesc(p,a)
#define IWineD3DSurface_GetData(p)                   (p)->lpVtbl->GetData(p)
#define IWineD3DSurface_SetFormat(p,a)               (p)->lpVtbl->SetFormat(p,a)
#endif

/*****************************************************************************
 * IWineD3DVolume interface
 */
#define INTERFACE IWineD3DVolume
DECLARE_INTERFACE_(IWineD3DVolume,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/    
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE, GetType)(THIS) PURE;    
    /*** IWineD3DVolume methods ***/    
    STDMETHOD(GetContainer)(THIS_ REFIID  riid, void ** ppContainer) PURE;
    STDMETHOD(GetDesc)(THIS_ WINED3DVOLUME_DESC * pDesc) PURE;
    STDMETHOD(LockBox)(THIS_ D3DLOCKED_BOX* pLockedVolume, CONST D3DBOX* pBox, DWORD Flags) PURE;
    STDMETHOD(UnlockBox)(THIS) PURE;
    STDMETHOD(AddDirtyBox)(THIS_ CONST D3DBOX* pDirtyBox) PURE;
    STDMETHOD(CleanDirtyBox)(THIS) PURE;
    STDMETHOD(LoadTexture)(THIS_ UINT gl_level) PURE;
    STDMETHOD(SetContainer)(THIS_ IUnknown *container) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DVolume_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DVolume_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DVolume_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DVolume_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DVolume_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DVolume_SetPrivateData(p,a,b,c,d)  (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DVolume_GetPrivateData(p,a,b,c)    (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DVolume_FreePrivateData(p,a)       (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DVolume_SetPriority(p,a)           (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DVolume_GetPriority(p)             (p)->lpVtbl->GetPriority(p)
#define IWineD3DVolume_PreLoad(p)                 (p)->lpVtbl->PreLoad(p)
#define IWineD3DVolume_GetType(p)                 (p)->lpVtbl->GetType(p)
/*** IWineD3DVolume methods ***/
#define IWineD3DVolume_GetContainer(p,a,b)        (p)->lpVtbl->GetContainer(p,a,b)
#define IWineD3DVolume_GetDesc(p,a)               (p)->lpVtbl->GetDesc(p,a)
#define IWineD3DVolume_LockBox(p,a,b,c)           (p)->lpVtbl->LockBox(p,a,b,c)
#define IWineD3DVolume_UnlockBox(p)               (p)->lpVtbl->UnlockBox(p)
#define IWineD3DVolume_AddDirtyBox(p,a)           (p)->lpVtbl->AddDirtyBox(p,a)
#define IWineD3DVolume_CleanDirtyBox(p)           (p)->lpVtbl->CleanDirtyBox(p)
#define IWineD3DVolume_LoadTexture(p,a)           (p)->lpVtbl->LoadTexture(p,a)
#define IWineD3DVolume_SetContainer(p,a)          (p)->lpVtbl->SetContainer(p,a)
#endif

/*****************************************************************************
 * IWineD3DVertexDeclaration interface
 */
#define INTERFACE IWineD3DVertexDeclaration
DECLARE_INTERFACE_(IWineD3DVertexDeclaration,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DVertexDeclaration methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice **ppDevice) PURE;
    STDMETHOD(GetDeclaration)(THIS_ VOID *pDecl, DWORD *pSize) PURE;
    STDMETHOD(SetDeclaration)(THIS_ VOID *pDecl) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DVertexDeclaration_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DVertexDeclaration_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DVertexDeclaration_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DVertexDeclaration methods ***/
#define IWineD3DVertexDeclaration_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DVertexDeclaration_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DVertexDeclaration_GetDeclaration(p,a,b)      (p)->lpVtbl->GetDeclaration(p,a,b)
#define IWineD3DVertexDeclaration_SetDeclaration(p,b)        (p)->lpVtbl->SetDeclaration(p,b)
#endif

/*****************************************************************************
 * IWineD3DStateBlock interface 
 */
#define INTERFACE IWineD3DStateBlock
DECLARE_INTERFACE_(IWineD3DStateBlock,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DStateBlock methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice **pDevice) PURE;
    STDMETHOD(Capture)(THIS) PURE;
    STDMETHOD(Apply)(THIS) PURE;
    STDMETHOD(InitStartupStateBlock)(THIS) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DStateBlock_QueryInterface(p,a,b)                (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DStateBlock_AddRef(p)                            (p)->lpVtbl->AddRef(p)
#define IWineD3DStateBlock_Release(p)                           (p)->lpVtbl->Release(p)
/*** IWineD3DStateBlock methods ***/
#define IWineD3DStateBlock_GetParent(p,a)                       (p)->lpVtbl->GetParent(p,a)
#define IWineD3DStateBlock_GetDevice(p,a)                       (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DStateBlock_Capture(p)                           (p)->lpVtbl->Capture(p)
#define IWineD3DStateBlock_Apply(p)                             (p)->lpVtbl->Apply(p)
#define IWineD3DStateBlock_InitStartupStateBlock(p)             (p)->lpVtbl->InitStartupStateBlock(p)
#endif

/*****************************************************************************
 * WineD3DQuery interface 
 */
#define INTERFACE IWineD3DQuery
DECLARE_INTERFACE_(IWineD3DQuery,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DQuery methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice **ppDevice) PURE;
    STDMETHOD(GetData)(THIS_  void *pData, DWORD dwSize, DWORD dwGetDataFlags) PURE;
    STDMETHOD_(DWORD,GetDataSize)(THIS) PURE;
    STDMETHOD_(WINED3DQUERYTYPE, GetType)(THIS) PURE;
    STDMETHOD(Issue)(THIS_ DWORD dwIssueFlags) PURE;
    
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DQuery_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DQuery_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DQuery_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DQuery methods ***/
#define IWineD3DQuery_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DQuery_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DQuery_GetData(p,a,b,c)             (p)->lpVtbl->GetData(p,a,b,c)
#define IWineD3DQuery_GetDataSize(p)               (p)->lpVtbl->GetDataSize(p)
#define IWineD3DQuery_GetType(p)                   (p)->lpVtbl->GetType(p)
#define IWineD3DQuery_Issue(p,a)                   (p)->lpVtbl->Issue(p,a)

#endif

/*****************************************************************************
 * IWineD3DSwapChain interface
 * TODO: add gamma-ramp setting functions to make life easier
 * (There kinda missing from Microsofts DirectX!)
 */
#define INTERFACE IWineD3DSwapChain
DECLARE_INTERFACE_(IWineD3DSwapChain,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IDirect3DSwapChain9 methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice **ppDevice) PURE;
    STDMETHOD(Present)(THIS_ CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion, DWORD dwFlags) PURE;
    STDMETHOD(GetFrontBufferData)(THIS_ IWineD3DSurface *pDestSurface) PURE;
    STDMETHOD(GetBackBuffer)(THIS_ UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IWineD3DSurface **ppBackBuffer) PURE;
    STDMETHOD(GetRasterStatus)(THIS_ D3DRASTER_STATUS *pRasterStatus) PURE;
    STDMETHOD(GetDisplayMode)(THIS_ D3DDISPLAYMODE *pMode) PURE;
    STDMETHOD(GetPresentParameters)(THIS_ D3DPRESENT_PARAMETERS *pPresentationParameters) PURE;
    STDMETHOD(SetGammaRamp)(THIS_ DWORD Flags, const D3DGAMMARAMP *pRamp) PURE;
    STDMETHOD(GetGammaRamp)(THIS_ D3DGAMMARAMP *pRamp) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DSwapChain_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DSwapChain_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DSwapChain_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DSwapChain methods ***/

#define IWineD3DSwapChain_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DSwapChain_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DSwapChain_Present(p,a,b,c,d,e)         (p)->lpVtbl->Present(p,a,b,c,d,e)
#define IWineD3DSwapChain_GetFrontBufferData(p,a)      (p)->lpVtbl->GetFrontBufferData(p,a)
#define IWineD3DSwapChain_GetBackBuffer(p,a,b,c)       (p)->lpVtbl->GetBackBuffer(p,a,b,c)
#define IWineD3DSwapChain_GetRasterStatus(p,a)         (p)->lpVtbl->GetRasterStatus(p,a)
#define IWineD3DSwapChain_GetDisplayMode(p,a)          (p)->lpVtbl->GetDisplayMode(p,a)
#define IWineD3DSwapChain_GetPresentParameters(p,a)    (p)->lpVtbl->GetPresentParameters(p,a)
#define IWineD3DSwapChain_SetGammaRamp(p,a,b)          (p)->lpVtbl->SetGammaRamp(p,a,b)
#define IWineD3DSwapChain_GetGammaRamp(p,a)            (p)->lpVtbl->GetGammaRamp(p,a)
#endif

/*****************************************************************************
 * IWineD3DVertexShader interface 
 */
#define INTERFACE IWineD3DVertexShader
DECLARE_INTERFACE_(IWineD3DVertexShader,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DVertexShader methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice** ppDevice) PURE;
    STDMETHOD(GetFunction)(THIS_ VOID *pData, UINT *pSizeOfData) PURE;
    STDMETHOD(SetFunction)(THIS_ CONST DWORD *pFunction) PURE;
    STDMETHOD(SetConstantB)(THIS_ UINT StartRegister, CONST BOOL*  pConstantData, UINT BoolCount) PURE;
    STDMETHOD(SetConstantI)(THIS_ UINT StartRegister, CONST INT*   pConstantData, UINT Vector4iCount) PURE;
    STDMETHOD(SetConstantF)(THIS_ UINT StartRegister, CONST FLOAT* pConstantData, UINT Vector4fCount) PURE;
    STDMETHOD(GetConstantB)(THIS_ UINT StartRegister, BOOL*  pConstantData, UINT BoolCount) PURE;
    STDMETHOD(GetConstantI)(THIS_ UINT StartRegister, INT*   pConstantData, UINT Vector4iCount) PURE;
    STDMETHOD(GetConstantF)(THIS_ UINT StartRegister, FLOAT* pConstantData, UINT Vector4fCount) PURE;
    /* Internal Interfaces */
    STDMETHOD_(DWORD, GetVersion)(THIS) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DVertexShader_QueryInterface(p,a,b)     (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DVertexShader_AddRef(p)                 (p)->lpVtbl->AddRef(p)
#define IWineD3DVertexShader_Release(p)                (p)->lpVtbl->Release(p)
/*** IWineD3DVertexShader methods ***/
#define IWineD3DVertexShader_GetParent(p,a)            (p)->lpVtbl->GetParent(p,a)
#define IWineD3DVertexShader_GetDevice(p,a)            (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DVertexShader_GetFunction(p,a,b)        (p)->lpVtbl->GetFunction(p,a,b)
#define IWineD3DVertexShader_SetFunction(p,a)          (p)->lpVtbl->SetFunction(p,a)
#define IWineD3DVertexShader_SetConstantB(p,a,b,c)     (p)->lpVtbl->SetConstantB(p,a,b,c)
#define IWineD3DVertexShader_SetConstantI(p,a,b,c)     (p)->lpVtbl->SetConstantI(p,a,b,c)
#define IWineD3DVertexShader_SetConstantF(p,a,b,c)     (p)->lpVtbl->SetConstantF(p,a,b,c)
#define IWineD3DVertexShader_GetConstantB(p,a,b,c)     (p)->lpVtbl->GetConstantB(p,a,b,c)
#define IWineD3DVertexShader_GetConstantI(p,a,b,c)     (p)->lpVtbl->GetConstantI(p,a,b,c)
#define IWineD3DVertexShader_GetConstantF(p,a,b,c)     (p)->lpVtbl->GetConstantF(p,a,b,c)
#define IWineD3DVertexShader_GetVersion(p)             (p)->lpVtbl->GetVersion(p)
#endif

/*****************************************************************************
 * IWineD3DPixelShader interface 
 */
#define INTERFACE IWineD3DPixelShader
DECLARE_INTERFACE_(IWineD3DPixelShader,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DPixelShader methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice** ppDevice) PURE;
    STDMETHOD(GetFunction)(THIS_ VOID* pData, UINT* pSizeOfData) PURE;
    STDMETHOD(SetConstantF)(THIS_ UINT StartRegister, CONST FLOAT* pConstantData, UINT Vector4fCount) PURE;
    STDMETHOD(GetConstantF)(THIS_ UINT StartRegister, FLOAT* pConstantData, UINT Vector4fCount) PURE;
    /* Internal Interfaces */
    STDMETHOD_(DWORD, GetVersion)(THIS) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DPixelShader_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DPixelShader_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DPixelShader_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DPixelShader methods ***/
#define IWineD3DPixelShader_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DPixelShader_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DPixelShader_GetFunction(p,a,b)         (p)->lpVtbl->GetFunction(p,a,b)
#define IWineD3DPixelShader_SetConstantF(p,a,b,c)      (p)->lpVtbl->SetConstantF(p,a,b,c)
#define IWineD3DPixelShader_GetConstantF(p,a,b,c)      (p)->lpVtbl->GetConstantF(p,a,b,c)
#define IWineD3DPixelShader_GetVersion(p)              (p)->lpVtbl->GetVersion(p)
#endif

#if 0 /* FIXME: During porting in from d3d8 - the following will be used */
extern HRESULT WINAPI IDirect3DVertexShaderImpl_ParseProgram(IDirect3DVertexShaderImpl* This, CONST DWORD* pFunction);
/* internal Interfaces */
extern HRESULT WINAPI IDirect3DVertexShaderImpl_ExecuteSW(IDirect3DVertexShaderImpl* This, VSHADERINPUTDATA* input, VSHADEROUTPUTDATA* output);
#endif /* Temporary #if 0 */


#endif
