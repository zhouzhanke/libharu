/*
 * << Haru Free PDF Library >> -- hpdf_shading.c
 *
 * URL: http://libharu.org
 *
 * Copyright (c) 1999-2006 Takeshi Kanno <takeshi_kanno@est.hi-ho.ne.jp>
 * Copyright (c) 2007-2009 Antony Dovgal <tony@daylessday.org>
 * Copyright (c) 2017 Kitware <kitware@kitware.com>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 * It is provided "as is" without express or implied warranty.
 *
 */

#include "hpdf.h"
#include "hpdf_utils.h"

#include "assert.h"

typedef struct _RGBVertex
{
  HPDF_UINT8 EdgeFlag;
  HPDF_UINT32 X;
  HPDF_UINT32 Y;
  HPDF_UINT8 RGB[3];
} RGBVertex;

static const char *COL_CMYK = "DeviceCMYK";
static const char *COL_RGB = "DeviceRGB";
static const char *COL_GRAY = "DeviceGray";

/* bbox is filled with xMin, xMax, yMin, yMax */
static HPDF_BOOL _GetDecodeArrayVertexValues(HPDF_Shading shading,
                                             HPDF_REAL *bbox)
{
  HPDF_Array decodeArray;
  HPDF_Real r;
  int i;

  if (!shading) {
    return HPDF_FALSE;
  }

  decodeArray = (HPDF_Array)(HPDF_Dict_GetItem(shading, "Decode",
                                               HPDF_OCLASS_ARRAY));
  if (!decodeArray) {
    return HPDF_FALSE;
  }

  for (i = 0; i < 4; ++i)
  {
    r = HPDF_Array_GetItem(decodeArray, i, HPDF_OCLASS_REAL);
    if (!r) {
      return HPDF_FALSE;
    }

    bbox[i] = r->value;
  }

  return HPDF_TRUE;
}

static void UINT32Swap (HPDF_UINT32  *value)
{
  HPDF_BYTE b[4];

  HPDF_MemCpy (b, (HPDF_BYTE *)value, 4);
  *value = (HPDF_UINT32)((HPDF_UINT32)b[0] << 24 |
           (HPDF_UINT32)b[1] << 16 |
           (HPDF_UINT32)b[2] << 8 |
           (HPDF_UINT32)b[3]);
}

/* Encode a position coordinate for writing */
static HPDF_UINT32 _EncodeValue(HPDF_REAL x, HPDF_REAL xMin, HPDF_REAL xMax)
{
  HPDF_DOUBLE norm = (x - xMin) / (xMax - xMin);
  HPDF_DOUBLE max = (HPDF_DOUBLE)(0xFFFFFFFF);
  HPDF_UINT32 enc = (HPDF_UINT32)(norm * max);
  UINT32Swap(&enc);
  return enc;
}

HPDF_EXPORT(HPDF_Shading)
HPDF_Shading_New  (HPDF_Doc         pdf,
                   HPDF_ShadingType type,
                   HPDF_ColorSpace  colorSpace,
                   HPDF_REAL xMin, HPDF_REAL xMax,
                   HPDF_REAL yMin, HPDF_REAL yMax)
{
  HPDF_Shading shading;
  HPDF_Array decodeArray;
  HPDF_STATUS ret = HPDF_OK;
  int i;

  HPDF_PTRACE((" HPDF_Shading_New\n"));

  if (!HPDF_HasDoc(pdf)) {
    return NULL;
  }

  /* Validate shading type: */
  switch (type)
  {
    case HPDF_SHADING_FREE_FORM_TRIANGLE_MESH:
      break;

    default:
      HPDF_SetError (pdf->mmgr->error, HPDF_INVALID_SHADING_TYPE, 0);
      return NULL;
  }

  decodeArray = HPDF_Array_New(pdf->mmgr);
  if (!decodeArray) {
    return NULL;
  }

  /* X-range */
  ret += HPDF_Array_AddReal(decodeArray, xMin);
  ret += HPDF_Array_AddReal(decodeArray, xMax);

  /* Y-range */
  ret += HPDF_Array_AddReal(decodeArray, yMin);
  ret += HPDF_Array_AddReal(decodeArray, yMax);

  const char *colName = NULL;
  switch (colorSpace) {
    case HPDF_CS_DEVICE_RGB:
      colName = COL_RGB;
      for (i = 0; i < 3; ++i) {
        ret += HPDF_Array_AddReal(decodeArray, 0.0);
        ret += HPDF_Array_AddReal(decodeArray, 1.0);
      }
      break;

    default:
      HPDF_SetError(pdf->mmgr->error, HPDF_INVALID_COLOR_SPACE, 0);
      return NULL;
  }

  if (ret != HPDF_OK) {
    return NULL;
  }

  shading = HPDF_DictStream_New(pdf->mmgr, pdf->xref);
  if (!shading) {
    return NULL;
  }

  shading->header.obj_class |= HPDF_OSUBCLASS_SHADING;
  ret += HPDF_Dict_AddNumber(shading, "ShadingType", type);
  ret += HPDF_Dict_AddName(shading, "ColorSpace", colName);

  switch (type)
  {
    case HPDF_SHADING_FREE_FORM_TRIANGLE_MESH:
      ret += HPDF_Dict_AddNumber(shading, "BitsPerCoordinate", 32);
      ret += HPDF_Dict_AddNumber(shading, "BitsPerComponent", 8);
      ret += HPDF_Dict_AddNumber(shading, "BitsPerFlag", 8);
      ret += HPDF_Dict_Add(shading, "Decode", decodeArray);
      break;

    default:
      HPDF_SetError (pdf->mmgr->error, HPDF_INVALID_SHADING_TYPE, 0);
      return NULL;
  }

  if (ret != HPDF_OK) {
    return NULL;
  }

  return shading;
}

HPDF_EXPORT(HPDF_STATUS)
HPDF_Shading_AddVertexRGB(HPDF_Shading shading,
                          HPDF_Shading_FreeFormTriangleMeshEdgeFlag edgeFlag,
                          HPDF_REAL x, HPDF_REAL y,
                          HPDF_UINT8 r, HPDF_UINT8 g, HPDF_UINT8 b)
{
  HPDF_STATUS ret = HPDF_OK;
  RGBVertex vert;
  float bbox[4];

  HPDF_PTRACE((" HPDF_Shading_AddVertexRGB\n"));

  if (!shading) {
    return HPDF_INVALID_OBJECT;
  }

  if (_GetDecodeArrayVertexValues(shading, bbox) != HPDF_TRUE) {
    return HPDF_SetError(shading->error, HPDF_INVALID_OBJECT, 0);
  }

  vert.EdgeFlag = (HPDF_UINT8)edgeFlag;
  vert.X = _EncodeValue(x, bbox[0], bbox[1]);
  vert.Y = _EncodeValue(y, bbox[2], bbox[3]);
  vert.RGB[0] = r;
  vert.RGB[1] = g;
  vert.RGB[2] = b;

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.EdgeFlag), sizeof(vert.EdgeFlag));
  if (ret != HPDF_OK)
  {
    return ret;
  }

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.X), sizeof(vert.X));
  if (ret != HPDF_OK)
  {
    return ret;
  }

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.Y), sizeof(vert.Y));
  if (ret != HPDF_OK)
  {
    return ret;
  }

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.RGB), sizeof(vert.RGB));

  return ret;
}
//----------------------------------------------------------------------------
// shading type 2, function type 2(only option), color space RGB(only option),
// shading from pointA to pointB, start with RGB C0 and end with RGB C1

// element, Domain [0 1], Extend [false false], are set as deafult

// x = x coords, y = y coords
// N = varible in function type 2 (interpolation_exponent)

//  0 <= C0 & C1 <= 1
//  0 < N
HPDF_EXPORT(HPDF_Shading)
HPDF_Shading_Type2 (HPDF_Doc         pdf,
                    HPDF_REAL PointA_x, HPDF_REAL PointA_y,
                    HPDF_REAL PointB_x, HPDF_REAL PointB_y,
                    HPDF_REAL C0_R, HPDF_REAL C0_G, HPDF_REAL C0_B,
                    HPDF_REAL C1_R, HPDF_REAL C1_G, HPDF_REAL C1_B,
                    HPDF_REAL N)
{
  HPDF_Shading shading;
  HPDF_Array C0, C1, Domain, Coords;
  HPDF_STATUS ret = HPDF_OK;
  HPDF_STATUS ret2 = HPDF_OK;

  HPDF_PTRACE((" HPDF_Shading_Type_2\n"));

  // input error handle
  if(C0_R < 0 || C0_R >1 || C0_G < 0 || C0_G >1 || C0_B < 0 || C0_B >1 ||
     C1_R < 0 || C1_R >1 || C1_G < 0 || C1_G >1 || C1_B < 0 || C1_B >1 ||
     N <= 0)
  {
      HPDF_SetError (pdf->mmgr->error, HPDF_REAL_OUT_OF_RANGE, 0);
      return NULL;
  }

  if (!HPDF_HasDoc(pdf)) {
    return NULL;
  }

  // generate array
  C0 = HPDF_Array_New(pdf->mmgr);
  C1 = HPDF_Array_New(pdf->mmgr);
  Domain = HPDF_Array_New(pdf->mmgr);
  Coords = HPDF_Array_New(pdf->mmgr);

  // collect elements into array for shading pattern
  const char *colName = NULL;
  colName = COL_RGB;
  ret += HPDF_Array_AddReal(C0, C0_R);
  ret += HPDF_Array_AddReal(C0, C0_G);
  ret += HPDF_Array_AddReal(C0, C0_B);
  ret += HPDF_Array_AddReal(C1, C1_R);
  ret += HPDF_Array_AddReal(C1, C1_G);
  ret += HPDF_Array_AddReal(C1, C1_B);
  ret += HPDF_Array_AddReal(Domain, 0);
  ret += HPDF_Array_AddReal(Domain, 1);
  ret += HPDF_Array_AddReal(Coords, PointA_x);
  ret += HPDF_Array_AddReal(Coords, PointA_y);
  ret += HPDF_Array_AddReal(Coords, PointB_x);
  ret += HPDF_Array_AddReal(Coords, PointB_y);

  if (ret != HPDF_OK) {
    return NULL;
  }

  shading = HPDF_DictStream_New(pdf->mmgr, pdf->xref);
  if (!shading) {
    return NULL;
  }

  // generate shading pattern
  shading->header.obj_class |= HPDF_OSUBCLASS_SHADING;
  ret += HPDF_Dict_AddNumber(shading, "ShadingType", 2);
  ret += HPDF_Dict_AddName(shading, "ColorSpace", colName);
  ret += HPDF_Dict_Add(shading, "Coords", Coords);

  HPDF_Dict functions;
  functions = HPDF_Dict_New (pdf->mmgr);
  HPDF_Dict_Add (shading, "Function", functions);

  ret += HPDF_Dict_AddNumber(functions, "FunctionType", 2);
  ret += HPDF_Dict_AddReal(functions, "N", N);
  ret += HPDF_Dict_Add(functions, "Domain", Domain);
  ret += HPDF_Dict_Add(functions, "C0", C0);
  ret += HPDF_Dict_Add(functions, "C1", C1);


  if (ret != HPDF_OK) {
    return NULL;
  }

  return shading;
}

//----------------------------------------------------------------------------
// shading type 3, function type 2(only option), color space RGB(only option),
// shading from circleA to circleB, start with RGB C0 and end with RGB C1

// element, Domain [0 1], Extend [false false], are set as deafult

// x = x coords, y = y coords, r = radius
// N = varible in function type 2 (interpolation_exponent)

//  0 <= C0 & C1 <= 1
//  0 < N
HPDF_EXPORT(HPDF_Shading)
HPDF_Shading_Type3 (HPDF_Doc         pdf,
                    HPDF_REAL PointA_x, HPDF_REAL PointA_y, HPDF_REAL PointA_r,
                    HPDF_REAL PointB_x, HPDF_REAL PointB_y, HPDF_REAL PointB_r,
                    HPDF_REAL C0_R, HPDF_REAL C0_G, HPDF_REAL C0_B,
                    HPDF_REAL C1_R, HPDF_REAL C1_G, HPDF_REAL C1_B,
                    HPDF_REAL N)
{
  HPDF_Shading shading;
  HPDF_Array C0, C1, Domain, Coords;
  HPDF_STATUS ret = HPDF_OK;
  HPDF_STATUS ret2 = HPDF_OK;

  HPDF_PTRACE((" HPDF_Shading_Type_2\n"));
  // input error handle
  if(C0_R < 0 || C0_R >1 || C0_G < 0 || C0_G >1 || C0_B < 0 || C0_B >1 ||
     C1_R < 0 || C1_R >1 || C1_G < 0 || C1_G >1 || C1_B < 0 || C1_B >1 ||
     N <= 0)
  {
      HPDF_SetError (pdf->mmgr->error, HPDF_REAL_OUT_OF_RANGE, 0);
      return NULL;
  }

  if (!HPDF_HasDoc(pdf)) {
    return NULL;
  }

  // generate array
  C0 = HPDF_Array_New(pdf->mmgr);
  C1 = HPDF_Array_New(pdf->mmgr);
  Domain = HPDF_Array_New(pdf->mmgr);
  Coords = HPDF_Array_New(pdf->mmgr);

  // collect elements into array for shading pattern
  const char *colName = NULL;
  colName = COL_RGB;
  ret += HPDF_Array_AddReal(C0, C0_R);
  ret += HPDF_Array_AddReal(C0, C0_G);
  ret += HPDF_Array_AddReal(C0, C0_B);
  ret += HPDF_Array_AddReal(C1, C1_R);
  ret += HPDF_Array_AddReal(C1, C1_G);
  ret += HPDF_Array_AddReal(C1, C1_B);
  ret += HPDF_Array_AddReal(Domain, 0);
  ret += HPDF_Array_AddReal(Domain, 1);
  ret += HPDF_Array_AddReal(Coords, PointA_x);
  ret += HPDF_Array_AddReal(Coords, PointA_y);
  ret += HPDF_Array_AddReal(Coords, PointA_r);
  ret += HPDF_Array_AddReal(Coords, PointB_x);
  ret += HPDF_Array_AddReal(Coords, PointB_y);
  ret += HPDF_Array_AddReal(Coords, PointB_r);

  if (ret != HPDF_OK) {
    return NULL;
  }

  shading = HPDF_DictStream_New(pdf->mmgr, pdf->xref);
  if (!shading) {
    return NULL;
  }

  // generate shading pattern
  shading->header.obj_class |= HPDF_OSUBCLASS_SHADING;
  ret += HPDF_Dict_AddNumber(shading, "ShadingType", 3);
  ret += HPDF_Dict_AddName(shading, "ColorSpace", colName);
  ret += HPDF_Dict_Add(shading, "Coords", Coords);

  HPDF_Dict functions;
  functions = HPDF_Dict_New (pdf->mmgr);
  HPDF_Dict_Add (shading, "Function", functions);

  ret += HPDF_Dict_AddNumber(functions, "FunctionType", 2);
  ret += HPDF_Dict_AddReal(functions, "N", N);
  ret += HPDF_Dict_Add(functions, "Domain", Domain);
  ret += HPDF_Dict_Add(functions, "C0", C0);
  ret += HPDF_Dict_Add(functions, "C1", C1);


  if (ret != HPDF_OK) {
    return NULL;
  }

  return shading;
}

//----------------------------------------------------------------------------
// shading type 4, color space RGB(only option),
HPDF_EXPORT(HPDF_Shading)
HPDF_Shading_Type4 (HPDF_Doc         pdf,
                   HPDF_REAL xMin, HPDF_REAL xMax,
                   HPDF_REAL yMin, HPDF_REAL yMax)
{
  HPDF_Shading shading;
  HPDF_Array decodeArray;
  HPDF_STATUS ret = HPDF_OK;
  int i;

  HPDF_PTRACE((" HPDF_Shading_New\n"));

  if (!HPDF_HasDoc(pdf)) {
    return NULL;
  }


  decodeArray = HPDF_Array_New(pdf->mmgr);
  if (!decodeArray) {
    return NULL;
  }

  /* X-range */
  ret += HPDF_Array_AddReal(decodeArray, xMin);
  ret += HPDF_Array_AddReal(decodeArray, xMax);

  /* Y-range */
  ret += HPDF_Array_AddReal(decodeArray, yMin);
  ret += HPDF_Array_AddReal(decodeArray, yMax);

  const char *colName = NULL;
  colName = COL_RGB;
  for (i = 0; i < 3; ++i) {
    ret += HPDF_Array_AddReal(decodeArray, 0.0);
    ret += HPDF_Array_AddReal(decodeArray, 1.0);
  }

  if (ret != HPDF_OK) {
    return NULL;
  }

  shading = HPDF_DictStream_New(pdf->mmgr, pdf->xref);
  if (!shading) {
    return NULL;
  }

  shading->header.obj_class |= HPDF_OSUBCLASS_SHADING;
  ret += HPDF_Dict_AddNumber(shading, "ShadingType", 4);
  ret += HPDF_Dict_AddName(shading, "ColorSpace", colName);

  ret += HPDF_Dict_AddNumber(shading, "BitsPerCoordinate", 32);
  ret += HPDF_Dict_AddNumber(shading, "BitsPerComponent", 8);
  ret += HPDF_Dict_AddNumber(shading, "BitsPerFlag", 8);
  ret += HPDF_Dict_Add(shading, "Decode", decodeArray);

  if (ret != HPDF_OK) {
    return NULL;
  }

  return shading;
}

// add vertex to shading pattern 4
// edge flag,
// f0 = require 3 vertex, A, B, C
// f1 = shading from BC to new vertex
// f2 = shading from CA (or AC) to new vertex
HPDF_EXPORT(HPDF_STATUS)
HPDF_Shading_Type4_AddVertexRGB(
                          HPDF_Shading shading,
                          HPDF_Shading_Type4_Flag edgeFlag,
                          HPDF_REAL x, HPDF_REAL y,
                          HPDF_REAL R, HPDF_REAL G, HPDF_REAL B)
{
  HPDF_STATUS ret = HPDF_OK;
  RGBVertex vert;
  float bbox[4];

  HPDF_PTRACE((" HPDF_Shading_AddVertexRGB\n"));
  // input error handle
  if(R < 0 || R >1 || G < 0 || G >1 || B < 0 || B >1 )
  {
      return HPDF_RaiseError(shading->error, HPDF_REAL_OUT_OF_RANGE, 0);
  }

  // transfer 1 bit to 8 bit RGB
  HPDF_UINT8 r = R * 255;
  HPDF_UINT8 g = G * 255;
  HPDF_UINT8 b = B * 255;

  if (!shading) {
    return HPDF_INVALID_OBJECT;
  }

  if (_GetDecodeArrayVertexValues(shading, bbox) != HPDF_TRUE) {
    return HPDF_SetError(shading->error, HPDF_INVALID_OBJECT, 0);
  }

  vert.EdgeFlag = (HPDF_UINT8)edgeFlag;
  vert.X = _EncodeValue(x, bbox[0], bbox[1]);
  vert.Y = _EncodeValue(y, bbox[2], bbox[3]);
  vert.RGB[0] = r;
  vert.RGB[1] = g;
  vert.RGB[2] = b;

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.EdgeFlag), sizeof(vert.EdgeFlag));
  if (ret != HPDF_OK)
  {
    return ret;
  }

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.X), sizeof(vert.X));
  if (ret != HPDF_OK)
  {
    return ret;
  }

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.Y), sizeof(vert.Y));
  if (ret != HPDF_OK)
  {
    return ret;
  }

  ret = HPDF_Stream_Write(shading->stream,
                          (HPDF_BYTE*)(&vert.RGB), sizeof(vert.RGB));

  return ret;
}
