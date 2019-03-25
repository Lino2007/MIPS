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
CPU_State CURRENT_STATE, NEXT_STATE, LAST_STATE;

typedef struct  {
    enum {ADD, SUB, SW, LW} opc;
    uint32_t  rd,rt, rs,im;
    enum {RF, IF} form;
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

void flush_instruction () {
    instruc.im=instruc.opc=instruc.form=instruc.rd=instruc.rs=instruc.rt=-1;
}


void decode_instruction(uint32_t instruction)
{
    flush_instruction();
    uint32_t opcode=(instruction>>26)&0x3F;
    int rs = (instruction>>21)&0x1F;
    //  char registri[]= {"$0","$s", "$t" };
    if (opcode==0) {
        int funct=instruction&0x3F;

        instruc.opc=-1;
        instruc.form=RF;
        int rd=(instruction>>16)&0x1F, rt=(instruction>>11)&0x1F;
        //Provjera validnosti registara
        if ( rt<8 || rt>23 || rs<8 || rs>23 || rd<8 || rd>23) {
            printf ("Neka druga instrukcija\n");
            goto Label;
        }
        ////////////////////////////////
        //Koja je operacija u pitanju
        if (funct==32 ) {
            printf("add");
            instruc.opc=ADD;
        }
        else if (funct==34  ) {
            printf ("sub");
            instruc.opc=SUB;
        }
        else  { printf ("Neka druga instrukcija");
            goto Label;
            return ;
        }
        /////////////////////////////
        //Postavka registara
        //Za RT
        if (rt>=8 && rt<=15) {
            printf (" $t%d,", (instruc.rd=rt)-8);
        } else   printf (" $s%d,", (instruc.rd=rt)-16);
        //////////////////////
        //RS
        if (rs>=8 && rs<=15) {
            printf (" $t%d,", (instruc.rs=rs)-8);
        } else if (rs==0)  printf (" $0,");
        else if (rs>=16 && rs<=23)   printf (" $s%d,", (instruc.rs=rs)-16);
        ////////////////////
        //RD
        if (rd>=8 && rd<=15) {
            printf (" $t%d", (instruc.rt=rd)-8);
        } else if (rd==0)  printf ("$0");
        else   printf (" $s%d",  (instruc.rt=rd)-16);
       /////////////////////
      // printf ("%d %d %d %d %d ")
    } else {
        short int imm= instruction&0xFFFF;
        int rt=(instruction>>16)&0x1F;
        instruc.im=imm;
       /* if ( rt<8 || rt>23 || rs<8 || rs>23) {
            printf ("Neka druga instrukcija\n");
            goto Label;
        } */
        if (opcode==35) {
            printf ("lw ");
            instruc.opc=LW;
        } else   if (opcode==43) {
            printf ("sw ");
            instruc.opc=SW;
        } else {
            printf("Neka druga instrukcija");
            goto Label;
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
    if (0) {
        Label: return;
    }
}

void execute_instruction (uint32_t instruction) {
     decode_instruction(instruction);
     if (instruc.opc==ADD ){
         NEXT_STATE.REGS[instruc.rd]=CURRENT_STATE.REGS[instruc.rd]+(CURRENT_STATE.REGS[instruc.rs]+CURRENT_STATE.REGS[instruc.rt]);
       CURRENT_STATE.REGS[instruc.rd]=NEXT_STATE.REGS[instruc.rd];

     }
     else if (instruc.opc==SUB) {
         NEXT_STATE.REGS[instruc.rd]=CURRENT_STATE.REGS[instruc.rd] - (CURRENT_STATE.REGS[instruc.rs] + CURRENT_STATE.REGS[instruc.rt]);
         CURRENT_STATE.REGS[instruc.rd] = NEXT_STATE.REGS[instruc.rd];
     }
     else if (instruc.opc==SW ) {

     }
     else if (instruc.opc==LW) {
         uint8_t *pom= (uint8_t *) (MEM_DATA_START+instruc.rs+ instruc.im);
         uint8_t *poms= (uint8_t *) (MEM_DATA_START);
       NEXT_STATE.REGS[instruc.rs]= (poms);
         CURRENT_STATE.REGS[instruc.rs] = NEXT_STATE.REGS[instruc.rs];
         printf ("%d", CURRENT_STATE.REGS[instruc.rs]);
     }
     else {
         printf("Instrukcija nije podrzana ili ne postoji!\n");
     }
     flush_instruction();

}

void reg_state () {
    for (int i=0 ; i<32; i++ ){
        printf ("Registar %d : %x \n", i, CURRENT_STATE.REGS[i]);
    }
}
void reg_value_init () {
    for (int i=0 ; i<32; i++ ){
       CURRENT_STATE.REGS[i]=i;
    }
}


int main(void)
{
    init_memory();
    load_program("Resources/program.txt");
    reg_value_init();
  //  reg_state();
    uint32_t p=MEM_INSTRUCTIONS_START;
    printf ("Izvrsene su sljedece instrukcije: \n");
    do {
        execute_instruction(mem_read(p));
        p+=4;
    } while (p<LAST_STATE.PC);
 //   reg_state();
    return 0;
}
