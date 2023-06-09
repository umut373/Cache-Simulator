/*
    CSE2138 Systems Programming Project 3 - Bonus
    Umut Özil - 150121019
    Mustafa Emir Uyar - 150120007
    Burak Karayağlı - 150121824
    Ege Keklikçi - 150121029
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cacheLine {
    int valid;
    long long tag;
    int time;
    unsigned char* data;
} cacheLine;

typedef struct cacheSet {
    cacheLine* lines;
    int setId;
} cacheSet;

typedef struct cache {
    char* name;
    int setSize;
    int blockSize;
    int associativity;
    cacheSet* sets;

} cache;

// construct a cache with given parameters
cache constructCache(char* name, int s, int b, int e) {
    cache c = {name, 1<<s, 1<<b, 1<<e, NULL};
    // allocate memory for sets
    c.sets = malloc(sizeof(cacheSet) * c.setSize);
    for (int i = 0; i < c.setSize; i++) {
        c.sets[i].setId = i;
        // allocate memory for lines
        c.sets[i].lines = malloc(sizeof(cacheLine) * c.associativity);
        for (int j = 0; j < c.associativity; j++) {
            // initialize values to zero
            c.sets[i].lines[j].valid = 0;
            c.sets[i].lines[j].tag = 0;
            c.sets[i].lines[j].time = 0;
            c.sets[i].lines[j].data = malloc(sizeof(unsigned char) * c.blockSize);
        }
    }
    return c;
}

// global variables
int time = 0;
cache L1I, L1D, L2;
unsigned char** ramData;
long ramSize;
FILE *output;
int L1s, L1E, L1b, L2s, L2E, L2b;
char* tracefile;

// hit - miss - evacuation
int L1D_counter[3] = {0, 0, 0};
int L1I_counter[3] = {0, 0, 0};
int L2_counter[3] = {0, 0, 0};

void takeArguments(int argc, char *argv[]);
void readTrace(char* filepath, int s1, int b1, int s2, int b2);
unsigned char** readRam();
int load(cache* c, int setIndex, int tag, int ramIndex, unsigned char** ramData);
int storeCache(cache* c, int setIndex, int tag, int blockOffset, int ramIndex, int size, unsigned char** data);
void storeRam(int ramIndex, int ramOffset, int size, unsigned char** data);
void printOutput();
void printRam();

int main(int argc, char *argv[]) {
    // take arguments from command line
    takeArguments(argc, argv);

    output = fopen("output.txt", "w");

    // construct caches
    L1I = constructCache("L1I",L1s, L1b, L1E);
    L1D = constructCache("L1D", L1s, L1b, L1E);
    L2 = constructCache("L2", L2s, L2b, L2E);

    ramData = readRam();
    readTrace(tracefile, L1s, L1b, L2s, L2b);

    printRam();
    fclose(output);
    printOutput();
    return 0;
}

void takeArguments(int argc, char *argv[]) {
    // Check if the number of arguments is correct
    if (argc != 15) {
        printf("Invalid number of arguments. Please provide all required arguments.\n");
        exit(1);
    }

    for (int i = 1; i < argc; i +=2) {
        if (strcmp(argv[i], "-L1s") == 0)
            L1s = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L1E") == 0)
            L1E = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L1b") == 0)
            L1b = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L2s") == 0)
            L2s = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L2E") == 0)
            L2E = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L2b") == 0)
            L2b = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-t") == 0)
            tracefile = argv[i + 1];
        else {
            printf("Invalid argument: %s\n", argv[i]);
            exit(1);
        }
    }
}

void readTrace(char* filepath, int s1, int b1, int s2, int b2) {
    FILE *trace = fopen(filepath, "r"); // open trace file

    char instr = fgetc(trace);
    while (instr != EOF) {
        // address and size are common for every instruction
        int address;
        int size;
        fscanf(trace, "%x, %d", &address, &size);

        // this part added for skiping large address
        if (address >= ramSize) {
            printf("Invalid address: %x\n", address);
            exit(1);
        }

        // claculate the index of data in RAM from address
        int ramIndex = address / 8;
        int ramOffset = address % 8;

        // calculate set index, block offset and tag for L1 and L2 cahce
        int L1_setIndex = (address >> b1) & ((1 << s1) - 1);
        int L1_blockOffset = address & ((1 << b1) - 1);
        int L1_tag = address >> (s1 + b1);

        int L2_setIndex = (address >> b2) & ((1 << s2) - 1);
        int L2_blockOffset = address & ((1 << b2) - 1);
        int L2_tag = address >> (s2 + b2);

        // for instruction S and M
        char data[size*2 + 1];
        unsigned char* dataSplit[size];

        switch (instr) {
            case 'I':
                fprintf(output, "I %08x, %d\n", address, size);
                    
                int L1State = load(&L1I, L1_setIndex, L1_tag, ramIndex, ramData);
                L1State ? fprintf(output, "\tL1I hit, ") : fprintf(output, "\tL1I miss, ");
                int L2State = load(&L2, L2_setIndex, L2_tag, ramIndex, ramData);
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                if (L1State && L2State) {
                    // L2 hit L1I hit
                    L2_counter[0]++;
                    L1I_counter[0]++;
                }
                else if(L1State && !L2State) {
                    // L2 miss L1I hit
                    L2_counter[1]++;
                    L1I_counter[0]++;
                    fprintf(output, "\tPlace in L2 set %d\n", L2_setIndex);
                }
                else if(!L1State && L2State) {
                    // L2 hit L1I miss
                    L2_counter[0]++;
                    L1I_counter[1]++;
                    fprintf(output, "\tPlace in L1I set %d\n", L1_setIndex);
                }
                else {
                    // L2 miss L1I miss
                    L2_counter[1]++;
                    L1I_counter[1]++;
                    fprintf(output, "\tPlace in L2 set %d, L1I set %d\n", L2_setIndex, L1_setIndex);
                }

                break;

            case 'L':
                fprintf(output, "D %08x, %d\n", address, size);

                L1State = load(&L1D, L1_setIndex, L1_tag, ramIndex, ramData);
                L1State ? fprintf(output, "\tL1D hit, ") : fprintf(output, "\tL1D miss, ");
                L2State = load(&L2, L2_setIndex, L2_tag, ramIndex, ramData);
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                if (L1State && L2State) {
                    // L2 hit L1D hit
                    L2_counter[0]++;
                    L1D_counter[0]++;
                }
                else if(L1State && !L2State) {
                    // L2 miss L1D hit
                    L2_counter[1]++;
                    L1D_counter[0]++;
                    fprintf(output, "\tPlace in L2 set %d\n", L2_setIndex);
                }
                else if(!L1State && L2State) {
                    // L2 hit L1D miss
                    L2_counter[0]++;
                    L1D_counter[1]++;
                    fprintf(output, "\tPlace in L1D set %d\n", L1_setIndex);
                }
                else {
                    // L2 miss L1D miss
                    L2_counter[1]++;
                    L1D_counter[1]++;
                    fprintf(output, "\tPlace in L2 set %d, L1D set %d\n", L2_setIndex, L1_setIndex);
                }
                break;

            case 'S':
                // skip ',' and ' '
                for (int i = 0; i < size*2; i++) {
                    char c = fgetc(trace);
                    if (c != ',' && c != ' ')
                        data[i] = c;
                    else 
                        i--;
                }
                data[size*2] = '\0';
                
                fprintf(output, "S %08x, %d, %s\n", address, size, data);
                
                // split data value into bytes
                for (int i = 0; i < size; i++) {
                    dataSplit[i] = malloc(sizeof(unsigned char) * 3);
                    strncpy(dataSplit[i], &data[i*2], 2);
                    dataSplit[i][2] = '\0';
                }

                storeRam(ramIndex, ramOffset, size, dataSplit);

                L1State = storeCache(&L1D, L1_setIndex, L1_tag, L1_blockOffset, ramIndex, size, dataSplit);
                L2State = storeCache(&L2, L2_setIndex, L2_tag, L2_blockOffset, ramIndex, size, dataSplit);

                L1State ? fprintf(output, "\tL1D hit, ") : fprintf(output, "\tL1D miss, ");
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                fprintf(output,"\tStore in ");

                if (L1State) {
                    // L1D hit
                    L1D_counter[0]++;
                    fprintf(output, "L1D, ");
                } else {
                    // L1D miss
                    L1D_counter[1]++;
                }
                if (L2State) {
                    // L2 hit
                    L2_counter[0]++;
                    fprintf(output, "L2, ");
                } 
                else {
                    // L2 miss
                    L2_counter[1]++;
                }
                fprintf(output,"RAM\n");
            break;    

            case 'M':
                // skip ',' and ' '
                for (int i = 0; i < size*2; i++) {
                    char c = fgetc(trace);
                    if (c != ',' && c != ' ')
                        data[i] = c;
                    else
                        i--;
                }
                data[size*2] = '\0';

                fprintf(output, "M %08x, %d, %s\n", address, size, data);

                // split data value into bytes
                for (int i = 0; i < size; i++) {
                    dataSplit[i] = malloc(sizeof(unsigned char) * 3);
                    strncpy(dataSplit[i], &data[i*2], 2);
                    dataSplit[i][2] = '\0';
                }

                L1State = load(&L1D, L1_setIndex, L1_tag, ramIndex, ramData);
                L1State ? fprintf(output, "\tL1D hit, ") : fprintf(output, "\tL1D miss, ");
                L2State = load(&L2, L2_setIndex, L2_tag, ramIndex, ramData);
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                fprintf(output,"\tModify in L2, L1D, RAM\n");

                if (L1State && L2State) {
                    // L1D hit L2 hit
                    L1D_counter[0]++;
                    L2_counter[0]++;
                }
                else if(L1State && !L2State) {
                    // L1D hit L2 miss
                    L1D_counter[0]++;
                    L2_counter[1]++;
                }
                else if(!L1State && L2State) {
                    // L1D miss L2 hit
                    L1D_counter[1]++;
                    L2_counter[0]++;
                }
                else {
                    // L1D miss L2 miss
                    L1D_counter[1]++;
                    L2_counter[1]++;
                }

                // storeRam(L2_blockOffset, ramIndex, size, dataSplit);
                L1State = storeCache(&L1D, L1_setIndex, L1_tag, L1_blockOffset, ramIndex, size, dataSplit);
                L2State = storeCache(&L2, L2_setIndex, L2_tag, L2_blockOffset, ramIndex, size, dataSplit);
                storeRam(ramIndex, ramOffset, size, dataSplit);

                if (L1State && L2State) {
                    // L1D hit L2 hit
                    L1D_counter[0]++;
                    L2_counter[0]++;
                }
                else if(L1State && !L2State) {
                    // L1D hit L2 miss
                    L1D_counter[0]++;
                    L2_counter[1]++;
                }
                else if(!L1State && L2State) {
                    // L1D miss L2 hit
                    L1D_counter[1]++;
                    L2_counter[0]++;
                }
                else {
                    // L1D miss L2 miss
                    L1D_counter[1]++;
                    L2_counter[1]++;
                }
        }
        // read next instruction
        fgetc(trace);
        instr = fgetc(trace);

        time++;
    }
}

unsigned char** readRam() {
    FILE *ram = fopen("RAM.dat", "rb"); // open ram file

    // get ram size
    fseek(ram, 0, SEEK_END);
    ramSize = ftell(ram);
    rewind(ram);

    // create tow-dim array for ram data
    unsigned char **ramData = malloc(sizeof(unsigned char) * ramSize);
    // fill ram data
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

    //If no empty line, find the least recently used line using fifo
    if (lineIndex == -1) {
        int minTime = c->sets[setIndex].lines[0].time;
        lineIndex = 0;
        for (int i = 0; i < c->associativity; i++) {
            //check for the time
            if(c->sets[setIndex].lines[i].time < minTime){
                minTime = c->sets[setIndex].lines[i].time;
                lineIndex = i;
            }
        }
        //Increment the counter for eviction
        if (strcmp(c->name, "L1D")==0){
            L1D_counter[2]++;
        }
        else if (strcmp(c->name, "L1I")==0){
            L1I_counter[2]++;
        }
        else if (strcmp(c->name, "L2")==0){
            L2_counter[2]++;
        }
    }

    //Load data from ram and update cache
    c->sets[setIndex].lines[lineIndex].valid = 1;
    c->sets[setIndex].lines[lineIndex].tag = tag;
    c->sets[setIndex].lines[lineIndex].time = time;
    
    int ramOffset = 0;
    for (int i = 0; i < c->blockSize; i++) {
        if (ramOffset >= 8) {
            ramIndex++;
            ramOffset = 0;
        }
        c->sets[setIndex].lines[lineIndex].data[i] = ramData[ramIndex][ramOffset];
        ramOffset++;
    }
    return 0;
}

//Store data to ram
void storeRam(int ramIndex, int ramOffset, int size, unsigned char** data) {
    for (int i = 0; i < size; i++) {
        if (ramOffset+i >= 8) {
            ramIndex++;
            ramOffset = 0 - i;
        }
        ramData[ramIndex][ramOffset+i] = strtol(data[i], NULL, 16);
    }
}

int storeCache(cache* c, int setIndex, int tag, int blockOffset, int ramIndex, int size, unsigned char** data) {
    //Check for hit
    int hit = 0;
    for (int i = 0; i < c->associativity; i++) {
        if(c->sets[setIndex].lines[i].valid == 1 && c->sets[setIndex].lines[i].tag == tag){
            hit = 1;
            //Hit, print data to ram
            for (int j = 0; j < size; j++) {
                c->sets[setIndex].lines[i].data[blockOffset+j] = strtol(data[j], NULL, 16);
            }
            break;
        }
    }
    return hit;
}

void printOutput() {
    // print hit - miss - eviction values for each cache
    printf("\t\tL1I-hits:%d ", L1I_counter[0]);
    printf("L1I-misses:%d ", L1I_counter[1]);
    printf("L1I-evictions:%d\n", L1I_counter[2]);
    printf("\t\tL1D-hits:%d ", L1D_counter[0]);
    printf("L1D-misses:%d ", L1D_counter[1]);
    printf("L1D-evictions:%d\n", L1D_counter[2]);
    printf("\t\tL2-hits:%d ", L2_counter[0]);
    printf("L2-misses:%d ", L2_counter[1]);
    printf("L2-evictions:%d\n", L2_counter[2]);

    FILE* output = fopen("output.txt", "r");

    // read output file and print it to stdout
    char c;
    c = getc(output);
    while(c != EOF) {
        printf("%c", c);
        c = getc(output);
    }
    fclose(output);
}

// print updated ram to binary file
void printRam() {
    FILE *ram = fopen("RAM_output.dat", "wb"); // open ram file

    for (int i = 0; i < ramSize/8; i++) {
        fwrite(ramData[i], sizeof(unsigned char), 8, ram);
    }
    fclose(ram);
}