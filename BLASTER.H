#ifndef BLASTER_H
#define BLASTER_H

typedef struct {
    unsigned char SoundCharacteristic[2];
    unsigned char Level[2];
    unsigned char AttackDecay[2];
    unsigned char SustainRelease[2];
    unsigned char WaveSelect[2];
    unsigned char Feedback;
    unsigned char filler[6];
} FM_Instrument;

void WriteFM(int chip, int addr, unsigned char data);

int Sb_Get_Params(void);

void Sb_FM_Reset(void);

void Sb_FM_Key_Off(int voice);

void Sb_FM_Key_On(int voice, int freq, int octave);

void Sb_FM_Voice_Volume(int voice, int vol);

void Sb_FM_Set_Voice(int voice_num, FM_Instrument *ins);

extern int note_octaves[];

extern int note_fnums[];

#endif