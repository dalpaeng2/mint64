[ORG 0x00]
[BITS 16]

SECTION .text

jmp 0x1000:START

SECTORCOUNT:      dw  0x0000    ; 현재 실행중인 섹터 번호
TOTALSECTORCOUNT  equ 1024      ; 가상 OS의 총 섹터 수

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 코드 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
START:
    mov ax, cs
    mov ds, ax
    mov ax, 0xB800
    mov es, ax

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		; 각 섹터 별로 코드를 생성
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    %assign i 0
		%rep TOTALSECTORCOUNT
		    %assign i i + 1

				mov ax, 2
				mul word [ SECTORCOUNT ]
				mov si, ax

				mov byte [ es: si + (160 * 2) ], '0' + (i % 10)
				add word [ SECTORCOUNT ], 1

				%if i == TOTALSECTORCOUNT
				    jmp $
				%else
				    jmp (0x1000 + i * 0x20): 0x0000
				%endif

				times (512 - ($ - $$) % 512)  db 0x00
		%endrep

