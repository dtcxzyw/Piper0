export  void sobolKernel(uniform const uint32 matrix[], uniform uint32 res[], uniform const uint32 dims, uniform const uint32 index) {
    for(uint32 i = 0; i < 32;++i) {
        if(index&(1<<i)) {
            foreach(j = 0 ... dims)
                res[j]^=matrix[(i<<10)+j];
        }
    }

    foreach (i = 0 ... dims)
        res[i] = intbits(min(0x1.fffffep-1, (float)(res[i]) * (1.0f / (float)(1ULL << 32))));
}
