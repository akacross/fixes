/* Mirrors with anti-aliasing, for gta_sa.exe 1.0 US (MTA's); included by
 * crashfix.c and switched by the mirroraa option.
 *
 * CMirrors::CreateBuffer (0x7230A0) makes the mirror's render target raster.
 * With multisampling on, RwD3D9's multisample type and quality (0xC9C050,
 * 0xC9C054) carry into that raster, which then can't be a texture, so the
 * mirror draws nothing or garbage. The one call (0x72701D) goes through
 * cf_mirror_buffer, which zeroes both around it and puts them back.
 */
void __cdecl cf_mirror_buffer(void);

#define MIRROR(FJ, FP) \
    FP(mirroraa, "mirroraa", KIND_CALL, "CMirrors::BeforeConstructRenderList: the mirror buffer made without multisampling", 0x72701D, 5, \
        "\xE8\x7E\xC0\xFF\xFF", cf_mirror_buffer)

MIRROR(FDECLARE_J, FDECLARE_P)

void __cdecl cf_mirror_buffer(void)
{
    volatile uint32_t *type = (volatile uint32_t *)0xC9C050, *quality = (volatile uint32_t *)0xC9C054;
    uint32_t savedType = *type, savedQuality = *quality;
    *type = 0, *quality = 0;
    ((void (__cdecl *)(void))0x7230A0)();           /* CMirrors::CreateBuffer */
    *type = savedType, *quality = savedQuality;
}
