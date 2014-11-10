[BITS 64]

SECTION .text

global kInPortByte, kOutPortByte, kLoadGDTR, kLoadTR, kLoadIDTR

kInPortByte:
  push rdx

  mov  rdx, rdi
  mov  rax, 0
  in   al, dx

  pop  rdx
  ret

kOutPortByte:
  push rdx
  push rax

  mov  rdx, rdi
  mov  rax, rsi
  out  dx, al

  pop  rax
  pop  rdx
  ret

; GDTR 레지스터에 GDT 테이블을 설정
;   PARAM: GDT 테이블에 정보를 저장하는 자료구조의 어드레스
kLoadGDTR:
  lgdt [ rdi ]
  ret

; TR 레지스터에 TSS 세그먼트 디스크립터 설정
;   PARAM: TSS 세그먼트 디스크립터의 오프셋
kLoadTR:
  ltr  di
  ret

; IDTR 레지스터에 IDT 테이블을 설정
;   PARAM: IDT 테이블의 정보를 저장하는 자료구조의 어드레스
kLoadIDTR:
  lidt [ rdi ]
  ret
