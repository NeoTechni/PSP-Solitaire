#include <pspaudio.h>
#include <pspaudiolib.h>
#include <psppower.h>

//MP3IsPlaying
char* FILE_NAME;
char* FILETITLEREAL;
bool LOADED= false;
bool RANDOM = false;

int CURR_FILE= -1, PREV_FILE = -1;

extern "C" {
	#include "mp3player.h"
}

/*
 red = color >> and 255
 
 void  me_startproc(u32 func, u32 param) 
 {             
   me_sceKernelDcacheWritebackInvalidateAll();     
    
   memcpy((void *) 0xbfc00040, me_stub, (int) (me_stub_end - me_stub)); 
        
   _sw(func,  0xbfc00600); 
   _sw(param, 0xbfc00604); 
    
   sceKernelDcacheWritebackAll(); 
    
   sceSysregMeResetEnable(); 
   sceSysregMeBusClockEnable();       
   sceSysregMeResetDisable(); 
 } 
 */
 
void InitAudio(){
	scePowerSetClockFrequency(333, 333, 166);
    pspAudioInit();
}

void LoadMP3(char * filename, int Channel = 1) {
	MP3_Init(Channel);
	if (LOADED) { MP3_FreeTune(); }
	
	if(FILE_NAME) { free(FILE_NAME); }
	FILE_NAME = strdup (filename);
	
	FILETITLEREAL = strrchr(FILE_NAME, '/');
	if (FILETITLEREAL) { 
		FILETITLEREAL++;
	} else {
		FILETITLEREAL = FILE_NAME; 
	}
	
    MP3_Load(FILE_NAME);
	LOADED=true;
}

void UnloadMP3(){
	MP3_FreeTune();
}	

void Play(){
	MP3_Play();
}

void Pause(){
	MP3_Pause();
}

void Stop(){
	MP3_Stop();
}

bool IsMP3Ended(){
	return (MP3_EndOfStream() == 1) ;
}





