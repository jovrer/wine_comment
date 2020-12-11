/*
 * Direct3D wine types include file
 *
 * Copyright 2002-2003 The wine-d3d team
 * Copyright 2002-2003 Jason Edmeades
 *                     Raphael Junqueira
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

#ifndef __WINE_WINED3D_TYPES_H
#define __WINE_WINED3D_TYPES_H

/* TODO: remove the dependency on d3d9 or d3d8 */
#if !defined( __WINE_D3D8_H ) && !defined( __WINE_D3D9_H )
# error You must include d3d8.h or d3d9.h header to use this header
#endif

#define WINED3D_VSHADER_MAX_CONSTANTS 96
#define WINED3D_PSHADER_MAX_CONSTANTS 32

typedef struct _WINED3DVECTOR_3 {
    float x;
    float y;
    float z;
} WINED3DVECTOR_3;

typedef struct _WINED3DVECTOR_4 {
    float x;
    float y;
    float z;
    float w;
} WINED3DVECTOR_4;

typedef struct WINED3DSHADERVECTOR {
  float x;
  float y;
  float z;
  float w;
} WINED3DSHADERVECTOR;

typedef struct WINED3DSHADERSCALAR {
  float x;
} WINED3DSHADERSCALAR;

typedef WINED3DSHADERVECTOR WINEVSHADERCONSTANTS8[WINED3D_VSHADER_MAX_CONSTANTS];

typedef struct VSHADERDATA {
  /** Run Time Shader Function Constants */
  /*D3DXBUFFER* constants;*/
  WINEVSHADERCONSTANTS8 C;
  /** Shader Code as char ... */
  CONST DWORD* code;
  UINT codeLength;
} VSHADERDATA;

/** temporary here waiting for buffer code */
typedef struct VSHADERINPUTDATA {
  WINED3DSHADERVECTOR V[17];
} WINEVSHADERINPUTDATA;

/** temporary here waiting for buffer code */
typedef struct VSHADEROUTPUTDATA {
  WINED3DSHADERVECTOR oPos;
  WINED3DSHADERVECTOR oD[2];
  WINED3DSHADERVECTOR oT[8];
  WINED3DSHADERVECTOR oFog;
  WINED3DSHADERVECTOR oPts;
} WINEVSHADEROUTPUTDATA;

typedef WINED3DSHADERVECTOR WINEPSHADERCONSTANTS8[WINED3D_PSHADER_MAX_CONSTANTS];

typedef struct PSHADERDATA {
  /** Run Time Shader Function Constants */
  /*D3DXBUFFER* constants;*/
  WINEPSHADERCONSTANTS8 C;
  /** Shader Code as char ... */
  CONST DWORD* code;
  UINT codeLength;
} PSHADERDATA;

/** temporary here waiting for buffer code */
typedef struct PSHADERINPUTDATA {
  WINED3DSHADERVECTOR V[2];
  WINED3DSHADERVECTOR T[8];
  WINED3DSHADERVECTOR S[16];
  /*D3DSHADERVECTOR R[12];*/
} WINEPSHADERINPUTDATA;

/** temporary here waiting for buffer code */
typedef struct PSHADEROUTPUTDATA {
  WINED3DSHADERVECTOR oC[4];
  WINED3DSHADERVECTOR oDepth;
} WINEPSHADEROUTPUTDATA;

/*****************************************************************************
 * WineD3D Structures to be used when d3d8 and d3d9 are incompatible
 */

typedef enum _WINED3DDEGREETYPE {
    WINED3DDEGREE_LINEAR      = 1,
    WINED3DDEGREE_QUADRATIC   = 2,
    WINED3DDEGREE_CUBIC       = 3,
    WINED3DDEGREE_QUINTIC     = 5,
    
    WINED3DDEGREE_FORCE_DWORD   = 0x7fffffff
} WINED3DDEGREETYPE;


typedef enum _WINED3DFORMAT {
    WINED3DFMT_UNKNOWN              =   0,

    WINED3DFMT_R8G8B8               =  20,
    WINED3DFMT_A8R8G8B8             =  21,
    WINED3DFMT_X8R8G8B8             =  22,
    WINED3DFMT_R5G6B5               =  23,
    WINED3DFMT_X1R5G5B5             =  24,
    WINED3DFMT_A1R5G5B5             =  25,
    WINED3DFMT_A4R4G4B4             =  26,
    WINED3DFMT_R3G3B2               =  27,
    WINED3DFMT_A8                   =  28,
    WINED3DFMT_A8R3G3B2             =  29,
    WINED3DFMT_X4R4G4B4             =  30,
    WINED3DFMT_A2B10G10R10          =  31,
    WINED3DFMT_A8B8G8R8             =  32,
    WINED3DFMT_X8B8G8R8             =  33,
    WINED3DFMT_G16R16               =  34,
    WINED3DFMT_A2R10G10B10          =  35,
    WINED3DFMT_A16B16G16R16         =  36,
  

    WINED3DFMT_A8P8                 =  40,
    WINED3DFMT_P8                   =  41,

    WINED3DFMT_L8                   =  50,
    WINED3DFMT_A8L8                 =  51,
    WINED3DFMT_A4L4                 =  52,

    WINED3DFMT_V8U8                 =  60,
    WINED3DFMT_L6V5U5               =  61,
    WINED3DFMT_X8L8V8U8             =  62,
    WINED3DFMT_Q8W8V8U8             =  63,
    WINED3DFMT_V16U16               =  64,
    WINED3DFMT_W11V11U10            =  65,
    WINED3DFMT_A2W10V10U10          =  67,

    WINED3DFMT_UYVY                 =  MAKEFOURCC('U', 'Y', 'V', 'Y'),
    WINED3DFMT_YUY2                 =  MAKEFOURCC('Y', 'U', 'Y', '2'),
    WINED3DFMT_DXT1                 =  MAKEFOURCC('D', 'X', 'T', '1'),
    WINED3DFMT_DXT2                 =  MAKEFOURCC('D', 'X', 'T', '2'),
    WINED3DFMT_DXT3                 =  MAKEFOURCC('D', 'X', 'T', '3'),
    WINED3DFMT_DXT4                 =  MAKEFOURCC('D', 'X', 'T', '4'),
    WINED3DFMT_DXT5                 =  MAKEFOURCC('D', 'X', 'T', '5'),
    WINED3DFMT_MULTI2_ARGB          =  MAKEFOURCC('M', 'E', 'T', '1'),
    WINED3DFMT_G8R8_G8B8            =  MAKEFOURCC('G', 'R', 'G', 'B'),
    WINED3DFMT_R8G8_B8G8            =  MAKEFOURCC('R', 'G', 'B', 'G'),

    WINED3DFMT_D16_LOCKABLE         =  70,
    WINED3DFMT_D32                  =  71,
    WINED3DFMT_D15S1                =  73,
    WINED3DFMT_D24S8                =  75,
    WINED3DFMT_D24X8                =  77,
    WINED3DFMT_D24X4S4              =  79,
    WINED3DFMT_D16                  =  80,
    WINED3DFMT_D32F_LOCKABLE        =  82,
    WINED3DFMT_D24FS8               =  83,

    WINED3DFMT_VERTEXDATA           = 100,
    WINED3DFMT_INDEX16              = 101,
    WINED3DFMT_INDEX32              = 102,
    WINED3DFMT_Q16W16V16U16         = 110,
    /* Flaoting point formats */
    WINED3DFMT_R16F                 = 111,
    WINED3DFMT_G16R16F              = 112,
    WINED3DFMT_A16B16G16R16F        = 113,
    
    /* IEEE formats */
    WINED3DFMT_R32F                 = 114,
    WINED3DFMT_G32R32F              = 115,
    WINED3DFMT_A32B32G32R32F        = 116,
    
    WINED3DFMT_CxV8U8               = 117,


    WINED3DFMT_FORCE_DWORD          = 0xFFFFFFFF
} WINED3DFORMAT;




typedef enum _WINED3DRENDERSTATETYPE {
    WINED3DRS_ZENABLE                   =   7,
    WINED3DRS_FILLMODE                  =   8,
    WINED3DRS_SHADEMODE                 =   9,
    WINED3DRS_LINEPATTERN               =  10,
    WINED3DRS_ZWRITEENABLE              =  14,
    WINED3DRS_ALPHATESTENABLE           =  15,
    WINED3DRS_LASTPIXEL                 =  16,
    WINED3DRS_SRCBLEND                  =  19,
    WINED3DRS_DESTBLEND                 =  20,
    WINED3DRS_CULLMODE                  =  22,
    WINED3DRS_ZFUNC                     =  23,
    WINED3DRS_ALPHAREF                  =  24,
    WINED3DRS_ALPHAFUNC                 =  25,
    WINED3DRS_DITHERENABLE              =  26,
    WINED3DRS_ALPHABLENDENABLE          =  27,
    WINED3DRS_FOGENABLE                 =  28,
    WINED3DRS_SPECULARENABLE            =  29,
    WINED3DRS_ZVISIBLE                  =  30,
    WINED3DRS_FOGCOLOR                  =  34,
    WINED3DRS_FOGTABLEMODE              =  35,
    WINED3DRS_FOGSTART                  =  36,
    WINED3DRS_FOGEND                    =  37,
    WINED3DRS_FOGDENSITY                =  38,
    WINED3DRS_EDGEANTIALIAS             =  40,
    WINED3DRS_ZBIAS                     =  47,
    WINED3DRS_RANGEFOGENABLE            =  48,
    WINED3DRS_STENCILENABLE             =  52,
    WINED3DRS_STENCILFAIL               =  53,
    WINED3DRS_STENCILZFAIL              =  54,
    WINED3DRS_STENCILPASS               =  55,
    WINED3DRS_STENCILFUNC               =  56,
    WINED3DRS_STENCILREF                =  57,
    WINED3DRS_STENCILMASK               =  58,
    WINED3DRS_STENCILWRITEMASK          =  59,
    WINED3DRS_TEXTUREFACTOR             =  60,
    WINED3DRS_WRAP0                     = 128,
    WINED3DRS_WRAP1                     = 129,
    WINED3DRS_WRAP2                     = 130,
    WINED3DRS_WRAP3                     = 131,
    WINED3DRS_WRAP4                     = 132,
    WINED3DRS_WRAP5                     = 133,
    WINED3DRS_WRAP6                     = 134,
    WINED3DRS_WRAP7                     = 135,
    WINED3DRS_CLIPPING                  = 136,
    WINED3DRS_LIGHTING                  = 137,
    WINED3DRS_AMBIENT                   = 139,
    WINED3DRS_FOGVERTEXMODE             = 140,
    WINED3DRS_COLORVERTEX               = 141,
    WINED3DRS_LOCALVIEWER               = 142,
    WINED3DRS_NORMALIZENORMALS          = 143,
    WINED3DRS_DIFFUSEMATERIALSOURCE     = 145,
    WINED3DRS_SPECULARMATERIALSOURCE    = 146,
    WINED3DRS_AMBIENTMATERIALSOURCE     = 147,
    WINED3DRS_EMISSIVEMATERIALSOURCE    = 148,
    WINED3DRS_VERTEXBLEND               = 151,
    WINED3DRS_CLIPPLANEENABLE           = 152,  
    WINED3DRS_SOFTWAREVERTEXPROCESSING  = 153,
    WINED3DRS_POINTSIZE                 = 154,
    WINED3DRS_POINTSIZE_MIN             = 155,
    WINED3DRS_POINTSPRITEENABLE         = 156,
    WINED3DRS_POINTSCALEENABLE          = 157,
    WINED3DRS_POINTSCALE_A              = 158,
    WINED3DRS_POINTSCALE_B              = 159,
    WINED3DRS_POINTSCALE_C              = 160,
    WINED3DRS_MULTISAMPLEANTIALIAS      = 161,
    WINED3DRS_MULTISAMPLEMASK           = 162,
    WINED3DRS_PATCHEDGESTYLE            = 163,
    WINED3DRS_PATCHSEGMENTS             = 164,
    WINED3DRS_DEBUGMONITORTOKEN         = 165,
    WINED3DRS_POINTSIZE_MAX             = 166,
    WINED3DRS_INDEXEDVERTEXBLENDENABLE  = 167,
    WINED3DRS_COLORWRITEENABLE          = 168,
    WINED3DRS_TWEENFACTOR               = 170,
    WINED3DRS_BLENDOP                   = 171,
    WINED3DRS_POSITIONORDER             = 172,
    WINED3DRS_NORMALORDER               = 173,
    WINED3DRS_POSITIONDEGREE            = 172,
    WINED3DRS_NORMALDEGREE              = 173,
    WINED3DRS_SCISSORTESTENABLE         = 174,
    WINED3DRS_SLOPESCALEDEPTHBIAS       = 175,
    WINED3DRS_ANTIALIASEDLINEENABLE     = 176,
    WINED3DRS_MINTESSELLATIONLEVEL      = 178,
    WINED3DRS_MAXTESSELLATIONLEVEL      = 179,
    WINED3DRS_ADAPTIVETESS_X            = 180,
    WINED3DRS_ADAPTIVETESS_Y            = 181,
    WINED3DRS_ADAPTIVETESS_Z            = 182,
    WINED3DRS_ADAPTIVETESS_W            = 183,
    WINED3DRS_ENABLEADAPTIVETESSELLATION= 184,
    WINED3DRS_TWOSIDEDSTENCILMODE       = 185,
    WINED3DRS_CCW_STENCILFAIL           = 186,
    WINED3DRS_CCW_STENCILZFAIL          = 187,
    WINED3DRS_CCW_STENCILPASS           = 188,
    WINED3DRS_CCW_STENCILFUNC           = 189,
    WINED3DRS_COLORWRITEENABLE1         = 190,
    WINED3DRS_COLORWRITEENABLE2         = 191,
    WINED3DRS_COLORWRITEENABLE3         = 192,
    WINED3DRS_BLENDFACTOR               = 193,
    WINED3DRS_SRGBWRITEENABLE           = 194,
    WINED3DRS_DEPTHBIAS                 = 195,
    WINED3DRS_WRAP8                     = 198,
    WINED3DRS_WRAP9                     = 199,
    WINED3DRS_WRAP10                    = 200,
    WINED3DRS_WRAP11                    = 201,
    WINED3DRS_WRAP12                    = 202,
    WINED3DRS_WRAP13                    = 203,
    WINED3DRS_WRAP14                    = 204,
    WINED3DRS_WRAP15                    = 205,
    WINED3DRS_SEPARATEALPHABLENDENABLE  = 206,
    WINED3DRS_SRCBLENDALPHA             = 207,
    WINED3DRS_DESTBLENDALPHA            = 208,
    WINED3DRS_BLENDOPALPHA              = 209,

    WINED3DRS_FORCE_DWORD               = 0x7fffffff
} WINED3DRENDERSTATETYPE;

#define WINEHIGHEST_RENDER_STATE   WINED3DRS_BLENDOPALPHA
        /* Highest D3DRS_ value   */

typedef enum _WINED3DSWAPEFFECT {
    WINED3DSWAPEFFECT_DISCARD         = 1,
    WINED3DSWAPEFFECT_FLIP            = 2,
    WINED3DSWAPEFFECT_COPY            = 3,
    WINED3DSWAPEFFECT_COPY_VSYNC      = 4,
    WINED3DSWAPEFFECT_FORCE_DWORD     = 0xFFFFFFFF
} WINED3DSWAPEFFECT;


typedef enum _WINED3DSAMPLERSTATETYPE {
    WINED3DSAMP_ADDRESSU       = 1,
    WINED3DSAMP_ADDRESSV       = 2,
    WINED3DSAMP_ADDRESSW       = 3,
    WINED3DSAMP_BORDERCOLOR    = 4,
    WINED3DSAMP_MAGFILTER      = 5,
    WINED3DSAMP_MINFILTER      = 6,
    WINED3DSAMP_MIPFILTER      = 7,
    WINED3DSAMP_MIPMAPLODBIAS  = 8,
    WINED3DSAMP_MAXMIPLEVEL    = 9,
    WINED3DSAMP_MAXANISOTROPY  = 10,
    WINED3DSAMP_SRGBTEXTURE    = 11,
    WINED3DSAMP_ELEMENTINDEX   = 12,
    WINED3DSAMP_DMAPOFFSET     = 13,

    WINED3DSAMP_FORCE_DWORD   = 0x7fffffff,
} WINED3DSAMPLERSTATETYPE;
#define WINED3D_HIGHEST_SAMPLER_STATE WINED3DSAMP_DMAPOFFSET

typedef enum _WINED3DTEXTURESTAGESTATETYPE {
    WINED3DTSS_COLOROP               =  1,
    WINED3DTSS_COLORARG1             =  2,
    WINED3DTSS_COLORARG2             =  3,
    WINED3DTSS_ALPHAOP               =  4,
    WINED3DTSS_ALPHAARG1             =  5,
    WINED3DTSS_ALPHAARG2             =  6,
    WINED3DTSS_BUMPENVMAT00          =  7,
    WINED3DTSS_BUMPENVMAT01          =  8,
    WINED3DTSS_BUMPENVMAT10          =  9,
    WINED3DTSS_BUMPENVMAT11          = 10,
    WINED3DTSS_TEXCOORDINDEX         = 11,
    WINED3DTSS_BUMPENVLSCALE         = 22,
    WINED3DTSS_BUMPENVLOFFSET        = 23,
    WINED3DTSS_TEXTURETRANSFORMFLAGS = 24,
    WINED3DTSS_ADDRESSW              = 25,
    WINED3DTSS_COLORARG0             = 26,
    WINED3DTSS_ALPHAARG0             = 27,
    WINED3DTSS_RESULTARG             = 28,
    WINED3DTSS_CONSTANT              = 32,

    WINED3DTSS_FORCE_DWORD           = 0x7fffffff
} WINED3DTEXTURESTAGESTATETYPE;

#define WINED3D_HIGHEST_TEXTURE_STATE WINED3DTSS_CONSTANT

typedef struct _WINEDD3DRECTPATCH_INFO {
    UINT                StartVertexOffsetWidth;
    UINT                StartVertexOffsetHeight;
    UINT                Width;
    UINT                Height;
    UINT                Stride;
    D3DBASISTYPE        Basis;
    WINED3DDEGREETYPE   Degree;
} WINED3DRECTPATCH_INFO;


typedef struct _WINED3DADAPTER_IDENTIFIER {
    char           *Driver;
    char           *Description;
    char           *DeviceName;
    LARGE_INTEGER  *DriverVersion; 
    DWORD          *VendorId;
    DWORD          *DeviceId;
    DWORD          *SubSysId;
    DWORD          *Revision;
    GUID           *DeviceIdentifier;
    DWORD          *WHQLLevel;
} WINED3DADAPTER_IDENTIFIER;

typedef struct _WINED3DPRESENT_PARAMETERS {
    UINT                *BackBufferWidth;
    UINT                *BackBufferHeight;
    WINED3DFORMAT       *BackBufferFormat;
    UINT                *BackBufferCount;
    D3DMULTISAMPLE_TYPE *MultiSampleType;
    DWORD               *MultiSampleQuality;
    D3DSWAPEFFECT       *SwapEffect;
    HWND                *hDeviceWindow;
    BOOL                *Windowed;
    BOOL                *EnableAutoDepthStencil;
    WINED3DFORMAT       *AutoDepthStencilFormat;
    DWORD               *Flags;
    UINT                *FullScreen_RefreshRateInHz;
    UINT                *PresentationInterval;
} WINED3DPRESENT_PARAMETERS;

typedef struct _WINED3DSURFACE_DESC
{
    WINED3DFORMAT           *Format;
    D3DRESOURCETYPE     *Type;
    DWORD               *Usage;
    D3DPOOL             *Pool;
    UINT                *Size;

    D3DMULTISAMPLE_TYPE *MultiSampleType;
    DWORD               *MultiSampleQuality;
    UINT                *Width;
    UINT                *Height;
} WINED3DSURFACE_DESC;

typedef struct _WINED3DVOLUME_DESC
{
    WINED3DFORMAT       *Format;
    D3DRESOURCETYPE     *Type;
    DWORD               *Usage;
    D3DPOOL             *Pool;
    UINT                *Size;

    UINT                *Width;
    UINT                *Height;
    UINT                *Depth;
} WINED3DVOLUME_DESC;

typedef struct _WINED3DVERTEXELEMENT {
  WORD    Stream;
  WORD    Offset;
  BYTE    Type;
  BYTE    Method;
  BYTE    Usage;
  BYTE    UsageIndex;
} WINED3DVERTEXELEMENT, *LPWINED3DVERTEXELEMENT;


typedef enum _WINED3DQUERYTYPE {
    WINED3DQUERYTYPE_VCACHE             = 4,
    WINED3DQUERYTYPE_RESOURCEMANAGER    = 5,
    WINED3DQUERYTYPE_VERTEXSTATS        = 6,
    WINED3DQUERYTYPE_EVENT              = 8,
    WINED3DQUERYTYPE_OCCLUSION          = 9,
    WINED3DQUERYTYPE_TIMESTAMP          = 10,
    WINED3DQUERYTYPE_TIMESTAMPDISJOINT  = 11,
    WINED3DQUERYTYPE_TIMESTAMPFREQ      = 12,
    WINED3DQUERYTYPE_PIPELINETIMINGS    = 13,
    WINED3DQUERYTYPE_INTERFACETIMINGS   = 14,
    WINED3DQUERYTYPE_VERTEXTIMINGS      = 15,
    WINED3DQUERYTYPE_PIXELTIMINGS       = 16,
    WINED3DQUERYTYPE_BANDWIDTHTIMINGS   = 17,
    WINED3DQUERYTYPE_CACHEUTILIZATION   = 18
} WINED3DQUERYTYPE;

#define WINED3DISSUE_BEGIN   (1 << 1)
#define WINED3DISSUE_END     (1 << 0)
#define WINED3DGETDATA_FLUSH (1 << 0)

typedef struct _WINED3DDEVICE_CREATION_PARAMETERS {
    UINT          AdapterOrdinal;
    D3DDEVTYPE    DeviceType;
    HWND          hFocusWindow;
    DWORD         BehaviorFlags;
} WINED3DDEVICE_CREATION_PARAMETERS;

typedef struct _WINED3DDEVINFO_BANDWIDTHTIMINGS {
    float         MaxBandwidthUtilized;
    float         FrontEndUploadMemoryUtilizedPercent;
    float         VertexRateUtilizedPercent;
    float         TriangleSetupRateUtilizedPercent;
    float         FillRateUtilizedPercent;
} WINED3DDEVINFO_BANDWIDTHTIMINGS;

typedef struct _WINED3DDEVINFO_CACHEUTILIZATION {
    float         TextureCacheHitRate;
    float         PostTransformVertexCacheHitRate;
} WINED3DDEVINFO_CACHEUTILIZATION;

typedef struct _WINED3DDEVINFO_INTERFACETIMINGS {
    float         WaitingForGPUToUseApplicationResourceTimePercent;
    float         WaitingForGPUToAcceptMoreCommandsTimePercent;
    float         WaitingForGPUToStayWithinLatencyTimePercent;
    float         WaitingForGPUExclusiveResourceTimePercent;
    float         WaitingForGPUOtherTimePercent;
} WINED3DDEVINFO_INTERFACETIMINGS;

typedef struct _WINED3DDEVINFO_PIPELINETIMINGS {
    float         VertexProcessingTimePercent;
    float         PixelProcessingTimePercent;
    float         OtherGPUProcessingTimePercent;
    float         GPUIdleTimePercent;
} WINED3DDEVINFO_PIPELINETIMINGS;

typedef struct _WINED3DDEVINFO_STAGETIMINGS {
    float         MemoryProcessingPercent;
    float         ComputationProcessingPercent;
} WINED3DDEVINFO_STAGETIMINGS;


typedef struct WINED3DRESOURCESTATS {
    BOOL                bThrashing;
    DWORD               ApproxBytesDownloaded;
    DWORD               NumEvicts;
    DWORD               NumVidCreates;
    DWORD               LastPri;
    DWORD               NumUsed;
    DWORD               NumUsedInVidMem;
    DWORD               WorkingSet;
    DWORD               WorkingSetBytes;
    DWORD               TotalManaged;
    DWORD               TotalBytes;
} WINED3DRESOURCESTATS;

typedef enum _WINED3DRESOURCETYPE {
    WINED3DRTYPE_SURFACE                =  1,
    WINED3DRTYPE_VOLUME                 =  2,
    WINED3DRTYPE_TEXTURE                =  3,
    WINED3DRTYPE_VOLUMETEXTURE          =  4,
    WINED3DRTYPE_CUBETEXTURE            =  5,
    WINED3DRTYPE_VERTEXBUFFER           =  6,
    WINED3DRTYPE_INDEXBUFFER            =  7,

    WINED3DRTYPE_FORCE_DWORD            = 0x7fffffff
} WINED3DRESOURCETYPE;

#define WINED3DRTYPECOUNT (WINED3DRTYPE_INDEXBUFFER+1)

typedef struct _WINED3DDEVINFO_RESOURCEMANAGER {
    WINED3DRESOURCESTATS stats[WINED3DRTYPECOUNT];
} WINED3DDEVINFO_RESOURCEMANAGER;

typedef struct _WINED3DDEVINFO_VERTEXSTATS {
    DWORD NumRenderedTriangles;
    DWORD NumExtraClippingTriangles;
} WINED3DDEVINFO_VERTEXSTATS;


/*Vertex cache optimization hints.*/
typedef struct WINED3DDEVINFO_VCACHE {
    /*Must be a 4 char code FOURCC (e.g. CACH)*/
    DWORD         Pattern; 
    /*0 to get the longest  strips, 1 vertex cache*/
    DWORD         OptMethod; 
     /*Cache size to use (only valid if OptMethod==1) */
    DWORD         CacheSize;
    /*internal for deciding when to restart strips, non user modifyable (only valid if OptMethod==1)*/
    DWORD         MagicNumber; 
} WINED3DDEVINFO_VCACHE;

/*
 * The wined3dcaps structure
 */

typedef struct _WINED3DVSHADERCAPS2_0 {
  DWORD  *Caps;
  INT    *DynamicFlowControlDepth;
  INT    *NumTemps;
  INT    *StaticFlowControlDepth;
} WINED3DVSHADERCAPS2_0;

typedef struct _WINED3DPSHADERCAPS2_0 {
  DWORD  *Caps;
  INT    *DynamicFlowControlDepth;
  INT    *NumTemps;
  INT    *StaticFlowControlDepth;
  INT    *NumInstructionSlots;
} WINED3DPSHADERCAPS2_0;

typedef struct _WINED3DCAPS {
  D3DDEVTYPE          *DeviceType;
  UINT                *AdapterOrdinal;

  DWORD               *Caps;
  DWORD               *Caps2;
  DWORD               *Caps3;
  DWORD               *PresentationIntervals;

  DWORD               *CursorCaps;

  DWORD               *DevCaps;

  DWORD               *PrimitiveMiscCaps;
  DWORD               *RasterCaps;
  DWORD               *ZCmpCaps;
  DWORD               *SrcBlendCaps;
  DWORD               *DestBlendCaps;
  DWORD               *AlphaCmpCaps;
  DWORD               *ShadeCaps;
  DWORD               *TextureCaps;
  DWORD               *TextureFilterCaps;
  DWORD               *CubeTextureFilterCaps;
  DWORD               *VolumeTextureFilterCaps;
  DWORD               *TextureAddressCaps;
  DWORD               *VolumeTextureAddressCaps;

  DWORD               *LineCaps;

  DWORD               *MaxTextureWidth;
  DWORD               *MaxTextureHeight;
  DWORD               *MaxVolumeExtent;

  DWORD               *MaxTextureRepeat;
  DWORD               *MaxTextureAspectRatio;
  DWORD               *MaxAnisotropy;
  float               *MaxVertexW;

  float               *GuardBandLeft;
  float               *GuardBandTop;
  float               *GuardBandRight;
  float               *GuardBandBottom;

  float               *ExtentsAdjust;
  DWORD               *StencilCaps;

  DWORD               *FVFCaps;
  DWORD               *TextureOpCaps;
  DWORD               *MaxTextureBlendStages;
  DWORD               *MaxSimultaneousTextures;

  DWORD               *VertexProcessingCaps;
  DWORD               *MaxActiveLights;
  DWORD               *MaxUserClipPlanes;
  DWORD               *MaxVertexBlendMatrices;
  DWORD               *MaxVertexBlendMatrixIndex;

  float               *MaxPointSize;

  DWORD               *MaxPrimitiveCount;
  DWORD               *MaxVertexIndex;
  DWORD               *MaxStreams;
  DWORD               *MaxStreamStride;

  DWORD               *VertexShaderVersion;
  DWORD               *MaxVertexShaderConst;

  DWORD               *PixelShaderVersion;
  float               *PixelShader1xMaxValue;

  /* DX 9 */
  DWORD               *DevCaps2;

  float               *MaxNpatchTessellationLevel;
  DWORD               *Reserved5; /*undocumented*/

  UINT                *MasterAdapterOrdinal;
  UINT                *AdapterOrdinalInGroup;
  UINT                *NumberOfAdaptersInGroup;
  DWORD               *DeclTypes;
  DWORD               *NumSimultaneousRTs;
  DWORD               *StretchRectFilterCaps;
  WINED3DVSHADERCAPS2_0   VS20Caps;
  WINED3DPSHADERCAPS2_0   PS20Caps;
  DWORD               *VertexTextureFilterCaps;
  DWORD               *MaxVShaderInstructionsExecuted;
  DWORD               *MaxPShaderInstructionsExecuted;
  DWORD               *MaxVertexShader30InstructionSlots; 
  DWORD               *MaxPixelShader30InstructionSlots;
  DWORD               *Reserved2;/* Not in the microsoft headers but documented */
  DWORD               *Reserved3;

} WINED3DCAPS;

typedef enum _WINED3DSTATEBLOCKTYPE {
    WINED3DSBT_INIT          = 0,
    WINED3DSBT_ALL           = 1,
    WINED3DSBT_PIXELSTATE    = 2,
    WINED3DSBT_VERTEXSTATE   = 3,

    WINED3DSBT_FORCE_DWORD   = 0xffffffff
} WINED3DSTATEBLOCKTYPE;

typedef struct glDescriptor {
    UINT          textureName;
    int           level;
    int           target;
    int/*GLenum*/ glFormat;
    int/*GLenum*/ glFormatInternal;
    int/*GLenum*/ glType;
} glDescriptor;
/**************************** 
 *  * Vertex Shaders Declaration
 *   */

typedef enum _WINED3DDECLUSAGE {
      WINED3DSHADERDECLUSAGE_POSITION     = 0,
      WINED3DSHADERDECLUSAGE_BLENDWEIGHT,
      WINED3DSHADERDECLUSAGE_BLENDINDICES,
      WINED3DSHADERDECLUSAGE_NORMAL,
      WINED3DSHADERDECLUSAGE_PSIZE ,
      WINED3DSHADERDECLUSAGE_TEXCOORD0,
      WINED3DSHADERDECLUSAGE_TEXCOORD1,
      WINED3DSHADERDECLUSAGE_TEXCOORD2,
      WINED3DSHADERDECLUSAGE_TEXCOORD3,
      WINED3DSHADERDECLUSAGE_TEXCOORD4,
      WINED3DSHADERDECLUSAGE_TEXCOORD5,
      WINED3DSHADERDECLUSAGE_TEXCOORD6,
      WINED3DSHADERDECLUSAGE_TEXCOORD7,
      WINED3DSHADERDECLUSAGE_TANGENT,
      WINED3DSHADERDECLUSAGE_BINORMAL,
      WINED3DSHADERDECLUSAGE_TESSFACTOR,
      WINED3DSHADERDECLUSAGE_POSITIONT,
      WINED3DSHADERDECLUSAGE_DIFFUSE,
      WINED3DSHADERDECLUSAGE_SPECULAR,
      WINED3DSHADERDECLUSAGE_FOG,
      WINED3DSHADERDECLUSAGE_DEPTH,
      WINED3DSHADERDECLUSAGE_SAMPLE,
      WINED3DSHADERDECLUSAGE_POSITION2,
      WINED3DSHADERDECLUSAGE_POSITION21,
      WINED3DSHADERDECLUSAGE_POSITION22,
      WINED3DSHADERDECLUSAGE_POSITION23,
      WINED3DSHADERDECLUSAGE_POSITION24,
      WINED3DSHADERDECLUSAGE_POSITION25,
      WINED3DSHADERDECLUSAGE_POSITION26,
      WINED3DSHADERDECLUSAGE_POSITION27,
      WINED3DSHADERDECLUSAGE_POSITION28,
      WINED3DSHADERDECLUSAGE_NORMAL2,
      WINED3DSHADERDECLUSAGE_POSITIONT2,
      WINED3DSHADERDECLUSAGE_MAX_USAGE
} WINED3DSHADERDECLUSAGE;

#endif
