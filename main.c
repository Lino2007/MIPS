#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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
CPU_State CURRENT_STATE, NEXT_STATE, LAST_STATE;


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
    uint32_t w=0, adresa=MEM_INSTRUCTIONS_START;
    //U globalnom opsegu je definiran dodatni parametar tipa CPU_State
    while (fscanf (ulaz,"%x\n", &w)!=EOF) {
        mem_write (adresa,w);
        adresa+=4;
        LAST_STATE.PC=adresa;
    }
    CURRENT_STATE.PC=MEM_INSTRUCTIONS_START;
    fclose(ulaz);
}

// funkcija koja dekodira instrukciju
void decode_instruction(uint32_t instruction)
{
    uint32_t opcode=(instruction>>26)&0x3F;
    int rs = (instruction>>21)&0x1F;
    //  char registri[]= {"$0","$s", "$t" };
    if (opcode==0) {
        int funct=instruction&0x3F;
        if (funct==32) printf ("add");
        else if (funct==34) printf ("sub");
        else { printf ("Neka druga instrukcija");
            goto Label;
            return ;
        }

        int rd=(instruction>>16)&0x1F, rt=(instruction>>11)&0x1F;
        if (rt>=8 && rt<=15) {
            printf (" $t%d,", rt-8);
        } else   printf (" $s%d,", rt-16);

        if (rs>=8 && rs<=15) {
            printf (" $t%d,", rs-8);
        } else if (rs==0)  printf (" $0,");
        else   printf (" $s%d,", rs-16);

        if (rd>=8 && rd<=15) {
            printf (" $t%d", rd-8);
        } else if (rd==0)  printf ("$0");
        else   printf (" $s%d", rd-16);

    } else {
        short int imm= instruction&0xFFFF;
        int rt=(instruction>>16)&0x1F;

        if (opcode==35) {
            printf ("lw ");
        } else   if (opcode==43) {
            printf ("sw ");
        } else {
            printf("Neka druga instrukcija");
            goto Label;
            return ;
        }

        if (rt>=8 && rt<=15) {
            printf (" $t%d,", rt-8);
        } else if (rt==0)  printf (" $0,");
        else   printf (" $s%d,", rt-16);


        if (rs>=8 && rs<=15) {
            printf ("%d($t%d)", imm,rs-8);
        } else if (rs==0)  printf (" $0");
        else   printf ("%d($s%d)", imm,rs-16);


    }
    printf ("\n");
    if (0) {
        Label: return;
    }
}




int main()
{
    init_memory();
    load_program("Resources/program.txt");
    uint32_t p=MEM_INSTRUCTIONS_START;
    do {
        decode_instruction(mem_read(p));
        p+=4;
    } while (p<LAST_STATE.PC);
    return 0;
}
