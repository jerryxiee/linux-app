/*!
\file       mps_clds_dev.h
\brief      Cloud storage api of mining

------history----------
\author     Zhengxianwei
\version    0.01
*/


#ifndef __mps_clds_dev_h__
#define __mps_clds_dev_h__

#if !defined(__len_str_defined) 
#define __len_str_defined

/*!< length string */
typedef struct len_str
{
    unsigned long       len;           
    char                *data; 
}_len_str;
#endif /* !defined(__len_str_defined) */

struct mps_clds_dev_mod;
struct mps_clds_dev_channel;

/*
 cert: 
*/
struct mps_clds_dev_create_param
{
	struct len_str cert;            /*!< now, useless, you can set it empty */
};


/*!
 *@param chl 		get from mps_clds_dec_channel_create
 *@param evt_type  	connect.ok, connect.fail, connect.close, pause, play
 *@param refer 		user_data
 */
typedef long (* mps_clds_dev_channel_on_event) (struct mps_clds_dev_channel *chl, 
												struct len_str *evt_type, 
												struct len_str *data, 
												void *refer);

struct mps_clds_dev_channel_create_param
{
	struct len_str  token;                       /*!< get from cloud service provider */
	void*           refer;                       /*!< used by mps_clds_dev_channel_on_event forth parameter */
    mps_clds_dev_channel_on_event chl_on_event;  /*!< callback function */
};

struct mps_clds_dev_sample 
{
    /**
     * media sample push to cloud, in this version, we only support h264.
     * the h264 nal video, must be start with nal length in big endian, eg.
     *
     * normal nal frame, length 0x6732, start with :
     *  00 00 00 01 
     *
     * now , it become start with :
     *  00 00 67 32
     */
	unsigned char*      data;           /*!< nal data, start with nal length in big endian */
    long                len;            /*!< nal length, 4 */
	unsigned long long  ab_time;        /*!< absolute time, milliseconds from 1970 epochs */
    unsigned long       time_tick;      /*!< relative time, default is 0, can be omitted */
    struct 
	{
		unsigned    is_key_sample:1;    /*!< mark this frame is key frame */
		unsigned    is_video:1;         /*!< video or audio, in this version, only support video 
                                           so u must be set to 1 */
        /**
         * mark the last frame in the stream, notify the cloud 
		 * service to close and free resource. If you forgot to
		 * set it, the cloud service will be free resouce in timeout .
         */
		unsigned    is_end:1;           
	} flag;
};

/*! mps_clds_dev_mod_create
 \brief allocate cloud stroage mod
 \param param empty
 \return NULL 			failure	
 		 other 			success
*/
struct mps_clds_dev_mod*    mps_clds_dev_mod_create (struct mps_clds_dev_create_param *param);

/*! mps_clds_dev_mod_destroy
 \param mod created by mps_clds_dev_mod_create
 \return 	0 succes, other failure
*/
long                    mps_clds_dev_mod_destroy (struct mps_clds_dev_mod *mod);

/*! mps_clds_dev_channel_create
 \param mod    created by mps_clds_dev_mod_create
 \param param  user_data
 \return 	NULL 		failure,	other success
*/
struct mps_clds_dev_channel *mps_clds_dev_channel_create(struct mps_clds_dev_mod *mod, struct mps_clds_dev_channel_create_param *param);

/*! mps_clds_dev_channel_destroy
 \param chl created by mps_clds_dev_channel_destroy
 \return 	0 		success, other 	failure
*/
long                    mps_clds_dev_channel_destroy(struct mps_clds_dev_channel *chl);

/*! mps_clds_dev_channel_sample_write
 \param chl    channel create by mps_clds_dev_channel_create    
 \param sample user data
 \return 	0 	success, other 	failure
*/
long                    mps_clds_dev_channel_sample_write (struct mps_clds_dev_channel *chl, struct mps_clds_dev_sample *sample);

/*! mps_clds_dev_mod_debug_dump_set
 \param mod    mod create by mps_clds_dev_mod_create
 \param error_level     1-6
 \return 	0 	success, other 	failure
*/
long                    mps_clds_dev_mod_debug_dump_set (struct mps_clds_dev_mod *mod, int error_level);

#endif // end of mps_clds_dev_h