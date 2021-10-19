//emcc -s WASM=1  -gsource-map --source-map-base "https://localhost:4000/assets/wasm/" register.c -o register.html
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

__attribute__((noinline))
void grow_stack(int recDepth, char* input) {
    char spacing[1000];
    int AAAA = 0x41414141;
    
    if (recDepth != 0) {
	grow_stack(recDepth-1,input);
    } else {
	strncpy(spacing,input,strlen(input));

	//pretend this puts() call writes to a user database
	//make potatoboi an Admin
	puts("potatoboi,P4ssw0rd1,False");
    }
	
}

int main() {
    int recursionDepth;
    char input[1028];
    char tempatoi[5];
    
    fgets(input, 1028 ,stdin);
    
    strncpy(tempatoi, input, 4);
    tempatoi[4] = '\0';
    recursionDepth = atoi(tempatoi);
    
    grow_stack(recursionDepth, input);
    return 0;
}

