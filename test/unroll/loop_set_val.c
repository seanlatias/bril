#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
int main(){
int64_t v12;
int64_t i;
int64_t v14;
int v15;
int64_t v21;
int64_t v23;
int64_t v25;
int v0[5];
v12 = 0;
i = v12;
for_cond_11:;
v14 = 5;
v15 = i < v14;
if(v15)
    goto for_body_11;
else
    goto for_end_11;
for_body_11:;
v0[i] = i;
v21 = 1;
i = i + v21;
goto for_cond_11;
for_end_11:;
v23 = 0;
v25 = v0[v23];
printf("%" PRId64 "\n", v25);
return 0;
}
