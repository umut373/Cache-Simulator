# Cache-Simulator
In this project, we wrote a cache simulator which takes an image of memory and a memory trace as input, simulates
the hit/miss behavior of a cache memory on this trace, and outputs the total number of hits, misses, and evictions for each
cache type along with the content of each cache at the end.

**Usage** : ./main -L1s <*L1s*> -L1E <*L1E*> -L1b <*L1b*>
               -L2s <*L2s*> -L2E <*L2E*> -L2b <*L2b*>
               -t <*tracefile*>
