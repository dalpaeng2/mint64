[BITS 32]

; C 언어에서 호출할 수 있도록 이름을 노출(Export)
global kReadCPUID, kSwitchAndExecute64bitKernel

SECTION .text

; CPUID를 반환
;   PARAM: DWORD dwEAX, DWORD * pdwEAX, * pdwEBX, * pdwECX, * pdwEDX
kReadCPUID:
  push ebp
  mov  ebp, esp
  push eax
  push ebx
  push ecx
  push edx
  push esi

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; EAX 레지스터의 값으로 CPUID 명령어 실행
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov  eax, dword [ ebp + 8 ]          ; 파라미터 1(dwEAX)
  cpuid

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; 반환된 값을 파라미터에 저장
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov  esi, dword [ ebp + 12 ]
  mov  dword [ esi ], eax

  mov  esi, dword [ ebp + 16 ]
  mov  dword [ esi ], ebx

  mov  esi, dword [ ebp + 20 ]
  mov  dword [ esi ], ecx

  mov  esi, dword [ ebp + 24 ]
  mov  dword [ esi ], edx

  pop  esi
  pop  edx
  pop  ecx
  pop  ebx
  pop  eax
  pop  ebp
  ret

; IA-32e 모드로 전환하고 64비트 커널을 수행
;   PARAM: 없음
kSwitchAndExecute64bitKernel:
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; CR4 컨트롤 레지스터의 PAE 비트와 OSXMMEXCPT 비트, OSFXSR 비트를 1로 설정
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov  eax, cr4
  or   eax, 0x620                      ; PAE 비트 (비트 5), OSXMMEXCPT(비트 10)
                                       ; OSFXSR (비트 9)를 1로 설정
  mov  cr4, eax

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; CR3 컨드롤 레지스터에 PML4 테이블의 어드레스와 캐시 활성화
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov  eax, 0x100000                   ; PML4 테이블이 존재하는 0x100000 (1MB) 주소
  mov  cr3, eax

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; IA32_EFER.LME 를 1로 설정하여 IA-32e 모드를 활성화
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov  ecx, 0xC0000080
  rdmsr

  or   eax, 0x0100                     ; IA32_EFER의 하위 32비트 LME비트 (비트 8)을 1로 설정

  wrmsr

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; CR0 컨트롤 레지스터를 NW=0, CD=0, PG=1 로 설정하여 캐시, 페이징 활성화하고
  ; TS(비트3)=1, EM(비트2)=0, MP(비트1)=1 로 설정하여 FPU를 활성화
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov  eax, cr0
  or   eax, 0xE000000E
  xor  eax, 0x60000004
  mov  cr0, eax

  jmp  0x08:0x200000                   ; CS 세그먼트 셀렉터를 IA-32e 모드용 코드 셀렉터로
                                       ; 교체하고 0x200000(2MB) 어드레스로 이동

  jmp  $                               ; Not reachable
