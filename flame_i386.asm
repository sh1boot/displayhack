%if PrefixUnderscore
%define FlameRaw _FlameRaw
%endif


			BITS	32
			SECTION	.text
			GLOBAL	FlameRaw

FlameRaw:		pushad
.globl FlameRaw

			mov	eax, 0x80
			push	eax

			mov	edi, [esp + 0x28]	; destination
;			mov	esi, [esp + 0x2C]	; source
			mov	ebp, [esp + 0x30]	; map
			mov	edx, [esp + 0x34]	; data pitch
			mov	ecx, [esp + 0x38]	; count

			shr	ecx, 1
			dec	ecx
			dec	edi
			pxor	mm7, mm7

			mov	ebx, [ebp]
			mov	esi, [ebp + 4]
			add	ebx, [esp + 0x2C]
			movq	mm1, [esi]
			movd	mm0, [ebx]
			punpcklbw mm0, mm7
			movd	mm2, [ebx + edx]
			movq	mm3, mm1
			punpcklbw mm2, mm7
			punpcklbw mm1, mm7
			movd	mm4, [ebx + edx * 2]
			jmp	.MainEntry
;			ALIGN	16
.MainLoop:	add eax, [esp]
			movd	mm4, [ebx + edx * 2]
			mov	[edi], ah
	mov [esp], al
.MainEntry:		pmaddwd	mm0, mm1
			punpckhbw mm3, mm7
			movd	mm5, [esi + 8]
			mov	ebx, [ebp + 8]
			mov	esi, [ebp + 12]
			add	ebp, 16
			punpcklbw mm4, mm7
			movq	mm1, [esi]
			punpcklbw mm5, mm7
			pmaddwd	mm2, mm3
			inc	edi
			movq	mm3, mm1
			punpcklbw mm1, mm7
			pmaddwd	mm4, mm5
			movd	mm5, [esi + 8]
			add	ebx, [esp + 0x2C]
			punpckhbw mm3, mm7
			paddd	mm4, mm2
			punpcklbw mm5, mm7
			paddd	mm4, mm0
			movq	mm2, mm4
			movd	mm0, [ebx]
			psrlq	mm2, 32
			paddd	mm4, mm2
			punpcklbw mm0, mm7
			pmaddwd	mm0, mm1
			movd	mm2, [ebx + edx]
			punpcklbw mm2, mm7
			movd	eax, mm4
			movd	mm4, [ebx + edx * 2]
	add eax, [esp]
			pmaddwd	mm2, mm3
.Store1:		mov	[edi], ah
	mov [esp], al
.PostStore1:		inc	edi
			punpcklbw mm4, mm7
			mov	ebx, [ebp]
			pmaddwd	mm4, mm5
			add	ebx, [esp + 0x2C]
			mov	esi, [ebp + 4]		; ***
			movq	mm1, [esi]
			paddd	mm2, mm0
			movd	mm0, [ebx]
			paddd	mm4, mm2
			movq	mm5, mm4
			movq	mm3, mm1
			psrlq	mm5, 32
			movd	mm2, [ebx + edx]
			punpcklbw mm0, mm7
			paddd	mm4, mm5
			punpcklbw mm2, mm7
			punpcklbw mm1, mm7
			movd	eax, mm4
			dec	ecx
			jnz	NEAR .MainLoop

.StoreExit:		movd	eax, mm4
.PostStoreExit:		mov	[edi], ah

			emms
			pop	eax
			popad
.Finish:		ret
