#include <stdio.h>
#include <SDL2/SDL.h>
#include "annie/mod.h"
#include "annie/audio.h"

int main(int argc, char **argv){

	
	if(argc < 2){
		printf("Usage: %s <input>\n", argv[0]);
		return 1;
	}

	AnnieAudioThread audioThread;	
    AnnieAudio_Init(&audioThread);

	char *path = argv[1];

	MODFile_t file;

    if(MODFile_Load(&file, argv[1]))
        AnnieAudio_PlayMOD(&audioThread, &file);

    while(1){
    	SDL_Delay(1000);
    }

    AnnieAudio_Close(&audioThread);
	return 0;

}
