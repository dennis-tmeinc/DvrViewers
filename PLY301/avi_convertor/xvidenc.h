#ifndef _XVID_ENC_H_
#define _XVID_ENC_H_
#include "xvid.h"

#define MAX_ZONES   64
#define DEFAULT_QUANT 400

typedef struct
{
	int frame;

	int type;
	int mode;
	int modifier;

	unsigned int greyscale;
	unsigned int chroma_opt;
	unsigned int bvop_threshold;
	unsigned int cartoon_mode;
} zone_t;


struct xvid_setup {
	unsigned char *mp4_buffer;
	void *enc_handle;
	int input_num, output_num;
	int bitrate; /* 0 */
	int reaction; /* 16 */
	int averaging; /* 100 */
	int smoother; /* 100 */
	int kboost; /* 10 */
	int chigh, clow; /* 0 */
	int overstrength, overimprove, overdegrade; /* 5 */
	int overhead; /* 0 */
	int kreduction; /* 20 */
	int kthresh; /* 1 */
	int vbvsize, vbvmaxrate, vbvpeakrate; /* 0 */
	int num_zones; /* 0 */
	int debug, vopdebug, dump, stats, full1pass; /* 0 */
	int single; /* single pass: 1 */
	int lumimasking; /* 0 */
	char *pass1, *pass2; /* NULL */
	zone_t zones[MAX_ZONES];
	int ssim; /* -1 */
	char *ssim_path; /* NULL */
	int threads; /* 0 */
	int maxkeyinterval; /* 300 */
	int width, height;
	int dwrate, dwscale; /* 25, 1 */
	float framerate; /* 25.0 */
	int quants[6]; /* {2, 31, 2, 31, 2, 31} */
	int maxbframes; /* 2 */
	int maxframenr; /* ABS_MAXFRAMENR */
	int bqratio; /* 150 */
	int bqoffset; /* 100 */ 
	int framedrop; /* 0 */
	int packed; /* 1 */
	int closed_gop; /* 1 */
	double cq; /* 0 */
	int quality; /* 6 */
	int startframenr; /* 0 */
	int par; /* 1 */
	int parwidth, parheight; /* 0 */
	int colorspace; /* XVID_CSP_YV12 */
	int qtype, qmatrix; /* 0 */
	int qpel, gmc, interlacing; /* 0 */
	int trellis; /* 1 */
	int chromame; /* 1 */
	int turbo; /* 0 */
	int bvhq; /* 0 */
	int vhqmode; /* 1 */
	unsigned char qmatrix_intra[64];
	unsigned char qmatrix_inter[64];
};

extern bool g_xvidlib_initialized;
extern xvid_gbl_init_t xvid_gbl_init;

int xvidlib_init(int use_assembler, struct xvid_setup *xs);
int xvid_init(struct xvid_setup *xs);
void xvid_close(struct xvid_setup *xs);
int enc_init(struct xvid_setup *xs);
void xvid_setup_default(struct xvid_setup *xs);
void apply_zone_modifiers(struct xvid_setup *xs, xvid_enc_frame_t * frame, int framenum);
int xvid_decode(struct xvid_setup *xs, PBYTE in_buffer, int *key);

#endif