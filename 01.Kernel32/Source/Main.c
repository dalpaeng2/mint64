#include "Types.h"

void kPrintString( int iX, int iY, const char * pcString );
BOOL kInitializeKernel64Area( void );

void Main( void )
{
  kPrintString( 0, 3, "C Language Kernel Started~!!!" );

  // IA-32e 모드의 커널 영역을 초기화
  kInitializeKernel64Area();
  kPrintString( 0, 4, "IA-32e Kernel Area Initialization Complete" );

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
