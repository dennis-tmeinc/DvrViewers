#ifndef _AOUT_OUTPUT_H_
#define _AOUT_OUTPUT_H_

#include "es.h"
#include "aout_common.h"

class CAoutOutput
{
public:
	CAoutOutput(tme_object_t *pIpc, CAudioOutput * pAout,
				audio_format_t * pFormat);
	virtual ~CAoutOutput();
	virtual void Play(bool bRestart) = 0;
	int PostInit(); // should be called by subclass at the end of its constructor

	CAudioOutput *m_pAout;
	tme_object_t *m_pIpc;
    audio_format_t   m_output;
    /* Indicates whether the audio output is currently starving, to avoid
     * printing a 1,000 "output is starving" messages. */
    bool              m_bStarving;

    /* post-filters */
    aout_filter_t *         m_ppFilters[AOUT_MAX_FILTERS];
    int                     m_iNbFilters;

    aout_fifo_t             m_fifo;

	int						m_iNbSamples;
    /* Current volume for the output */
    audio_volume_t          m_iVolume;

    /* If b_error == 1, there is no audio output pipeline. */
    bool              m_bError;

	bool			m_bResetPlayer;

	void OutputPlay( aout_buffer_t * p_buffer );
	aout_buffer_t * OutputNextBuffer(mtime_t start_date, bool b_can_sleek);
};


#endif