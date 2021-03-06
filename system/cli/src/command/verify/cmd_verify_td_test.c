#include <FreeRTOS.h>
#include <bsp.h>
#include <task.h>
#include <printlog.h>
#include <libmid_fatfs/ff.h>
#include <cmd_verify.h>
#include <libmid_td/td.h>
#include <audio.h>
#include <audio/audio_dri.h>
#include <audio/audio_error.h>

/** \defgroup cmd_verify_td Verify Tone dtetction commands
 *  \ingroup cmd_verify
 * @{
 */

/**
* @brief Tone detection test
* \n Tone detection test that sound data for mobile APPs
* @param None
* @details Example: td
*/
int cmd_verify_tone_detection_test(int argc, char* argv[])
{
	start_tone_detection_verify(NULL);

	return 0;
}

#define INLENGTH        256
#define DetectTrigLen	0
/**
* @brief Tone detection loopback test
* \n Tone detection check pattern from SD card
* @param None
* @details Example: tdlo
*/
int cmd_verify_tone_detection_loopback(int argc, char* argv[])
{
	struct audio_stream_st *audio_stream_cap = NULL;
	struct params_st *params;
	int ret = 0;
	uint8_t *data, *pdata;
	char decode_data[100];
	long CommandVal;
	short CmdFrmCnt;
	int j = 0;
	uint32_t format = sizeof(short);
	uint32_t size = INLENGTH * format;
	uint32_t is_block = 0;
	uint32_t tone_data_len = 0;
	int32_t status;
	int32_t analog_gain, digital_gain;
	int frames = 2000;

	//Malloc data buffer
	data = pvPortMalloc(size, GFP_KERNEL, MODULE_CLI);
	if(data == NULL)
	{
		print_msg_queue("Failed to allocate memory. --- %d\n", size);
		goto out;
	}

	//Open audio device
	ret = audio_open(&audio_stream_cap, AUD_CAP_STREAM);
	if(ret < 0)
	{
		print_msg_queue("Failed to open device for capture.\n");
		goto out;
	}

	//Set audio params
	ret = audio_params_alloca(&params);
	if(ret < 0)
	{
		print_msg_queue("Failed to allocate memory.\n");
		goto out;
	}
	audio_params_set_rate(audio_stream_cap, params, 48000);
	audio_params_set_format(audio_stream_cap, params, AUD_FORMAT_S16_LE);
	ret = audio_set_params(audio_stream_cap, params);
	if(ret < 0)
	{
		print_msg_queue("Failed to set parameters of capture .\n");
		goto out;
	}

	analog_gain = audio_high_ctrl(audio_stream_cap, AUD_GET_SIG_VOL, 0);
	print_msg_queue("Get analog_gain=%d\n",analog_gain);
	/* Set MIC analog gain max */
	print_msg_queue("Set analog_gain=%d\n",0x1F);
	audio_high_ctrl(audio_stream_cap, AUD_SET_SIG_VOL, 0x1F);

	digital_gain = audio_high_ctrl(audio_stream_cap, AUD_GET_CAP_VOL, 0);
	print_msg_queue("Get digital_gain=%d\n",digital_gain);
	/* Set MIC digital gain 0x0 */
	print_msg_queue("Set digital_gain=%d\n",0x0);
	audio_high_ctrl(audio_stream_cap, AUD_SET_CAP_VOL, 0x0);

	//Initial tone detection
	ToneDetectInit_AF_SA3(42,32,4);
	CmdFrmCnt = DetectTrigLen;
	CommandVal = -1;

	while(1) {
		//audio read 512bytes
		pdata = data;
		size = INLENGTH * format;
reread:
		while (size > 0)
		{
			ret = audio_read(audio_stream_cap, pdata, size, is_block);
			if (ret <= 0)
			{
				if(ret == -EAGAIN){
					vTaskDelay(10 / portTICK_RATE_MS);
					goto reread;
				}

				audio_status(audio_stream_cap, &status);
				if(status == AUD_STATE_XRUN){
					print_msg_queue("overrun\n");
					audio_resume(audio_stream_cap);
					continue;
				}
				else {
					print_msg_queue("Error capture status\n");
					goto out;
				}
			}
			else {
				pdata += ret;
				size -= ret;
			}
		}

        /* Porting tag */
		CommandVal = ToneDetectCore_AF((short *)data,decode_data);
        if (CommandVal >= 1)
		{
			tone_data_len = CommandVal;
			print_msg_queue("\nDecode Data Checking(Hex) :\n");
			print_msg_queue("%s\n",decode_data);
			for (j = 0 ; j < tone_data_len ; j++) {
					if (j > 7 && j % 8 == 0)
						print_msg_queue("\n");
					print_msg_queue("0x%02x ", decode_data[j]);
			}
			print_msg_queue("\n");
			CmdFrmCnt = DetectTrigLen;
			CommandVal = -1;
			goto out;

        }
		else if (CommandVal == 0) {
			//printf("NONE\n---------------------------------------------------\n");
		}
		else if (CommandVal == -1) {
			print_msg_queue("ECC ERR\n---------------------------------------------------\n");
		}
	}

out:
	/* Set MIC analog gain max */
	print_msg_queue("Set analog_gain=%d\n",analog_gain);
	audio_high_ctrl(audio_stream_cap, AUD_SET_SIG_VOL, analog_gain);

	/* Set MIC digital gain 0x0 */
	print_msg_queue("Set digital_gain=%d\n",digital_gain);
	audio_high_ctrl(audio_stream_cap, AUD_SET_CAP_VOL, digital_gain);

	if (audio_stream_cap) {
		audio_drain(audio_stream_cap);
		audio_close(audio_stream_cap);
		audio_stream_cap = NULL;
	}
	if (data) {
		vPortFree(data);
		data = NULL;
	}

	return 0;
}
/** @} */
