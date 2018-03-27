#ifndef _INPUT_STREAM_H_
#define _INPUT_STREAM_H_

#include "tme_thread.h"
#include "block.h"
#include "decoder.h"

enum input_control_e
{
    INPUT_CONTROL_SET_DIE,

    INPUT_CONTROL_SET_STATE,

    INPUT_CONTROL_SET_RATE,
    INPUT_CONTROL_SET_RATE_SLOWER,
    INPUT_CONTROL_SET_RATE_FASTER,

    INPUT_CONTROL_SET_POSITION,
    INPUT_CONTROL_SET_POSITION_OFFSET,

    INPUT_CONTROL_SET_TIME,
    INPUT_CONTROL_SET_TIME_OFFSET,

    INPUT_CONTROL_SET_AUDIO_DELAY,
    INPUT_CONTROL_SET_SPU_DELAY,

    INPUT_CONTROL_SET_FORWARD_SPEED,
    INPUT_CONTROL_SET_FORWARD_STEP,
    INPUT_CONTROL_SET_FORWARD_STEP_DELAYED,

    INPUT_CONTROL_SET_RESET_BUFFER
};

#define CR_MEAN_PTS_GAP 300000
#define CR_MAX_GAP 2000000
//#define OSD_TEST

enum /* Synchro states */
{
    SYNCHRO_OK     = 0,
    SYNCHRO_START  = 1,
    SYNCHRO_REINIT = 2,
};

typedef struct
{
    /* Synchronization information */
    mtime_t                 delta_cr; /* used to smooth a big change in system time */
    mtime_t                 cr_ref, sysdate_ref; /* cr_ref: (server_wallclock+11)*9/100
												  * sysdate_ref: time in mdate() format(high res sys timer) value when ref was created
												  */

    mtime_t                 last_sysdate;
    mtime_t                 last_cr; /* reference to detect unexpected stream
                                      * discontinuities                      */
    mtime_t                 last_pts;
    int                     i_synchro_state;

    bool              b_master;

    /* Config */
    int                     i_cr_average;
    int                     i_delta_cr_residue;

	bool b_added;
} input_clock_t;

/* "state" value */
enum input_state_e
{
    INIT_S,
    OPENING_S,
    BUFFERING_S,
    PLAYING_S,
    PAUSE_S,
    END_S,
    ERROR_S
};

/* "rate" default, min/max
 * A rate below 1000 plays the movie faster,
 * A rate above 1000 plays the movie slower.
 */
#define INPUT_RATE_DEFAULT  1000
#define INPUT_RATE_MIN       125            /* Up to 8/1 */
#define INPUT_RATE_MAX      8000            /* Up to 1/8 */

// dvr frame type
#define FRAME_TYPE_UNKNOWN		(0)
#define FRAME_TYPE_KEYVIDEO		(1)
#define FRAME_TYPE_VIDEO		(2)
#define FRAME_TYPE_AUDIO		(3)

#define INPUT_CONTROL_FIFO_SIZE    100

class CInputStream 
{
public:
	CInputStream(int channelID);
	~CInputStream();
	int Open(HWND hwnd);
	//int InputData(PBYTE pBuf, int size, int type);
	int InputData(block_t *pBlock);
	int InputRun();
	void DestroyThread();
	int ResetBuffer();
	int Play();
	int Pause();
	int PlayStep();
	int PlayForwardSpeed(int speed);
	int AudioOn(bool on);
	int SnapShot(char * bmpfilename);
	bool IsAudioOn();
	int GetChannelID();
	int GetBufferDelay();
  bool m_bCreatingVideoOutput;

private:
	HWND m_hwnd;
	bool m_bEnd, m_bError;
	//tme_object_t m_ipc;
	HANDLE m_hThread;
    block_fifo_t *m_pFifo;
	CDecoder *m_pAudioDecoder, *m_pVideoDecoder, *m_pTextDecoder;
	__int64 m_iPtsDelay;
	input_clock_t m_inputClock;
	int m_iCrAverage, m_iRate, m_iState;
	bool m_bCanPaceControl, m_bOutPaceControl;
	bool m_bStepPlay, m_bFastPlay;
	bool m_bAudioOn;
	int m_channelID;
	mtime_t m_lastDataTime, m_lastBufferTime;
	int m_iBufferDelay;

    tme_mutex_t m_lockControl;
    tme_mutex_t m_lockPts;
    tme_mutex_t m_lock;
    tme_cond_t      m_wait;

    int m_iControl;
    struct
    {
        /* XXX: val isn't duplicated so it won't work with string */
        int         i_type;
		tme_value_t val;
    } m_control[INPUT_CONTROL_FIFO_SIZE];

	void CheckInputClock();
	mtime_t ClockToSysdate( input_clock_t *cl, mtime_t i_clock );
	mtime_t ClockCurrent(input_clock_t *cl );
	void ClockSetPCR( input_clock_t *cl, mtime_t i_clock );
	mtime_t ClockGetTS( input_clock_t *cl, mtime_t i_ts );
	void ResetPCR(bool synchro_reinit);
	void AllDecodersDiscontinuity();
	int ControlPopNoLock( int *pi_type, tme_value_t *p_val );
	void ControlPush(int i_type, tme_value_t *p_val);
	bool Control( int i_type, tme_value_t val );
	void SetState(int state);
	int ExtractOSD(block_t *pBlock, block_t **ppBlock, int size);
	//void ValidateDelayedRequest();
};
#endif