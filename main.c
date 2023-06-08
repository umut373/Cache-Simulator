#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cacheLine{
    int valid;
    long long tag;
    int time;
    unsigned char* data;
} cacheLine;

typedef struct cacheSet{
    cacheLine* lines;
    int setId;
} cacheSet;

typedef struct cache{
    char* name;
    int setSize;
    int blockSize;
    int associativity;
    cacheSet* sets;

} cache;

cache constructCache(char* name,int s, int b, int e){

    cache c = {name,1<<s,1<<b,1<<e,NULL};
    c.sets = malloc(sizeof(cacheSet)*c.setSize);
    for (int i = 0; i < c.setSize; i++)
    {
        c.sets[i].setId = i;
        c.sets[i].lines = malloc(sizeof(cacheLine)*c.associativity);
        for (int j = 0; j < c.associativity; j++)
        {
            c.sets[i].lines[j].valid = 0;
            c.sets[i].lines[j].tag = 0;
            c.sets[i].lines[j].time = 0;
            c.sets[i].lines[j].data = malloc(sizeof(char)*c.blockSize);
        }
    }
    return c;
}

int time = 0;
cache L1I, L1D, L2;
unsigned char **ramData;

void readTrace(char* filepath, int s1, int b1, int s2, int b2);
unsigned char** readRam();
int load(cache* c, int setIndex, int tag, int ramIndex, unsigned char** ramData);

int main(){
    L1I = constructCache("L1I",2,3,2);
    L1D = constructCache("L1D",2,3,2);
    L2 = constructCache("L2",1,5,2);


    // TEST CODE
    char path[30] = "traces\\";
    char filename[20];
    
    printf("Enter file name: ");
    scanf("%s", filename);
    strcat(path, filename);
    ramData = readRam();

    readTrace(path, 2, 3 ,1, 5);
    
    return 0;
}


void readTrace(char* filepath, int s1, int b1, int s2, int b2) {
    FILE *trace = fopen(filepath, "r"); // opean trace file

    char instr = fgetc(trace);
    while (instr != EOF) {
        // address and size are common for every instruction
        int address;
        int size;
        fscanf(trace, "%x, %d", &address, &size);

        // claculate the index of data in RAM from address
        int ramIndex = address / 8;

        // calculate set index, block offset and tag for L1 and L2 cahce
        int L1_setIndex = (address >> b1) & ((1 << s1) - 1);
        int L1_blockOffset = address & ((1 << b1) - 1);
        int L1_tag = address >> (s1 + b1);

        int L2_setIndex = (address >> b2) & ((1 << s2) - 1);
        int L2_blockOffset = address & ((1 << b2) - 1);
        int L2_tag = address >> (s2 + b2);

        // for instruction S and M
        char* data[size*2 + 1];
        char* dataSplit[size];

        switch (instr) {
            case 'I':
                if(load(&L1I, L1_setIndex, L1_tag, ramIndex, ramData)) {
                    printf("L1I hit,");
                }
                else {
                    printf("L1I miss,");
                    if(load(&L2, L2_setIndex, L2_tag, ramIndex, ramData)){
                        printf("L2 hit\n");
                        printf("Place in L1I set %d\n", L1_setIndex);
                    }
                    else {
                        printf("L2 miss\n");
                        printf("Place in L2 set %d,L1I set %d\n", L2_setIndex, L1_setIndex);
                    }
                }
                break;

                case 'D':
                if (load(&L1D, L1_setIndex, L1_tag,ramIndex, ramData)){
                    printf("L1D hit,");
                }
                else {
                    printf("L1D miss,");
                    if (load(&L2, L2_setIndex, L2_tag, ramIndex, ramData)) {
                        printf("L2 hit\n");
                        printf("Place in L1D set %d\n", L1_setIndex);
                    }
                    else {
                        printf("L2 miss\n");
                        printf("Place in L2 set %d,L1D set %d\n", L2_setIndex, L1_setIndex);
                    }
                }
                break;

            case 'S':
                // read data value and split it into bytes
                fscanf(trace, "%s", &data);
                for (int i = 0; i < size; i++) {
                    dataSplit[i] = malloc(sizeof(char) * 2);
                    strncpy(dataSplit[i], data + i*2, 2);
                }
                // call func
                break;

            case 'M':
                // read data value and split it into bytes
                fscanf(trace, "%s", &data);
                for (int i = 0; i < size; i++) {
                    dataSplit[i] = malloc(sizeof(char) * 2);
                    strncpy(dataSplit[i], data + i*2, 2);
                }
                break;
        }
        // read next instruction
        fgetc(trace); // skip '\n' character
        instr = fgetc(trace);

        time++;
    }
}

unsigned char** readRam() {
    FILE *ram = fopen("RAM.dat", "rb"); // open ram file

    fseek(ram, 0, SEEK_END);
    long ramSize = ftell(ram);
    rewind(ram);

    unsigned char **ramData = malloc(sizeof(unsigned char) * ramSize);
    for (int i = 0; i < ramSize/8; i++) {
        ramData[i] = malloc(sizeof(unsigned char) * 8);
        fread(ramData[i], sizeof(unsigned char), 8, ram);
    }
    return ramData;
}

int load(cache* c, int setIndex, int tag, int ramIndex, unsigned char** ramData) {
    //Check for hit
    for (int i = 0; i < c->associativity; i++) {
        if(c->sets[setIndex].lines[i].valid == 1 && c->sets[setIndex].lines[i].tag == tag){
            //Hit
            c->sets[setIndex].lines[i].time = time;
            return 1;
        }
    }
    int lineIndex = -1;

    //Find the first empty line
    for (int i = 0; i < c->associativity; i++) {
        if (c->sets[setIndex].lines[i].valid == 0){
            lineIndex = i;
            break;
        }
    }
    if (lineIndex == -1){
        int minTime = c->sets[setIndex].lines[0].time;
        for (int i = 0; i < c->associativity; i++) {
            if(c->sets[setIndex].lines[i].time < minTime){
                minTime = c->sets[setIndex].lines[i].time;
                lineIndex = i;
            }
        }
    }

    c->sets[setIndex].lines[lineIndex].valid = 1;
    c->sets[setIndex].lines[lineIndex].tag = tag;
    c->sets[setIndex].lines[lineIndex].time = time;
    
    for (int i = 0; i < c->blockSize; i++) {
        c->sets[setIndex].lines[lineIndex].data[i] = ramData[ramIndex][i];
        //printf("%x",c->sets[setIndex].lines[lineIndex].data[i]);
    }
    //printf("\n");
    return 0;
}