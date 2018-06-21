#ifndef _WAVE_PLAYER_CLASS_
#define _WAVE_PLAYER_CLASS_

#include <MMSystem.h>
#include <stdlib.h>
#include <stdarg.h>

class CWavePlayer
{
private:
	enum
	{ 
		Default_Sample_Size = 2205,
		Num_of_Buffer = 5
	};

	typedef struct _wave_sample {
		WAVEFORMATEX WaveFormatEx;
		char *pSampleData;
		UINT Index;
		UINT Size;
		DWORD dwId;
		DWORD bPlaying;
	} WAVE_SAMPLE, *PWAVE_SAMPLE;

	typedef struct {

		HWAVEOUT hWaveOut;
		HANDLE hEvent;
		HANDLE hThread;
		WAVE_SAMPLE WaveSample;
		BOOL bWaveShouldDie;
		WAVEHDR WaveHdr[Num_of_Buffer];
		char AudioBuffer[Num_of_Buffer][Default_Sample_Size];
		BOOL bPaused;
	} WAVELIB, *PWAVELIB;

	PWAVELIB m_pWaveLib;

	typedef struct _Control_Thread
	{
		HANDLE hMainThread;
		BOOL bThreadShouldDie;
		HANDLE hStartEvent;
		HANDLE m_hStopEvent;
	}Control_Thread, *PControl_Thread;

	PControl_Thread m_pMainThread;

public:
	void PlayWave()
	{
		SetEvent( m_pMainThread->hStartEvent );
	}
	void StopWave()
	{
		SetEvent( m_pMainThread->m_hStopEvent );
	}
	BOOL InitThread();
	void DeInitThread();
	DWORD WaveLib_Init( LPTSTR pAudioFile, BOOL bPause );	// Complete
	DWORD WaveLib_Init( LPTSTR pAudioFile, WAVEFORMATEX& waveFormat, BOOL bPause );
	void WaveLib_UnInit();	//Resource Free

	CWavePlayer() : m_pWaveLib( NULL ), m_pMainThread( NULL )
	{
	}
	~CWavePlayer()
	{
	}
	void WaveLib_Pause( BOOL bPause );

protected:
	BOOL WaveLib_ReadWaveFile( LPTSTR pFileName );
	BOOL WaveLib_ReadPCMFile( LPTSTR pFileName );
	void WaveLib_CreateThread();
	void WaveLib_SetupAudio();
	void WaveLib_AudioBuffer( UINT Index );

	//WaveOut Callback
	static void CALLBACK WaveLib_WaveOutputCallback( HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 );

	//WaveOut Thread
	static DWORD WINAPI WaveLib_AudioThread( PVOID pDataInput );
	static DWORD WINAPI WaveLib_MainThread( PVOID pDataInput );

	//Log Function
	void WaveLib_WriteLog( TCHAR* szLog, ... );
};

#endif