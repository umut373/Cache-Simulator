#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cacheLine{
    int valid;
    long long tag;
    int time;
    char* data;
} cacheLine;
typedef struct cacheSet{
    cacheLine* lines;
    int setId;
} cacheSet;


typedef struct cache{
    int setSize;
    int blockSize;
    int associativity;
    cacheSet* sets;

}cache;



cache constructCache(int s, int b, int e){

    cache c = {1<<s,1<<b,1<<e,NULL};
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





int main(){
    cache L1I = constructCache(0,2,3);
    cache L1D = constructCache(0,2,3);
    cache L2 = constructCache(1,2,3);


    printf("%d",0x0000fcaa % blockSize);
    printf("%d",0x0000fcaa / blockSize*setSize);
    printf("%d",(0x0000fcaa / blockSize) % setSize );

}