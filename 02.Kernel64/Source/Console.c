#include <stdarg.h>
#include "Console.h"
#include "Keyboard.h"

CONSOLEMANAGER gs_stConsoleManager = { 0, };

void kInitializeConsole( int iX, int iY )
{
  kMemSet( &gs_stConsoleManager, 0, sizeof( gs_stConsoleManager ) );

  kSetCursor( iX, iY );
}

void kSetCursor( int iX, int iY )
{
  int iLinearValue;

  iLinearValue = iY * CONSOLE_WIDTH + iX;

  // CRTC 컨트롤 어드레스 레지스터(포트 0x3D4)에 0x0E를 전송하여
  // 상위 커서 위치 레지스터를 선택
  kOutPortByte( VGA_PORT_INDEX, VGA_INDEX_UPPERCURSOR );
  // CRTC 컨트롤 데이터 레지스터(포트 0x3D5)에 커서의 상위 바이트를 출력
  kOutPortByte( VGA_PORT_DATA, iLinearValue >> 8 );

  // CRTC 컨트롤 어드레스 레지스터(포트 0x3D4)에 0x0F를 전송하여
  // 하위 커서 위치 레지스터를 선택
  kOutPortByte( VGA_PORT_INDEX, VGA_INDEX_LOWERCURSOR );
  // CRTC 컨트롤 데이터 레지스터(포트 0x3D5)에 커서의 하위 바이트를 출력
  kOutPortByte( VGA_PORT_DATA, iLinearValue & 0xFF );

  // 문자를 출력할 위치 업데이트
  gs_stConsoleManager.iCurrentPrintOffset = iLinearValue;
}

void kGetCursor( int * piX, int * piY )
{
  *piX = gs_stConsoleManager.iCurrentPrintOffset % CONSOLE_WIDTH;
  *piY = gs_stConsoleManager.iCurrentPrintOffset / CONSOLE_WIDTH;
}

void kPrintf( const char * pcFormatString, ... )
{
  va_list ap;
  char vcBuffer[1024];
  int iNextPrintOffset;

  va_start( ap, pcFormatString );
  kVSPrintf( vcBuffer, pcFormatString, ap );
  va_end( ap );

  iNextPrintOffset = kConsolePrintString( vcBuffer );

  kSetCursor( iNextPrintOffset % CONSOLE_WIDTH, iNextPrintOffset / CONSOLE_WIDTH );
}


// \n, \t 같은 문자가 포함된 문자열을 출력한 후 화면상의 다음 출력할 위치를 반환
int kConsolePrintString( const char * pcBuffer )
{
  CHARACTER * pstScreen = ( CHARACTER * ) CONSOLE_VIDEOMEMORYADDRESS;
  int i, j;
  int iLength;
  int iPrintOffset;

  iPrintOffset = gs_stConsoleManager.iCurrentPrintOffset;

  iLength = kStrLen( pcBuffer );
  for( i = 0; i < iLength; i++ )
  {
    // 줄바꿈 처리
    if( pcBuffer[i] == '\n' )
    {
      iPrintOffset += ( CONSOLE_WIDTH - ( iPrintOffset % CONSOLE_WIDTH ) );
    }
    // 탭 처리
    else if( pcBuffer[i] == '\t' )
    {
      iPrintOffset += ( 8 - ( iPrintOffset % 8 ) );
    }
    // 일반 문자열 출력
    else
    {
      pstScreen[iPrintOffset].bCharactor = pcBuffer[i];
      pstScreen[iPrintOffset].bAttribute = CONSOLE_DEFAULTTEXTCOLOR;
      iPrintOffset++;
    }

    // 출력할 위치가 화면의 최댓값(80 *25)을 벗어났으면 스크롤 처리
    if( iPrintOffset >= ( CONSOLE_HEIGHT * CONSOLE_WIDTH ) )
    {
      // 가장 윗줄을 제외한 나머지를 한 줄 위로 복사
      kMemCpy( CONSOLE_VIDEOMEMORYADDRESS,
                CONSOLE_VIDEOMEMORYADDRESS + CONSOLE_WIDTH * sizeof( CHARACTER ),
                ( CONSOLE_HEIGHT - 1 ) * CONSOLE_WIDTH * sizeof( CHARACTER ) );

      for( j = ( CONSOLE_HEIGHT - 1 ) * (CONSOLE_WIDTH ); j < ( CONSOLE_HEIGHT * CONSOLE_WIDTH ); j++ )
      {
        // 공백 출력
        pstScreen[j].bCharactor = ' ';
        pstScreen[j].bAttribute = CONSOLE_DEFAULTTEXTCOLOR;
      }

      // 출력할 위치를 가장 아래쪽 라인의 처음으로 설정
      iPrintOffset = ( CONSOLE_HEIGHT - 1 ) * CONSOLE_WIDTH;
    }
  }
  return iPrintOffset;
}

void kClearScreen( void )
{
  CHARACTER * pstScreen = ( CHARACTER * ) CONSOLE_VIDEOMEMORYADDRESS;
  int i;

  for( i = 0; i < CONSOLE_WIDTH * CONSOLE_HEIGHT; i++ )
  {
    pstScreen[i].bCharactor = ' ';
    pstScreen[i].bAttribute = CONSOLE_DEFAULTTEXTCOLOR;
  }

  kSetCursor( 0, 0 );
}

BYTE kGetCh( void )
{
  KEYDATA stData;

  while( 1 )
  {
    // 키 큐에 데이터가 수신될 때까지 대기
    while( kGetKeyFromKeyQueue( &stData ) == FALSE )
    {
      ;
    }

    if( stData.bFlags & KEY_FLAGS_DOWN )
    {
      return stData.bASCIICode;
    }
  }
}

void kPrintStringXY( int iX, int iY, const char * pcString )
{
  CHARACTER * pstScreen = ( CHARACTER * ) CONSOLE_VIDEOMEMORYADDRESS;
  int i;

  pstScreen += ( iY * 80 ) + iX;

  for( i = 0; pcString[i] != 0; i++ )
  {
    pstScreen[i].bCharactor = pcString[i];
    pstScreen[i].bAttribute = CONSOLE_DEFAULTTEXTCOLOR;
  }
}
