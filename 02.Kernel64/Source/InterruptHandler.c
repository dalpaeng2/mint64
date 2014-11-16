#include "InterruptHandler.h"
#include "PIC.h"
#include "Keyboard.h"
#include "Console.h"
#include "Utility.h"
#include "Task.h"
#include "Descriptor.h"

void kCommonExceptionHandler( int iVectorNumber, QWORD qwErrorCode )
{
  char vcBuffer[3] = {0, };

  vcBuffer[0] = '0' + iVectorNumber / 10;
  vcBuffer[1] = '0' + iVectorNumber % 10;

  kPrintStringXY( 0, 0, "=========================================================" );
  kPrintStringXY( 0, 1, "                Exception Occur~!!!                      " );
  kPrintStringXY( 0, 2, "                   Vector:                               " );
  kPrintStringXY( 27, 2, vcBuffer );
  kPrintStringXY( 0, 3, "=========================================================" );

  while( 1 );
}

void kCommonInterruptHandler( int iVectorNumber )
{
  char vcBuffer[] = "[INT:  , ]";
  static int g_iCommonInterruptCount = 0;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iCommonInterruptCount;
  g_iCommonInterruptCount = ( g_iCommonInterruptCount + 1 ) % 10;
  kPrintStringXY( 70, 0, vcBuffer );

  // EOI 전송
  kSendEOIToPIC( iVectorNumber - PIC_IRQSTARTVECTOR );
}

void kKeyboardHandler( int iVectorNumber )
{
  char vcBuffer[] = "[INT:  , ]";
  static int g_iKeyboardInterruptCount = 0;
  BYTE bTemp;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iKeyboardInterruptCount;
  g_iKeyboardInterruptCount = ( g_iKeyboardInterruptCount + 1 ) % 10;
  kPrintStringXY( 0, 0, vcBuffer );

  if( kIsOutputBufferFull() == TRUE )
  {
    bTemp = kGetKeyboardScanCode();
    kConvertScanCodeAndPutQueue( bTemp );
  }

  kSendEOIToPIC( iVectorNumber - PIC_IRQSTARTVECTOR );
}

void kTimerHandler( int iVectorNumber )
{
  char vcBuffer[] = "[INT:  , ]";
  static int g_iTimerInterruptCount = 0;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iTimerInterruptCount;
  g_iTimerInterruptCount = ( g_iTimerInterruptCount + 1 ) % 10;
  kPrintStringXY( 70, 0, vcBuffer );

  // EOI 전송
  kSendEOIToPIC( iVectorNumber - PIC_IRQSTARTVECTOR );

  // 타이머 발생 횟수를 증가
  g_qwTickCount++;

  // 태스크가 사용한 프로세서의 시간을 줄임
  kDecreaseProcessorTime();
  // 프로세서가 사용할 수 있는 시간을 다 썼다면 태스크 전환 수행
  if( kIsProcessorTimeExpired() == TRUE )
  {
    kScheduleInInterrupt();
  }
}
