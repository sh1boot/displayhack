%if PrefixUnderscore
%define OutlineRaw _OutlineRaw
%endif

			BITS	32
			SECTION	.text
			GLOBAL	OutlineRaw

OutlineRaw:		pushad
			mov	edi, [esp + 0x24]	; destination
			mov	esi, [esp + 0x28]	; source
			mov	eax, [esp + 0x2C]	; data pitch
			mov	ecx, [esp + 0x30]	; count

			shr	ecx, 4
			add	eax, eax
			mov	edx, 0C0C0C0C0h
			ALIGN	16
.MainLoop:		movq	mm0, [esi + 0]
			movd	mm7, edx
			movq	mm1, [esi + 2]
			punpcklbw mm7, mm7
			movq	mm2, [esi + eax]
			pand	mm0, mm7
			movq	mm4, [esi + 8]
			pand	mm1, mm7
			movq	mm5, [esi + 10]
			pand	mm2, mm7
			movq	mm6, [esi + eax + 8]
			pand	mm4, mm7

			pand	mm5, mm7
			pand	mm6, mm7

			movq	mm3, mm0			; horizontal delta
			movq	mm7, mm4
			psubusb	mm0, mm1
			psubusb	mm1, mm3
			psubusb	mm4, mm5
			psubusb	mm5, mm7
			por	mm0, mm1
			por	mm4, mm5

			movq	mm1, mm3			; vertical delta
			movq	mm5, mm7
			psubusb	mm3, mm2
			psubusb	mm2, mm1
			psubusb	mm7, mm6
			psubusb	mm6, mm5
			por	mm2, mm3
			por	mm6, mm7

			paddusb	mm0, mm2			; added together
			paddusb	mm4, mm6

			movq	mm2, [edi + 0]
			psrlq	mm0, 1
			movq	mm6, [edi + 8]
			psrlq	mm4, 1

			paddusb	mm2, mm0			; and added into target
			paddusb	mm6, mm4

			movq	[edi + 0], mm2
			movq	[edi + 8], mm6

			add	esi, 16
			add	edi, 16
			dec	ecx
			jnz	NEAR .MainLoop

			emms
			popad
.Finish:		ret
