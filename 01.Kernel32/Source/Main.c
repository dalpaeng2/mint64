#include "Types.h"
#include "Page.h"

void kPrintString( int iX, int iY, const char * pcString );
BOOL kInitializeKernel64Area( void );
BOOL kIsMemoryEnough( void );

void Main( void )
{
  kPrintString( 0, 3, "C Language Kernel Started...................[Pass]" );

  // 최소 메모리 크기를 만족하는 지 검사
  kPrintString( 0, 4, "Minumum Memory Size Check...................[    ]" ) ;
  kPrintString( 0, 5, "IA-32e Kernel Area Initialize...............[    ]" );
  if( kIsMemoryEnough() == FALSE )
  {
    kPrintString( 45, 4, "Fail" );
    kPrintString( 0, 5, "Not Enough Memory~!! MINT64 OS Requires Over 64MByte Memory~!!" );

    while( 1 );
  }
  else
  {
    kPrintString( 45, 4, "Pass" );
  }

  // IA-32e 모드의 커널 영역을 초기화
  kPrintString( 0, 5, "IA-32e Kernel Area Initialize...............[    ]" );
  if( kInitializeKernel64Area() == FALSE )
  {
    kPrintString( 45, 5, "Faile" );
    kPrintString( 0, 6, "Kernel Area Initialization Fail~!!" );
    while( 1 );
  }
  kPrintString( 45, 5, "Pass" );

  // IA-32e 모드 커널을 위한 페이지 테이블 생성
  kPrintString( 0, 6, "IA-32e Page Tables Initialize...............[    ]" );
  kInitializePageTables();
  kPrintString( 45, 6, "Pass" );

  while( 1 );
}

void kPrintString( int iX, int iY, const char * pcString )
{
  CHARACTER * pstScreen = ( CHARACTER * ) 0xB8000;
  int i;

  pstScreen += ( iY * 80 ) + iX;
  for ( i = 0; pcString[ i ] != 0; i++ )
  {
    pstScreen[ i ].bCharactor  = pcString[ i ];
  }
}

// IA-32e 모드용 커널 영역을 0으로 초기화
BOOL kInitializeKernel64Area( void )
{
  DWORD * pdwCurrentAddress;

  // 초기화를 시작할 어드레스인 0x100000(1MB)을 설정
  pdwCurrentAddress = ( DWORD * ) 0x100000;

  // 마지막 어드레스인 0x600000(6MB)까지 루프를 돌면서 4바이트씩 0으로 채움
  while( ( DWORD ) pdwCurrentAddress < 0x600000 )
  {
    *pdwCurrentAddress = 0x00;

    if( *pdwCurrentAddress != 0 )
    {
      return FALSE;
    }

    pdwCurrentAddress++;
  }

  return TRUE;
}

// MINT64 OS를 실행하기에 충분한 메모리를 가지고 있는지 체크
BOOL kIsMemoryEnough( void )
{
  DWORD * pdwCurrentAddress;

  // 0x100000(1MB)부터 검사 시작
  pdwCurrentAddress = ( DWORD * ) 0x100000;

  // 0x4000000(64MB)까지 루프를 돌면서 확인
  while( ( DWORD ) pdwCurrentAddress < 0x4000000 )
  {
    *pdwCurrentAddress = 0x12345678;

    // 방금 메모리에 기록한 값을 읽지 못하면 문제, 종료한다.
    if( *pdwCurrentAddress != 0x12345678 )
    {
      return FALSE;
    }

    // 1MB씩 이동하면서 확인
    pdwCurrentAddress += ( 0x100000 / 4 );
  }

  return TRUE;
}
