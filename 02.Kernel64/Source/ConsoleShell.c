#include "ConsoleShell.h"
#include "Console.h"
#include "Keyboard.h"
#include "Utility.h"

SHELLCOMMANDENTRY gs_vstCommandTable[] =
{
  { "help", "Show help", kHelp },
  { "cls", "Clear Screen", kCls },
  { "totalram", "Show Total RAM Size", kShowTotalRAMSize },
  { "strtod", "String To Decimal/Hex Convert", kStringToDecimalHexTest },
  { "shutdown", "Shutdown And Reboot OS", kShutdown },
};

//==============================================================================
// 실제 셸을 구성하는 코드
//==============================================================================
void kStartConsoleShell( void )
{
  char vcCommandBuffer[CONSOLESHELL_MAXCOMMANDBUFFERCOUNT];
  int iCommandBufferIndex = 0;
  BYTE bKey;
  int iCursorX, iCursorY;

  // 프롬프트 출력
  kPrintf( CONSOLESHELL_PROMPTMESSAGE );

  while( 1 )
  {
    bKey = kGetCh();

    if( bKey == KEY_BACKSPACE )
    {
      if( iCommandBufferIndex > 0 )
      {
        // 현재 커서 위치를 얻어서 한 문자 앞으로 이동한 다음 공백을 출력하고
        // 커맨드 버퍼에서 마지막 문자 삭제
        kGetCursor( &iCursorX, &iCursorY );
        kPrintStringXY( iCursorX - 1, iCursorY, " " );
        kSetCursor( iCursorX - 1, iCursorY );
        iCommandBufferIndex--;
      }
    }
    else if( bKey == KEY_ENTER )
    {
      kPrintf( "\n" );

      if( iCommandBufferIndex > 0 )
      {
        vcCommandBuffer[iCommandBufferIndex] = '\0';
        kExecuteCommand( vcCommandBuffer );
      }

      // 프롬프트 출력 및 커맨드 버퍼 초기화
      kPrintf( "%s", CONSOLESHELL_PROMPTMESSAGE );
      kMemSet( vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
      iCommandBufferIndex = 0;
    }
    else if( ( bKey == KEY_LSHIFT ) || ( bKey == KEY_RSHIFT ) ||
              ( bKey == KEY_CAPSLOCK ) || ( bKey == KEY_NUMLOCK ) ||
              ( bKey == KEY_SCROLLLOCK ) )
    {
      ;
    }
    else
    {
      if( bKey == KEY_TAB )
      {
        bKey = ' ';
      }

      if( iCommandBufferIndex < CONSOLESHELL_MAXCOMMANDBUFFERCOUNT )
      {
        vcCommandBuffer[iCommandBufferIndex++] = bKey;
        kPrintf( "%c", bKey );
      }
    }
  }
}

void kExecuteCommand( const char * pcCommandBuffer )
{
  int i, iSpaceIndex;
  int iCommandBufferLength, iCommandLength;
  int iCount;

  // 공백으로 구분된 커맨드를 추출
  iCommandBufferLength = kStrLen( pcCommandBuffer );
  for( iSpaceIndex = 0; iSpaceIndex < iCommandBufferLength; iSpaceIndex++ )
  {
    if( pcCommandBuffer[iSpaceIndex] == ' ' )
    {
      break;
    }
  }

  // 커맨드 테이블을 검사해서 같은 이름의 커맨드가 있는지 확인
  iCount = sizeof( gs_vstCommandTable ) / sizeof( SHELLCOMMANDENTRY );
  for( i = 0; i < iCount; i++ )
  {
    iCommandLength = kStrLen( gs_vstCommandTable[i].pcCommand );
    // 커맨드의 길이와 내용이 완전히 일치하는지 검사
    if( (iCommandLength == iSpaceIndex ) &&
        ( kMemCmp( gs_vstCommandTable[i].pcCommand, pcCommandBuffer, iSpaceIndex ) == 0 ) )
    {
      gs_vstCommandTable[i].pfFunction( pcCommandBuffer + iSpaceIndex + 1 );
      break;
    }
  }

  if( i >= iCount )
  {
    kPrintf( "'%s' is not found.\n", pcCommandBuffer );
  }
}

void kInitializeParameter( PARAMETERLIST * pstList, const char * pcParameter )
{
  pstList->pcBuffer = pcParameter;
  pstList->iLength = kStrLen( pcParameter );
  pstList->iCurrentPosition = 0;
}

int kGetNextParameter( PARAMETERLIST * pstList, char * pcParameter )
{
  int i;
  int iLength;

  if( pstList->iLength <= pstList->iCurrentPosition )
  {
    return 0;
  }

  for( i = pstList->iCurrentPosition; i < pstList->iLength; i++ )
  {
    if( pstList->pcBuffer[i] == ' ' )
    {
      break;
    }
  }

  kMemCpy( pcParameter, pstList->pcBuffer + pstList->iCurrentPosition, i );
  iLength = i - pstList->iCurrentPosition;
  pcParameter[iLength] = '\0';

  pstList->iCurrentPosition += iLength + 1;
  return iLength;
}

//==============================================================================
// 커맨드를 처리하는 코드
//==============================================================================
void kHelp( const char * pcCommandBuffer )
{
  int i;
  int iCount;
  int iCursorX, iCursorY;
  int iLength, iMaxCommandLength = 0;

  kPrintf( "========================================================\n" );
  kPrintf( "                  MINT 64 Shell Help                    \n" );
  kPrintf( "========================================================\n" );

  iCount = sizeof( gs_vstCommandTable ) / sizeof( SHELLCOMMANDENTRY );

  for( i = 0; i < iCount; i++ )
  {
    iLength = kStrLen( gs_vstCommandTable[i].pcCommand );
    if( iLength > iMaxCommandLength )
    {
      iMaxCommandLength = iLength;
    }
  }

  for( i = 0; i < iCount; i++ )
  {
    kPrintf( "%s", gs_vstCommandTable[i].pcCommand );
    kGetCursor( &iCursorX, &iCursorY );
    kSetCursor( iMaxCommandLength, iCursorY );
    kPrintf( " - %s\n", gs_vstCommandTable[i].pcHelp );
  }
}

void kCls( const char * pcParameterBuffer )
{
  kClearScreen();
  kSetCursor( 0, 1 );
}

void kShowTotalRAMSize( const char * pcParameterBuffer )
{
  kPrintf( "Total RAM Size = %d MB\n", kGetTotalRAMSize() );
}

void kStringToDecimalHexTest( const char * pcParameterBuffer )
{
  char vcParameter[100];
  int iLength;
  PARAMETERLIST stList;
  int iCount = 0;
  long lValue;

  kInitializeParameter( &stList, pcParameterBuffer );

  while( 1 )
  {
    iLength = kGetNextParameter( &stList, vcParameter );
    if( iLength == 0 )
    {
      break;
    }

    kPrintf( "Param %d = '%s', Length = %d, ", iCount + 1, vcParameter, iLength );

    if( kMemCmp( vcParameter, "0x", 2 ) == 0 )
    {
      lValue = kAToI( vcParameter + 2, 16 );
      kPrintf( "HEX Value = %q\n", lValue );
    }
    else
    {
      lValue = kAToI( vcParameter, 10 );
      kPrintf( "Decimal Value = %d\n", lValue );
    }

    iCount++;
  }
}

void kShutdown( const char * pcParameterBuffer )
{
  kPrintf( "System Shutdown Start...\n" );

  // 키보드 컨트롤러를 통해 PC를 재시작
  kPrintf( "Press Any Key To Reboot PC..." );
  kGetCh();
  kReboot();
}
