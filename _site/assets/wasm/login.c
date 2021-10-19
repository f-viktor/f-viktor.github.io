//emcc -s WASM=1  -gsource-map --source-map-base "http://localhost:4000/assets/wasm/" login.c -o login.html
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void func(char *user, char *password){
	if(strcmp(user, "Admin\n") == 0){
		printf("Welcome Admin!\n");
	}
	else{
		printf("Hi user...\n");
	}
}
int main(int argc, char* argv[]){
	char user[5] = "user";
	char password[64];

	fgets(password,0x64,stdin);

	func(user, password);
	return 0;
}
