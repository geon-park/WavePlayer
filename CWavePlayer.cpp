#include "stdafx.h"
#include "CWavePlayer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#include <crtdbg.h>
#ifdef malloc
#undef malloc
#endif
#define malloc(s) (_malloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__ ))
#ifdef calloc
#undef calloc
#endif
#define calloc(c, s) (_calloc_dbg(c, s, _NORMAL_BLOCK, __FILE__, __LINE__ ))
#ifdef realloc
#undef realloc
#endif
#define realloc(p, s) (_realloc_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__ ))
#ifdef _expand
#undef _expand
#endif
#define _expand(p, s) (_expand_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__  ))
#ifdef free
#undef free
#endif
#define free(p) (_free_dbg(p, _NORMAL_BLOCK))
#ifdef _msize
#undef _msize
#endif
#define _msize(p) (_msize_dbg(p, _NORMAL_BLOCK))
#endif

DWORD WINAPI CWavePlayer::WaveLib_MainThread( PVOID pDataInput )
{
	CWavePlayer* pWavePlayer = ( CWavePlayer* )pDataInput;
	while ( !pWavePlayer->m_pMainThread->bThreadShouldDie )
	{
		ResetEvent( pWavePlayer->m_pMainThread->hStartEvent );
		WaitForSingleObject( pWavePlayer->m_pMainThread->hStartEvent, INFINITE );

		ResetEvent( pWavePlayer->m_pMainThread->m_hStopEvent );
		pWavePlayer->WaveLib_Init( _T("Reference_Sent.wav"), FALSE );
		WaitForSingleObject( pWavePlayer->m_pMainThread->m_hStopEvent, INFINITE );

		pWavePlayer->WaveLib_UnInit();
	}
	pWavePlayer->WaveLib_WriteLog( _T("[TRACE] WaveLib_MainThread End \r\n") );
	return 0;
}

BOOL CWavePlayer::InitThread()
{
	m_pMainThread = ( PControl_Thread )LocalAlloc( LMEM_ZEROINIT, sizeof( Control_Thread ) );
	if ( m_pMainThread )
	{
		m_pMainThread->bThreadShouldDie = FALSE;
		DWORD dwThreadID;
		m_pMainThread->hStartEvent = CreateEvent( NULL, FALSE, FALSE, _T("") );
		m_pMainThread->m_hStopEvent = CreateEvent( NULL, FALSE, FALSE, _T("") );
		m_pMainThread->hMainThread = CreateThread( NULL, 0, ( LPTHREAD_START_ROUTINE )CWavePlayer::WaveLib_MainThread, this, 0, &dwThreadID );
	}

	return TRUE;
}

void CWavePlayer::DeInitThread()
{
	SetEvent( m_pMainThread->m_hStopEvent );
	if ( !m_pMainThread )
	{
		m_pMainThread->bThreadShouldDie = TRUE;
		if ( NULL != m_pMainThread->hStartEvent )
		{
			SetEvent( m_pMainThread->hStartEvent );
			WaitForSingleObject( m_pMainThread->hMainThread, INFINITE );

			CloseHandle( m_pMainThread->hMainThread );
			CloseHandle( m_pMainThread->hStartEvent );
			CloseHandle( m_pMainThread->m_hStopEvent );

			m_pMainThread->hMainThread = NULL;
			m_pMainThread->hStartEvent = NULL;
			m_pMainThread->m_hStopEvent = NULL;
		}
	}

	WaveLib_WriteLog( _T("[TRACE] DeInitThread \r\n") );
}

BOOL CWavePlayer::WaveLib_ReadWaveFile( LPTSTR pFileName )
{
	// Open the wave file. 

	HMMIO hMMIO = mmioOpen( pFileName, NULL, MMIO_READ | MMIO_ALLOCBUF );
	if (hMMIO == NULL)
		return FALSE;

	MMCKINFO mmCkInfoRIFF;
	mmCkInfoRIFF.fccType = mmioFOURCC( 'W', 'A', 'V', 'E' );
	if ( MMSYSERR_NOERROR != mmioDescend( hMMIO, &mmCkInfoRIFF, NULL, MMIO_FINDRIFF ) )
	{
		mmioClose( hMMIO, 0 );
		return FALSE;
	}

	MMCKINFO mmCkInfoChunk;
	mmCkInfoChunk.ckid = mmioFOURCC( 'f', 'm', 't', ' ' );
	if ( MMSYSERR_NOERROR != mmioDescend(hMMIO, &mmCkInfoChunk, &mmCkInfoRIFF, MMIO_FINDCHUNK) )
	{
		mmioClose( hMMIO, 0 );
		return FALSE;
	}

	if ( 0 >= mmioRead( hMMIO, ( HPSTR )&m_pWaveLib->WaveSample.WaveFormatEx, sizeof( WAVEFORMATEX ) ) )
	{
		mmioClose( hMMIO, 0 );
		return FALSE;
	}

	if ( MMSYSERR_NOERROR != mmioAscend( hMMIO, &mmCkInfoChunk, 0 ) )
	{
		mmioClose( hMMIO, 0 );
		return FALSE;
	}

	mmCkInfoChunk.ckid = mmioFOURCC ('d', 'a', 't', 'a');
	if ( MMSYSERR_NOERROR != mmioDescend( hMMIO, &mmCkInfoChunk, &mmCkInfoRIFF, MMIO_FINDCHUNK ) ) 
	{
		mmioClose( hMMIO, 0 );
		return FALSE;
	}

	m_pWaveLib->WaveSample.Size = mmCkInfoChunk.cksize;

	m_pWaveLib->WaveSample.pSampleData = (char *)LocalAlloc( LMEM_ZEROINIT, m_pWaveLib->WaveSample.Size );
	if ( NULL == m_pWaveLib->WaveSample.pSampleData )
	{
		mmioClose( hMMIO, 0 );
		return FALSE;
	}
	if ( 0 >= mmioRead( hMMIO, (char*)m_pWaveLib->WaveSample.pSampleData, m_pWaveLib->WaveSample.Size ) )
	{
		LocalFree( m_pWaveLib->WaveSample.pSampleData );
		m_pWaveLib->WaveSample.pSampleData = NULL;
		m_pWaveLib->WaveSample.Size = 0;

		mmioClose( hMMIO, 0 );
		return FALSE;
	}
	mmioClose( hMMIO, 0 );

	return TRUE;
}

DWORD CWavePlayer::WaveLib_Init( LPTSTR pAudioFile, BOOL bPause )
{
	if ( NULL != m_pWaveLib )
		WaveLib_UnInit();

	m_pWaveLib = ( PWAVELIB )LocalAlloc( LMEM_ZEROINIT, sizeof( WAVELIB ) );

	if ( NULL != m_pWaveLib )
	{
		m_pWaveLib->bPaused = bPause;

		if ( WaveLib_ReadWaveFile( pAudioFile ) )
		{
			if ( MMSYSERR_NOERROR != waveOutOpen( &m_pWaveLib->hWaveOut, WAVE_MAPPER, &m_pWaveLib->WaveSample.WaveFormatEx, ( UINT )WaveLib_WaveOutputCallback, ( ULONG )m_pWaveLib, CALLBACK_FUNCTION ) )
			{
				WaveLib_UnInit();
				m_pWaveLib = NULL;

				return 3;
			}
			else
			{
				if ( m_pWaveLib->bPaused )
				{
					waveOutPause( m_pWaveLib->hWaveOut );
				}

				WaveLib_CreateThread();
			}
		}
		else
		{
			WaveLib_UnInit();
			m_pWaveLib = NULL;

			return 2;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

BOOL CWavePlayer::WaveLib_ReadPCMFile( LPTSTR pFileName )
{
	HANDLE hFile = CreateFile( pFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( INVALID_HANDLE_VALUE == hFile )
	{
		return FALSE;
	}

	DWORD dwSize = GetFileSize( hFile, NULL );
	if ( 0xFFFFFFFF == dwSize )
	{
		CloseHandle( hFile );
		return FALSE;
	}

	m_pWaveLib->WaveSample.Size = dwSize;

	m_pWaveLib->WaveSample.pSampleData = (char *)LocalAlloc( LMEM_ZEROINIT, m_pWaveLib->WaveSample.Size );
	if ( NULL == m_pWaveLib->WaveSample.pSampleData )
	{
		CloseHandle( hFile );
		return FALSE;
	}

	DWORD dwReadOfBytes = 0;
	if ( 0 == ReadFile( hFile, m_pWaveLib->WaveSample.pSampleData, m_pWaveLib->WaveSample.Size, &dwReadOfBytes, NULL ) )
	{
		LocalFree( m_pWaveLib->WaveSample.pSampleData );
		m_pWaveLib->WaveSample.pSampleData = NULL;
		m_pWaveLib->WaveSample.Size = 0;

		CloseHandle( hFile );
		return FALSE;
	}

	m_pWaveLib->WaveSample.Size = dwReadOfBytes;

	CloseHandle( hFile );

	return TRUE;
}

DWORD CWavePlayer::WaveLib_Init( LPTSTR pAudioFile, WAVEFORMATEX& waveFormat, BOOL bPause )
{
	if ( NULL != m_pWaveLib )
		WaveLib_UnInit();

	m_pWaveLib = ( PWAVELIB )LocalAlloc( LMEM_ZEROINIT, sizeof( WAVELIB ) );

	if ( NULL != m_pWaveLib )
	{
		m_pWaveLib->bPaused = bPause;

		memcpy_s( &m_pWaveLib->WaveSample.WaveFormatEx, sizeof( WAVEFORMATEX ), &waveFormat, sizeof( WAVEFORMATEX ) );

		if ( WaveLib_ReadPCMFile( pAudioFile ) )
		{
			if ( MMSYSERR_NOERROR != waveOutOpen( &m_pWaveLib->hWaveOut, WAVE_MAPPER, &m_pWaveLib->WaveSample.WaveFormatEx, ( UINT )WaveLib_WaveOutputCallback, ( ULONG )m_pWaveLib, CALLBACK_FUNCTION ) )
			{
				WaveLib_UnInit();
				m_pWaveLib = NULL;

				return 3;
			}
			else
			{

				if ( m_pWaveLib->bPaused )
				{
					waveOutPause( m_pWaveLib->hWaveOut );
				}
				WaveLib_CreateThread();
			}
		}
		else
		{
			WaveLib_UnInit();
			m_pWaveLib = NULL;

			return 2;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

void CWavePlayer::WaveLib_CreateThread()
{
	DWORD dwThreadId;

	m_pWaveLib->hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

	m_pWaveLib->hThread = CreateThread( NULL, 0, WaveLib_AudioThread, ( LPVOID )this, 0, &dwThreadId );
}

void CWavePlayer::WaveLib_SetupAudio()
{
	UINT Index = 0;
	for( Index = 0 ; Index < Num_of_Buffer ; Index++ )
	{
		m_pWaveLib->WaveHdr[Index].dwBufferLength = Default_Sample_Size;
		m_pWaveLib->WaveHdr[Index].lpData = m_pWaveLib->AudioBuffer[Index];
		waveOutPrepareHeader( m_pWaveLib->hWaveOut, &m_pWaveLib->WaveHdr[Index], sizeof( WAVEHDR ) );

		WaveLib_AudioBuffer( Index );

		waveOutWrite( m_pWaveLib->hWaveOut, &m_pWaveLib->WaveHdr[Index], sizeof( WAVEHDR ) );
	}
}

void CWavePlayer::WaveLib_AudioBuffer( UINT Index )
{
	UINT uiBytesNotUsed = Default_Sample_Size;

	m_pWaveLib->WaveHdr[Index].dwFlags &= ~WHDR_DONE;

	if ( m_pWaveLib->WaveSample.Size - m_pWaveLib->WaveSample.Index == 0 )
	{
		uiBytesNotUsed = 0;
		memset( m_pWaveLib->AudioBuffer[Index], 0x00, Default_Sample_Size );
		return ;
	}
	else if( m_pWaveLib->WaveSample.Size - m_pWaveLib->WaveSample.Index < Default_Sample_Size )
	{
		memcpy( m_pWaveLib->AudioBuffer[Index], m_pWaveLib->WaveSample.pSampleData + m_pWaveLib->WaveSample.Index, m_pWaveLib->WaveSample.Size - m_pWaveLib->WaveSample.Index );

		uiBytesNotUsed = m_pWaveLib->WaveSample.Size - m_pWaveLib->WaveSample.Index;
		m_pWaveLib->WaveSample.Index = m_pWaveLib->WaveSample.Size;
	}
	else
	{
		memcpy( m_pWaveLib->AudioBuffer[Index], m_pWaveLib->WaveSample.pSampleData + m_pWaveLib->WaveSample.Index, Default_Sample_Size );

		m_pWaveLib->WaveSample.Index += Default_Sample_Size;
		uiBytesNotUsed = Default_Sample_Size;
	}

	m_pWaveLib->WaveHdr[Index].lpData = m_pWaveLib->AudioBuffer[Index];
	m_pWaveLib->WaveHdr[Index].dwBufferLength = uiBytesNotUsed;
}

void CWavePlayer::WaveLib_UnInit()
{
	if ( m_pWaveLib )
	{
		if( m_pWaveLib->hThread )
		{
			m_pWaveLib->bWaveShouldDie = TRUE;

			SetEvent( m_pWaveLib->hEvent );
			WaitForSingleObject( m_pWaveLib->hThread, INFINITE );

			CloseHandle( m_pWaveLib->hEvent );
			CloseHandle( m_pWaveLib->hThread );
		}

		if ( m_pWaveLib->hWaveOut )
		{
			waveOutClose( m_pWaveLib->hWaveOut );
		}

		if ( m_pWaveLib->WaveSample.pSampleData )
		{
			LocalFree( m_pWaveLib->WaveSample.pSampleData );
			m_pWaveLib->WaveSample.pSampleData = NULL;
		}

		LocalFree( m_pWaveLib );
		m_pWaveLib = NULL;
	}

	WaveLib_WriteLog( _T("[TRACE] WaveUninit \r\n") );
}

void CALLBACK CWavePlayer::WaveLib_WaveOutputCallback( HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 )
{
	PWAVELIB pWaveLib = ( PWAVELIB )dwInstance;
	switch ( uMsg )
	{
	case WOM_OPEN:
		break;
	case WOM_DONE:
		SetEvent( pWaveLib->hEvent );
		break;
	case WOM_CLOSE:
		break;
	}
}

DWORD WINAPI CWavePlayer::WaveLib_AudioThread( PVOID pDataInput )
{
	CWavePlayer* pWavePlayer = (CWavePlayer*)pDataInput;

	CWavePlayer::PWAVELIB pWaveLib = pWavePlayer->m_pWaveLib;
	UINT idx;

	pWavePlayer->WaveLib_SetupAudio();

	while( !pWaveLib->bWaveShouldDie && pWaveLib->WaveSample.Size > pWaveLib->WaveSample.Index )
	{
		WaitForSingleObject( pWaveLib->hEvent, INFINITE );

		for ( idx = 0 ; idx < Num_of_Buffer ; idx++ )
		{
			if ( pWaveLib->WaveHdr[idx].dwFlags & WHDR_DONE )
			{
				pWavePlayer->WaveLib_AudioBuffer( idx );
				waveOutWrite( pWaveLib->hWaveOut, &pWaveLib->WaveHdr[idx], sizeof( WAVEHDR ) );
			}
		}
	}
	waveOutReset( pWaveLib->hWaveOut );

	SetEvent( pWavePlayer->m_pMainThread->m_hStopEvent );

	return 0;
}

void CWavePlayer::WaveLib_Pause( BOOL bPause )
{
	if ( NULL != m_pWaveLib )
	{
		m_pWaveLib->bPaused = bPause;

		if( m_pWaveLib->bPaused )
		{
			waveOutPause( m_pWaveLib->hWaveOut );
		}
		else
		{
			waveOutRestart( m_pWaveLib->hWaveOut );
		}
	}
}

void CWavePlayer::WaveLib_WriteLog( TCHAR* szLog, ... )
{
	va_list ap;
	va_start( ap, szLog );

	TCHAR buf[512] = {0, };

	_vsntprintf_s( buf, _countof( buf ), _TRUNCATE, szLog, ap );

	TRACE( buf );

	va_end( ap );
}