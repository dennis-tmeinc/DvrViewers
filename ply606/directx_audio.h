#ifndef _DIRECTX_AUDIO_H_
#define _DIRECTX_AUDIO_H_

#include "mtime.h"
#include <mmsystem.h>
#include <dsound.h>
#include "audio_output.h"
#include "aout_output.h"


typedef struct notification_thread_t
{
    CAudioOutput *p_aout;
    int i_frame_size;                          /* size in bytes of one frame */
    int i_write_slot;       /* current write position in our circular buffer */

    mtime_t start_date;
    HANDLE hEvent;
	bool b_die;
} notification_thread_t;


class CDirectXAudio : public CAoutOutput
{
public:
	CDirectXAudio(tme_object_t *pIpc, CAudioOutput *pAout, audio_format_t *pFormat);
	virtual ~CDirectXAudio();
	virtual void Play(bool bRestart);

	//tme_object_t m_ipc;
	HANDLE m_hThread;

	HINSTANCE           m_hdsoundDll;      /* handle of the opened dsound dll */

    int                 m_iDeviceId;                 /*  user defined device */
    LPGUID              m_pDeviceGuid;

    LPDIRECTSOUND       m_pDsobject;              /* main Direct Sound object */
    LPDIRECTSOUNDBUFFER m_pDsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */

    notification_thread_t m_notif;                  /* DirectSoundThread id */

    bool m_bPlaying;                                         /* playing status */

    int m_iFrameSize;                         /* Size in bytes of one frame */

    bool m_bChanReorder;              /* do we need channel reordering */
    int m_piChanTable[AOUT_CHAN_MAX];
    UINT32 m_iChannelMask;
    UINT32 m_iBitsPerSample;
    UINT32 m_iChannels;

	int OpenAudio();
	void CloseAudio();
	int InitDirectSound();
	void Probe();
	int CreateDSBufferPCM( UINT32 *i_format,
                              int i_channels, int i_nb_channels, int i_rate,
                              bool b_probe );
	int CreateDSBuffer( int i_format,
                           int i_channels, int i_nb_channels, int i_rate,
                           int i_bytes_per_frame, bool b_probe );
	void DestroyDSBuffer();
	int FillBuffer( int i_frame, aout_buffer_t *p_buffer );
	void DirectSoundThread();
	int StartDirectSoundThread();
	void StopDirectSoundThread();

};

#endif
