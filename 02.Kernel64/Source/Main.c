#include "Types.h"
#include "Keyboard.h"

void kPrintString( int iX, int iY, const char * pcString );

void Main( void )
{
  char vcTemp[2] = {0, };
  BYTE bFlags;
  BYTE bTemp;
  int i = 0;

  kPrintString( 0, 10, "Switch To IA-32e Mode Success~!!" );
  kPrintString( 0, 11, "IA-32e C Language Kernel Start..............[Pass]" );
  kPrintString( 0, 12, "Keyboard Activate...........................[    ]" );

  // 키보드 활성화
  if( kActivateKeyboard() == TRUE )
  {
    kPrintString( 45, 12, "Pass" );
    kChangeKeyboardLED( FALSE, FALSE, FALSE );
  }
  else
  {
    kPrintString( 45, 12, "Fail" );
    while( 1 );
  }

  while( 1 )
  {
    // 출력 버퍼가 차 있으면 스캔 코드를 읽을 수 있음
    if( kIsOutputBufferFull() == TRUE )
    {
      // 출력 버퍼에서 스캔 코드를 읽어서 저장
      bTemp = kGetKeyboardScanCode();

      // 스캔 코드 => ASCII 코드, 눌림/떨어짐 정보
      if( kConvertScanCodeToASCIICode( bTemp, &(vcTemp[0]), &bFlags ) == TRUE )
      {
        // 키가 눌러졌으면 ASCII 코드 값 화면에 출력
        if( bFlags & KEY_FLAGS_DOWN )
        {
          kPrintString( i++, 13, vcTemp );
        }
      }
    }
  }
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
