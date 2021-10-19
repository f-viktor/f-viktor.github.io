//emcc -s WASM=1 -s MALLOC=emmalloc -g heap.c -o heap.html
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *randstring(size_t length) {

    static char charset[] = "XYZ";        
    char *randomString = NULL;

    if (length) {
        randomString = malloc(sizeof(char) * (length +1));

        if (randomString) {            
            for (int n = 0;n < length;n++) {            
                int key = rand() % (int)(sizeof(charset) -1);
                randomString[n] = charset[key];
            }

            randomString[length] = '\0';
        }
    }

    return randomString;
}

void welcome(){
	printf("Wow, you're cool!\n");
}

void not_welcome(){
	printf("You Lose!\n");
	exit(0);
}

char cast_to_hex(char b) {
	if (b >= '0' && b <= '9') {
		return (b-'0');
	}
	else if (b >= 'A' && b <= 'Z') {
		return (b-'A'+10);
	}
	else {
		return (b-'a'+10);
	}
}

void login(){
	printf("checking...\n");
	char credentials[64]; 
	char rawInput[1000];
	void (*FuncPtr)();

	char *one = randstring(0x80);
	char *two = randstring(0x80);
	char *you = randstring(0x80);

	// are you cool yet?
	if (strcmp(you, "cool") == 0) {
		FuncPtr = welcome;
	} else {
		FuncPtr = not_welcome;
	}

	fgets(rawInput, 1000, stdin);

	int j = 0;
	for (int i = 0; i < strlen(rawInput)-1 ; i=i+2) {
		credentials[j] = cast_to_hex(rawInput[i])*16 + cast_to_hex(rawInput[i+1]);
		j++;
	}

	free(one);

	FuncPtr();

}

int main(){
	printf("Turbofast WASM login system.\n");
	login();
	printf("Now I can safely trust you that you have credential :)\n");
	return 0;
}
