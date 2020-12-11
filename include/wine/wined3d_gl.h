/*
 * Direct3D wine OpenGL include file
 *
 * Copyright 2002-2003 The wine-d3d team
 * Copyright 2002-2004 Jason Edmeades
 *                     Raphael Junqueira
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

#ifndef __WINE_WINED3D_GL_H
#define __WINE_WINED3D_GL_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#ifdef HAVE_OPENGL

#undef APIENTRY
#undef CALLBACK
#undef WINAPI

#define XMD_H /* This is to prevent the Xmd.h inclusion bug :-/ */
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#ifdef HAVE_GL_GLEXT_H
# include <GL/glext.h>
#endif
#undef  XMD_H

#undef APIENTRY
#define APIENTRY

/****************************************************
 * OpenGL Extensions (EXT and ARB)
 *     #defines and functions pointer
 ****************************************************/

/* GL_ARB_vertex_blend */
#ifndef GL_ARB_vertex_blend
#define GL_ARB_vertex_blend 1
#define GL_MAX_VERTEX_UNITS_ARB           0x86A4
#define GL_ACTIVE_VERTEX_UNITS_ARB        0x86A5
#define GL_WEIGHT_SUM_UNITY_ARB           0x86A6
#define GL_VERTEX_BLEND_ARB               0x86A7
#define GL_CURRENT_WEIGHT_ARB             0x86A8
#define GL_WEIGHT_ARRAY_TYPE_ARB          0x86A9
#define GL_WEIGHT_ARRAY_STRIDE_ARB        0x86AA
#define GL_WEIGHT_ARRAY_SIZE_ARB          0x86AB
#define GL_WEIGHT_ARRAY_POINTER_ARB       0x86AC
#define GL_WEIGHT_ARRAY_ARB               0x86AD
#define GL_MODELVIEW0_ARB                 0x1700
#define GL_MODELVIEW1_ARB                 0x850A
#define GL_MODELVIEW2_ARB                 0x8722
#define GL_MODELVIEW3_ARB                 0x8723
#define GL_MODELVIEW4_ARB                 0x8724
#define GL_MODELVIEW5_ARB                 0x8725
#define GL_MODELVIEW6_ARB                 0x8726
#define GL_MODELVIEW7_ARB                 0x8727
#define GL_MODELVIEW8_ARB                 0x8728
#define GL_MODELVIEW9_ARB                 0x8729
#define GL_MODELVIEW10_ARB                0x872A
#define GL_MODELVIEW11_ARB                0x872B
#define GL_MODELVIEW12_ARB                0x872C
#define GL_MODELVIEW13_ARB                0x872D
#define GL_MODELVIEW14_ARB                0x872E
#define GL_MODELVIEW15_ARB                0x872F
#define GL_MODELVIEW16_ARB                0x8730
#define GL_MODELVIEW17_ARB                0x8731
#define GL_MODELVIEW18_ARB                0x8732
#define GL_MODELVIEW19_ARB                0x8733
#define GL_MODELVIEW20_ARB                0x8734
#define GL_MODELVIEW21_ARB                0x8735
#define GL_MODELVIEW22_ARB                0x8736
#define GL_MODELVIEW23_ARB                0x8737
#define GL_MODELVIEW24_ARB                0x8738
#define GL_MODELVIEW25_ARB                0x8739
#define GL_MODELVIEW26_ARB                0x873A
#define GL_MODELVIEW27_ARB                0x873B
#define GL_MODELVIEW28_ARB                0x873C
#define GL_MODELVIEW29_ARB                0x873D
#define GL_MODELVIEW30_ARB                0x873E
#define GL_MODELVIEW31_ARB                0x873F
#endif
typedef void (APIENTRY * PGLFNGLWEIGHTPOINTERARB) (GLint size, GLenum type, GLsizei stride, GLvoid* pointer);
/* GL_EXT_secondary_color */
#ifndef GL_EXT_secondary_color
#define GL_EXT_secondary_color 1
#define GL_COLOR_SUM_EXT                     0x8458
#define GL_CURRENT_SECONDARY_COLOR_EXT       0x8459
#define GL_SECONDARY_COLOR_ARRAY_SIZE_EXT    0x845A
#define GL_SECONDARY_COLOR_ARRAY_TYPE_EXT    0x845B
#define GL_SECONDARY_COLOR_ARRAY_STRIDE_EXT  0x845C
#define GL_SECONDARY_COLOR_ARRAY_POINTER_EXT 0x845D
#define GL_SECONDARY_COLOR_ARRAY_EXT         0x845E
#endif
typedef void (APIENTRY * PGLFNGLSECONDARYCOLOR3FEXTPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (APIENTRY * PGLFNGLSECONDARYCOLOR3FVEXTPROC) (const GLfloat *v);
typedef void (APIENTRY * PGLFNGLSECONDARYCOLOR3UBEXTPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (APIENTRY * PGLFNGLSECONDARYCOLORPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
/* GL_EXT_paletted_texture */
#ifndef GL_EXT_paletted_texture
#define GL_EXT_paletted_texture 1
#define GL_COLOR_INDEX1_EXT               0x80E2
#define GL_COLOR_INDEX2_EXT               0x80E3
#define GL_COLOR_INDEX4_EXT               0x80E4
#define GL_COLOR_INDEX8_EXT               0x80E5
#define GL_COLOR_INDEX12_EXT              0x80E6
#define GL_COLOR_INDEX16_EXT              0x80E7
#define GL_TEXTURE_INDEX_SIZE_EXT         0x80ED
#endif
typedef void (APIENTRY * PGLFNGLCOLORTABLEEXTPROC) (GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
/* GL_EXT_point_parameters */
#ifndef GL_EXT_point_parameters
#define GL_EXT_point_parameters 1
#define GL_POINT_SIZE_MIN_EXT             0x8126
#define GL_POINT_SIZE_MAX_EXT             0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT  0x8128
#define GL_DISTANCE_ATTENUATION_EXT       0x8129
#endif
typedef void (APIENTRY * PGLFNGLPOINTPARAMETERFEXTPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRY * PGLFNGLPOINTPARAMETERFVEXTPROC) (GLenum pname, const GLfloat *params);
/* GL_EXT_texture_env_combine */
#ifndef GL_EXT_texture_env_combine
#define GL_EXT_texture_env_combine 1
#define GL_COMBINE_EXT                    0x8570
#define GL_COMBINE_RGB_EXT                0x8571
#define GL_COMBINE_ALPHA_EXT              0x8572
#define GL_RGB_SCALE_EXT                  0x8573
#define GL_ADD_SIGNED_EXT                 0x8574
#define GL_INTERPOLATE_EXT                0x8575
#define GL_SUBTRACT_EXT                   0x84E7
#define GL_CONSTANT_EXT                   0x8576
#define GL_PRIMARY_COLOR_EXT              0x8577
#define GL_PREVIOUS_EXT                   0x8578
#define GL_SOURCE0_RGB_EXT                0x8580
#define GL_SOURCE1_RGB_EXT                0x8581
#define GL_SOURCE2_RGB_EXT                0x8582
#define GL_SOURCE3_RGB_EXT                0x8583
#define GL_SOURCE4_RGB_EXT                0x8584
#define GL_SOURCE5_RGB_EXT                0x8585
#define GL_SOURCE6_RGB_EXT                0x8586
#define GL_SOURCE7_RGB_EXT                0x8587
#define GL_SOURCE0_ALPHA_EXT              0x8588
#define GL_SOURCE1_ALPHA_EXT              0x8589
#define GL_SOURCE2_ALPHA_EXT              0x858A
#define GL_SOURCE3_ALPHA_EXT              0x858B
#define GL_SOURCE4_ALPHA_EXT              0x858C
#define GL_SOURCE5_ALPHA_EXT              0x858D
#define GL_SOURCE6_ALPHA_EXT              0x858E
#define GL_SOURCE7_ALPHA_EXT              0x858F
#define GL_OPERAND0_RGB_EXT               0x8590
#define GL_OPERAND1_RGB_EXT               0x8591
#define GL_OPERAND2_RGB_EXT               0x8592
#define GL_OPERAND3_RGB_EXT               0x8593
#define GL_OPERAND4_RGB_EXT               0x8594
#define GL_OPERAND5_RGB_EXT               0x8595
#define GL_OPERAND6_RGB_EXT               0x8596
#define GL_OPERAND7_RGB_EXT               0x8597
#define GL_OPERAND0_ALPHA_EXT             0x8598
#define GL_OPERAND1_ALPHA_EXT             0x8599
#define GL_OPERAND2_ALPHA_EXT             0x859A
#define GL_OPERAND3_ALPHA_EXT             0x859B
#define GL_OPERAND4_ALPHA_EXT             0x859C
#define GL_OPERAND5_ALPHA_EXT             0x859D
#define GL_OPERAND6_ALPHA_EXT             0x859E
#define GL_OPERAND7_ALPHA_EXT             0x859F
#endif
/* GL_EXT_texture_env_dot3 */
#ifndef GL_EXT_texture_env_dot3
#define GL_EXT_texture_env_dot3 1
#define GL_DOT3_RGB_EXT			  0x8740
#define GL_DOT3_RGBA_EXT		  0x8741
#endif
/* GL_EXT_texture_lod_bias */
#ifndef GL_EXT_texture_lod_bias
#define GL_EXT_texture_lod_bias 1
#define GL_MAX_TEXTURE_LOD_BIAS_EXT       0x84FD
#define GL_TEXTURE_FILTER_CONTROL_EXT     0x8500
#define GL_TEXTURE_LOD_BIAS_EXT           0x8501
#endif
/* GL_ARB_texture_border_clamp */
#ifndef GL_ARB_texture_border_clamp
#define GL_ARB_texture_border_clamp 1
#define GL_CLAMP_TO_BORDER_ARB            0x812D
#endif
/* GL_ARB_texture_mirrored_repeat (full support GL1.4) */
#ifndef GL_ARB_texture_mirrored_repeat
#define GL_ARB_texture_mirrored_repeat 1
#define GL_MIRRORED_REPEAT_ARB            0x8370
#endif
/* GL_ATI_texture_mirror_once */
#ifndef GL_ATI_texture_mirror_once
#define GL_ATI_texture_mirror_once 1
#define GL_MIRROR_CLAMP_ATI               0x8742
#define GL_MIRROR_CLAMP_TO_EDGE_ATI       0x8743
#endif
/* GL_ARB_texture_env_dot3 */
#ifndef GL_ARB_texture_env_dot3
#define GL_ARB_texture_env_dot3 1
#define GL_DOT3_RGB_ARB                   0x86AE
#define GL_DOT3_RGBA_ARB                  0x86AF
#endif
/* GL_EXT_texture_env_dot3 */
#ifndef GL_EXT_texture_env_dot3
#define GL_EXT_texture_env_dot3 1
#define GL_DOT3_RGB_EXT                   0x8740
#define GL_DOT3_RGBA_EXT                  0x8741
#endif
/* GL_ARB_vertex_program */
#ifndef GL_ARB_vertex_program
#define GL_ARB_vertex_program 1
#define GL_VERTEX_PROGRAM_ARB             0x8620
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB  0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB    0x8643
#define GL_COLOR_SUM_ARB                  0x8458
#define GL_PROGRAM_FORMAT_ASCII_ARB       0x8875
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB   0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB   0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB 0x886A
#define GL_CURRENT_VERTEX_ATTRIB_ARB      0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB 0x8645
#define GL_PROGRAM_LENGTH_ARB             0x8627
#define GL_PROGRAM_FORMAT_ARB             0x8876
#define GL_PROGRAM_BINDING_ARB            0x8677
#define GL_PROGRAM_INSTRUCTIONS_ARB       0x88A0
#define GL_MAX_PROGRAM_INSTRUCTIONS_ARB   0x88A1
#define GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB 0x88A2
#define GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB 0x88A3
#define GL_PROGRAM_TEMPORARIES_ARB        0x88A4
#define GL_MAX_PROGRAM_TEMPORARIES_ARB    0x88A5
#define GL_PROGRAM_NATIVE_TEMPORARIES_ARB 0x88A6
#define GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB 0x88A7
#define GL_PROGRAM_PARAMETERS_ARB         0x88A8
#define GL_MAX_PROGRAM_PARAMETERS_ARB     0x88A9
#define GL_PROGRAM_NATIVE_PARAMETERS_ARB  0x88AA
#define GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB 0x88AB
#define GL_PROGRAM_ATTRIBS_ARB            0x88AC
#define GL_MAX_PROGRAM_ATTRIBS_ARB        0x88AD
#define GL_PROGRAM_NATIVE_ATTRIBS_ARB     0x88AE
#define GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB 0x88AF
#define GL_PROGRAM_ADDRESS_REGISTERS_ARB  0x88B0
#define GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB 0x88B1
#define GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB 0x88B2
#define GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB 0x88B3
#define GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB 0x88B4
#define GL_MAX_PROGRAM_ENV_PARAMETERS_ARB 0x88B5
#define GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB 0x88B6
#define GL_PROGRAM_STRING_ARB             0x8628
#define GL_PROGRAM_ERROR_POSITION_ARB     0x864B
#define GL_CURRENT_MATRIX_ARB             0x8641
#define GL_TRANSPOSE_CURRENT_MATRIX_ARB   0x88B7
#define GL_CURRENT_MATRIX_STACK_DEPTH_ARB 0x8640
#define GL_MAX_VERTEX_ATTRIBS_ARB         0x8869
#define GL_MAX_PROGRAM_MATRICES_ARB       0x862F
#define GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB 0x862E
#define GL_PROGRAM_ERROR_STRING_ARB       0x8874
#define GL_MATRIX0_ARB                    0x88C0
#define GL_MATRIX1_ARB                    0x88C1
#define GL_MATRIX2_ARB                    0x88C2
#define GL_MATRIX3_ARB                    0x88C3
#define GL_MATRIX4_ARB                    0x88C4
#define GL_MATRIX5_ARB                    0x88C5
#define GL_MATRIX6_ARB                    0x88C6
#define GL_MATRIX7_ARB                    0x88C7
#define GL_MATRIX8_ARB                    0x88C8
#define GL_MATRIX9_ARB                    0x88C9
#define GL_MATRIX10_ARB                   0x88CA
#define GL_MATRIX11_ARB                   0x88CB
#define GL_MATRIX12_ARB                   0x88CC
#define GL_MATRIX13_ARB                   0x88CD
#define GL_MATRIX14_ARB                   0x88CE
#define GL_MATRIX15_ARB                   0x88CF
#define GL_MATRIX16_ARB                   0x88D0
#define GL_MATRIX17_ARB                   0x88D1
#define GL_MATRIX18_ARB                   0x88D2
#define GL_MATRIX19_ARB                   0x88D3
#define GL_MATRIX20_ARB                   0x88D4
#define GL_MATRIX21_ARB                   0x88D5
#define GL_MATRIX22_ARB                   0x88D6
#define GL_MATRIX23_ARB                   0x88D7
#define GL_MATRIX24_ARB                   0x88D8
#define GL_MATRIX25_ARB                   0x88D9
#define GL_MATRIX26_ARB                   0x88DA
#define GL_MATRIX27_ARB                   0x88DB
#define GL_MATRIX28_ARB                   0x88DC
#define GL_MATRIX29_ARB                   0x88DD
#define GL_MATRIX30_ARB                   0x88DE
#define GL_MATRIX31_ARB                   0x88DF
#endif
typedef void (APIENTRY * PGLFNVERTEXATTRIB1DARBPROC) (GLuint index, GLdouble x);
typedef void (APIENTRY * PGLFNVERTEXATTRIB1DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB1FARBPROC) (GLuint index, GLfloat x);
typedef void (APIENTRY * PGLFNVERTEXATTRIB1FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB1SARBPROC) (GLuint index, GLshort x);
typedef void (APIENTRY * PGLFNVERTEXATTRIB1SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB2DARBPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRY * PGLFNVERTEXATTRIB2DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB2FARBPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRY * PGLFNVERTEXATTRIB2FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB2SARBPROC) (GLuint index, GLshort x, GLshort y);
typedef void (APIENTRY * PGLFNVERTEXATTRIB2SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB3DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PGLFNVERTEXATTRIB3DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB3FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PGLFNVERTEXATTRIB3FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB3SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (APIENTRY * PGLFNVERTEXATTRIB3SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4NBVARBPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4NIVARBPROC) (GLuint index, const GLint *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4NSVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4NUBARBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4NUBVARBPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4NUIVARBPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4NUSVARBPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4BVARBPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4IVARBPROC) (GLuint index, const GLint *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4UBVARBPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4UIVARBPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIB4USVARBPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRY * PGLFNVERTEXATTRIBPOINTERARBPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PGLFNENABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (APIENTRY * PGLFNDISABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (APIENTRY * PGLFNPROGRAMSTRINGARBPROC) (GLenum target, GLenum format, GLsizei len, const GLvoid *string);
typedef void (APIENTRY * PGLFNBINDPROGRAMARBPROC) (GLenum target, GLuint program);
typedef void (APIENTRY * PGLFNDELETEPROGRAMSARBPROC) (GLsizei n, const GLuint *programs);
typedef void (APIENTRY * PGLFNGENPROGRAMSARBPROC) (GLsizei n, GLuint *programs);
typedef void (APIENTRY * PGLFNPROGRAMENVPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * PGLFNPROGRAMENVPARAMETER4DVARBPROC) (GLenum target, GLuint index, const GLdouble *params);
typedef void (APIENTRY * PGLFNPROGRAMENVPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * PGLFNPROGRAMENVPARAMETER4FVARBPROC) (GLenum target, GLuint index, const GLfloat *params);
typedef void (APIENTRY * PGLFNPROGRAMLOCALPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * PGLFNPROGRAMLOCALPARAMETER4DVARBPROC) (GLenum target, GLuint index, const GLdouble *params);
typedef void (APIENTRY * PGLFNPROGRAMLOCALPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * PGLFNPROGRAMLOCALPARAMETER4FVARBPROC) (GLenum target, GLuint index, const GLfloat *params);
typedef void (APIENTRY * PGLFNGETPROGRAMENVPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (APIENTRY * PGLFNGETPROGRAMENVPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (APIENTRY * PGLFNGETPROGRAMLOCALPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (APIENTRY * PGLFNGETPROGRAMLOCALPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (APIENTRY * PGLFNGETPROGRAMIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETPROGRAMSTRINGARBPROC) (GLenum target, GLenum pname, GLvoid *string);
typedef void (APIENTRY * PGLFNGETVERTEXATTRIBDVARBPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRY * PGLFNGETVERTEXATTRIBFVARBPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PGLFNGETVERTEXATTRIBIVARBPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETVERTEXATTRIBPOINTERVARBPROC) (GLuint index, GLenum pname, GLvoid* *pointer);
typedef GLboolean (APIENTRY * PGLFNISPROGRAMARBPROC) (GLuint program);
#ifndef GL_ARB_fragment_program
#define GL_ARB_fragment_program 1
#define GL_FRAGMENT_PROGRAM_ARB           0x8804
#define GL_PROGRAM_ALU_INSTRUCTIONS_ARB   0x8805
#define GL_PROGRAM_TEX_INSTRUCTIONS_ARB   0x8806
#define GL_PROGRAM_TEX_INDIRECTIONS_ARB   0x8807
#define GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB 0x8808
#define GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB 0x8809
#define GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB 0x880A
#define GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB 0x880B
#define GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB 0x880C
#define GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB 0x880D
#define GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB 0x880E
#define GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB 0x880F
#define GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB 0x8810
#define GL_MAX_TEXTURE_COORDS_ARB         0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB    0x8872
/* All ARB_fragment_program entry points are shared with ARB_vertex_program. */
#endif
/* GL_ARB_vertex_buffer_object */
#ifndef GL_ARB_vertex_buffer_object
#define GL_BUFFER_SIZE_ARB                0x8764
#define GL_BUFFER_USAGE_ARB               0x8765
#define GL_ARRAY_BUFFER_ARB               0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB       0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB       0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB  0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB  0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB   0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB   0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB   0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB       0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB  0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB          0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB   0x889F
#define GL_READ_ONLY_ARB                  0x88B8
#define GL_WRITE_ONLY_ARB                 0x88B9
#define GL_READ_WRITE_ARB                 0x88BA
#define GL_BUFFER_ACCESS_ARB              0x88BB
#define GL_BUFFER_MAPPED_ARB              0x88BC
#define GL_BUFFER_MAP_POINTER_ARB         0x88BD
#define GL_STREAM_DRAW_ARB                0x88E0
#define GL_STREAM_READ_ARB                0x88E1
#define GL_STREAM_COPY_ARB                0x88E2
#define GL_STATIC_DRAW_ARB                0x88E4
#define GL_STATIC_READ_ARB                0x88E5
#define GL_STATIC_COPY_ARB                0x88E6
#define GL_DYNAMIC_DRAW_ARB               0x88E8
#define GL_DYNAMIC_READ_ARB               0x88E9
#define GL_DYNAMIC_COPY_ARB               0x88EA
#endif
typedef void (APIENTRY * PGLFNBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRY * PGLFNDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRY * PGLFNGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef GLboolean (APIENTRY * PGLFNISBUFFERARBPROC) (GLuint buffer);
typedef void (APIENTRY * PGLFNBUFFERDATAARBPROC) (GLenum target, ptrdiff_t size, const GLvoid *data, GLenum usage);
typedef void (APIENTRY * PGLFNBUFFERSUBDATAARBPROC) (GLenum target, ptrdiff_t offset, ptrdiff_t size, const GLvoid *data);
typedef void (APIENTRY * PGLFNGETBUFFERSUBDATAARBPROC) (GLenum target, ptrdiff_t offset, ptrdiff_t size, GLvoid *data);
typedef GLvoid* (APIENTRY * PGLFNMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRY * PGLFNUNMAPBUFFERARBPROC) (GLenum target);
typedef void (APIENTRY * PGLFNGETBUFFERPARAMETERIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETBUFFERPOINTERVARBPROC) (GLenum target, GLenum pname, GLvoid* *params);
/* GL_EXT_texture_compression_s3tc */
#ifndef GL_EXT_texture_compression_s3tc
#define GL_EXT_texture_compression_s3tc 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif
typedef void (APIENTRY * PGLFNCOMPRESSEDTEXIMAGE3DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PGLFNCOMPRESSEDTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PGLFNCOMPRESSEDTEXIMAGE1DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PGLFNCOMPRESSEDTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PGLFNCOMPRESSEDTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PGLFNCOMPRESSEDTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PGLFNGETCOMPRESSEDTEXIMAGEPROC) (GLenum target, GLint level, void *img);
/* GL_EXT_stencil_wrap */
#ifndef GL_EXT_stencil_wrap
#define GL_EXT_stencil_wrap 1
#define GL_INCR_WRAP_EXT                  0x8507
#define GL_DECR_WRAP_EXT                  0x8508
#endif
/* GL_NV_fog_distance */
#ifndef GL_NV_fog_distance
#define GL_NV_fog_distance 1
#define GL_FOG_DISTANCE_MODE_NV           0x855A
#define GL_EYE_RADIAL_NV                  0x855B
#define GL_EYE_PLANE_ABSOLUTE_NV          0x855C
/* reuse GL_EYE_PLANE */
#endif
/* GL_NV_texgen_reflection */
#ifndef GL_NV_texgen_reflection
#define GL_NV_texgen_reflection 1
#define GL_NORMAL_MAP_NV                  0x8511
#define GL_REFLECTION_MAP_NV              0x8512
#endif
/* GL_NV_register_combiners */
#ifndef GL_NV_register_combiners
#define GL_NV_register_combiners 1
#define GL_REGISTER_COMBINERS_NV          0x8522
#define GL_VARIABLE_A_NV                  0x8523
#define GL_VARIABLE_B_NV                  0x8524
#define GL_VARIABLE_C_NV                  0x8525
#define GL_VARIABLE_D_NV                  0x8526
#define GL_VARIABLE_E_NV                  0x8527
#define GL_VARIABLE_F_NV                  0x8528
#define GL_VARIABLE_G_NV                  0x8529
#define GL_CONSTANT_COLOR0_NV             0x852A
#define GL_CONSTANT_COLOR1_NV             0x852B
#define GL_PRIMARY_COLOR_NV               0x852C
#define GL_SECONDARY_COLOR_NV             0x852D
#define GL_SPARE0_NV                      0x852E
#define GL_SPARE1_NV                      0x852F
#define GL_DISCARD_NV                     0x8530
#define GL_E_TIMES_F_NV                   0x8531
#define GL_SPARE0_PLUS_SECONDARY_COLOR_NV 0x8532
#define GL_UNSIGNED_IDENTITY_NV           0x8536
#define GL_UNSIGNED_INVERT_NV             0x8537
#define GL_EXPAND_NORMAL_NV               0x8538
#define GL_EXPAND_NEGATE_NV               0x8539
#define GL_HALF_BIAS_NORMAL_NV            0x853A
#define GL_HALF_BIAS_NEGATE_NV            0x853B
#define GL_SIGNED_IDENTITY_NV             0x853C
#define GL_SIGNED_NEGATE_NV               0x853D
#define GL_SCALE_BY_TWO_NV                0x853E
#define GL_SCALE_BY_FOUR_NV               0x853F
#define GL_SCALE_BY_ONE_HALF_NV           0x8540
#define GL_BIAS_BY_NEGATIVE_ONE_HALF_NV   0x8541
#define GL_COMBINER_INPUT_NV              0x8542
#define GL_COMBINER_MAPPING_NV            0x8543
#define GL_COMBINER_COMPONENT_USAGE_NV    0x8544
#define GL_COMBINER_AB_DOT_PRODUCT_NV     0x8545
#define GL_COMBINER_CD_DOT_PRODUCT_NV     0x8546
#define GL_COMBINER_MUX_SUM_NV            0x8547
#define GL_COMBINER_SCALE_NV              0x8548
#define GL_COMBINER_BIAS_NV               0x8549
#define GL_COMBINER_AB_OUTPUT_NV          0x854A
#define GL_COMBINER_CD_OUTPUT_NV          0x854B
#define GL_COMBINER_SUM_OUTPUT_NV         0x854C
#define GL_MAX_GENERAL_COMBINERS_NV       0x854D
#define GL_NUM_GENERAL_COMBINERS_NV       0x854E
#define GL_COLOR_SUM_CLAMP_NV             0x854F
#define GL_COMBINER0_NV                   0x8550
#define GL_COMBINER1_NV                   0x8551
#define GL_COMBINER2_NV                   0x8552
#define GL_COMBINER3_NV                   0x8553
#define GL_COMBINER4_NV                   0x8554
#define GL_COMBINER5_NV                   0x8555
#define GL_COMBINER6_NV                   0x8556
#define GL_COMBINER7_NV                   0x8557
/* reuse GL_TEXTURE0_ARB */
/* reuse GL_TEXTURE1_ARB */
/* reuse GL_ZERO */
/* reuse GL_NONE */
/* reuse GL_FOG */
#endif
typedef void (APIENTRY * PGLFNCOMBINERPARAMETERFVNVPROC) (GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PGLFNCOMBINERPARAMETERFNVPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRY * PGLFNCOMBINERPARAMETERIVNVPROC) (GLenum pname, const GLint *params);
typedef void (APIENTRY * PGLFNCOMBINERPARAMETERINVPROC) (GLenum pname, GLint param);
typedef void (APIENTRY * PGLFNCOMBINERINPUTNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
typedef void (APIENTRY * PGLFNCOMBINEROUTPUTNVPROC) (GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum);
typedef void (APIENTRY * PGLFNFINALCOMBINERINPUTNVPROC) (GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
typedef void (APIENTRY * PGLFNGETCOMBINERINPUTPARAMETERFVNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PGLFNGETCOMBINERINPUTPARAMETERIVNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETCOMBINEROUTPUTPARAMETERFVNVPROC) (GLenum stage, GLenum portion, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PGLFNGETCOMBINEROUTPUTPARAMETERIVNVPROC) (GLenum stage, GLenum portion, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETFINALCOMBINERINPUTPARAMETERFVNVPROC) (GLenum variable, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PGLFNGETFINALCOMBINERINPUTPARAMETERIVNVPROC) (GLenum variable, GLenum pname, GLint *params);
/* GL_NV_register_combiners2 */
#ifndef GL_NV_register_combiners2
#define GL_NV_register_combiners2 1
#define GL_PER_STAGE_CONSTANTS_NV         0x8535
#endif
typedef void (APIENTRY * PGLFNCOMBINERSTAGEPARAMETERFVNVPROC) (GLenum stage, GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PGLFNGETCOMBINERSTAGEPARAMETERFVNVPROC) (GLenum stage, GLenum pname, GLfloat *params);
/* GL_NV_texture_shader */
#ifndef GL_NV_texture_shader
#define GL_NV_texture_shader 1
#define GL_OFFSET_TEXTURE_RECTANGLE_NV    0x864C
#define GL_OFFSET_TEXTURE_RECTANGLE_SCALE_NV 0x864D
#define GL_DOT_PRODUCT_TEXTURE_RECTANGLE_NV 0x864E
#define GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV 0x86D9
#define GL_UNSIGNED_INT_S8_S8_8_8_NV      0x86DA
#define GL_UNSIGNED_INT_8_8_S8_S8_REV_NV  0x86DB
#define GL_DSDT_MAG_INTENSITY_NV          0x86DC
#define GL_SHADER_CONSISTENT_NV           0x86DD
#define GL_TEXTURE_SHADER_NV              0x86DE
#define GL_SHADER_OPERATION_NV            0x86DF
#define GL_CULL_MODES_NV                  0x86E0
#define GL_OFFSET_TEXTURE_MATRIX_NV       0x86E1
#define GL_OFFSET_TEXTURE_SCALE_NV        0x86E2
#define GL_OFFSET_TEXTURE_BIAS_NV         0x86E3
#define GL_OFFSET_TEXTURE_2D_MATRIX_NV    GL_OFFSET_TEXTURE_MATRIX_NV
#define GL_OFFSET_TEXTURE_2D_SCALE_NV     GL_OFFSET_TEXTURE_SCALE_NV
#define GL_OFFSET_TEXTURE_2D_BIAS_NV      GL_OFFSET_TEXTURE_BIAS_NV
#define GL_PREVIOUS_TEXTURE_INPUT_NV      0x86E4
#define GL_CONST_EYE_NV                   0x86E5
#define GL_PASS_THROUGH_NV                0x86E6
#define GL_CULL_FRAGMENT_NV               0x86E7
#define GL_OFFSET_TEXTURE_2D_NV           0x86E8
#define GL_DEPENDENT_AR_TEXTURE_2D_NV     0x86E9
#define GL_DEPENDENT_GB_TEXTURE_2D_NV     0x86EA
#define GL_DOT_PRODUCT_NV                 0x86EC
#define GL_DOT_PRODUCT_DEPTH_REPLACE_NV   0x86ED
#define GL_DOT_PRODUCT_TEXTURE_2D_NV      0x86EE
#define GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV 0x86F0
#define GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV 0x86F1
#define GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV 0x86F2
#define GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV 0x86F3
#define GL_HILO_NV                        0x86F4
#define GL_DSDT_NV                        0x86F5
#define GL_DSDT_MAG_NV                    0x86F6
#define GL_DSDT_MAG_VIB_NV                0x86F7
#define GL_HILO16_NV                      0x86F8
#define GL_SIGNED_HILO_NV                 0x86F9
#define GL_SIGNED_HILO16_NV               0x86FA
#define GL_SIGNED_RGBA_NV                 0x86FB
#define GL_SIGNED_RGBA8_NV                0x86FC
#define GL_SIGNED_RGB_NV                  0x86FE
#define GL_SIGNED_RGB8_NV                 0x86FF
#define GL_SIGNED_LUMINANCE_NV            0x8701
#define GL_SIGNED_LUMINANCE8_NV           0x8702
#define GL_SIGNED_LUMINANCE_ALPHA_NV      0x8703
#define GL_SIGNED_LUMINANCE8_ALPHA8_NV    0x8704
#define GL_SIGNED_ALPHA_NV                0x8705
#define GL_SIGNED_ALPHA8_NV               0x8706
#define GL_SIGNED_INTENSITY_NV            0x8707
#define GL_SIGNED_INTENSITY8_NV           0x8708
#define GL_DSDT8_NV                       0x8709
#define GL_DSDT8_MAG8_NV                  0x870A
#define GL_DSDT8_MAG8_INTENSITY8_NV       0x870B
#define GL_SIGNED_RGB_UNSIGNED_ALPHA_NV   0x870C
#define GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV 0x870D
#define GL_HI_SCALE_NV                    0x870E
#define GL_LO_SCALE_NV                    0x870F
#define GL_DS_SCALE_NV                    0x8710
#define GL_DT_SCALE_NV                    0x8711
#define GL_MAGNITUDE_SCALE_NV             0x8712
#define GL_VIBRANCE_SCALE_NV              0x8713
#define GL_HI_BIAS_NV                     0x8714
#define GL_LO_BIAS_NV                     0x8715
#define GL_DS_BIAS_NV                     0x8716
#define GL_DT_BIAS_NV                     0x8717
#define GL_MAGNITUDE_BIAS_NV              0x8718
#define GL_VIBRANCE_BIAS_NV               0x8719
#define GL_TEXTURE_BORDER_VALUES_NV       0x871A
#define GL_TEXTURE_HI_SIZE_NV             0x871B
#define GL_TEXTURE_LO_SIZE_NV             0x871C
#define GL_TEXTURE_DS_SIZE_NV             0x871D
#define GL_TEXTURE_DT_SIZE_NV             0x871E
#define GL_TEXTURE_MAG_SIZE_NV            0x871F
#endif
/* GL_NV_texture_shader2 */
#ifndef GL_NV_texture_shader2
#define GL_NV_texture_shader2 1
#define GL_DOT_PRODUCT_TEXTURE_3D_NV      0x86EF
#endif
/* GL_NV_texture_shader3 */
#ifndef GL_NV_texture_shader3
#define GL_NV_texture_shader3 1
#define GL_OFFSET_PROJECTIVE_TEXTURE_2D_NV 0x8850
#define GL_OFFSET_PROJECTIVE_TEXTURE_2D_SCALE_NV 0x8851
#define GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_NV 0x8852
#define GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_SCALE_NV 0x8853
#define GL_OFFSET_HILO_TEXTURE_2D_NV      0x8854
#define GL_OFFSET_HILO_TEXTURE_RECTANGLE_NV 0x8855
#define GL_OFFSET_HILO_PROJECTIVE_TEXTURE_2D_NV 0x8856
#define GL_OFFSET_HILO_PROJECTIVE_TEXTURE_RECTANGLE_NV 0x8857
#define GL_DEPENDENT_HILO_TEXTURE_2D_NV   0x8858
#define GL_DEPENDENT_RGB_TEXTURE_3D_NV    0x8859
#define GL_DEPENDENT_RGB_TEXTURE_CUBE_MAP_NV 0x885A
#define GL_DOT_PRODUCT_PASS_THROUGH_NV    0x885B
#define GL_DOT_PRODUCT_TEXTURE_1D_NV      0x885C
#define GL_DOT_PRODUCT_AFFINE_DEPTH_REPLACE_NV 0x885D
#define GL_HILO8_NV                       0x885E
#define GL_SIGNED_HILO8_NV                0x885F
#define GL_FORCE_BLUE_TO_ONE_NV           0x8860
#endif
/* GL_ATI_texture_env_combine3 */
#ifndef GL_ATI_texture_env_combine3
#define GL_ATI_texture_env_combine3 1
#define GL_MODULATE_ADD_ATI               0x8744
#define GL_MODULATE_SIGNED_ADD_ATI        0x8745
#define GL_MODULATE_SUBTRACT_ATI          0x8746
/* #define ONE */
/* #define ZERO */
#endif

/* Point sprites */
#ifndef GL_ARB_point_sprite
#define GL_ARB_point_sprite 1
#define GL_POINT_SPRITE_ARB               0x8861
#define GL_COORD_REPLACE_ARB              0x8862
#endif

/* TODO: GL_NV_point_sprite */

/* Occlusion Queries */

typedef void (APIENTRY * PGLFNGENQUERIESARBPROC) (GLsizei n, GLuint *queries);
typedef void (APIENTRY * PGLFNDELETEQUERIESARBPROC) (GLsizei n, const GLuint *queries);
typedef GLboolean (APIENTRY * PGLFNISQUERYARBPROC) (GLuint query);
typedef void (APIENTRY * PGLFNBEGINQUERYARBPROC) (GLenum target, GLuint query);
typedef void (APIENTRY * PGLFNENDQUERYARBPROC) (GLenum target);
typedef void (APIENTRY * PGLFNGETQUERYIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETQUERYOBJECTIVARBPROC) (GLuint query, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETQUERYOBJECTUIVARBPROC) (GLuint query, GLenum pname, GLuint *params);

#ifndef GL_ARB_occlusion_query
#define GL_ARB_occlusion_query 1

#define GL_SAMPLES_PASSED_ARB                             0x8914
#define GL_QUERY_COUNTER_BITS_ARB                         0x8864
#define GL_CURRENT_QUERY_ARB                              0x8865
#define GL_QUERY_RESULT_ARB                               0x8866
#define GL_QUERY_RESULT_AVAILABLE_ARB                     0x8867
#endif

typedef void (APIENTRY * PGLFNGENOCCLUSIONQUERIESNVPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRY * PGLFNDELETEOCCLUSIONQUERIESNVPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (APIENTRY * PGLFNISOCCLUSIONQUERYNVPROC) (GLuint id);
typedef void (APIENTRY * PGLFNBEGINOCCLUSIONQUERYNVPROC) (GLuint id);
typedef void (APIENTRY * PGLFNENDOCCLUSIONQUERYNVPROC) (void);
typedef void (APIENTRY * PGLFNGETOCCLUSIONQUERYIVNVPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (APIENTRY * PGLFNGETOCCLUSIONQUERYUIVNVPROC) (GLuint id, GLenum pname, GLuint *params);

/* GL_HP_occlusion_test isn't complete, but it's constants are used by GL_NV_occlusion_query */
#ifndef GL_HP_occlusion_test
#define GL_HP_occlusion_test 1
#define GL_OCCLUSION_TEST_HP                 0x8165
#define GL_OCCLUSION_TEST_RESULT_HP          0x8165
#endif


#ifndef GL_NV_occlusion_query
#define GL_NV_occlusion_query 1
#define GL_PIXEL_COUNTER_BITS_NV          0x8864
#define GL_CURRENT_OCCLUSION_QUERY_ID_NV  0x8865
#define GL_PIXEL_COUNT_NV                 0x8866
#define GL_PIXEL_COUNT_AVAILABLE_NV       0x8867
#endif



/****************************************************
 * OpenGL Official Version 
 *  defines 
 ****************************************************/
/* GL_VERSION_1_3 */
#if !defined(GL_DOT3_RGBA)
# define GL_DOT3_RGBA                     0x8741
#endif
#if !defined(GL_SUBTRACT)
# define GL_SUBTRACT                      0x84E7
#endif


/****************************************************
 * OpenGL GLX Extensions
 *  defines and functions pointer
 ****************************************************/



/****************************************************
 * OpenGL GLX Official Version
 *  defines and functions pointer
 ****************************************************/
/* GLX_VERSION_1_3 */
typedef GLXFBConfig * (APIENTRY * PGLXFNGLXGETFBCONFIGSPROC) (Display *dpy, int screen, int *nelements);
typedef GLXFBConfig * (APIENTRY * PGLXFNGLXCHOOSEFBCONFIGPROC) (Display *dpy, int screen, const int *attrib_list, int *nelements);
typedef int           (APIENTRY * PGLXFNGLXGETFBCONFIGATTRIBPROC) (Display *dpy, GLXFBConfig config, int attribute, int *value);
typedef XVisualInfo * (APIENTRY * PGLXFNGLXGETVISUALFROMFBCONFIGPROC) (Display *dpy, GLXFBConfig config);
typedef GLXWindow     (APIENTRY * PGLXFNGLXCREATEWINDOWPROC) (Display *dpy, GLXFBConfig config, Window win, const int *attrib_list);
typedef void          (APIENTRY * PGLXFNGLXDESTROYWINDOWPROC) (Display *dpy, GLXWindow win);
typedef GLXPixmap     (APIENTRY * PGLXFNGLXCREATEPIXMAPPROC) (Display *dpy, GLXFBConfig config, Pixmap pixmap, const int *attrib_list);
typedef void          (APIENTRY * PGLXFNGLXDESTROYPIXMAPPROC) (Display *dpy, GLXPixmap pixmap);
typedef GLXPbuffer    (APIENTRY * PGLXFNGLXCREATEPBUFFERPROC) (Display *dpy, GLXFBConfig config, const int *attrib_list);
typedef void          (APIENTRY * PGLXFNGLXDESTROYPBUFFERPROC) (Display *dpy, GLXPbuffer pbuf);
typedef void          (APIENTRY * PGLXFNGLXQUERYDRAWABLEPROC) (Display *dpy, GLXDrawable draw, int attribute, unsigned int *value);
typedef GLXContext    (APIENTRY * PGLXFNGLXCREATENEWCONTEXTPROC) (Display *dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct);
typedef Bool          (APIENTRY * PGLXFNGLXMAKECONTEXTCURRENTPROC) (Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx);
typedef GLXDrawable   (APIENTRY * PGLXFNGLXGETCURRENTREADDRAWABLEPROC) (void);
typedef Display *     (APIENTRY * PGLXFNGLXGETCURRENTDISPLAYPROC) (void);
typedef int           (APIENTRY * PGLXFNGLXQUERYCONTEXTPROC) (Display *dpy, GLXContext ctx, int attribute, int *value);
typedef void          (APIENTRY * PGLXFNGLXSELECTEVENTPROC) (Display *dpy, GLXDrawable draw, unsigned long event_mask);
typedef void          (APIENTRY * PGLXFNGLXGETSELECTEDEVENTPROC) (Display *dpy, GLXDrawable draw, unsigned long *event_mask);


/****************************************************
 * Enumerated types
 ****************************************************/
typedef enum _GL_Vendors {
  VENDOR_WINE   = 0x0,
  VENDOR_MESA   = 0x1,
  VENDOR_ATI    = 0x1002,
  VENDOR_NVIDIA = 0x10de
} GL_Vendors;

typedef enum _GL_Cards {
  CARD_WINE  = 0x0,
  CARD_ATI_RADEON_8500 = 0x514c,
  CARD_ATI_RADEON_9700PRO = 0x4e44,
  CARD_ATI_RADEON_9800PRO = 0x4e48,
  CARD_NVIDIA_GEFORCE4_TI4600 = 0x0250,
  CARD_NVIDIA_GEFORCEFX_5900ULTRA = 0x0330
} GL_Cards;

typedef enum _GL_VSVersion {
  VS_VERSION_NOT_SUPPORTED = 0x0,
  VS_VERSION_10 = 0x10,
  VS_VERSION_11 = 0x11,
  VS_VERSION_20 = 0x20,
  VS_VERSION_30 = 0x30,
  /*Force 32-bits*/
  VS_VERSION_FORCE_DWORD = 0x7FFFFFFF
} GL_VSVersion;

typedef enum _GL_PSVersion {
  PS_VERSION_NOT_SUPPORTED = 0x0,
  PS_VERSION_10 = 0x10,
  PS_VERSION_11 = 0x11,
  PS_VERSION_12 = 0x12,
  PS_VERSION_13 = 0x13,
  PS_VERSION_14 = 0x14,
  PS_VERSION_20 = 0x20,
  PS_VERSION_30 = 0x30,
  /*Force 32-bits*/
  PS_VERSION_FORCE_DWORD = 0x7FFFFFFF
} GL_PSVersion;

#define MAKEDWORD_VERSION(maj, min)  ((maj & 0x0000FFFF) << 16) | (min & 0x0000FFFF)

/* OpenGL Supported Extensions (ARB and EXT) */
typedef enum _GL_SupportedExt {
  /* ARB */
  ARB_FRAGMENT_PROGRAM,
  ARB_MULTISAMPLE,
  ARB_MULTITEXTURE,
  ARB_OCCLUSION_QUERY,
  ARB_POINT_PARAMETERS,
  ARB_POINT_SPRITE,
  ARB_TEXTURE_COMPRESSION,
  ARB_TEXTURE_CUBE_MAP,
  ARB_TEXTURE_ENV_ADD,
  ARB_TEXTURE_ENV_COMBINE,
  ARB_TEXTURE_ENV_DOT3,
  ARB_TEXTURE_BORDER_CLAMP,
  ARB_TEXTURE_MIRRORED_REPEAT,
  ARB_VERTEX_PROGRAM,
  ARB_VERTEX_BLEND,
  ARB_VERTEX_BUFFER_OBJECT,
  /* EXT */
  EXT_FOG_COORD,
  EXT_PALETTED_TEXTURE,
  EXT_PIXEL_BUFFER_OBJECT,
  EXT_POINT_PARAMETERS,
  EXT_SECONDARY_COLOR,
  EXT_STENCIL_WRAP,
  EXT_TEXTURE_COMPRESSION_S3TC,
  EXT_TEXTURE_FILTER_ANISOTROPIC,
  EXT_TEXTURE_LOD,
  EXT_TEXTURE_LOD_BIAS,
  EXT_TEXTURE_ENV_ADD,
  EXT_TEXTURE_ENV_COMBINE,
  EXT_TEXTURE_ENV_DOT3,
  EXT_VERTEX_WEIGHTING,
  /* NVIDIA */
  NV_FOG_DISTANCE,
  NV_FRAGMENT_PROGRAM,
  NV_OCCLUSION_QUERY,
  NV_REGISTER_COMBINERS,
  NV_REGISTER_COMBINERS2,
  NV_TEXGEN_REFLECTION,
  NV_TEXTURE_ENV_COMBINE4,
  NV_TEXTURE_SHADER,
  NV_TEXTURE_SHADER2,
  NV_TEXTURE_SHADER3,
  NV_VERTEX_PROGRAM,
  /* ATI */
  ATI_TEXTURE_ENV_COMBINE3,
  ATI_TEXTURE_MIRROR_ONCE,
  EXT_VERTEX_SHADER,

  OPENGL_SUPPORTED_EXT_END
} GL_SupportedExt;


/****************************************************
 * #Defines       
 ****************************************************/
#define GL_EXT_FUNCS_GEN \
    /** ARB Extensions **/ \
    /* GL_ARB_texture_compression */ \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXIMAGE2DPROC,       glCompressedTexImage2DARB); \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXIMAGE3DPROC,       glCompressedTexImage3DARB); \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXSUBIMAGE2DPROC,    glCompressedTexSubImage2DARB); \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXSUBIMAGE3DPROC,    glCompressedTexSubImage3DARB); \
    USE_GL_FUNC(PGLFNGETCOMPRESSEDTEXIMAGEPROC,      glGetCompressedTexImageARB); \
    /* GL_ARB_vertex_blend */ \
    USE_GL_FUNC(PGLFNGLWEIGHTPOINTERARB,             glWeightPointerARB); \
    /* GL_ARB_vertex_buffer_object */ \
    USE_GL_FUNC(PGLFNBINDBUFFERARBPROC,              glBindBufferARB); \
    USE_GL_FUNC(PGLFNDELETEBUFFERSARBPROC,           glDeleteBuffersARB); \
    USE_GL_FUNC(PGLFNGENBUFFERSARBPROC,              glGenBuffersARB); \
    USE_GL_FUNC(PGLFNISBUFFERARBPROC,                glIsBufferARB); \
    USE_GL_FUNC(PGLFNBUFFERDATAARBPROC,              glBufferDataARB); \
    USE_GL_FUNC(PGLFNBUFFERSUBDATAARBPROC,           glBufferSubDataARB); \
    USE_GL_FUNC(PGLFNGETBUFFERSUBDATAARBPROC,        glGetBufferSubDataARB); \
    USE_GL_FUNC(PGLFNMAPBUFFERARBPROC,               glMapBufferARB); \
    USE_GL_FUNC(PGLFNUNMAPBUFFERARBPROC,             glUnmapBufferARB); \
    USE_GL_FUNC(PGLFNGETBUFFERPARAMETERIVARBPROC,    glGetBufferParameterivARB); \
    USE_GL_FUNC(PGLFNGETBUFFERPOINTERVARBPROC,       glGetBufferPointervARB); \
    /** EXT Extensions **/ \
    /* GL_EXT_fog_coord */ \
    /* GL_EXT_paletted_texture */ \
    USE_GL_FUNC(PGLFNGLCOLORTABLEEXTPROC,             glColorTableEXT); \
    /* GL_EXT_point_parameters */ \
    USE_GL_FUNC(PGLFNGLPOINTPARAMETERFEXTPROC,        glPointParameterfEXT); \
    USE_GL_FUNC(PGLFNGLPOINTPARAMETERFVEXTPROC,       glPointParameterfvEXT); \
    /* GL_EXT_secondary_color */ \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLOR3UBEXTPROC,      glSecondaryColor3ubEXT); \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLOR3FEXTPROC,       glSecondaryColor3fEXT); \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLOR3FVEXTPROC,      glSecondaryColor3fvEXT); \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLORPOINTEREXTPROC,  glSecondaryColorPointerEXT); \
    /* GL_EXT_secondary_color */ \
    USE_GL_FUNC(PGLFNGENPROGRAMSARBPROC,              glGenProgramsARB); \
    USE_GL_FUNC(PGLFNBINDPROGRAMARBPROC,              glBindProgramARB); \
    USE_GL_FUNC(PGLFNPROGRAMSTRINGARBPROC,            glProgramStringARB); \
    USE_GL_FUNC(PGLFNDELETEPROGRAMSARBPROC,           glDeleteProgramsARB); \
    USE_GL_FUNC(PGLFNPROGRAMENVPARAMETER4FVARBPROC,   glProgramEnvParameter4fvARB); \
    USE_GL_FUNC(PGLFNVERTEXATTRIBPOINTERARBPROC,      glVertexAttribPointerARB); \
    USE_GL_FUNC(PGLFNENABLEVERTEXATTRIBARRAYARBPROC,  glEnableVertexAttribArrayARB); \
    USE_GL_FUNC(PGLFNDISABLEVERTEXATTRIBARRAYARBPROC, glDisableVertexAttribArrayARB); \

#define GLX_EXT_FUNCS_GEN \
    /** GLX_VERSION_1_3 **/ \
    USE_GL_FUNC(PGLXFNGLXCREATEPBUFFERPROC,          glXCreatePbuffer); \
    USE_GL_FUNC(PGLXFNGLXDESTROYPBUFFERPROC,         glXDestroyPbuffer); \
    USE_GL_FUNC(PGLXFNGLXCREATEPIXMAPPROC,           glXCreatePixmap); \
    USE_GL_FUNC(PGLXFNGLXDESTROYPIXMAPPROC,          glXDestroyPixmap); \
    USE_GL_FUNC(PGLXFNGLXCREATENEWCONTEXTPROC,       glXCreateNewContext); \
    USE_GL_FUNC(PGLXFNGLXMAKECONTEXTCURRENTPROC,     glXMakeContextCurrent); \
    USE_GL_FUNC(PGLXFNGLXCHOOSEFBCONFIGPROC,         glXChooseFBConfig); \

#undef APIENTRY
#undef CALLBACK
#undef WINAPI

/* Redefines the constants */
#define CALLBACK    __stdcall
#define WINAPI      __stdcall
#define APIENTRY    WINAPI

/****************************************************
 * Structures       
 ****************************************************/
#define USE_GL_FUNC(type, pfn) type pfn;
typedef struct _WineD3D_GL_Info {

  DWORD  glx_version;
  DWORD  gl_version;

  GL_Vendors gl_vendor;
  GL_Cards   gl_card;
  DWORD  gl_driver_version;
  CHAR   gl_renderer[255];
  /**
   * CAPS Constants 
   */
  UINT   max_lights;
  UINT   max_textures;
  UINT   max_samplers;
  UINT   max_clipplanes;
  UINT   max_texture_size;
  float  max_pointsize;
  UINT   max_blends;
  UINT   max_anisotropy;

  GL_PSVersion ps_arb_version;
  GL_PSVersion ps_nv_version;

  GL_VSVersion vs_arb_version;
  GL_VSVersion vs_nv_version;
  GL_VSVersion vs_ati_version;

  BOOL supported[OPENGL_SUPPORTED_EXT_END + 1];

  /** OpenGL EXT and ARB functions ptr */
  GL_EXT_FUNCS_GEN;
  /** OpenGL GLX functions ptr */
  GLX_EXT_FUNCS_GEN;
  /**/
} WineD3D_GL_Info;
#undef USE_GL_FUNC

typedef struct _WineD3D_GLContext {
  GLXContext   glCtx; 
  XVisualInfo* visInfo;
  Display*     display;
  Drawable     drawable;
  LONG         ref;
} WineD3D_Context;

#endif /* HAVE_OPENGL */

#endif /* __WINE_WINED3D_GL */
