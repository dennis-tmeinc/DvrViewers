#include "stdafx.h"
#include "xvidenc.h"

/* Maximum number of frames to encode */
#define ABS_MAXFRAMENR -1 /* no limit */
#define IMAGE_SIZE(x,y) ((x)*(y)*3/2)

// Equivalent to vfw's pmvfast_presets
static const int motion_presets[] = {
	/* quality 0 */
	0,

	/* quality 1 */
	0,

	/* quality 2 */
	0,

	/* quality 3 */
	0,

	/* quality 4 */
	0 | XVID_ME_HALFPELREFINE16 | 0,

	/* quality 5 */
	0 | XVID_ME_HALFPELREFINE16 | 0 | XVID_ME_ADVANCEDDIAMOND16,

	/* quality 6 */
	XVID_ME_HALFPELREFINE16 | XVID_ME_EXTSEARCH16 |	XVID_ME_HALFPELREFINE8 | 0 | XVID_ME_USESQUARES16

};
#define ME_ELEMENTS (sizeof(motion_presets)/sizeof(motion_presets[0]))

static const int vop_presets[] = {
	/* quality 0 */
	0,

	/* quality 1 */
	0,

	/* quality 2 */
	0,

	/* quality 3 */
	0,

	/* quality 4 */
	0,

	/* quality 5 */
	XVID_VOP_INTER4V,

	/* quality 6 */
	XVID_VOP_INTER4V,

};
#define VOP_ELEMENTS (sizeof(vop_presets)/sizeof(vop_presets[0]))

void
xvid_setup_default(struct xvid_setup *xs)
{
	SecureZeroMemory(xs, sizeof(struct xvid_setup));
	xs->reaction = 16;
	xs->averaging = 100;
	xs->smoother = 100;
	xs->kboost = 10;
	xs->overstrength = xs->overimprove = xs->overdegrade = 5;
	xs->kreduction = 20;
	xs->single = 1;
	xs->kthresh = 1;
	xs->ssim = -1;
	xs->maxkeyinterval = 300;
	xs->dwrate = 25;
	xs->dwscale = 1;
	xs->framerate = 25.0;
	xs->quants[0] = 2;
	xs->quants[1] = 31;
	xs->quants[2] = 2;
	xs->quants[3] = 31;
	xs->quants[4] = 2;
	xs->quants[5] = 31;
	xs->maxbframes = 2;
	xs->bqratio = 150;
	xs->bqoffset = 100;
	xs->packed = 1;
	xs->closed_gop = 1;
	xs->quality = 6;
	xs->par = 1;
	xs->colorspace = XVID_CSP_YV12;
	xs->trellis = 1;
	xs->chromame = 1;
	xs->vhqmode = 1;
	xs->maxframenr = ABS_MAXFRAMENR;
}

static int
enc_info(struct xvid_setup *xs)
{
	xvid_gbl_info_t xvid_gbl_info;
	int ret;

	memset(&xvid_gbl_info, 0, sizeof(xvid_gbl_info));
	xvid_gbl_info.version = XVID_VERSION;
	ret = xvid_global(NULL, XVID_GBL_INFO, &xvid_gbl_info, NULL);
	//if (xvid_gbl_info.build != NULL) {
		//TRACE(_T("xvidcore build version: %s\n"), xvid_gbl_info.build);
	//}
	TRACE(_T("Bitstream version: %d.%d.%d\n"), XVID_VERSION_MAJOR(xvid_gbl_info.actual_version), XVID_VERSION_MINOR(xvid_gbl_info.actual_version), XVID_VERSION_PATCH(xvid_gbl_info.actual_version));
	TRACE(_T("Detected CPU flags: "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_ASM)
		TRACE(_T("ASM "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_MMX)
		TRACE(_T("MMX "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_MMXEXT)
		TRACE(_T("MMXEXT "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_SSE)
		TRACE(_T("SSE "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_SSE2)
		TRACE(_T("SSE2 "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_SSE3)
		TRACE(_T("SSE3 "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_SSE41)
		TRACE(_T("SSE41 "));
    if (xvid_gbl_info.cpu_flags & XVID_CPU_3DNOW)
		TRACE(_T("3DNOW "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_3DNOWEXT)
		TRACE(_T("3DNOWEXT "));
	if (xvid_gbl_info.cpu_flags & XVID_CPU_TSC)
		TRACE(_T("TSC "));
	TRACE(_T("\n"));
	TRACE(_T("Detected %d cpus,"), xvid_gbl_info.num_threads);
	if (!xs->threads)
		xs->threads = xvid_gbl_info.num_threads;
	TRACE(_T(" using %d threads.\n"), xs->threads);
	return ret;
}

void
sort_zones(zone_t * zones, int zone_num, int * sel)
{
	int i, j;
	zone_t tmp;
	for (i = 0; i < zone_num; i++) {
		int cur = i;
		int min_f = zones[i].frame;
		for (j = i + 1; j < zone_num; j++) {
			if (zones[j].frame < min_f) {
				min_f = zones[j].frame;
				cur = j;
			}
		}
		if (cur != i) {
			tmp = zones[i];
			zones[i] = zones[cur];
			zones[cur] = tmp;
			if (i == *sel) *sel = cur;
			else if (cur == *sel) *sel = i;
		}
	}
}

/* constant-quant zones for fixed quant encoding */
static void
prepare_cquant_zones(struct xvid_setup *xs) {
	
	int i = 0;
	if (xs->num_zones == 0 || xs->zones[0].frame != 0) {
		/* first zone does not start at frame 0 or doesn't exist */

		if (xs->num_zones >= MAX_ZONES) xs->num_zones--; /* we sacrifice last zone */

		xs->zones[xs->num_zones].frame = 0;
		xs->zones[xs->num_zones].mode = XVID_ZONE_QUANT;
		xs->zones[xs->num_zones].modifier = xs->cq;
		xs->zones[xs->num_zones].type = XVID_TYPE_AUTO;
		xs->zones[xs->num_zones].greyscale = 0;
		xs->zones[xs->num_zones].chroma_opt = 0;
		xs->zones[xs->num_zones].bvop_threshold = 0;
		xs->zones[xs->num_zones].cartoon_mode = 0;
		xs->num_zones++;

		sort_zones(xs->zones, xs->num_zones, &i);
	}

	/* step 2: let's change all weight zones into quant zones */
	
	for(i = 0; i < xs->num_zones; i++)
		if (xs->zones[i].mode == XVID_ZONE_WEIGHT) {
			xs->zones[i].mode = XVID_ZONE_QUANT;
			xs->zones[i].modifier = (100*xs->cq) / xs->zones[i].modifier;
		}
}

/* full first pass zones */
static void
prepare_full1pass_zones(struct xvid_setup *xs) {
	
	int i = 0;
	if (xs->num_zones == 0 || xs->zones[0].frame != 0) {
		/* first zone does not start at frame 0 or doesn't exist */

		if (xs->num_zones >= MAX_ZONES) xs->num_zones--; /* we sacrifice last zone */

		xs->zones[xs->num_zones].frame = 0;
		xs->zones[xs->num_zones].mode = XVID_ZONE_QUANT;
		xs->zones[xs->num_zones].modifier = 200;
		xs->zones[xs->num_zones].type = XVID_TYPE_AUTO;
		xs->zones[xs->num_zones].greyscale = 0;
		xs->zones[xs->num_zones].chroma_opt = 0;
		xs->zones[xs->num_zones].bvop_threshold = 0;
		xs->zones[xs->num_zones].cartoon_mode = 0;
		xs->num_zones++;

		sort_zones(xs->zones, xs->num_zones, &i);
	}

	/* step 2: let's change all weight zones into quant zones */
	
	for(i = 0; i < xs->num_zones; i++)
		if (xs->zones[i].mode == XVID_ZONE_WEIGHT) {
			xs->zones[i].mode = XVID_ZONE_QUANT;
			xs->zones[i].modifier = 200;
		}
}

int xvidlib_init(int use_assembler, struct xvid_setup *xs)
{

	if (g_xvidlib_initialized)
		return 0;

	/*------------------------------------------------------------------------
	 * XviD core initialization
	 *----------------------------------------------------------------------*/

	/* Set version -- version checking will done by xvidcore */
	memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init));
	xvid_gbl_init.version = XVID_VERSION;
	xvid_gbl_init.debug = xs->debug;

	/* Do we have to enable ASM optimizations ? */
	if (use_assembler) {
#ifdef ARCH_IS_IA64
		xvid_gbl_init.cpu_flags = XVID_CPU_FORCE | XVID_CPU_ASM;
#else
		xvid_gbl_init.cpu_flags = 0;
#endif
	} else {
		xvid_gbl_init.cpu_flags = XVID_CPU_FORCE;
	}

	/* Initialize XviD core -- Should be done once per __process__ */
	xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);
	enc_info(xs);
	g_xvidlib_initialized = true;

	return 0;
}

/* Initialize encoder for first use, pass all needed parameters to the codec */
int
enc_init(struct xvid_setup *xs)
{
	int xerr;
    xvid_plugin_single_t single;
	xvid_plugin_2pass1_t rc2pass1;
	xvid_plugin_2pass2_t rc2pass2;
	xvid_plugin_ssim_t ssim;
	xvid_enc_plugin_t plugins[8];
	xvid_enc_create_t xvid_enc_create;
	int i;

	/*------------------------------------------------------------------------
	 * XviD encoder initialization
	 *----------------------------------------------------------------------*/

	/* Version again */
	memset(&xvid_enc_create, 0, sizeof(xvid_enc_create));
	xvid_enc_create.version = XVID_VERSION;

	/* Width and Height of input frames */
	xvid_enc_create.width = xs->width;
	xvid_enc_create.height = xs->height;
	xvid_enc_create.profile = 0xf5; /* Unrestricted */

	xvid_enc_create.plugins = plugins;
	xvid_enc_create.num_plugins = 0;

	if (xs->single) {
		memset(&single, 0, sizeof(xvid_plugin_single_t));
		single.version = XVID_VERSION;
		single.bitrate = xs->bitrate;
		single.reaction_delay_factor = xs->reaction;
		single.averaging_period = xs->averaging;
		single.buffer = xs->smoother;
		

		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_single;
		plugins[xvid_enc_create.num_plugins].param = &single;
		xvid_enc_create.num_plugins++;
		if (!xs->bitrate)
			prepare_cquant_zones(xs);
	}

	if (xs->pass2) {
		memset(&rc2pass2, 0, sizeof(xvid_plugin_2pass2_t));
		rc2pass2.version = XVID_VERSION;
		rc2pass2.filename = xs->pass2;
		rc2pass2.bitrate = xs->bitrate;

		rc2pass2.keyframe_boost = xs->kboost;
		rc2pass2.curve_compression_high = xs->chigh;
		rc2pass2.curve_compression_low = xs->clow;
		rc2pass2.overflow_control_strength = xs->overstrength;
		rc2pass2.max_overflow_improvement = xs->overimprove;
		rc2pass2.max_overflow_degradation = xs->overdegrade;
		rc2pass2.kfreduction = xs->kreduction;
		rc2pass2.kfthreshold = xs->kthresh;
		rc2pass2.container_frame_overhead = xs->overhead;

//		An example of activating VBV could look like this 
		rc2pass2.vbv_size     =  xs->vbvsize;
		rc2pass2.vbv_initial  =  (xs->vbvsize*3)/4;
		rc2pass2.vbv_maxrate  =  xs->vbvmaxrate;
		rc2pass2.vbv_peakrate =  xs->vbvpeakrate*3;


		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_2pass2;
		plugins[xvid_enc_create.num_plugins].param = &rc2pass2;
		xvid_enc_create.num_plugins++;
	}

	if (xs->pass1) {
		memset(&rc2pass1, 0, sizeof(xvid_plugin_2pass1_t));
		rc2pass1.version = XVID_VERSION;
		rc2pass1.filename = xs->pass1;
		if (xs->full1pass)
			prepare_full1pass_zones(xs);
		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_2pass1;
		plugins[xvid_enc_create.num_plugins].param = &rc2pass1;
		xvid_enc_create.num_plugins++;
	}

	/* Zones stuff */
	xvid_enc_create.zones = (xvid_enc_zone_t*)malloc(sizeof(xvid_enc_zone_t) * xs->num_zones);
	xvid_enc_create.num_zones = xs->num_zones;
	for (i=0; i < xvid_enc_create.num_zones; i++) {
		xvid_enc_create.zones[i].frame = xs->zones[i].frame;
		xvid_enc_create.zones[i].base = 100;
		xvid_enc_create.zones[i].mode = xs->zones[i].mode;
		xvid_enc_create.zones[i].increment = xs->zones[i].modifier;
	}


	if (xs->lumimasking) {
		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_lumimasking;
		plugins[xvid_enc_create.num_plugins].param = NULL;
		xvid_enc_create.num_plugins++;
	}

	if (xs->dump) {
		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_dump;
		plugins[xvid_enc_create.num_plugins].param = NULL;
		xvid_enc_create.num_plugins++;
	}

	if (xs->ssim>=0 || xs->ssim_path != NULL) {
        memset(&ssim, 0, sizeof(xvid_plugin_ssim_t));

        plugins[xvid_enc_create.num_plugins].func = xvid_plugin_ssim;

		if( xs->ssim >=0){
			ssim.b_printstat = 1;
			ssim.acc = xs->ssim;
		} else {
			ssim.b_printstat = 0;
			ssim.acc = 2;
		}

		if(xs->ssim_path != NULL){		
			ssim.stat_path = xs->ssim_path;
		}

        ssim.cpu_flags = xvid_gbl_init.cpu_flags;
		ssim.b_visualize = 0;
		plugins[xvid_enc_create.num_plugins].param = &ssim;
		xvid_enc_create.num_plugins++;
	}

	xvid_enc_create.num_threads = xs->threads;

	/* Frame rate  */
	xvid_enc_create.fincr = xs->dwscale;
	xvid_enc_create.fbase = xs->dwrate;

	/* Maximum key frame interval */
	if (xs->maxkeyinterval > 0) {
        xvid_enc_create.max_key_interval = xs->maxkeyinterval;
    }else {
		xvid_enc_create.max_key_interval = (int) xs->framerate *10;
    }

	xvid_enc_create.min_quant[0]=xs->quants[0];
	xvid_enc_create.min_quant[1]=xs->quants[2];
	xvid_enc_create.min_quant[2]=xs->quants[4];
	xvid_enc_create.max_quant[0]=xs->quants[1];
	xvid_enc_create.max_quant[1]=xs->quants[3];
	xvid_enc_create.max_quant[2]=xs->quants[5];

	/* Bframes settings */
	xvid_enc_create.max_bframes = xs->maxbframes;
	xvid_enc_create.bquant_ratio = xs->bqratio;
	xvid_enc_create.bquant_offset = xs->bqoffset;

	/* Frame drop ratio */
	xvid_enc_create.frame_drop_ratio = xs->framedrop;

	/* Global encoder options */
	xvid_enc_create.global = 0;

	if (xs->packed)
		xvid_enc_create.global |= XVID_GLOBAL_PACKED;

	if (xs->closed_gop)
		xvid_enc_create.global |= XVID_GLOBAL_CLOSED_GOP;

	if (xs->stats)
		xvid_enc_create.global |= XVID_GLOBAL_EXTRASTATS_ENABLE;

	/* I use a small value here, since will not encode whole movies, but short clips */
	xerr = xvid_encore(NULL, XVID_ENC_CREATE, &xvid_enc_create, NULL);

	/* Retrieve the encoder instance from the structure */
	xs->enc_handle = xvid_enc_create.handle;

	free(xvid_enc_create.zones);

	return (xerr);
}

int xvid_init(struct xvid_setup *xs)
{
	/* Set constant quant to default if no bitrate given for single pass */
	if (xs->single && (!xs->bitrate) && (!xs->cq))
			xs->cq = DEFAULT_QUANT;

	/* this should really be enough memory ! */
	xs->mp4_buffer = (unsigned char *) malloc(IMAGE_SIZE(xs->width, xs->height) * 2);
	if (!xs->mp4_buffer)
		return 1;

	return enc_init(xs);
}

void xvid_close(struct xvid_setup *xs)
{
	if (xs->mp4_buffer)
		free(xs->mp4_buffer);
}

void apply_zone_modifiers(struct xvid_setup *xs, xvid_enc_frame_t * frame, int framenum)
{
	int i;

	for (i=0; i<xs->num_zones && xs->zones[i].frame <= framenum; i++) ;

	if (--i < 0) return; /* there are no zones, or we're before the first zone */

	if (framenum == xs->zones[i].frame)
		frame->type = xs->zones[i].type;

	if (xs->zones[i].greyscale) {
		frame->vop_flags |= XVID_VOP_GREYSCALE;
	}

	if (xs->zones[i].chroma_opt) {
		frame->vop_flags |= XVID_VOP_CHROMAOPT;
	}

	if (xs->zones[i].cartoon_mode) {
		frame->vop_flags |= XVID_VOP_CARTOON;
		frame->motion |= XVID_ME_DETECT_STATIC_MOTION;
	}

	if (xs->maxbframes) {
		frame->bframe_threshold = xs->zones[i].bvop_threshold;
	}
}

int
enc_main(struct xvid_setup *xs,
		 unsigned char *image,
		 unsigned char *bitstream,
		 int *key,
		 int *stats_type,
		 int *stats_quant,
		 int *stats_length,
		 int sse[3],
		 int framenum)
{
	int ret;

	xvid_enc_frame_t xvid_enc_frame;
	xvid_enc_stats_t xvid_enc_stats;

	/* Version for the frame and the stats */
	memset(&xvid_enc_frame, 0, sizeof(xvid_enc_frame));
	xvid_enc_frame.version = XVID_VERSION;

	memset(&xvid_enc_stats, 0, sizeof(xvid_enc_stats));
	xvid_enc_stats.version = XVID_VERSION;

	/* Bind output buffer */
	xvid_enc_frame.bitstream = bitstream;
	xvid_enc_frame.length = -1;

	/* Initialize input image fields */
	if (image) {
		xvid_enc_frame.input.plane[0] = image;
#ifndef READ_PNM
		xvid_enc_frame.input.csp = xs->colorspace;
		xvid_enc_frame.input.stride[0] = xs->width;
#else
		xvid_enc_frame.input.csp = XVID_CSP_BGR;
		xvid_enc_frame.input.stride[0] = XDIM*3;
#endif
	} else {
		xvid_enc_frame.input.csp = XVID_CSP_NULL;
	}

	/* Set up core's general features */
	xvid_enc_frame.vol_flags = 0;
	if (xs->stats)
		xvid_enc_frame.vol_flags |= XVID_VOL_EXTRASTATS;
	if (xs->qtype) {
		xvid_enc_frame.vol_flags |= XVID_VOL_MPEGQUANT;
		if (xs->qmatrix) {
			xvid_enc_frame.quant_intra_matrix = xs->qmatrix_intra;
			xvid_enc_frame.quant_inter_matrix = xs->qmatrix_inter;
		}
		else {
			/* We don't use special matrices */
			xvid_enc_frame.quant_intra_matrix = NULL;
			xvid_enc_frame.quant_inter_matrix = NULL;
		}
	}

	if (xs->par)
		xvid_enc_frame.par = xs->par;
	else {
		xvid_enc_frame.par = XVID_PAR_EXT;
		xvid_enc_frame.par_width = xs->parwidth;
		xvid_enc_frame.par_height = xs->parheight;
	}


	if (xs->qpel) {
		xvid_enc_frame.vol_flags |= XVID_VOL_QUARTERPEL;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16 | XVID_ME_QUARTERPELREFINE8;
	}
	if (xs->gmc) {
		xvid_enc_frame.vol_flags |= XVID_VOL_GMC;
		xvid_enc_frame.motion |= XVID_ME_GME_REFINE;
	}

	/* Set up core's general features */
	xvid_enc_frame.vop_flags = vop_presets[xs->quality];

	if (xs->interlacing) {
		xvid_enc_frame.vol_flags |= XVID_VOL_INTERLACING;
		if (xs->interlacing == 2)
			xvid_enc_frame.vop_flags |= XVID_VOP_TOPFIELDFIRST;
	}

	xvid_enc_frame.vop_flags |= XVID_VOP_HALFPEL;
	xvid_enc_frame.vop_flags |= XVID_VOP_HQACPRED;

    if (xs->vopdebug) {
        xvid_enc_frame.vop_flags |= XVID_VOP_DEBUG;
    }

    if (xs->trellis) {
        xvid_enc_frame.vop_flags |= XVID_VOP_TRELLISQUANT;
    }

	/* Frame type -- taken from function call parameter */
	/* Sometimes we might want to force the last frame to be a P Frame */
	xvid_enc_frame.type = *stats_type;

	/* Force the right quantizer -- It is internally managed by RC plugins */
	xvid_enc_frame.quant = 0;

	if (xs->chromame)
		xvid_enc_frame.motion |= XVID_ME_CHROMA_PVOP + XVID_ME_CHROMA_BVOP;

	/* Set up motion estimation flags */
	xvid_enc_frame.motion |= motion_presets[xs->quality];

	if (xs->turbo)
		xvid_enc_frame.motion |= XVID_ME_FASTREFINE16 | XVID_ME_FASTREFINE8 | 
								 XVID_ME_SKIP_DELTASEARCH | XVID_ME_FAST_MODEINTERPOLATE | 
								 XVID_ME_BFRAME_EARLYSTOP;

	if (xs->bvhq) 
		xvid_enc_frame.vop_flags |= XVID_VOP_RD_BVOP;

	switch (xs->vhqmode) /* this is the same code as for vfw */
	{
	case 1: /* VHQ_MODE_DECISION */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		break;

	case 2: /* VHQ_LIMITED_SEARCH */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16_RD;
		break;

	case 3: /* VHQ_MEDIUM_SEARCH */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_CHECKPREDICTION_RD;
		break;

	case 4: /* VHQ_WIDE_SEARCH */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_CHECKPREDICTION_RD;
		xvid_enc_frame.motion |= XVID_ME_EXTSEARCH_RD;
		break;

	default :
		break;
	}

	/* Not sure what this does */
	// force keyframe spacing in 2-pass 1st pass
	if (xs->quality == 0)
		xvid_enc_frame.type = XVID_TYPE_IVOP;

	/* frame-based stuff */
	apply_zone_modifiers(xs, &xvid_enc_frame, framenum);


	/* Encode the frame */
	ret = xvid_encore(xs->enc_handle, XVID_ENC_ENCODE, &xvid_enc_frame,
					  &xvid_enc_stats);

	*key = (xvid_enc_frame.out_flags & XVID_KEYFRAME);
	*stats_type = xvid_enc_stats.type;
	*stats_quant = xvid_enc_stats.quant;
	*stats_length = xvid_enc_stats.length;
	sse[0] = xvid_enc_stats.sse_y;
	sse[1] = xvid_enc_stats.sse_u;
	sse[2] = xvid_enc_stats.sse_v;

	return (ret);
}

int xvid_decode(struct xvid_setup *xs, PBYTE in_buffer, int *key)
{
	int totalsize;
	int m4v_size = 0;
	int stats_type;
	int stats_quant;
	int stats_length;
	int fakenvop = 0;
	int nvop_counter;
	int sse[3];


	if (xs->input_num >= (unsigned int)xs->maxframenr-1 && xs->maxbframes)	
		stats_type = XVID_TYPE_PVOP;	
	else
		stats_type = XVID_TYPE_AUTO;

	m4v_size = 	enc_main(xs, in_buffer, xs->mp4_buffer, key, &stats_type,
					 &stats_quant, &stats_length, sse, xs->input_num);
	xs->input_num++;

	return m4v_size;
}
