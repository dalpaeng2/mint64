[BITS 64]

SECTION .text

global kInPortByte, kOutPortByte, kLoadGDTR, kLoadTR, kLoadIDTR
global kEnableInterrupt, kDisableInterrupt, kReadRFLAGS
global kReadTSC
global kSwitchContext

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

; 인터럽트를 활성화
kEnableInterrupt:
  sti
  ret

; 인터럽트를 비활성화
kDisableInterrupt:
  cli
  ret

; RFLAGS 레지스터를 읽어서 되돌려줌
kReadRFLAGS:
  pushfq
  pop rax

  ret

; 타임 스탬프 카운터를 읽어서 반환
kReadTSC:
  push rdx

  rdtsc

  shl  rdx, 32        ; RDX 레지스터에서 상위 32비트 TSC 값을 읽고
  or   rax, rdx       ; RAX 레시트터에서 하위 32비트 TSC 값을 읽는다.

  pop  rdx

  ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 태스크 관련 어셈블리어 함수
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro KSAVECONTEXT 0
  push rbp
  push rax
  push rbx
  push rcx
  push rdx
  push rdi
  push rsi
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  mov  ax, ds
  push rax
  mov  ax, es
  push rax
  push fs
  push gs
%endmacro

%macro KLOADCONTEXT 0
  pop  gs
  pop  fs
  pop  rax
  mov  es, ax
  pop  rax
  mov  ds, ax

  pop  r15
  pop  r14
  pop  r13
  pop  r12
  pop  r11
  pop  r10
  pop  r9
  pop  r8
  pop  rsi
  pop  rdi
  pop  rdx
  pop  rcx
  pop  rbx
  pop  rax
  pop  rbp
%endmacro

; Current Context에 현재 콘택스트를 저장하고 Next Task에서 콘텍스트를 복구
;   PARAM: Current Context, Next Context
kSwitchContext:
  push rbp
  mov  rbp, rsp

  ; Current Context 가 NULL이면 콘텍스트를 저장할 필요 없음
  pushfq
  cmp  rdi, 0
  je   .LoadContext
  popfq

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; 현재 태스크의 콘텍스트를 저장
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  push rax

  mov  ax, ss                               ; SS 레지스터 저장
  mov  qword[ rdi + ( 23 * 8 ) ], rax

  mov  rax, rbp                             ; RBP에 저장된 RSP 레지스터 저장
  add  rax, 16                              ; RSP 레지스터는 push rbp와 Return Address를
  mov  qword[ rdi + ( 22 * 8 ) ], rax       ; 제외한 값으로 저장

  pushfq                                    ; RFLAGS 저장
  pop  rax
  mov  qword[ rdi + ( 21 * 8 ) ], rax

  mov  ax, cs                               ; CS 레지스터 저장
  mov  qword[ rdi + ( 20 * 8 ) ], rax

  mov  rax, qword[ rbp + 8 ]                ; RIP 레지스터를 Return Address로 설정하여
  mov  qword[ rdi + ( 19 * 8 ) ], rax       ; 다음 콘텍스트 복원 시에 이 함수를 호출한
                                            ; 위치로 이동하게 함

  pop  rax
  pop  rbp

  ; 가장 끝부분에 SS, RSP, RFLAGS, CS, RIP 레지스터를 저장했으므로, 이전 영역에
  ; push 명령어로 콘텍스트를 저장하기 위해 스택을 변경
  add  rdi, ( 19 * 8 )
  mov  rsp, rdi
  sub  rdi, ( 19 * 8 )

  ; 나머지 레지스터를 모두 Context 자료구조에 저장
  KSAVECONTEXT

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; 다음 태스크의 콘텍스트 복원
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.LoadContext:
  mov  rsp, rsi

  ; Context 자료구조에서 레지스터를 복원
  KLOADCONTEXT
  iretq
