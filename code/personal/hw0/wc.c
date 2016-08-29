#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
	FILE *input;

	//read from file
	if(argc>1){
		//Opening file
		char *fileName=argv[1];
		char *mode = "r";
		input=fopen(fileName, mode);

		if(input == NULL){
			printf("Can't open file %s!\n", fileName);
			exit(0);
		}
	}
	//read from stdin
	else{
		input=stdin;
	}

	//Counting
	int numOfLines=0, numOfWords=0, numOfCharacters=0;
	int c;
	char prevTemp=' ';
	char currTemp;

	while((c=fgetc(input)) != EOF){
		currTemp=(char)c;

		if (currTemp=='\n' || currTemp==' ' || currTemp=='\t'){
			if (prevTemp != '\n' && prevTemp != ' ' && prevTemp != '\r' && prevTemp != '\t'){
				numOfWords+=1;
			}

			if (currTemp=='\n'){
				numOfLines+=1;
			}
		}

		numOfCharacters+=1;

		//saving current char to compare with next char input
		prevTemp=currTemp;
	}

	if (prevTemp!='\n' && prevTemp!=' ' && prevTemp!='\t' && prevTemp!='\x00'){
		numOfWords+=1;
	}

	//Closing file and print the results
	if(argc>1){
		fclose(input);
		printf(" %i %i %i %s", numOfLines, numOfWords, numOfCharacters, argv[1]);
	}
	else{
		printf("\t%i\t%i\t%i\t%p", numOfLines, numOfWords, numOfCharacters, input);
	}
	
	return 0;
}
