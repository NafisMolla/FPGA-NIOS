/*=========================================================================*/
/*  Includes                                                               */
/*=========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <system.h>
#include <sys/alt_irq.h>
#include <sys/alt_alarm.h>
#include <io.h>

#include "fatfs.h"
#include "diskio.h"

#include "ff.h"
#include "monitor.h"
#include "uart.h"

#include "alt_types.h"
#include <altera_avalon_pio_regs.h>
#include <altera_up_avalon_audio.h>
#include <altera_up_avalon_audio_and_video_config.h>

#define PSTR(_a)  _a

static alt_alarm alarm;
static unsigned long Systick = 0;
static volatile unsigned short Timer; /* 1000Hz increment timer */

/***************************************************************************/
static alt_u32 TimerFunction(void *context) {
	static unsigned short wTimer10ms = 0;

	(void) context;

	Systick++;
	wTimer10ms++;
	Timer++; /* Performance counter for this module */

	if (wTimer10ms == 10) {
		wTimer10ms = 0;
		ffs_DiskIOTimerproc(); /* Drive timer procedure of low level disk I/O module */
	}

	return (1);
} /* TimerFunction */

static void IoInit(void) {
	uart0_init(115200);

	/* Init diskio interface */
	ffs_DiskIOInit();

	//SetHighSpeed();

	/* Init timer system */
	alt_alarm_start(&alarm, 1, &TimerFunction, NULL);

} /* IoInit */

//-----------------------------------------------------------------------globals--------------------------------------------------------------------------
char * play_mode = "STOP";
char* speed_mode = "";
int play_pause_flag = 0;
int stop_flag = 1;
int check_screen_block = 0;
int reset_open_flag = 1;
int loop_status = 0;
int skip_flag = 0;

//holds the name of the files
char* file_name_array[20][20];

//holds the size of the files
int file_size_array[20];

//holds the index of the files
int file_index_array[20];

int file_store_index = 0;

int curr_file = 0;

FILE *lcd;
void update_display(FILE *lcd) {
	lcd = fopen("/dev/lcd_display", "w");
	if (lcd != NULL) {
		fprintf(lcd, "%d - %s\n", file_index_array[curr_file],
				&file_name_array[curr_file]);
		fprintf(lcd, "%s - %s\n", play_mode,speed_mode);
	}

}

int old_val = 0;

static void pio_button_ISR(void * context, alt_u32 id) {

	//clear the ISR so it's not always on
	IOWR(TIMER_0_BASE, 1, 0b101);
	IOWR(BUTTON_PIO_BASE, 2, 0x0);

}

static void pio_timer_ISR(void * context, alt_u32 id) {
	int button_val = IORD(BUTTON_PIO_BASE, 0);

	//pause/play case

	if (button_val == 13) {
		old_val = 13;
	}
	if (button_val == 14) {
		old_val = 14;
	}
	if (button_val == 7) {
		old_val = 7;
	}
	if (button_val == 11) {
		old_val = 11;
	}

	if (button_val == 15) {

		if (old_val == 13) {
			play_pause_flag = !(play_pause_flag);
			play_mode = play_pause_flag ? "PLAY" : "PAUSE";
			stop_flag = 0;
			update_display(lcd);
			printf("%s\n", play_mode);

			old_val = 0;
		}

		if (old_val == 14) {
			curr_file++;
			if (curr_file < 0) {
				curr_file = 15;
			} else {
				curr_file = curr_file % 16;
			}
			skip_flag = 1;
			update_display(lcd);
			old_val = 0;
		}

		if (old_val == 7) {
			curr_file--;
			if (curr_file < 0) {
				curr_file = 15;
			} else {
				curr_file = curr_file % 16;
			}
			skip_flag = 1;
			update_display(lcd);
			old_val = 0;
		}

		if (old_val == 11) {
			stop_flag = 1;
			play_pause_flag = 0;
			play_mode = "STOP";
			update_display(lcd);
			old_val = 0;
		}
	}
	IOWR(TIMER_0_BASE, 0, 0b01);
	IOWR(BUTTON_PIO_BASE, 3, 0x0);
	IOWR(BUTTON_PIO_BASE, 2, 0xf);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t acc_size; /* Work register for fs command */
uint16_t acc_files, acc_dirs;
FILINFO Finfo;
#if _USE_LFN
char Lfname[512];
#endif

char Line[256]; /* Console input buffer */

FATFS Fatfs[_VOLUMES]; /* File system object for each logical drive */
FIL File1, File2; /* File objects */
DIR Dir; /* Directory object */
uint8_t Buff[1024] __attribute__ ((aligned(4))); /* Working buffer */

static
void put_rc(FRESULT rc) {
	const char *str =
			"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
					"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
					"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
					"LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0";
	FRESULT i;

	for (i = 0; i != rc && *str; i++) {
		while (*str++)
			;
	}
	xprintf("rc=%u FR_%s\n", (uint32_t) rc, str);
}

//----------------------------CHECK IF THE FILE IS A WAV FILE-------------------------------
int isWav(char* file_name) {
	return (strcmp(&file_name[strlen(file_name) - 4], ".WAV") == 0);
}

//------------------------------------------------------------------------------------------

int fifospace;
char *ptr, *ptr2;
long p1, p2, p3;
uint8_t res, b1, drv = 0;
uint16_t w1;
uint32_t s1, s2, cnt, blen = sizeof(Buff);
static const uint8_t ft[] = { 0, 12, 16, 32 };
uint32_t ofs = 0, sect = 0, blk[2];
FATFS *fs; /* Pointer to file system object */

//--------------------arrays to hold the file data------------------------------

//----------------------audio playing

int incrementer = 4;
int music_mode = 2;

/* used for audio record/playback */
unsigned int l_buf;
unsigned int r_buf;
// open the Audio port

/////////////////////////////////////////////////


/***************************************************************************/
/*  main                                                                   */
/***************************************************************************/

int main(void) {

	alt_up_audio_dev * audio_dev;
	audio_dev = alt_up_audio_open_dev("/dev/Audio");
	if (audio_dev == NULL)
		alt_printf("Error: could not open audio device \n");
	else
		alt_printf("Opened audio device \n");

//-------------------interrupt stuff
	IOWR(BUTTON_PIO_BASE, 3, 0x0);
	IOWR(BUTTON_PIO_BASE, 2, 0xf);
	alt_irq_register(BUTTON_PIO_IRQ, (void *) 0, pio_button_ISR);
	alt_irq_register(TIMER_0_IRQ, (void *) 0, pio_timer_ISR);
	IOWR(TIMER_0_BASE, 2, 0x49F0);
	IOWR(TIMER_0_BASE, 3, 0x2);

//-------------------------------------
	IoInit();
	disk_initialize(0);
	put_rc(f_mount(0, &Fatfs[0]));
//---------------------fl--------------------------------------------------

	res = f_opendir(&Dir, 0);
	if (res) // if res in non-zero there is an error; print the error.
	{
		put_rc(res);
	}
	p1 = s1 = s2 = 0; // otherwise initialize the pointers and proceed.
	int loop_counter = 0;
	for (;;) {
		res = f_readdir(&Dir, &Finfo);

		if ((res != FR_OK) || !Finfo.fname[0])
			break;
		if (Finfo.fattrib & AM_DIR) {
			s2++;
		} else {
			s1++;
			p1 += Finfo.fsize;
		}

		xprintf(" %9lu  %s %d\n", Finfo.fsize, &(Finfo.fname[0]),
				isWav(&(Finfo.fname[0])));

		//-----------------------adding all the information to the releavant buffers------------------------
		if (isWav(&(Finfo.fname[0]))) {

			//add to the name buffer
			strcpy(&file_name_array[file_store_index], &(Finfo.fname[0]));
			//add to the size buffer
			file_size_array[file_store_index] = Finfo.fsize;

			//add to the index array
			file_index_array[file_store_index] = loop_counter+1;

			//increment the file_store_index
			file_store_index++;
		}
		loop_counter++;

	}

	/* Write some simple text to the LCD. */
//	while (1) {
////		printf("mode in loop: %s\n", play_mode);
////		printf("outside the loop\n");
//
//		if (check_screen_block) {
//
//			if (curr_file < 0) {
//				curr_file = 15;
//			} else {
//				curr_file = curr_file % 16;
//			}
//
//			if (lcd != NULL) {
//				fprintf(lcd, "%d - %s\n", file_index_array[curr_file],
//						&file_name_array[curr_file]);
//				fprintf(lcd, "%s\n", play_mode);
//			}
//			check_screen_block = !(check_screen_block);
//		}
//
//		//open the file
//		if (reset_open_flag) {
//			int f_res = f_open(&File1, file_name_array[curr_file], 1);
//		}
//
//		//open the file
//		ofs = File1.fptr;
//		int i = 0;
//
//		//----------------------------------------------------------------
//
//		//skip
//		if (loop_status == 14) {
//			printf("1 pressed\n");
//			curr_file++;
//			//update the display
//			check_screen_block = !(check_screen_block);
//			loop_status = 0;
//			break;
//		}
//
//		// play/pause
//		if (loop_status == 13) {
//			while (1) {
//
//				//if paused
//				if(play_pause_flag == 0){
//					//set the display flag
//					play_pause_flag = !(play_pause_flag);
//					//we dont want to reset the song
//					reset_open_flag = 0;
//					break;
//				}
//
//				//if stopped
//
////				xprintf("tasks \n");
//
//			}
//			loop_status = 0;
//			break;
//		}
//
//		//stop
//		if (loop_status == 11) {
//			printf("3 pressed\n");
//			check_screen_block = !(check_screen_block);
//			loop_status = 0;
//			break;
//		}
//represents the spot we are in the loop
	int music_mode = 1;
	int incrementer = 4;
	int i = 0;

//update the display
	update_display(lcd);

	while (1) {

		//read the swtiches
		int switch_input1 = IORD(SWITCH_PIO_BASE, 0);

		//normal
		if (switch_input1 == 0) {
			incrementer = 4;
			music_mode = 1;
			speed_mode = "NORMAL";
		}
		//double
		if (switch_input1 == 2) {
			incrementer = 8;
			music_mode = 1;
			speed_mode = "DOUBLE";
		}
		//mono
		if (switch_input1 == 3) {
			music_mode = 3;
			incrementer = 4;
			speed_mode = "MONO";

		}
		//half
		if (switch_input1 == 1) {
			music_mode = 2;
			speed_mode = "HALF";
		}

		//-------------------------------

		ofs = File1.fptr;
		int i = 0;

		p1 = file_size_array[curr_file];
		//open the file
		f_open(&File1, file_name_array[curr_file], 1);

		if (stop_flag == 0) {
			while (p1) {

				if (skip_flag == 1) {
					break;
				}
				if (stop_flag) {
					break;
				}

				if (play_pause_flag == 0) {
					continue;
				}

				if (play_pause_flag) {

					if ((uint32_t) p1 >= blen) {
						cnt = blen;
						p1 -= blen;
					} else {
						cnt = p1;
						p1 = 0;
					}
					res = f_read(&File1, Buff, cnt, &s2);

					//////////

					for (i = 0; i < cnt; i += incrementer) {

						l_buf = (Buff[i + 1] << 8) | Buff[i];
						r_buf = (Buff[i + 3] << 8) | Buff[i + 2];

						//normal and double
						if (music_mode == 1) {

							//waiting for the audio
							while (alt_up_audio_write_fifo_space(audio_dev,
							ALT_UP_AUDIO_RIGHT) == 0) {
							}
							while (alt_up_audio_write_fifo_space(audio_dev,
							ALT_UP_AUDIO_LEFT) == 0) {
							}

							alt_up_audio_write_fifo(audio_dev, &(l_buf), 1,
							ALT_UP_AUDIO_LEFT);
							//check right fifo space
							alt_up_audio_write_fifo(audio_dev, &(r_buf), 1,
							ALT_UP_AUDIO_RIGHT);
						} else if (music_mode == 2) {

							int w;
							for (w = 0; w < 2; w++) {
								while (alt_up_audio_write_fifo_space(audio_dev,
								ALT_UP_AUDIO_RIGHT) == 0) {
								}
								while (alt_up_audio_write_fifo_space(audio_dev,
								ALT_UP_AUDIO_LEFT) == 0) {
								}

								alt_up_audio_write_fifo(audio_dev, &(l_buf), 1,
								ALT_UP_AUDIO_LEFT);
								//check right fifo space
								alt_up_audio_write_fifo(audio_dev, &(r_buf), 1,
								ALT_UP_AUDIO_RIGHT);
							}
						}

						//monospeed
						else if (music_mode == 3) {
							while (alt_up_audio_write_fifo_space(audio_dev,
							ALT_UP_AUDIO_RIGHT) == 0) {
							}
							while (alt_up_audio_write_fifo_space(audio_dev,
							ALT_UP_AUDIO_LEFT) == 0) {
							}

							alt_up_audio_write_fifo(audio_dev, &(l_buf), 1,
							ALT_UP_AUDIO_LEFT);
							//check right fifo space
							alt_up_audio_write_fifo(audio_dev, &(l_buf), 1,
							ALT_UP_AUDIO_RIGHT);
						}

					}

					//////////

					if (res != FR_OK) {
						put_rc(res); // output a read error if a read error occurs
						break;
					}
					p2 += s2; // increment p2 by the s2 referenced value
					if (cnt != s2) //error if cnt does not equal s2 referenced value ???
						//if the file is done
						break;
				}
			}
			//update the display after finishing playing it
			if (skip_flag == 1) {
				update_display(lcd);
				skip_flag = 0;
				continue;

			} else {

				play_pause_flag = 0;
				play_mode = "STOP";
				stop_flag = 1;
				update_display(lcd);
			}

		}

	}

	return (0);

}
