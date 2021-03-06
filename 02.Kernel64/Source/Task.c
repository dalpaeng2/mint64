#include "Task.h"
#include "Descriptor.h"

// 스케쥴러 관련 자료구조
static SCHEDULER gs_stScheduler;
static TCBPOOLMANAGER gs_stTCBPoolManager;

////////////////////////////////////////////////////////////////////////////////
// 태스크 풀과 태스크 관련
////////////////////////////////////////////////////////////////////////////////
static void kInitializeTCBPool( void )
{
  int i;
  kMemSet( &(gs_stTCBPoolManager ), 0, sizeof( gs_stTCBPoolManager ) );

  gs_stTCBPoolManager.pstStartAddress = ( TCB * ) TASK_TCBPOOLADDRESS;
  kMemSet( TASK_TCBPOOLADDRESS, 0, sizeof( TCB ) * TASK_MAXCOUNT );

  for( i = 0; i < TASK_MAXCOUNT; i++ )
  {
    gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID = i;
  }

  gs_stTCBPoolManager.iMaxCount = TASK_MAXCOUNT;
  gs_stTCBPoolManager.iAllocatedCount = 1;
}

static TCB * kAllocateTCB( void )
{
  TCB * pstEmptyTCB;
  int i;

  if( gs_stTCBPoolManager.iUseCount == gs_stTCBPoolManager.iMaxCount )
  {
    return NULL;
  }

  for( i = 0; i < gs_stTCBPoolManager.iMaxCount; i++ )
  {
    if( ( gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID >> 32 ) == 0 )
    {
      pstEmptyTCB = &( gs_stTCBPoolManager.pstStartAddress[i] );
      break;
    }
  }

  // 상위 32비트를 0이 아닌 값으로 설정해서 할당된 TCB로 설정
  pstEmptyTCB->stLink.qwID = ( ( QWORD ) gs_stTCBPoolManager.iAllocatedCount << 32 ) | i;
  gs_stTCBPoolManager.iUseCount++;
  gs_stTCBPoolManager.iAllocatedCount++;
  if( gs_stTCBPoolManager.iAllocatedCount == 0 )
  {
    gs_stTCBPoolManager.iAllocatedCount = 1;
  }
  return pstEmptyTCB;
}

static void kFreeTCB( QWORD qwID )
{
  int i;

  // 태스크 ID의 하위 32비트가 인텍스 열할을 함
  i = GETTCBOFFSET( qwID );

  kMemSet( &( gs_stTCBPoolManager.pstStartAddress[i].stContext ), 0, sizeof( CONTEXT ) );
  gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID = i;

  gs_stTCBPoolManager.iUseCount--;
}

TCB * kCreateTask( QWORD qwFlags, void * pvMemoryAddress, QWORD qwMemorySize,
                   QWORD qwEntryPointAddress )
{
  TCB * pstTask, * pstProcess;
  void * pvStackAddress;
  BOOL bPreviousFlag;

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();
  pstTask = kAllocateTCB();
  if( pstTask == NULL )
  {
    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );
    return NULL;
  }

  // 현재 프로세스 또는 스레드가 속한 프로세스를 검색
  pstProcess = kGetProcessByThread( kGetRunningTask() );
  // 프로세스가 없다면 아무런 작업도 하지 않음
  if( pstProcess == NULL )
  {
    kFreeTCB( pstTask->stLink.qwID );
    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );
    return NULL;
  }

  // 스레드를 생성하는 경우라면 내가 속한 프로세스의 자식 스레드 리스트에 연결함
  if( qwFlags & TASK_FLAGS_THREAD )
  {
    // 현재 스레드의 프로세스를 찾아서 생성할 스레드에 프로세스 정보를 상속
    pstTask->qwParentProcessID = pstProcess->stLink.qwID;
    pstTask->pvMemoryAddress = pstProcess->pvMemoryAddress;
    pstTask->qwMemorySize = pstProcess->qwMemorySize;

    // 부모 프로세스의 자식 스레드 리스트에 추가
    kAddListToTail( &( pstProcess->stChildThreadList ), &( pstTask->stThreadLink ) );
  }
  // 프로세스는 파라미터로 넘어온 값을 그대로 설정
  else
  {
    pstTask->qwParentProcessID = pstProcess->stLink.qwID;
    pstTask->pvMemoryAddress = pvMemoryAddress;
    pstTask->qwMemorySize = qwMemorySize;
  }

  // 스레드의 ID를 태스트의 ID와 동일하게 설정
  pstTask->stThreadLink.qwID = pstTask->stLink.qwID;

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );

  // 태스크 ID로 스택 어드레스 계산, 하위 32비트가 스택 풀의 오프셋 역할 수행
  pvStackAddress = ( void * ) ( TASK_STACKPOOLADDRESS +
                    ( TASK_STACKSIZE * GETTCBOFFSET( pstTask->stLink.qwID ) ) );

  // TCB를 설정한 후 준비 리스트에 삽입하여 스케줄링될 수 있도록 함
  kSetUpTask( pstTask, qwFlags, qwEntryPointAddress, pvStackAddress,
              TASK_STACKSIZE );

  // 자식 스레드 리스트를 초기화
  kInitializeList( &( pstTask->stChildThreadList ) );

  // FPU 사용 여부를 사용하지 않은 것으로 초기화
  pstTask->bFPUUsed = FALSE;

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  // 태스크를 준비 리스트에 삽입
  kAddTaskToReadyList( pstTask );

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );

  return pstTask;
}

static void kSetUpTask( TCB * pstTCB, QWORD qwFlags, QWORD qwEntryPointAddress,
                  void * pvStackAddress, QWORD qwStackSize )
{
  kMemSet( pstTCB->stContext.vqRegister, 0, sizeof( pstTCB->stContext.vqRegister ) );

  // 스택에 관련된 RSP, RBP 레지스트 tjfwjd
  pstTCB->stContext.vqRegister[ TASK_RSPOFFSET ] = ( QWORD ) pvStackAddress + qwStackSize - 8;
  pstTCB->stContext.vqRegister[ TASK_RBPOFFSET ] = ( QWORD ) pvStackAddress + qwStackSize - 8;

  // Return Address 영역에 kExitTask() 함수의 어드레스를 삽입하여 태스크의 엔트리 포인트 함수를
  // 빠져나감과 동시에 kExitTask() 함수로 이동하도록 함
  *( QWORD * ) ( ( QWORD ) pvStackAddress + qwStackSize - 8 ) = ( QWORD ) kExitTask;

  pstTCB->stContext.vqRegister[ TASK_CSOFFSET ] = GDT_KERNELCODESEGMENT;
  pstTCB->stContext.vqRegister[ TASK_DSOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_ESOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_FSOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_GSOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_SSOFFSET ] = GDT_KERNELDATASEGMENT;

  pstTCB->stContext.vqRegister[ TASK_RIPOFFSET ] = qwEntryPointAddress;

  pstTCB->stContext.vqRegister[ TASK_RFLAGSOFFSET ] |= 0x0200;

  pstTCB->pvStackAddress = pvStackAddress;
  pstTCB->qwStackSize = qwStackSize;
  pstTCB->qwFlags = qwFlags;
}

////////////////////////////////////////////////////////////////////////////////
// 스케줄러 관련
////////////////////////////////////////////////////////////////////////////////
void kInitializeScheduler( void )
{
  int i;
  TCB * pstTask;

  // 태스크 풀 초기화
  kInitializeTCBPool();

  for( i = 0; i < TASK_MAXREADYLISTCOUNT; i++ )
  {
    kInitializeList( &( gs_stScheduler.vstReadyList[i] ) );
    gs_stScheduler.viExecuteCount[i] = 0;
  }
  kInitializeList( &( gs_stScheduler.stWaitList ) );

  // TCB를 할당받아 부팅을 수행한 태스크를 커널 최초의 프로세스로 설정
  pstTask = kAllocateTCB();
  gs_stScheduler.pstRunningTask = pstTask;
  pstTask->qwFlags = TASK_FLAGS_HIGHEST | TASK_FLAGS_PROCESS | TASK_FLAGS_SYSTEM;
  pstTask->qwParentProcessID = pstTask->stLink.qwID;
  pstTask->pvMemoryAddress = ( void * ) 0x100000;
  pstTask->qwMemorySize = 0x500000;
  pstTask->pvStackAddress = ( void * ) 0x600000;
  pstTask->qwStackSize = 0x100000;

  // 프로세서 사용율을 계산하는데 사용하는 자료구조 초기화
  gs_stScheduler.qwSpendProcessorTimeInIdleTask = 0;
  gs_stScheduler.qwProcessorLoad = 0;

  // FPU를 사용한 태스크 ID를 유효하지 않은 값으로 초기화
  gs_stScheduler.qwLastFPUUsedTaskID = TASK_INVALIDID;
}

void kSetRunningTask( TCB * pstTask )
{
  BOOL bPreviousFlag;

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  gs_stScheduler.pstRunningTask = pstTask;

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );
}

TCB * kGetRunningTask( void )
{
  BOOL bPreviousFlag;
  TCB * pstRunningTask;

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  pstRunningTask = gs_stScheduler.pstRunningTask;

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );

  return pstRunningTask;
}

static TCB * kGetNextTaskToRun( void )
{
  TCB * pstTarget = NULL;
  int iTaskCount, i, j;

  // 큐에 태스크가 있으나 모든 큐의 태스크가 1회씩 실행된 경우 모든 큐가 프로세서를 양보하여
  // 태스크를 선택하지 못할 수 있으니 NULL일 경우 한 번 더 수행
  for( j = 0; j < 2; j++ )
  {
    // 높은 우선순위에서 낮은 우선순위까지 리스트를 확인하여 스케줄링할 태스크를 실행함
    for( i = 0; i < TASK_MAXREADYLISTCOUNT; i++ )
    {
      iTaskCount = kGetListCount( &( gs_stScheduler.vstReadyList[i] ) );

      // 만약 실행한 횟수보다 리스트의 태스크 수가 더 많으면 현재 우선순위의 태스크를 실행함
      if( gs_stScheduler.viExecuteCount[i] < iTaskCount )
      {
        pstTarget = ( TCB * ) kRemoveListFromHeader( &( gs_stScheduler.vstReadyList[i] ) );
        gs_stScheduler.viExecuteCount[i]++;
        break;
      }
      // 만약 실행한 횟수가 더 많으면 실행 횟수를 초기화하고 다음 우선순위로 양보함
      else
      {
        gs_stScheduler.viExecuteCount[i] = 0;
      }
    }

    // 만약 수행할 태스크를 찾았으면 종료
    if( pstTarget != NULL )
    {
      break;
    }
  }
  return pstTarget;
}

static BOOL kAddTaskToReadyList( TCB * pstTask )
{
  BYTE bPriority;

  bPriority = GETPRIORITY( pstTask->qwFlags );
  if( bPriority >= TASK_MAXREADYLISTCOUNT )
  {
    return FALSE;
  }

  kAddListToTail( &( gs_stScheduler.vstReadyList[bPriority] ), pstTask );
  return TRUE;
}

static TCB * kRemoveTaskFromReadyList( QWORD qwTaskID )
{
  TCB * pstTarget;
  BYTE bPriority;

  if( GETTCBOFFSET( qwTaskID ) >= TASK_MAXCOUNT )
  {
    return NULL;
  }

  // TCB 풀에서 해당 태스크의 TCB를 찾아 실제로 ID가 일치하는가 확인
  pstTarget = &( gs_stTCBPoolManager.pstStartAddress[GETTCBOFFSET( qwTaskID )] );
  if( pstTarget->stLink.qwID != qwTaskID )
  {
    return NULL;
  }

  // 태스크가 존재하는 준비 리스트에서 태스크 제거
  bPriority = GETPRIORITY( pstTarget->qwFlags );

  pstTarget = kRemoveList( &( gs_stScheduler.vstReadyList[bPriority] ), qwTaskID );
  return pstTarget;
}

BOOL kChangePriority( QWORD qwTaskID, BYTE bPriority )
{
  TCB * pstTarget;
  BOOL bPreviousFlag;

  if( bPriority > TASK_MAXREADYLISTCOUNT )
  {
    return FALSE;
  }

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  // 현재 실행 중인 태스크이면 우선순위만 변경
  // PIT 컨트롤러의 인터럽트(IRQ 0)가 발생하여 태스크 전환이 수행될 때 변된
  // 우선순위의 리스트로 이동
  pstTarget = gs_stScheduler.pstRunningTask;
  if( pstTarget->stLink.qwID == qwTaskID )
  {
    SETPRIORITY( pstTarget->qwFlags, bPriority );
  }
  // 실행중인 태스크가 아니면 준비 리스트에서 찾아서 해당 우선순위의 리스트로 이동
  else
  {
    pstTarget = kRemoveTaskFromReadyList( qwTaskID );
    if( pstTarget == NULL )
    {
      pstTarget = kGetTCBInTCBPool( GETTCBOFFSET( qwTaskID ) );
      if( pstTarget != NULL )
      {
        SETPRIORITY( pstTarget->qwFlags, bPriority );
      }
    }
    else
    {
      SETPRIORITY( pstTarget->qwFlags, bPriority );
      kAddTaskToReadyList( pstTarget );
    }
  }

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );
  return TRUE;
}

// 다른 태스크를 찾아서 전환
//   인터럽트나 예외가 발생했을 때 호출하면 안됨
void kSchedule( void )
{
  TCB * pstRunningTask, * pstNextTask;
  BOOL bPreviousFlag;

  if( kGetReadyTaskCount() < 1 )
  {
    return ;
  }

  // 태스크 전환 중 인터럽트가 발생하지 않도록 함
  bPreviousFlag = kLockForSystemData();

  pstNextTask = kGetNextTaskToRun();
  if( pstNextTask == NULL )
  {
    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );
    return;
  }

  // 현재 수행 중인 태스크의 정보를 수정한 뒤 콘텍스트 전환
  pstRunningTask = gs_stScheduler.pstRunningTask;
  gs_stScheduler.pstRunningTask = pstNextTask;

  // 유휴 태스크에서 전환되었다면 사용한 프로세서의 시간을 증가시킴
  if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
  {
    gs_stScheduler.qwSpendProcessorTimeInIdleTask +=
      TASK_PROCESSORTIME - gs_stScheduler.iProcessorTime;
  }

  // 다음에 수행할 태스크가 FPU를 쓴 태스크가 아니라면 TS 비트 설정
  if( gs_stScheduler.qwLastFPUUsedTaskID != pstNextTask->stLink.qwID )
  {
    kSetTS();
  }
  else
  {
    kClearTS();
  }

  // 태스크 종료 플래그가 설정된 경우 콘텍스트를 저장할 필요가 없으므로, 대기 리스크에
  // 삽입하고 콘텍스트 전환
  if( pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK )
  {
    kAddListToTail( &( gs_stScheduler.stWaitList ), pstRunningTask );
    kSwitchContext( NULL, &( pstNextTask->stContext ) );
  }
  else
  {
    kAddTaskToReadyList( pstRunningTask );
    kSwitchContext( &( pstRunningTask->stContext ), &( pstNextTask->stContext ) );
  }

  gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );
}

// 인터럽트가 발생했을 때 다른 태스크를 찾아 전환
//   반드시 인터럽트나 예외가 발생했을 때 호출해야 함
BOOL kScheduleInInterrupt( void )
{
  TCB * pstRunningTask, * pstNextTask;
  char * pcContextAddress;
  BOOL bPreviousFlag;

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  pstNextTask = kGetNextTaskToRun();
  if( pstNextTask == NULL )
  {
    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );
    return FALSE;
  }

  //////////////////////////////////////////////////////////////////////////////
  // 태스크 전환 처리
  //   인터럽트 핸들러에서 저장한 콘텍스트를 다른 콘텍스트로 덮어쓰는 방법으로 처리
  //////////////////////////////////////////////////////////////////////////////
  pcContextAddress = ( char * ) IST_STARTADDRESS + IST_SIZE - sizeof( CONTEXT );

  // 현재 수행 중인 태스크의 정보를 수정한 뒤 콘텍스트 전환
  pstRunningTask = gs_stScheduler.pstRunningTask;
  gs_stScheduler.pstRunningTask = pstNextTask;

  // 유휴 태스크에서 전환되었다면 사용한 프로세서 시간을 증가시킴
  if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
  {
    gs_stScheduler.qwSpendProcessorTimeInIdleTask += TASK_PROCESSORTIME;
  }

  // 태스크 종료 플래그가 설정된 경우 콘텍스트를 저장하지 않고 대기 리스트에만 삽입
  if( pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK )
  {
    kAddListToTail( &( gs_stScheduler.stWaitList ), pstRunningTask );
  }
  // 태스크가 종료되지 않으면 IST에 있는 콘텍스트를 복사하고, 현재 태스크를 준비 리스트로 옮김
  else
  {
    kMemCpy( &( pstRunningTask->stContext ), pcContextAddress, sizeof( CONTEXT ) );
    kAddTaskToReadyList( pstRunningTask );
  }
  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );

  // 다음에 수행할 태스크가 FPU를 쓴 태스크가 아니라면 TS 비트 설정
  if( gs_stScheduler.qwLastFPUUsedTaskID != pstNextTask->stLink.qwID )
  {
    kSetTS();
  }
  else
  {
    kClearTS();
  }

  // 전환해서 실행할 태스크를 Running Task로 설정하고 콘텍스트를 IST에 복사해서
  // 자동으로 태스크 전환이 일어나도록 함
  kMemCpy( pcContextAddress, &( pstNextTask->stContext ), sizeof( CONTEXT ) );

  gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;
  return TRUE;
}

void kDecreaseProcessorTime( void )
{
  if( gs_stScheduler.iProcessorTime > 0 )
  {
    gs_stScheduler.iProcessorTime--;
  }
}

BOOL kIsProcessorTimeExpired( void )
{
  if( gs_stScheduler.iProcessorTime <= 0 )
  {
    return TRUE;
  }
  return FALSE;
}

BOOL kEndTask( QWORD qwTaskID )
{
  TCB * pstTarget;
  BYTE bPriority;
  BOOL bPreviousFlag;

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  // 현재 실행중인 태스크면 EndTask 비트를 설정하고, 태스크를 전환
  pstTarget = gs_stScheduler.pstRunningTask;
  if( pstTarget->stLink.qwID == qwTaskID )
  {
    pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
    SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );

    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );

    kSchedule();

    // 태스크가 전환되었으므로 아래 코드는 실행되지 않는다.
    while( 1 );
  }
  // 실행 중인 태스크가 아니면 준비 큐에서 직접 찾아서 대기 리스크에 연결
  else
  {
    pstTarget = kRemoveTaskFromReadyList( qwTaskID );
    if( pstTarget == NULL )
    {
      pstTarget = kGetTCBInTCBPool( GETTCBOFFSET( qwTaskID ) );
      if( pstTarget != NULL )
      {
        pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
        SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );
      }
      // 임계 영역 끝
      kUnlockForSystemData( bPreviousFlag );
      return TRUE;
    }

    pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
    SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );
    kAddListToTail( &( gs_stScheduler.stWaitList ), pstTarget );
  }
  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );
  return TRUE;
}

void kExitTask( void )
{
  kEndTask( gs_stScheduler.pstRunningTask->stLink.qwID );
}

int kGetReadyTaskCount( void )
{
  int iTotalCount = 0;
  int i;
  BOOL bPreviousFlag;

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  for(i = 0; i < TASK_MAXREADYLISTCOUNT; i++ )
  {
    iTotalCount += kGetListCount( &( gs_stScheduler.vstReadyList[i] ) );
  }

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );
  return iTotalCount;
}

int kGetTaskCount( void )
{
  int iTotalCount;
  BOOL bPreviousFlag;

  // 준비 큐 + 대기 큐 + 현재 실행중인 태스크
  iTotalCount = kGetReadyTaskCount();

  // 임계 영역 시작
  bPreviousFlag = kLockForSystemData();

  iTotalCount += kGetListCount( &( gs_stScheduler.stWaitList ) ) + 1;

  // 임계 영역 끝
  kUnlockForSystemData( bPreviousFlag );
  return iTotalCount;
}

TCB * kGetTCBInTCBPool( int iOffset )
{
  if( ( iOffset < -1 ) && ( iOffset > TASK_MAXCOUNT ) )
  {
    return NULL;
  }

  return &( gs_stTCBPoolManager.pstStartAddress[iOffset] );
}

BOOL kIsTaskExists( QWORD qwID )
{
  TCB * pstTCB;

  pstTCB = kGetTCBInTCBPool( GETTCBOFFSET( qwID ) );
  if( ( pstTCB == NULL ) || ( pstTCB->stLink.qwID != qwID ) )
  {
    return FALSE;
  }
  return TRUE;
}

QWORD kGetProcessorLoad( void )
{
  return gs_stScheduler.qwProcessorLoad;
}

static TCB * kGetProcessByThread( TCB * pstThread )
{
  TCB * pstProcess;

  // 자신이 프로세스이면 자신을 반환
  if( pstThread->qwFlags & TASK_FLAGS_PROCESS )
  {
    return pstThread;
  }

  pstProcess = kGetTCBInTCBPool( GETTCBOFFSET( pstThread->qwParentProcessID ) );

  if( ( pstProcess == NULL ) || ( pstProcess->stLink.qwID != pstThread->qwParentProcessID ) )
  {
    return NULL;
  }

  return pstProcess;
}

////////////////////////////////////////////////////////////////////////////////
// 유휴 태스크 관련
////////////////////////////////////////////////////////////////////////////////
// 유휴 태스크
//   대기 큐에 삭제 중인 태스크를 정리
void kIdleTask( void )
{
  TCB * pstTask, * pstChildThread, * pstProcess;
  QWORD qwLastMeasureTickCount, qwLastSpendTickInIdleTask;
  QWORD qwCurrentMeasureTickCount, qwCurrentSpendTickInIdleTask;
  BOOL bPreviousFlag;
  int i, iCount;
  QWORD qwTaskID;
  void * pstThreadLink;

  qwLastSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;
  qwLastMeasureTickCount = kGetTickCount();

  while( 1 )
  {
    qwCurrentMeasureTickCount = kGetTickCount();
    qwCurrentSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;

    // 프로세서 사용량 계산
    if( qwCurrentMeasureTickCount - qwLastMeasureTickCount == 0 )
    {
      gs_stScheduler.qwProcessorLoad = 0;
    }
    else
    {
      gs_stScheduler.qwProcessorLoad = 100 -
        ( qwCurrentSpendTickInIdleTask - qwLastSpendTickInIdleTask ) * 100
        / ( qwCurrentMeasureTickCount - qwLastMeasureTickCount );
    }

    qwLastMeasureTickCount = qwCurrentMeasureTickCount;
    qwLastSpendTickInIdleTask = qwCurrentSpendTickInIdleTask;

    // 프로세서를 쉬게 함
    kHaltProcessorByLoad();

    // 대기 큐에 대기 중인 태스크가 있으면 태스크를 종료함
    if( kGetListCount( &( gs_stScheduler.stWaitList ) ) >= 0 )
    {
      while( 1 )
      {
        // 임계 영역 시작
        bPreviousFlag = kLockForSystemData();
        pstTask = kRemoveListFromHeader( &( gs_stScheduler.stWaitList ) );
        if( pstTask == NULL )
        {
          // 임계 영역 끝
          kUnlockForSystemData( bPreviousFlag );
          break;
        }

        if( pstTask->qwFlags & TASK_FLAGS_PROCESS )
        {
          // 프로세스를 종료할 때 자식 스레드가 존재하면 스레드를 모두 종료하고,
          // 다시 자식 스레드 리스트에 삽입
          iCount = kGetListCount( &( pstTask->stChildThreadList ) );
          for( i = 0; i < iCount; i++ )
          {
            // 스레드 링크의 어드레스에서 꺼내 스레드를 종료시킴
            pstThreadLink = ( TCB * ) kRemoveListFromHeader( &( pstTask->stChildThreadList ) );
            if( pstThreadLink == NULL )
            {
              break;
            }

            pstChildThread = GETTCBFROMTHREADLINK( pstThreadLink );

            // 다시 자식 스레드 리스트에 삽입하여 해당 스레드가 종료될 때
            // 자식 스레드가 프로세스를 찾아 스스로 리스트에서 제거하도록 함
            kAddListToTail( &( pstTask->stChildThreadList ), &( pstChildThread->stThreadLink ) );

            // 자식 스레드 종료
            kEndTask( pstChildThread->stLink.qwID );
          }

          // 아직 자식 스레드가 남아있다면 자식 스레드가 다 종료될 때까지
          // 기다려야 하므로 다시 대기 리스트에 삽입
          if( kGetListCount( &( pstTask->stChildThreadList ) ) > 0 )
          {
            kAddListToTail( &( gs_stScheduler.stWaitList ), pstTask );

            // 임계 영역 끝
            kUnlockForSystemData( bPreviousFlag );
            continue;
          }
          else
          {
            // TODO: 프로세스 종료 코드
          }
        }
        else if( pstTask->qwFlags & TASK_FLAGS_THREAD )
        {
          // 스레드라면 프로세스의 자식 스레드 리스트에서 제거
          pstProcess = kGetProcessByThread( pstTask );
          if( pstProcess != NULL )
          {
            kRemoveList( &( pstProcess->stChildThreadList ), pstTask->stLink.qwID );
          }
        }

        qwTaskID = pstTask->stLink.qwID;
        kFreeTCB( pstTask->stLink.qwID );
        // 임계 영역 끝
        kUnlockForSystemData( bPreviousFlag );

        kPrintf( "Idle:: Task[0x%q] is completely ended.\n", pstTask->stLink.qwID );
      }
    }

    kSchedule();
  }
}

void kHaltProcessorByLoad( void )
{
  if( gs_stScheduler.qwProcessorLoad < 40 )
  {
    kHlt();
    kHlt();
    kHlt();
  }
  else if( gs_stScheduler.qwProcessorLoad < 80 )
  {
    kHlt();
    kHlt();
  }
  else if( gs_stScheduler.qwProcessorLoad < 95 )
  {
    kHlt();
  }
}

//==============================================================================
// FPU 관련
//==============================================================================
QWORD kGetLastFPUUsedTaskID( void )
{
  return gs_stScheduler.qwLastFPUUsedTaskID;
}

void kSetLastFPUUsedTaskID( QWORD qwTaskID )
{
  gs_stScheduler.qwLastFPUUsedTaskID = qwTaskID;
}
