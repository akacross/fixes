/* Anisotropic filtering's magnification, for gta_sa.exe 1.0 US; included by
 * crashfix.c and switched by the anisotropic option.
 *
 * RwD3D9SetTexture takes a texture's RpAnisot level over 1 as filter mode 7,
 * whose entry in the D3DTEXF table (0x884950) is {ANISOTROPIC, LINEAR}, and
 * sets that one value as both MINFILTER and MAGFILTER. Anisotropic
 * magnification is an optional cap (D3DPTFILTERCAPS_MAGFANISOTROPIC) many
 * drivers lack; DXVK takes it anyway. A texture drawn up close (magnified) can
 * then come out wrong on native Direct3D 9. The row keeps MAGFILTER linear for
 * mode 7, which is what anisotropic magnification looks like where it exists.
 */
#define ANISO(FJ, FP) \
    FJ(anisomag, "anisotropic", "RwD3D9SetTexture: an anisotropic texture magnifies linearly", 0x7FDFA7, 7, \
        "\x53\x89\x9F\x4C\x89\xC9\x00")

ANISO(FDECLARE_J, FDECLARE_P)

/* push ebx (the MAGFILTER value) / mov [edi+0xC9894C], ebx (RW's copy of it) */
__asm__(
".intel_syntax noprefix\n"
"HOOK anisomag\n"
"    cmp ebx, 3\n"                      /* D3DTEXF_ANISOTROPIC */
"    jne 1f\n"
"    mov dword ptr [edi+0xC9894C], 2\n" /* D3DTEXF_LINEAR */
"    push 2\n"
"    GO 0x7FDFAE\n"
"1:  mov dword ptr [edi+0xC9894C], ebx\n"
"    push ebx\n"
"    GO 0x7FDFAE\n"
".att_syntax\n");
