
// DVR stream in interface
//     a single channel dvr data streaming in 

class dvrstream {
public:
// constructors
	virtual ~dvrstream()=0;

// interfaces
	virtual int seek( struct dvrtime * dvrt )=0 ;		// seek to specified time.
	virtual int prev_keyframe()=0;						// seek to previous key frame position.
	virtual int next_keyframe()=0;						// seek to next key frame position.
	virtual int getdata()=0;							// get dvr data frame
	virtual int gettime( struct dvrtime * dvrt, unsigned long * timestamp )=0;	// get current time and timestamp
													// timestamp should be synchronized among channels in same DVR server.
													// timestamp in miliseconds
	virtual int getinfo()=0;							// get stream information
	virtual int getdayinfo()=0;							// get stream day data availability information
	virtual int gettimeinfo()=0;						// get data availability information
	virtual int getlockfileinfo()=0;					// get locked file availability information
} ;


class dvrpreviewstream : public dvrstream {
public:
	dvrpreviewstream(char * servername, int channel);	// open dvr server stream for preview

	~dvrpreviewstream();

	int seek( struct dvrtime * dvrt ) 		// seek to specified time.
	{
		return 0;	// not supported
	}

	int prev_keyframe()						// seek to previous key frame position.
	{
		return 0;	// not supported
	}

	int next_keyframe()						// seek to next key frame position.
	{
		return 0;	// not supported
	}

	int getdata();							// get dvr data frame

	int gettime( struct dvrtime * dvrt, unsigned long * timestamp );	// get current time and timestamp
	int getinfo()=0;							// get stream information
	int getdayinfo()							// get stream day data availability information
	{
		return 0;	// not used
	}
	int gettimeinfo()						// get data availability information
	{
		return 0;	// not used
	}
	int getlockfileinfo()					// get locked file availability information
	{
		return 0;	// not used
	}
};