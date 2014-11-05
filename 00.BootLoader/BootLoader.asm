[ORG 0x00]                    ; 코드의 시작 어드레스를 0x00으로 설정
[BITS 16]                     ; 이하의 코드는 16비트 코드로 설정

SECTION .text	                ; text 섹션(세그먼트)을 정의

jmp 0x07C0:START              ; CS 세그먼트 레지스터에 0x07C0을 복사하면서, START 레이블로 이동

START:
    mov ax, 0x07C0            ; 부트로터의 시작 어드레스(0x7C00)를 세그먼트 레지스터 값으로 변환
    mov ds, ax                ; DS 세그먼트 레지스터에 설정
    mov ax, 0xB800            ; 비디오 메모리의 시작 어드레스(0xB800)를 세그먼트 레지스터 값으로 변환
    mov es, ax                ; ES 세그먼트 레지스터에 설정

    mov si, 0                 ; SI 레지스터(문자열 원본 인덱스 레지스터)를 초기화


.SCREENCLEARLOOP:
    mov byte [ es: si ], 0        ; 비디오 메모리의 문자가 위치하는 어드레스에
		                              ; 0을 복사하여 문자를 삭제
    mov byte [ es: si + 1 ], 0x0A ; 비디오 메모리의 속성이 위치하는 어드레스에
                                  ; 0x0A을 복사

    add si, 2                     ; 다음 문자 위치

    cmp si, 80 * 25 * 2           ; 화면의 전체 크기와 비교

    jl .SCREENCLEARLOOP           ; SI < (80 * 25 * 2)이면 .SCREENCLEARLOOP로 이동

    mov si, 0                     ; SI 레지스터(문자열 원본 인덱스 레지스터) 초기화
    mov di, 0                     ; DI 레지스터(문자열 대상 인덱스 레지스터) 초기화


.MESSAGELOOP:
    mov cl, byte [ si + MESSAGE1 ]    ; MESSAGE1의 어드레스에서 SI 레지스터 값만큼
                                      ; 더한 위치의 문자를 CL 레지스터에 복사

    cmp cl, 0                         ; 복사한 문자와 0을 비교
    je .MESSAGEEND                    ; 복사한 문자가 0이면 .MESSAGEEND로 이동

    mov byte [ es: di ], cl           ; 0이 아니면 비디오 메모리 어드레스 0xB800:di에 문자를 출력

    add si, 1                         ; 다음 문자열
    add di, 2                         ; 비디오 메모리는 (문자, 속성) 2 바이트 이므로 2를 더함

    jmp .MESSAGELOOP                  ; 메시지 출력 루프 반복
.MESSAGEEND:

    jmp $				                 	    ; 현재 위치에서 무한 루프 수행

MESSAGE1:    db 'MINT64 OS Boot Loader Start', 0        ; 출력할 메시지 정의


times 510 - ($ - $$) db 0x00  ; $: 현재 라인의 어드레스
                              ; $$: 현재 섹션(.text)의 시작 어드레스
                              ; $ - $$: 현재 섹션을 기준으로 하는 오프셋
                              ; 510 - ($ - $$): 현재부터 어드레스 510까지
                              ; dw 0x00: 1바이트를 선언하고 값은 0x00
                              ; time: 반복 수행
                              ; 현재 위치에서 어드레스 510까지 0x00으로 채움

db 0x55                       ; 1바이트를 선언하고 값은 0x55
db 0xAA                       ; 1바이트를 선언하고 값은 0xAA

