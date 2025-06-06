#include <stdio.h>
#include <syslog.h>

//function declaration
int writer(const char *writefile, const char *writestr);


int main(int argc, char *argv[]){

	if (argc != 3){

		fprintf(stderr, "Usage: %s <writefile> <writestr>\n" ,argv[0]);

		return 1;
			}
	openlog("writer", LOG_PID, LOG_USER);

	int result = writer(argv[1], argv[2]);

	closelog();
	return result;
}


int writer(const char *writefile, const char *writestr){

	FILE* fptr = fopen(writefile, "w");
	if(fptr == NULL){
		perror("Error opening file");
		syslog(LOG_ERR, "Failed to open file %s", writefile);
		return 1;
	}

	fprintf(fptr, "%s", writestr);
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	fclose(fptr);
	return 0;

}


