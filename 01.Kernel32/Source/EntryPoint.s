[ORG 0x00]                   ; 코드의 시작 어드레스를 0x00으로 설정
[BITS 16]                    ; 이하의 코드는 16비트 코드로 설정

SECTION .text                ; text 섹션(세그먼트)을 정의

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 코드 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
START:
  mov ax, 0x1000             ; 보호 모드 엔트리 포인트의 시작 어드레스(0x10000)를
                             ; 세그먼트 레지스터 값으로 변환
  mov ds, ax                 ; DS 세그먼트 레지스터에 설정
  mov es, ax                 ; ES 세그먼트 레지스터에 설정

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; A20 게이트를 활성화
  ; BIOS를 이용한 전환이 실패했을 때 시스템 컨트롤 포트로 전환 시도
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; BIOS 서비스를 사용해서 A20 게이트를 활성화
  mov ax, 0x2401             ; A20 게이트 활성화 서비스 설정
  int 0x15                   ; BIOS 인터럽트 서비스 호출

  jc .A20GATEERROR           ; A20 게이트 활성화가 성공했는지 확인
  jmp .A20GATESUCCESS

.A20GATEERROR:
  ; 에러 발생 시, 시스템 컨트롤 포트로 전환 시도
  in al, 0x92                ; 시스템 컨트롤 포트(0x92)에서 1바이트를 읽음
  or al, 0x02                ; A20 게이트 비트 (비트 1)를 1로 설정
  and al, 0xFE               ; 리셋 방지를 위해 비트0을 0으로 설정
  out 0x92, al               ; 시스템 컨트롤 포트에 설정

.A20GATESUCCESS:
  cli                        ; 인터럽트가 발생하지 못하도록 설정
  lgdt [ GDTR ]              ; GDTR 자료구조를 프로세서에 설정하여 GDT 테이블을 로드

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; 보호 모드로 진입
  ; Disable Paging, Disable Cache, Internal FPU, Disable Align Check,
  ; Enable Protected mode
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov eax, 0x4000003B        ; PG=0, CD=1, NW=0, AM=0, WP=0, NE=1, ET=1, TS=1, EM=0, MP=1, PE=1
  mov cr0, eax               ; CR0 레지스터에 위의 플래그를 설정하여
                             ; 보호 모드로 전환

  ; 커널 코드 세그먼트를 0x00을 기준으로 하는 것으로 교체하고 EIP의 값을 0x00을 기준으로 재설정
  ; CS 세그먼트 셀렉터 : EIP
  jmp dword 0x08: ( PROTECTEDMODE - $$ + 0x10000 )

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 보호 모드로 진입
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[BITS 32]                    ; 이하의 코드는 32비트 코드로 설정
PROTECTEDMODE:
  mov ax, 0x10               ; 보호 모드 테이터 세그먼트 디스크립터를 AX 레지스터에 설정
                             ; 디스크립터의 크기 (8바이트) * 2 (세 번째 디스크립터)
                             ; | empty 디스크립터 | 코드 디스크립터 | 데이터 디스크립터 |
  mov ds, ax                 ; DS 세그먼트 셀렉터에 설정
  mov es, ax                 ; ES 세그먼트 셀렉터에 설정
  mov fs, ax                 ; FS 세그먼트 셀렉터에 설정
  mov gs, ax                 ; GS 세그먼트 셀렉터에 설정

  ; 스택을 0x00000000 ~ 0x0000FFFF 영역에 64KB 크기로 설정
  mov ss, ax                 ; SS 세그먼트 셀렉터에 설정
  mov esp, 0xFFFE            ; ESP 레지스터의 어드레스를 0xFFFE로 설정
  mov ebp, 0xFFFE            ; EBP 레지스터의 어드레스를 0xFFFE로 설정

  ; 화면에 보호 모드로 전환되었다는 메시지를 찍는다.
  push ( SWITCHSUCCESSMESSAGE - $$ + 0x10000 )
  push 2
  push 0
  call PRINTMESSAGE
  add esp, 12                ; 함수 호출 후 스택 정리 12 = 4바이트 * 3(파라미터)

  jmp dword 0x08: 0x10200    ; C 커널이 존재하는 0x10200으로 이동, CS 세그먼트 셀렉터는 2번째 디스크립터

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 함수 코드 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 문자열을 출력하는 함수
;   스택에 x 좌표, y 좌표, 문자열
PRINTMESSAGE:
  push ebp                   ; 베이스 포인터 레지스터(BP)를 스택에 삽입
  mov ebp, esp               ; 베이스 포인터 레지스터(BP)에 스택 포인터 레지스터(SP)의 값을 설정
  push esi                   ; 현재 레지스터 스택에 저장
  push edi
  push eax
  push ecx
  push edx

  mov eax, dword [ ebp + 12 ]
  mov esi, 160
  mul esi
  mov edi, eax

  mov eax, dword [ ebp + 8 ]
  mov esi, 2
  mul esi
  add edi, eax

  mov esi, dword [ ebp + 16 ]

.MESSAGELOOP:
  mov cl, byte [ esi ]

  cmp cl, 0
  je  .MESSAGEEND

  mov byte [ edi + 0xB8000 ], cl ; 보호 모드이기 때문에 선형 주소 바로 사용

  add esi, 1
  add edi, 2

  jmp .MESSAGELOOP

.MESSAGEEND:
  pop edx                    ; 스택에서 레지스터 복원
  pop ecx
  pop eax
  pop edi
  pop esi
  pop ebp
  ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 데이터 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 아래의 데이터들을 8바이트에 맞춰 정렬하기 위해 추가
align 8, db 0

; GDTR의 끝을 8바이트로 정렬하기 위해 추가
dw 0x0000
; GDTR 자료구조 정의
GDTR:
  dw GDTEND - GDT - 1        ; GDT 테이블의 전체 크기
  dd ( GDT - $$ + 0x10000 )  ; GDT 테이블의 시작 어드레스(커널이 0x10000에 로드되기 때문에 더해준다)

; GDT 테이블 정의
GDT:
  ; NULL 디스크립터, 0으로 초기화
  NULLDescriptor:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 0x00
    db 0x00
    db 0x00

  ; 보호 모드 커널용 코드 세그먼트 디스크립터
  CODEDESCRIPTOR:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A                  ; P=1, DPL=0, Code Segment, Execute/Read
    db 0xCF
    db 0x00

  DATADESCRIPTOR:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92                  ; P=1, DPL=0, Data Segment, Read/Write
    db 0xCF
    db 0x00
GDTEND:

SWITCHSUCCESSMESSAGE: db 'Switch To Protected Mode Successfully~!!', 0

times 512 - ( $ - $$ ) db 0x00
