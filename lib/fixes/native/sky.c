/* The sky without colour bands, for gta_sa.exe 1.0 US; included by crashfix.c
 * and switched by the skygrad option. The idea is skygrad.asi's (FraGae); the
 * code and shaders are ours.
 *
 * CClouds::RenderSkyPolys draws the sky as a few big quads whose vertex
 * colours blend from one to the next. The blend is smooth until it is written
 * to an 8-bit backbuffer: a gradient a few dozen steps tall across the screen
 * shows each step as a line. skydither draws the same quads, same colours,
 * through a pixel shader that rounds each pixel's colour up or down by a
 * per-pixel threshold (interleaved gradient noise), so the steps mix into one
 * another. Where they differ from skygrad's:
 *   - the game's sky geometry and colours are left as they are.
 *   - the threshold is computed in the shader; no grain texture.
 *   - the colour reaches the pixel shader as a texture coordinate, which
 *     keeps its precision on every card.
 * Without the shaders (or a device) the stock flush draws the sky.
 */

/* ------------------------------------------------------------------------ */
/* The row: key, option, what, site, bytes, stock                           */
/* ------------------------------------------------------------------------ */
#define SKY(FJ, FP) \
    FJ(skydither, "skygrad", "CClouds::RenderSkyPolys: the sky's gradient dithered, so it shows no colour bands", \
        0x7154A8, 5, "\xE9\xA3\xCC\x00\x00")

SKY(FDECLARE_J, FDECLARE_P)

typedef struct { float x, y, z, nx, ny, nz; uint32_t color; float u, v; } SkyVertex;     /* RwIm3DVertex */
#define SKY_VERTICES ((const SkyVertex *)0xC4D958)  /* TempBufferVertices */
#define SKY_INDICES ((const uint16_t *)0xC4B958)    /* aTempBufferIndices */
#define SKY_NVERTICES (*(uint16_t *)0xC4B950)       /* uiTempBufferVerticesStored */
#define SKY_NINDICES (*(uint16_t *)0xC4B954)        /* uiTempBufferIndicesStored */
#define SKY_DEVICE (*(void **)0xC97C28)             /* RwD3D9's IDirect3DDevice9 */
#define SKY_FVF 0x152                               /* XYZ | NORMAL | DIFFUSE | TEX1: RwIm3DVertex */

typedef int (__cdecl *CreateShaderFn)(const uint32_t *function, void **shader);
typedef void (__cdecl *SetShaderFn)(void *shader);
typedef void (__cdecl *SetConstantFn)(uint32_t reg, const void *data, uint32_t count);
typedef void (__cdecl *SetFvfFn)(uint32_t fvf);
typedef void (__cdecl *DrawIndexedUpFn)(uint32_t type, uint32_t first, uint32_t vertices, uint32_t triangles,
                                        const void *indices, const void *data, uint32_t stride);

/* vs_2_0: oPos = v0 * c0..c3 (view * projection, by columns); oT0 the pixel
 * position before the divide by w (c4 = w/2, -h/2, w/2, h/2); oT1 the colour */
static const uint32_t SKY_VS[] = {
    0xFFFE0200,
    0x0200001F, 0x80000000, 0x900F0000,             /* dcl_position v0 */
    0x0200001F, 0x8000000A, 0x900F0001,             /* dcl_color v1 */
    0x03000009, 0x80010000, 0x90E40000, 0xA0E40000, /* dp4 r0.x, v0, c0 */
    0x03000009, 0x80020000, 0x90E40000, 0xA0E40001, /* dp4 r0.y, v0, c1 */
    0x03000009, 0x80040000, 0x90E40000, 0xA0E40002, /* dp4 r0.z, v0, c2 */
    0x03000009, 0x80080000, 0x90E40000, 0xA0E40003, /* dp4 r0.w, v0, c3 */
    0x02000001, 0xC00F0000, 0x80E40000,             /* mov oPos, r0 */
    0x03000005, 0x80030001, 0x80FF0000, 0xA0EE0004, /* mul r1.xy, r0.w, c4.zw */
    0x04000004, 0xE0030000, 0x80E40000, 0xA0E40004, 0x80E40001,     /* mad oT0.xy, r0, c4, r1 */
    0x02000001, 0xE00C0000, 0x80FF0000,             /* mov oT0.zw, r0.w */
    0x02000001, 0xE00F0001, 0x90E40001,             /* mov oT1, v1 */
    0x0000FFFF,
};

/* ps_2_0: n = frac(52.98 * frac(dot(pixel, (0.0671, 0.00584)))); each
 * channel c * 255 goes to floor, or floor + 1 where n < its fraction */
static const uint32_t SKY_PS[] = {
    0xFFFF0200,
    0x05000051, 0xA00F0000, 0x3D897143, 0x3BBF4590, 0x4253EE82, 0x00000000,    /* def c0, 0.0671106, 0.0058372, 52.9829, 0 */
    0x05000051, 0xA00F0001, 0x437F0000, 0x00000000, 0x3F800000, 0x3B808081,    /* def c1, 255, 0, 1, 1/255 */
    0x0200001F, 0x80000000, 0xB00F0000,             /* dcl t0 */
    0x0200001F, 0x80000000, 0xB00F0001,             /* dcl t1 */
    0x02000006, 0x80080000, 0xB0FF0000,             /* rcp r0.w, t0.w */
    0x03000005, 0x80030000, 0xB0E40000, 0x80FF0000, /* mul r0.xy, t0, r0.w */
    0x0400005A, 0x80010000, 0x80E40000, 0xA0E40000, 0xA0FF0000,     /* dp2add r0.x, r0, c0, c0.w */
    0x02000013, 0x80010000, 0x80000000,             /* frc r0.x, r0.x */
    0x03000005, 0x80010000, 0x80000000, 0xA0AA0000, /* mul r0.x, r0.x, c0.z */
    0x02000013, 0x80010000, 0x80000000,             /* frc r0.x, r0.x */
    0x03000005, 0x800F0001, 0xB0E40001, 0xA0000001, /* mul r1, t1, c1.x */
    0x02000013, 0x800F0002, 0x80E40001,             /* frc r2, r1 */
    0x03000002, 0x800F0001, 0x80E40001, 0x81E40002, /* add r1, r1, -r2 */
    0x03000002, 0x800F0002, 0x80000000, 0x81E40002, /* add r2, r0.x, -r2 */
    0x04000058, 0x800F0002, 0x80E40002, 0xA0550001, 0xA0AA0001,     /* cmp r2, r2, c1.y, c1.z */
    0x03000002, 0x800F0001, 0x80E40001, 0x80E40002, /* add r1, r1, r2 */
    0x03000005, 0x800F0001, 0x80E40001, 0xA0FF0001, /* mul r1, r1, c1.w */
    0x02000001, 0x800F0800, 0x80E40001,             /* mov oC0, r1 */
    0x0000FFFF,
};

static void *sky_vs, *sky_ps;
static int sky_made;                                /* 1 the shaders made, -1 they could not be */

/* -> the shaders, made on the first sky with a device */
static int sky_shaders(void)
{
    if (sky_made) return sky_made > 0;
    if (((CreateShaderFn)0x7FAC60)(SKY_VS, &sky_vs) && ((CreateShaderFn)0x7FACC0)(SKY_PS, &sky_ps)) {
        sky_made = 1;
        return 1;
    }
    sky_made = -1;
    InterlockedIncrement(&cf_n_skydither);          /* fixes logs the stock sky once */
    return 0;
}

/* In place of RenderSkyPolys' tail jump to CBrightLights::RenderOutGeometryBuffer */
void __cdecl cf_sky_flush(void)
{
    if (!SKY_NINDICES || !SKY_DEVICE || !sky_shaders()) {
        ((void (__cdecl *)(void))0x722150)();
        return;
    }
    const float (*view)[4] = (const float (*)[4])0xC9BC80, (*proj)[4] = (const float (*)[4])0x8E2458;
    float c[5][4];
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += view[row][k] * proj[k][col];
            c[col][row] = s;
        }
    float w = *(const int32_t *)0xC17044 * 0.5f, h = *(const int32_t *)0xC17048 * 0.5f;    /* RsGlobal */
    c[4][0] = w, c[4][1] = -h, c[4][2] = w, c[4][3] = h;
    ((SetFvfFn)0x7F9F30)(SKY_FVF);
    ((SetShaderFn)0x7F9FB0)(sky_vs);
    ((SetShaderFn)0x7F9FF0)(sky_ps);
    ((SetConstantFn)0x7FACA0)(0, c, 5);
    ((DrawIndexedUpFn)0x7FA1F0)(4, 0, SKY_NVERTICES, SKY_NINDICES / 3, SKY_INDICES, SKY_VERTICES, sizeof(SkyVertex));
    ((SetShaderFn)0x7F9FB0)(NULL);
    ((SetShaderFn)0x7F9FF0)(NULL);
    SKY_NVERTICES = 0;
    SKY_NINDICES = 0;
}

/* ------------------------------------------------------------------------ */
/* The hook body                                                            */
/* ------------------------------------------------------------------------ */
__asm__(
".intel_syntax noprefix\n"
"HOOK skydither\n"
"    jmp _cf_sky_flush\n"
".att_syntax\n");
