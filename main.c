#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define MIPS_REGS 32 // broj registara
#define MEM_DATA_START 0x00000000 // početak dijela memorije za podatke
#define MEM_DATA_SIZE 0x00000100 // veličina dijela memorije za podatke
#define MEM_INSTRUCTIONS_START 0x00000100 // početak dijela memorije za instrukcije
#define MEM_INSTRUCTIONS_SIZE 0x00000080 // veličina dijela memorije za instrukcije

// stanje procesora
typedef struct CPU_State {
    uint32_t PC; // programski brojac
    uint32_t REGS[MIPS_REGS]; // register file
} CPU_State;


// regioni glavne memorije (dio za podatke, dio za instrukcije)
typedef struct {
    uint32_t start, size; // početak regiona i veličina
    uint8_t *mem; // sadržaj
} mem_region;

mem_region MEM_REGIONS[] = {
        {MEM_DATA_START, MEM_DATA_SIZE, NULL},
        {MEM_INSTRUCTIONS_START, MEM_INSTRUCTIONS_SIZE, NULL}
};

#define MEM_NREGIONS 2

// trenutno stanje CPU-a
//LAST_STATE naknadno dodan radi oznacavanja gdje se nalazi posljednja instrukcija
CPU_State CURRENT_STATE, NEXT_STATE, LAST_STATE;

// pomocna struktura za identifikaciju instrukcije
typedef struct  {
    enum {ADD, SUB, SW, LW, XOR} opc; //Tip instrukcije (XOR je bio moj zadatak za dodati)
    uint32_t  rd,rt, rs,im;  //Vrijednosti registara
} CUR_inst;

CUR_inst instruc;

// funkcija koja piše u glavnu memoriju
void mem_write(uint32_t address, uint32_t value)
{
    int i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        if (address >= MEM_REGIONS[i].start &&
            address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
            uint32_t offset = address - MEM_REGIONS[i].start;
            // memorija je byte-adresabilna, pa se jedna (32-bitna) riječ "dijeli" na 4 dijela
            MEM_REGIONS[i].mem[offset+3] = (value >> 24) & 0xFF;
            MEM_REGIONS[i].mem[offset+2] = (value >> 16) & 0xFF;
            MEM_REGIONS[i].mem[offset+1] = (value >>  8) & 0xFF;
            MEM_REGIONS[i].mem[offset+0] = (value >>  0) & 0xFF;
            return;
        }
    }
}

// memorija koja čita vrijednost iz glavne memorije
uint32_t mem_read(uint32_t address)
{
    int i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        if (address >= MEM_REGIONS[i].start &&
            address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
            uint32_t offset = address - MEM_REGIONS[i].start;
            // memorija je byte-adresabilna, pa pri čitanju 32-bitne riječi čitaju se 4 byte-a
            return
                    (MEM_REGIONS[i].mem[offset+3] << 24) |
                    (MEM_REGIONS[i].mem[offset+2] << 16) |
                    (MEM_REGIONS[i].mem[offset+1] <<  8) |
                    (MEM_REGIONS[i].mem[offset+0] <<  0);
        }
    }

    return 0;
}

// inicijalizacija glavne memorije
void init_memory()
{
    int i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        MEM_REGIONS[i].mem = malloc(MEM_REGIONS[i].size);
        memset(MEM_REGIONS[i].mem, 0, MEM_REGIONS[i].size);
    }
}

// funkcija koja učitava program iz .txt datoteke u memoriju instrukcija
void load_program(char *program_filename)
{
    FILE * ulaz= fopen (program_filename, "r");
    if (ulaz == NULL) {
        printf("#Greska: Datoteka nije otvorena\n");
        return ;
    }
    time_t t;
    srand(time(&t));
    //random vrijednost iz intervala [1,1000]
    uint32_t w=1+rand()%1000, adresa=MEM_INSTRUCTIONS_START;
    //U globalnom opsegu je definiran dodatni parametar tipa CPU_State
    while (fscanf (ulaz,"%x\n", &w)!=EOF) {
        mem_write (adresa,w);
        adresa+=4;
        LAST_STATE.PC=adresa;
    }
    CURRENT_STATE.PC=MEM_INSTRUCTIONS_START;
    fclose(ulaz);
}

//resetuje identifikator instrukcije
void flush_instruction () {
    instruc.im=instruc.opc=instruc.rd=instruc.rs=instruc.rt=-1;
}

//ispisuje koja je instrukcija u pitanju i unosi podatke u identifikator funkcije
void decode_instruction(uint32_t instruction)
{
    flush_instruction();
    uint32_t opcode=(instruction>>26)&0x3F;
    int rs = (instruction>>21)&0x1F;

    //Provjera formata instrukcije (r ili i)
    if (opcode==0) {
        int funct=instruction&0x3F;
        instruc.opc=-1;

        int rt=(instruction>>16)&0x1F, rd=(instruction>>11)&0x1F;
        //Provjera validnosti registara
        if ( rt<8 || rt>23 || rs<8 || rs>23 || rd<8 || rd>23) {
            printf ("Neka druga instrukcija\n");
        }

        //Utvrdjivanje vrste instrukcije
        if (funct==32) {
            printf("add");
            instruc.opc=ADD;
        }
        else if (funct==34 ) {
            printf ("sub");
            instruc.opc=SUB;
        }
        else if (funct == 38) {
            printf ("Izvrsena je XOR instrukcija..");
            instruc.opc=XOR;
        }
        else  {
            printf ("Neka druga instrukcija. \n");
            return ;
        }

        //Ispis i deklariranje za svaki registar
        if (rt>=8 && rt<=15) {
            printf (" $t%d,", (instruc.rt=rt)-8);
        } else   printf (" $s%d,", (instruc.rt=rt)-16);

        if (rs>=8 && rs<=15) {
            printf (" $t%d,", (instruc.rs=rs)-8);
        } else if (rs==0)  printf (" $0,");
        else if (rs>=16 && rs<=23)   printf (" $s%d,", (instruc.rs=rs)-16);

        if (rd>=8 && rd<=15) {
            printf (" $t%d", (instruc.rd=rd)-8);
        } else if (rd==0)  printf ("$0");
        else   printf (" $s%d",  (instruc.rd=rd)-16);

    } else {
        //Instrukcije i-formata
        short int imm= instruction&0xFFFF;
        int rt=(instruction>>16)&0x1F;
        instruc.im=imm;

        if (opcode==35) {
            printf ("lw ");
            instruc.opc=LW;
        } else   if (opcode==43) {
            printf ("sw ");
            instruc.opc=SW;
        } else {
            printf("Neka druga instrukcija. \n");
            return ;
        }

        if (rt>=8 && rt<=15) {
            printf (" $t%d,", (instruc.rt=rt)-8);
        } else if (rt==0)  printf (" $0,");
        else   printf (" $s%d,", (instruc.rt=rt)-16);

        if (rs>=8 && rs<=15) {
            printf ("%d($t%d)", imm, (instruc.rs=rs)-8);
        } else if (rs==0)  printf (" $0");
        else   printf ("%d($s%d)", imm, (instruc.rs=rs)-16);
    }
    printf ("\n");
}

void execute_instruction (uint32_t instruction) {
    decode_instruction(instruction);
    if (instruc.opc == ADD) {
        NEXT_STATE.REGS[instruc.rd] =
                CURRENT_STATE.REGS[instruc.rd] + (CURRENT_STATE.REGS[instruc.rs] + CURRENT_STATE.REGS[instruc.rt]);
        CURRENT_STATE.REGS[instruc.rd] = NEXT_STATE.REGS[instruc.rd]; //Refresh CURRENT_STATE-a (iako nije trazeno, NEXT_STATE je besmislen svakako)
    } else if (instruc.opc == SUB) {
        NEXT_STATE.REGS[instruc.rd] =
                CURRENT_STATE.REGS[instruc.rd] - (CURRENT_STATE.REGS[instruc.rs] + CURRENT_STATE.REGS[instruc.rt]);
        CURRENT_STATE.REGS[instruc.rd] = NEXT_STATE.REGS[instruc.rd];
    } else if (instruc.opc == SW) {
        mem_write( CURRENT_STATE.REGS[instruc.rs] + instruc.im, CURRENT_STATE.REGS[instruc.rt]);
    } else if (instruc.opc == LW) {
        NEXT_STATE.REGS[instruc.rt] = mem_read(NEXT_STATE.REGS[instruc.rs] + instruc.im);
        CURRENT_STATE.REGS[instruc.rs] = NEXT_STATE.REGS[instruc.rs];
    } else if (instruc.opc = XOR) {
        NEXT_STATE.REGS[instruc.rd] = CURRENT_STATE.REGS[instruc.rs] ^ CURRENT_STATE.REGS[instruc.rt];
        CURRENT_STATE.REGS[instruc.rd] = NEXT_STATE.REGS[instruc.rd];
    } else {
        printf("Instrukcija nije podrzana ili ne postoji!\n");
    }
    flush_instruction();
}

//Provjera sadrzaja registara
void reg_state () {
    for (int i=0 ; i<32; i++ ){
        printf ("Registar %d : %x \n", i, CURRENT_STATE.REGS[i]);
    }
}
//Punimo registre random vrijednostima
void reg_value_init () {
    time_t t;
    srand(time(&t));
    for (int i=0 ; i<32; i++ ){
        CURRENT_STATE.REGS[i]=1+ rand ()%50;
    }
}

//Punimo podrucije memorije sa random vrijednostima
void napuni () {
    time_t t;
    srand(time(&t));
    for (int i = 0 ; i<MEM_DATA_START+MEM_DATA_SIZE; i=i+4) {
        uint32_t w=1+rand()%1000;
        mem_write (MEM_DATA_START+i, w);
    }
}

//Provjera da li je podrucije memorije popunjeno
void procitaj () {
    for (int i = 0 ; i<MEM_DATA_START+MEM_DATA_SIZE; i=i+4) {
        printf( "%d: %x ", i/4, mem_read (MEM_DATA_START+i));
    }
    printf ("\n");
}


int main(void)
{
    init_memory();
    load_program("Resources/program.txt");

    printf ("Pocetne vrijednosti registara: \n");
    reg_value_init();
    reg_state();
    printf ("Pocetne vrijednosti podrucija podataka: \n");
    napuni();
    procitaj();

    uint32_t p=MEM_INSTRUCTIONS_START;
    printf ("Izvrsene su sljedece instrukcije: \n");
    do {
        execute_instruction(mem_read(p));
        p+=4;
    } while (p<LAST_STATE.PC);

    printf ("Krajnje vrijednosti registara: \n");
    reg_state();
    printf ("Krajnje stanje podrucija podataka: \n");
    procitaj();
    return 0;
}