#include "stdafx.h"
#include <crtdbg.h>
#include <process.h>
#include <mmreg.h>
#include <initguid.h>
#include "directx_audio.h"
#include "audio_output.h"
#include "trace.h"

#define FRAME_SIZE ((int)m_output.i_rate/20) /* Size in samples */
#define FRAMES_NUM 8                                      /* Needs to be > 3 */

static const UINT32 pi_channels_src[] =
    { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
      AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
      AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
      AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0 };
static const UINT32 pi_channels_in[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
      SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT,
      SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY, 0 };
static const UINT32 pi_channels_out[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
      SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT,
      SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT, 0 };

DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );

unsigned WINAPI DirectSoundThreadFunc(PVOID pvoid) {
	CDirectXAudio *p = (CDirectXAudio *)pvoid;
	p->DirectSoundThread();
	_endthreadex(0);
	return 0;
}

CDirectXAudio::CDirectXAudio(tme_object_t *pIpc, CAudioOutput *pAout, audio_format_t *pFormat)
					: CAoutOutput(pIpc, pAout, pFormat)
{
	int ret;

	//tme_thread_init(&m_ipc);
	m_hThread = NULL;
	ret = OpenAudio();
	if (ret == 0)
		PostInit();
}

CDirectXAudio::~CDirectXAudio()
{
	_RPT0(_CRT_WARN, "~CDirecXAudio()\n");
	CloseAudio();
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

int CDirectXAudio::OpenAudio()
{
    _RPT0(_CRT_WARN, "OpenAudio\n" );

    /* Initialize some variables */
    m_pDsobject = NULL;
    m_pDsbuffer = NULL;
    m_bPlaying = 0;

    //aout_VolumeSoftInit( m_pAout );

    m_iDeviceId = 0;
    m_pDeviceGuid = NULL;

    /* Initialise DirectSound */
    if( InitDirectSound() )
    {
        _RPT0(_CRT_WARN, "cannot initialize DirectSound\n" );
        goto error;
    }

    if( m_pAout->m_iAudioDevice < 0 )
    {
        Probe();
    }

    if( m_pAout->m_iAudioDevice < 0 )
    {
        goto error;
    }

    /* Open the device */
    if( m_pAout->m_iAudioDevice == AOUT_VAR_SPDIF )
    {
        m_output.i_format = TME_FOURCC('s','p','d','i');

        /* Calculate the frame size in bytes */
        m_iNbSamples = A52_FRAME_NB;
        m_output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        m_output.i_frame_length = A52_FRAME_NB;
        m_iFrameSize =
            m_output.i_bytes_per_frame;

        if( CreateDSBuffer( TME_FOURCC('s','p','d','i'),
                            m_output.i_physical_channels,
                            aout_FormatNbChannels( &m_output ),
                            m_output.i_rate,
                            m_iFrameSize, false )
            != SUCCESS )
        {
            _RPT0(_CRT_WARN, "cannot open directx audio device\n");
            return EGENERIC;
        }

        //aout_VolumeNoneInit( m_pAout );
    }
    else
    {
        if( m_pAout->m_iAudioDevice == AOUT_VAR_5_1 )
        {
            m_output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                   | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                   | AOUT_CHAN_LFE;
        }
        else if( m_pAout->m_iAudioDevice == AOUT_VAR_3F2R )
        {
            m_output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                   | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        }
        else if( m_pAout->m_iAudioDevice == AOUT_VAR_2F2R )
        {
            m_output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                   | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        }
        else if( m_pAout->m_iAudioDevice == AOUT_VAR_MONO )
        {
            m_output.i_physical_channels = AOUT_CHAN_CENTER;
        }
        else
        {
            m_output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        }

        if( CreateDSBufferPCM( &(m_output.i_format),
                               m_output.i_physical_channels,
                               aout_FormatNbChannels( &m_output ),
                               m_output.i_rate, false )
            != SUCCESS )
        {
            _RPT0(_CRT_WARN, "cannot open directx audio device\n");
            return EGENERIC;
        }

        /* Calculate the frame size in bytes */
        m_iNbSamples = FRAME_SIZE;
        aout_FormatPrepare( &m_output );
        //aout_VolumeSoftInit( m_pAout );
    }

	if (StartDirectSoundThread()) {
        goto error;
	}

	return SUCCESS;

 error:
    CloseAudio();
    return EGENERIC;
}

int CDirectXAudio::StartDirectSoundThread()
{
    /* Now we need to setup our DirectSound play notification structure */
	m_notif.p_aout = m_pAout;
	m_notif.b_die = false;

    m_notif.hEvent = CreateEvent( 0, FALSE, FALSE, 0 );
    m_notif.i_frame_size = m_iFrameSize;

    /* then launch the notification thread */
    _RPT0(_CRT_WARN, "creating DirectSoundThread\n");
	m_hThread = tme_thread_create( m_pIpc, "DirectSoundThread",
									DirectSoundThreadFunc, this,
									TME_THREAD_PRIORITY_HIGHEST, false );
	if (m_hThread == NULL) {
		_RPT0(_CRT_WARN, "cannot create DirectSoundThread\n" );
        CloseHandle( m_notif.hEvent );
        return 1;
	}

	return 0;
}

void CDirectXAudio::StopDirectSoundThread()
{
    /* kill the position notification thread, if any */
	if( m_hThread )
	{    
		m_notif.b_die = true;

		/* wake up the audio thread if needed */
		if( !m_bPlaying ) SetEvent( m_notif.hEvent );

		tme_thread_join( m_hThread );
	}
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
void CDirectXAudio::CloseAudio()
{
    _RPT0(_CRT_WARN, "closing audio device\n" );

	StopDirectSoundThread();

    /* release the secondary buffer */
    DestroyDSBuffer();

    /* finally release the DirectSound object */
    if( m_pDsobject ) IDirectSound_Release( m_pDsobject );
    
    /* free DSOUND.DLL */
    if( m_hdsoundDll ) FreeLibrary( m_hdsoundDll );

    if( m_pDeviceGuid )
        free( m_pDeviceGuid );
}

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
void CDirectXAudio::Probe()
{
    UINT32 i_format;
    unsigned int i_physical_channels;
    DWORD ui_speaker_config;

    /* Test for 5.1 support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                          AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
    if( m_output.i_physical_channels == i_physical_channels )
    {
        if( CreateDSBufferPCM( &i_format, i_physical_channels, 6,
                               m_output.i_rate, true )
            == SUCCESS )
        {
			m_pAout->m_iAudioDevice = AOUT_VAR_5_1;
            _RPT0(_CRT_WARN, "device supports 5.1 channels\n");
        }
    }

    /* Test for 3 Front 2 Rear support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                          AOUT_CHAN_REARRIGHT;
    if( m_output.i_physical_channels == i_physical_channels )
    {
        if( CreateDSBufferPCM( &i_format, i_physical_channels, 5,
                               m_output.i_rate, true )
            == SUCCESS )
        {
            m_pAout->m_iAudioDevice = AOUT_VAR_3F2R;
            _RPT0(_CRT_WARN, "device supports 5 channels\n");
        }
    }

    /* Test for 2 Front 2 Rear support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    if( ( m_output.i_physical_channels & i_physical_channels )
        == i_physical_channels )
    {
        if( CreateDSBufferPCM( &i_format, i_physical_channels, 4,
                               m_output.i_rate, true )
            == SUCCESS )
        {
            m_pAout->m_iAudioDevice =  AOUT_VAR_2F2R;
            _RPT0(_CRT_WARN, "device supports 4 channels\n");
        }
    }

    /* Test for stereo support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    if( CreateDSBufferPCM( &i_format, i_physical_channels, 2,
                           m_output.i_rate, true )
        == SUCCESS )
    {
        m_pAout->m_iAudioDevice =  AOUT_VAR_STEREO;
        _RPT0(_CRT_WARN, "device supports 2 channels\n");
    }

    /* Test for mono support */
    i_physical_channels = AOUT_CHAN_CENTER;
    if( CreateDSBufferPCM( &i_format, i_physical_channels, 1,
                           m_output.i_rate, true )
        == SUCCESS )
    {
        m_pAout->m_iAudioDevice =  AOUT_VAR_MONO;
        _RPT0(_CRT_WARN, "device supports 1 channel\n");
    }

    /* Check the speaker configuration to determine which channel config should
     * be the default */
    if FAILED( IDirectSound_GetSpeakerConfig( m_pDsobject,
                                              &ui_speaker_config ) )
    {
        ui_speaker_config = DSSPEAKER_STEREO;
    }
    switch( DSSPEAKER_CONFIG(ui_speaker_config) )
    {
    case DSSPEAKER_5POINT1:
        m_pAout->m_iAudioDevice =  AOUT_VAR_5_1;
        break;
    case DSSPEAKER_QUAD:
        m_pAout->m_iAudioDevice =  AOUT_VAR_2F2R;
        break;
    case DSSPEAKER_MONO:
        m_pAout->m_iAudioDevice = AOUT_VAR_MONO;
        break;
    case DSSPEAKER_SURROUND:
    case DSSPEAKER_STEREO:
    default:
        m_pAout->m_iAudioDevice = AOUT_VAR_STEREO;
        break;
    }

    /* Test for SPDIF support */
    if ( AOUT_FMT_NON_LINEAR( &m_output ) )
    {
        if( CreateDSBuffer( TME_FOURCC('s','p','d','i'),
                            m_output.i_physical_channels,
                            aout_FormatNbChannels( &m_output ),
                            m_output.i_rate,
                            AOUT_SPDIF_SIZE, true )
            == SUCCESS )
        {
            _RPT0(_CRT_WARN, "device supports A/52 over S/PDIF\n");
            //m_pAout->m_iAudioDevice = AOUT_VAR_SPDIF;
        }
    }
}

/*****************************************************************************
 * Play: we'll start playing the directsound buffer here because at least here
 *       we know the first buffer has been put in the aout fifo and we also
 *       know its date.
 *****************************************************************************/
void CDirectXAudio::Play(bool bRestart)
{
	//_RPT1(_CRT_WARN, "CDirectXAudio::Play(%d)\n", m_bPlaying);
    if( !m_bPlaying )
    {
        aout_buffer_t *p_buffer;
        int i;

        m_bPlaying = 1;

        /* get the playing date of the first aout buffer */
        m_notif.start_date =
            aout_FifoFirstDate( m_pAout, &m_fifo );

        /* fill in the first samples */
        for( i = 0; i < FRAMES_NUM; i++ )
        {
            p_buffer = aout_FifoPop( m_pAout, &m_fifo );
            if( !p_buffer ) break;
            FillBuffer( i, p_buffer );
        }

        /* wake up the audio output thread */
        SetEvent( m_notif.hEvent );
	} else if (bRestart) { /* nice try, but didn't help a lot */
		StopDirectSoundThread();
		StartDirectSoundThread();
        aout_buffer_t *p_buffer;
        int i;

        m_bPlaying = 1;

        /* get the playing date of the first aout buffer */
        m_notif.start_date =
            aout_FifoFirstDate( m_pAout, &m_fifo );

        /* fill in the first samples */
        for( i = 0; i < FRAMES_NUM; i++ )
        {
            p_buffer = aout_FifoPop( m_pAout, &m_fifo );
            if( !p_buffer ) break;
            FillBuffer( i, p_buffer );
        }

        /* wake up the audio output thread */
        SetEvent( m_notif.hEvent );
	}
}

/*****************************************************************************
 * CallBackDirectSoundEnum: callback to enumerate available devices
 *****************************************************************************/
static BOOL CALLBACK CallBackDirectSoundEnum( LPGUID p_guid, LPCTSTR psz_desc,
                                             LPCTSTR psz_mod, LPVOID _p_aout )
{
    CDirectXAudio *p_aout = (CDirectXAudio *)_p_aout;

    TRACE(_T("found device: %s\n"), psz_desc );

	if( p_aout->m_iDeviceId == 0 && p_guid )
    {
        p_aout->m_pDeviceGuid = (LPGUID)malloc( sizeof( GUID ) );
        *(p_aout->m_pDeviceGuid) = *p_guid;
        TRACE(_T("using device: %s\n"), psz_desc );
    }

    p_aout->m_iDeviceId--;
    return 1;
}

/*****************************************************************************
 * InitDirectSound: handle all the gory details of DirectSound initialisation
 *****************************************************************************/
int CDirectXAudio::InitDirectSound()
{
    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
    HRESULT (WINAPI *OurDirectSoundEnumerate)(LPDSENUMCALLBACK, LPVOID);

    m_hdsoundDll = LoadLibrary(_T("DSOUND.DLL"));
    if( m_hdsoundDll == NULL )
    {
        _RPT0(_CRT_WARN, "cannot open DSOUND.DLL\n" );
        goto error;
    }

    OurDirectSoundCreate = (HRESULT (WINAPI *)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN))
        GetProcAddress( m_hdsoundDll,
                        "DirectSoundCreate" );
    if( OurDirectSoundCreate == NULL )
    {
        _RPT0(_CRT_WARN, "GetProcAddress FAILED\n" );
        goto error;
    }

    /* Get DirectSoundEnumerate */
    OurDirectSoundEnumerate = (HRESULT (WINAPI *)(LPDSENUMCALLBACK, LPVOID))
       GetProcAddress( m_hdsoundDll,
#ifdef UNICODE
                       "DirectSoundEnumerateW" );
#else
                       "DirectSoundEnumerateA" );
#endif
    if( OurDirectSoundEnumerate )
    {
        /* Attempt enumeration */
        if( FAILED( OurDirectSoundEnumerate( (LPDSENUMCALLBACK)CallBackDirectSoundEnum, 
                                             this ) ) )
        {
            _RPT0(_CRT_WARN, "enumeration of DirectSound devices failed\n" );
        }
    }

    /* Create the direct sound object */
    if FAILED( OurDirectSoundCreate( m_pDeviceGuid, 
                                     &m_pDsobject,
                                     NULL ) )
    {
        _RPT0(_CRT_WARN, "cannot create a direct sound device\n");
        goto error;
    }

    /* Set DirectSound Cooperative level, ie what control we want over Windows
     * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
     * settings of the primary buffer, but also that only the sound of our
     * application will be hearable when it will have the focus.
     * !!! (this is not really working as intended yet because to set the
     * cooperative level you need the window handle of your application, and
     * I don't know of any easy way to get it. Especially since we might play
     * sound without any video, and so what window handle should we use ???
     * The hack for now is to use the Desktop window handle - it seems to be
     * working */
    if( IDirectSound_SetCooperativeLevel( m_pDsobject,
                                          GetDesktopWindow(),
                                          DSSCL_EXCLUSIVE) )
    {
        _RPT0(_CRT_WARN, "cannot set direct sound cooperative level\n");
    }

    return SUCCESS;

 error:
    m_pDsobject = NULL;
    if( m_hdsoundDll )
    {
        FreeLibrary( m_hdsoundDll );
        m_hdsoundDll = NULL;
    }
    return EGENERIC;

}

/*****************************************************************************
 * CreateDSBuffer: Creates a direct sound buffer of the required format.
 *****************************************************************************
 * This function creates the buffer we'll use to play audio.
 * In DirectSound there are two kinds of buffers:
 * - the primary buffer: which is the actual buffer that the soundcard plays
 * - the secondary buffer(s): these buffers are the one actually used by
 *    applications and DirectSound takes care of mixing them into the primary.
 *
 * Once you create a secondary buffer, you cannot change its format anymore so
 * you have to release the current one and create another.
 *****************************************************************************/
int CDirectXAudio::CreateDSBuffer( int i_format,
                           int i_channels, int i_nb_channels, int i_rate,
                           int i_bytes_per_frame, bool b_probe )
{
    WAVEFORMATEXTENSIBLE waveformat;
    DSBUFFERDESC         dsbdesc;
    unsigned int         i;

    /* First set the sound buffer format */
    waveformat.dwChannelMask = 0;
    for( i = 0; i < sizeof(pi_channels_src)/sizeof(UINT32); i++ )
    {
        if( i_channels & pi_channels_src[i] )
            waveformat.dwChannelMask |= pi_channels_in[i];
    }

    switch( i_format )
    {
    case TME_FOURCC('s','p','d','i'):
        i_nb_channels = 2;
        /* To prevent channel re-ordering */
        waveformat.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case TME_FOURCC('f','l','3','2'):
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case TME_FOURCC('s','1','6','l'):
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_PCM;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_PCM;
        break;
    }

    waveformat.Format.nChannels = i_nb_channels;
    waveformat.Format.nSamplesPerSec = i_rate;
    waveformat.Format.nBlockAlign =
        waveformat.Format.wBitsPerSample / 8 * i_nb_channels;
    waveformat.Format.nAvgBytesPerSec =
        waveformat.Format.nSamplesPerSec * waveformat.Format.nBlockAlign;

    m_iBitsPerSample = waveformat.Format.wBitsPerSample;
    m_iChannels = i_nb_channels;

    /* Then fill in the direct sound descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2/* Better position accuracy */
                    | DSBCAPS_GLOBALFOCUS;      /* Allows background playing */

    /* Only use the new WAVE_FORMAT_EXTENSIBLE format for multichannel audio */
    if( i_nb_channels <= 2 )
    {
        waveformat.Format.cbSize = 0;
    }
    else
    {
        waveformat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        waveformat.Format.cbSize =
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

        /* Needed for 5.1 on emu101k */
        dsbdesc.dwFlags |= DSBCAPS_LOCHARDWARE;
    }

    dsbdesc.dwBufferBytes = FRAMES_NUM * i_bytes_per_frame;   /* buffer size */
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&waveformat;

    if FAILED( IDirectSound_CreateSoundBuffer(
                   m_pDsobject, &dsbdesc,
                   &m_pDsbuffer, NULL) )
    {
        if( dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE )
        {
            /* Try without DSBCAPS_LOCHARDWARE */
            dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
            if FAILED( IDirectSound_CreateSoundBuffer(
                   m_pDsobject, &dsbdesc,
                   &m_pDsbuffer, NULL) )
            {
                return EGENERIC;
            }
            if( !b_probe )
                _RPT0(_CRT_WARN, "couldn't use hardware sound buffer\n");
        }
        else
        {
            return EGENERIC;
        }
    }

    /* Stop here if we were just probing */
    if( b_probe )
    {
        IDirectSoundBuffer_Release( m_pDsbuffer );
        m_pDsbuffer = NULL;
        return SUCCESS;
    }

    m_iFrameSize = i_bytes_per_frame;
    m_iChannelMask = waveformat.dwChannelMask;
    m_bChanReorder =
        aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                  waveformat.dwChannelMask, i_nb_channels,
								  m_piChanTable ) == 0 ? false : true;

    if( m_bChanReorder )
    {
        _RPT0(_CRT_WARN, "channel reordering needed\n");
    }

    return SUCCESS;
}

/*****************************************************************************
 * CreateDSBufferPCM: creates a PCM direct sound buffer.
 *****************************************************************************
 * We first try to create a WAVE_FORMAT_IEEE_FLOAT buffer if supported by
 * the hardware, otherwise we create a WAVE_FORMAT_PCM buffer.
 ****************************************************************************/
int CDirectXAudio::CreateDSBufferPCM( UINT32 *i_format,
                              int i_channels, int i_nb_channels, int i_rate,
                              bool b_probe )
{
    bool directxAudioFloat32 = false;

    /* Float32 audio samples are not supported for 5.1 output on the emu101k */

    if( !directxAudioFloat32 || i_nb_channels > 2 ||
        CreateDSBuffer( TME_FOURCC('f','l','3','2'),
                        i_channels, i_nb_channels, i_rate,
                        FRAME_SIZE * 4 * i_nb_channels, b_probe )
        != SUCCESS )
    {
        if ( CreateDSBuffer( TME_FOURCC('s','1','6','l'),
                             i_channels, i_nb_channels, i_rate,
                             FRAME_SIZE * 2 * i_nb_channels, b_probe )
             != SUCCESS )
        {
            return EGENERIC;
        }
        else
        {
            *i_format = TME_FOURCC('s','1','6','l');
            return SUCCESS;
        }
    }
    else
    {
        *i_format = TME_FOURCC('f','l','3','2');
        return SUCCESS;
    }
}

/*****************************************************************************
 * DestroyDSBuffer
 *****************************************************************************
 * This function destroys the secondary buffer.
 *****************************************************************************/
void CDirectXAudio::DestroyDSBuffer()
{
    if( m_pDsbuffer )
    {
        IDirectSoundBuffer_Release( m_pDsbuffer );
        m_pDsbuffer = NULL;
    }
}

/*****************************************************************************
 * FillBuffer: Fill in one of the direct sound frame buffers.
 *****************************************************************************
 * Returns SUCCESS on success.
 *****************************************************************************/
int CDirectXAudio::FillBuffer( int i_frame, aout_buffer_t *p_buffer )
{
    void *p_write_position, *p_wrap_around;
    DWORD l_bytes1, l_bytes2;
    HRESULT dsresult;

    /* Before copying anything, we have to lock the buffer */
    dsresult = IDirectSoundBuffer_Lock(
                m_pDsbuffer,                              /* DS buffer */
                i_frame * m_notif.i_frame_size,             /* Start offset */
                m_notif.i_frame_size,                    /* Number of bytes */
                &p_write_position,                  /* Address of lock start */
                &l_bytes1,       /* Count of bytes locked before wrap around */
                &p_wrap_around,            /* Buffer adress (if wrap around) */
                &l_bytes2,               /* Count of bytes after wrap around */
                0 );                                                /* Flags */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( m_pDsbuffer );
        dsresult = IDirectSoundBuffer_Lock(
                               m_pDsbuffer,
                               i_frame * m_notif.i_frame_size,
                               m_notif.i_frame_size,
                               &p_write_position,
                               &l_bytes1,
                               &p_wrap_around,
                               &l_bytes2,
                               0 );
    }
    if( dsresult != DS_OK )
    {
        _RPT0(_CRT_WARN, "cannot lock buffer\n");
        if( p_buffer ) aout_BufferFree( p_buffer );
        return EGENERIC;
    }

    if( p_buffer == NULL )
    {
        memset( p_write_position, 0, l_bytes1 );
    }
    else
    {
        if( m_bChanReorder )
        {
            /* Do the channel reordering here */
            aout_ChannelReorder( p_buffer->p_buffer, (int)p_buffer->i_nb_bytes,
                                 m_iChannels, m_piChanTable,
                                 m_iBitsPerSample );
        }

        memcpy( p_write_position, p_buffer->p_buffer,
                                  l_bytes1 );
        aout_BufferFree( p_buffer );
    }

    /* Now the data has been copied, unlock the buffer */
    IDirectSoundBuffer_Unlock( m_pDsbuffer, p_write_position, l_bytes1,
                               p_wrap_around, l_bytes2 );

    m_notif.i_write_slot = (i_frame + 1) % FRAMES_NUM;
    return SUCCESS;
}

/*****************************************************************************
 * DirectSoundThread: this thread will capture play notification events. 
 *****************************************************************************
 * We use this thread to emulate a callback mechanism. The thread probes for
 * event notification and fills up the DS secondary buffer when needed.
 *****************************************************************************/
void CDirectXAudio::DirectSoundThread()
{
    bool b_sleek;
    mtime_t last_time;
    HRESULT dsresult;
    long l_queued = 0;
	int missed = 0;

    /* We don't want any resampling when using S/PDIF output */
    b_sleek = m_output.i_format == TME_FOURCC('s','p','d','i');

    /* Tell the main thread that we are ready */
    //tme_thread_ready( p_notif );

    _RPT0(_CRT_WARN, "DirectSoundThread ready\n");

    /* Wait here until Play() is called */
    WaitForSingleObject( m_notif.hEvent, INFINITE );

    if( !m_notif.b_die )
    {
        mwait( m_notif.start_date - AOUT_PTS_TOLERANCE / 2 );

        /* start playing the buffer */
        dsresult = IDirectSoundBuffer_Play( m_pDsbuffer,
                                        0,                         /* Unused */
                                        0,                         /* Unused */
                                        DSBPLAY_LOOPING );          /* Flags */
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( m_pDsbuffer );
            dsresult = IDirectSoundBuffer_Play(
                                            m_pDsbuffer,
                                            0,                     /* Unused */
                                            0,                     /* Unused */
                                            DSBPLAY_LOOPING );      /* Flags */
        }
        if( dsresult != DS_OK )
        {
            _RPT0(_CRT_WARN, "cannot start playing buffer\n");
        }
    }
    last_time = mdate();

    while( !m_notif.b_die )
    {
        DWORD l_read, l_free_slots;
        mtime_t mtime = mdate();
        int i;

        /*
         * Fill in as much audio data as we can in our circular buffer
         */

        /* Find out current play position */
        if FAILED( IDirectSoundBuffer_GetCurrentPosition(
                   m_pDsbuffer, &l_read, NULL ) )
        {
            _RPT0(_CRT_WARN, "GetCurrentPosition() failed!\n");
            l_read = 0;
        }
		//_RPT3(_CRT_WARN, "CurrentPosition:%u,last:%I64d,now:%I64d\n",l_read,last_time,mtime);

        /* Detect underruns */
        if( l_queued && mtime - last_time >
            1000000LL * l_queued / m_output.i_rate )
        {
            _RPT0(_CRT_WARN, "detected underrun!\n");
        }
        last_time = mtime;

		//_RPT3(_CRT_WARN, "i_write_slot:%d,i_bytes_per_frame:%u,FRAME_SIZE:%d\n",m_notif.i_write_slot,m_output.i_bytes_per_frame,FRAME_SIZE);
		/* [Info]i_bytes_per_frame:4, i_rate:8000, FRAME_SIZE:400 */

		/* Try to fill in as many frame buffers as possible */
        l_read /= m_output.i_bytes_per_frame; 
        l_queued = m_notif.i_write_slot * FRAME_SIZE - l_read;
        if( l_queued < 0 ) l_queued += (FRAME_SIZE * FRAMES_NUM);
        l_free_slots = (FRAMES_NUM * FRAME_SIZE - l_queued) / FRAME_SIZE;

        for( i = 0; i < (int)l_free_slots; i++ )
        {
			//_RPT4(_CRT_WARN, "***%I64d,%d,%d,%u\n",mtime,i,l_queued,m_output.i_rate);
            aout_buffer_t *p_buffer = OutputNextBuffer( mtime + 1000000LL * (i * FRAME_SIZE + l_queued) /
                m_output.i_rate, b_sleek);

			if (!p_buffer) {
				missed++;
			} else {
				missed = 0;
			}

            /* If there is no audio data available and we have some buffered
             * already, then just wait for the next time */
			if( !p_buffer && (i || l_queued / FRAME_SIZE) ) {
				//_RPT4(_CRT_WARN, "***%p,%d,%d,%d\n",p_buffer,i,l_queued,FRAME_SIZE);
				break;
			}

            if( FillBuffer( m_notif.i_write_slot % FRAMES_NUM,
				p_buffer ) != SUCCESS ) {
					break;
			}
        }

        /* Sleep a reasonable amount of time */
        l_queued += (i * FRAME_SIZE);
        msleep( I64C(1000000) * l_queued / m_output.i_rate / 2 );
    }

    /* make sure the buffer isn't playing */
    IDirectSoundBuffer_Stop( m_pDsbuffer );

    /* free the event */
    CloseHandle( m_notif.hEvent );

    _RPT0(_CRT_WARN, "DirectSoundThread exiting\n");
}

