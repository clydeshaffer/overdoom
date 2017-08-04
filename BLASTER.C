#include <stdio.h>
#include <stdlib.h>

#include "BLASTER.H"

#define LEFT_FM_STATUS      0x00    /* Pro only */
#define LEFT_FM_ADDRESS     0x00    /* Pro only */
#define LEFT_FM_DATA        0x01    /* Pro only */
#define RIGHT_FM_STATUS     0x02    /* Pro only */
#define RIGHT_FM_ADDRESS    0x02    /* Pro only */
#define RIGHT_FM_DATA       0x03    /* Pro only */

unsigned SbIOaddr;
unsigned SbIRQ;
unsigned SbDMAchan;
int  SbType;

int note_octaves[] = { 0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8 };

int note_fnums[] = {0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B,0x158,0x16D,0x183,0x19A,0x1B2,0x1CC,0x1E7,0x204,0x223,0x244,0x266,0x28B};

int Sb_Get_Params(void)
{
    char *t, *t1, *blaster;

    /* Set arguments to reasonable values (Soundblaster defaults) */
    SbIOaddr = 0x220;
    SbIRQ = 7;
    SbDMAchan = 1;

    /* Attempt to read environment variable */
    t = getenv("BLASTER");

    /* Is the environment variable set? */
    if(t == NULL)
    return 1;

    /* Duplicate the string so that we don't trash our environment */
    blaster = strdup(t);

    /* Now parse the BLASTER variable */
    t = strtok(blaster," \t");
    while(t)
    {
    switch(toupper(t[0]))
    {
        case 'A':                               /* I/O address */
        SbIOaddr = (int)strtol(t+1,&t1,16);
        break;
        case 'I':                               /* Hardware IRQ */
        SbIRQ = atoi(t+1);
        break;
        case 'D':                               /* DMA channel */
        SbDMAchan = atoi(t+1);
        break;
        case 'T':                               /* Soundblaster type */
        SbType = atoi(t+1);
        break;
        default:
        printf("Unknown BLASTER option %c\n",t[0]);
        break;
    }
    t = strtok(NULL," \t");
    }
    free(blaster);
    return 0;
}

void WriteFM(int chip, int addr, unsigned char data)
{
    register int ChipAddr = SbIOaddr + ((chip) ? RIGHT_FM_ADDRESS :
                        LEFT_FM_ADDRESS);

    outportb(ChipAddr,addr);
    inportb(ChipAddr);

    outportb(ChipAddr+1,data);
    inportb(ChipAddr);
    inportb(ChipAddr);
    inportb(ChipAddr);
    inportb(ChipAddr);
}

void Sb_FM_Reset(void)
{
    WriteFM(0,1,0);
    WriteFM(1,1,0);
}

void Sb_FM_Key_Off(int voice)
{
    unsigned char reg_num;
    int chip = voice / 11;
    
    /* turn voice off */
    reg_num = 0xB0 + voice % 11;

    WriteFM(chip,reg_num,0);
}

void Sb_FM_Key_On(int voice, int freq, int octave)
{
    register unsigned char reg_num;
    unsigned char tmp;
    int chip = voice / 11;

    reg_num = 0xa0 + voice % 11;

    WriteFM(chip,reg_num,freq & 0xff);

    reg_num = 0xb0 + voice % 11;
    tmp = (freq >> 8) | (octave << 2) | 0x20;

    WriteFM(chip,reg_num,tmp);
}

void Sb_FM_Voice_Volume(int voice, int vol)
{
    unsigned char reg_num;
    int chip = voice / 11;

    reg_num = 0x40 + voice % 11;

    WriteFM(chip,reg_num,vol);
}

void Sb_FM_Set_Voice(int voice_num, FM_Instrument *ins)
{
    register unsigned char op_cell_num;
    int cell_offset;
    int i, chip = voice_num / 11;

    voice_num %= 11;

    /* check on voice_num range */
    cell_offset = voice_num % 3 + ((voice_num / 3) << 3);

    /* set sound characteristic */
    op_cell_num = 0x20 + (char)cell_offset;

    WriteFM(chip,op_cell_num,ins->SoundCharacteristic[0]);
    op_cell_num += 3;
    WriteFM(chip,op_cell_num,ins->SoundCharacteristic[1]);

    /* set level/output */
    op_cell_num = 0x40 + (char)cell_offset;
    WriteFM(chip,op_cell_num,ins->Level[0]);
    op_cell_num += 3;
    WriteFM(chip,op_cell_num,ins->Level[1]);

    /* set Attack/Decay */
    op_cell_num = 0x60 + (char)cell_offset;
    WriteFM(chip,op_cell_num,ins->AttackDecay[0]);
    op_cell_num += 3;
    WriteFM(chip,op_cell_num,ins->AttackDecay[1]);

    /* set Sustain/Release */
    op_cell_num = 0x80 + (char)cell_offset;
    WriteFM(chip,op_cell_num,ins->SustainRelease[0]);
    op_cell_num += 3;
    WriteFM(chip,op_cell_num, ins->SustainRelease[1]);

    /* set Wave Select */
    op_cell_num = 0xE0 + (char)cell_offset;
    WriteFM(chip,op_cell_num,ins->WaveSelect[0]);
    op_cell_num += 3;
    WriteFM(chip,op_cell_num,ins->WaveSelect[1]);

    /* set Feedback/Selectivity */
    op_cell_num = (unsigned char)0xC0 + (unsigned char)voice_num;
    WriteFM(chip,op_cell_num,ins->Feedback);
}