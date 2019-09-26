%if PrefixUnderscore
%define AddSaturatedRaw _AddSaturatedRaw
%endif

			BITS	32
			SECTION	.text
			GLOBAL	AddSaturatedRaw

AddSaturatedRaw:	pushad
			mov	edi, [esp + 0x24]	; destination
			mov	esi, [esp + 0x28]	; source
			mov	ecx, [esp + 0x2C]	; count

			shr	ecx, 4
			mov	eax, 0f0f0f0fh
			movd	mm7, eax
			punpcklbw mm7, mm7
;			ALIGN	16
.MainLoop:		movq	mm0, [esi + 0]
			movq	mm1, [edi + 0]
			movq	mm2, [esi + 8]
			movq	mm3, [edi + 8]
;			movq	mm4, [esi + 16]
;			movq	mm5, [edi + 16]
;			movq	mm6, [esi + 24]
;			movq	mm7, [edi + 24]
			
			psrlw	mm0, 4
			psrlw	mm2, 4
;			psrlw	mm4, 4
;			psrlw	mm6, 4
			pand	mm0, mm7
			pand	mm2, mm7

			paddusb	mm0, mm1
			paddusb	mm2, mm3
;			paddusb	mm4, mm5
;			paddusb	mm6, mm7

			movq	[edi + 0], mm0
			movq	[edi + 8], mm2
;			movq	[edi + 16], mm4
;			movq	[edi + 24], mm6

;			add	esi, 32
;			add	edi, 32
			add	esi, 16
			add	edi, 16
			dec	ecx
			jnz	.MainLoop

			emms
			popad
.Finish:		ret
