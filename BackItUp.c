#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

struct thread_args{
	char* original_path;
	char* copy_path;
};

extern int errno;
int MAX_FILE_LENGTH = 255;
int MAX_FILE_NO = 99999;
char backUpFolder[]="/.backup";
int total_bytes = 0;
int total_files = 0;
pthread_t tid[99999];
int thread_count=0;
int mode=-1;
struct thread_args all_paths[99999];

int traverse(char path[], char copy_path[]);
void copy(char original_path[], char copy_path[]);

void* copyThread(void* input){

	if(mode==1){
		printf("[Thread %i]Backing up %s\n", thread_count, ((struct thread_args*)input)->original_path);
	}
	else if(mode == 0){
		printf("[Thread %i]Restoring %s\n", thread_count, ((struct thread_args*)input)->original_path);
	}
	char* original_path = ((struct thread_args*)input)->original_path;
	char* copy_path = ((struct thread_args*)input)->copy_path;
	int original_fd;
	int copy_fd;
	int bytes_read;
	char buffer[4096];
	int okay_to_write = 0;
	if ((original_fd=open(original_path, O_RDONLY)) == -1){
		printf("Unable to open original file.\n");
		exit(1);
	}

	// only copy if file has been modified since last backup
	if ((copy_fd=open(copy_path, O_RDWR)) == -1){

		// cant open file so create it
		if ((copy_fd=creat(copy_path, 0644)) == -1){
			okay_to_write=0;
		}
		else {
			// file has been created so it can be written to
			okay_to_write = 1;
		}

	}
	else {
		okay_to_write = 1;
		// can open file so check if its been modified
		struct stat original_info;
		struct stat copy_info;
		if (stat(original_path, &original_info) == -1){
			printf("Error getting stat info on original file.\n");
			exit(1);
		}
		if ( stat(copy_path, &copy_info) == -1){
			printf("Error getting stat info on file copy.\n");
			exit(1);
		}


		if (original_info.st_mtime > copy_info.st_mtime){
			okay_to_write = 1;
		}
		else {
			printf("[Thread %d]NOTICE: %s is already the most current version\n", thread_count, copy_path);
			okay_to_write=0;
		}
	}

	// File is new or has been modified since last backup.
	// Create file copy.
	if (okay_to_write == 1){
		printf("[Thread %d]WARNING: Overwriting %s\n", thread_count, copy_path);
		int bytes=0;
		while ((bytes_read = read(original_fd, buffer, 4096)) > 0){
			if (write(copy_fd, buffer, bytes_read) == -1){
				perror("Error writing to file copy");
			}
			bytes=bytes+bytes_read;
		}
		printf("[Thread %d]Copied %i bytes from %s to %s\n", thread_count, bytes, original_path, copy_path);
		total_bytes = total_bytes + bytes;
		total_files = total_files + 1;
	}
	close(original_fd);
	close(copy_fd);
}


int restore(){
	printf("Restoring data\n");

	struct stat st = {0};
	if(stat(".backup", &st)==-1){ //if backup folder not exist, return -1
		perror("Stat");
		printf("Error in opening .backup. Restore done\n");
		return -1;
	}
	char backUpPath[255];
	realpath(".", backUpPath);
	strncat(backUpPath, backUpFolder, strlen(backUpFolder));

	// traverse all directories
	char cwd[256];
	if (getcwd(cwd,sizeof(cwd)) == NULL){
		perror("getcwd() error");
	}
	traverse(backUpPath, cwd);
	return 1;
}

int backup(){
	printf("Backing up data\n");

	// create backup directory
	struct stat st = {0};
	if(stat(".backup", &st)==-1){ //if backup folder not exist, return -1
		perror("Stat");
		mkdir(".backup", 0700);
		printf("Created folder .backup since .backup not exist\n");
	}
	char backUpPath[255];
	realpath(".", backUpPath);
	strncat(backUpPath, backUpFolder, strlen(backUpFolder));

	// traverse all directories from cwd
	char cwd[256];
	if (getcwd(cwd,sizeof(cwd)) == NULL){
		perror("getcwd() error");
	}
	traverse(cwd,backUpPath);
}

int traverse(char path[],char copy_path[]) {

	DIR *dir;
  struct stat s, *current_file = &s;
  struct dirent * entry;
	struct thread_args *filePaths = (struct thread_args*)malloc(sizeof(struct thread_args));
    // Recursively traverse directories
    if ((dir = opendir(path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if ((strcmp(entry->d_name, "..") != 0) && (strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name,".backup") != 0)) {

                // append pathname
                char fullpath[PATH_MAX + 1];
                strcpy(fullpath, path);
                strcat(fullpath, "/");
                strcat(fullpath, entry->d_name);

		// append copy path
		char fullcopypath[PATH_MAX + 1];
		strcpy(fullcopypath,copy_path);
		strcat(fullcopypath,"/");
		strcat(fullcopypath, entry->d_name);

    // if directory: create copy, traverse inward
    // if file: create copy
    if (lstat(fullpath,current_file) == 0 ) {
    	if (S_ISDIR (current_file->st_mode)) {
				mkdir(fullcopypath, 0700);
        traverse(fullpath, fullcopypath);
    	}
      else {
				// copy_path_file leads to ./backup without extra correct folders
				char copy_path_file[PATH_MAX + 1];
				strcpy(copy_path_file, copy_path);
				strcat(copy_path_file, "/");
				strcat(copy_path_file, entry->d_name);
				if(mode==1) //backup mode
					strcat(copy_path_file, ".bak");
				else if(mode==0){ //restore mode
					int length=strlen(copy_path_file)-4;
					char temp[length+1];
					memcpy(temp, copy_path_file, length);
					temp[length]='\0';
					strcpy(copy_path_file, temp);
				}

				// original_path_file
				char original_path_file[PATH_MAX + 1];
				strcpy(original_path_file, path);
				strcat(original_path_file, "/");
				strcat(original_path_file, entry->d_name);

				filePaths->original_path = original_path_file;
				filePaths->copy_path = copy_path_file;
				all_paths[thread_count]=*filePaths;

				pthread_create(&tid[thread_count], NULL, &copyThread,(void *) filePaths);
				pthread_join(tid[thread_count], NULL);
				thread_count++;
                	}
                }
                else {
                	perror("Error using lstat()");
                }
            }
        }
       closedir(dir);
    }
    else {
        perror("Error opening specified directory");
    }
    return 0;
}

int getLength(char** array){
	int i=0;
	for(i=0; strcmp("\0", array[i])!=0; i++){
	}
	return i;
}

char** getFileName(char folderName[]){//get the names of all files in the folder
	DIR *directory;
	struct dirent *dire;
	
	char** nameArray=malloc(MAX_FILE_NO*sizeof(char*));
	int i=0;
	for(i=0; i<MAX_FILE_NO; i++){
		nameArray[i]=malloc(MAX_FILE_LENGTH*sizeof(char));
	}

	directory=opendir(folderName);
	i=0;
    if(directory){
		while((dire=readdir(directory)) !=NULL){
			if(strcmp(dire->d_name, ".")==0 || strcmp(dire->d_name, "..")==0)
				continue;
			strcpy(nameArray[i], dire->d_name);
			i++;
		}
	}
	else
		printf("Can't open %s\n", folderName);

	//print file names
	closedir(directory); //close the directory
	int fileNo=i;
	printf("no of files=%i\n", fileNo);
	for(i=0; i<fileNo; i++){
		printf("%s, size=%i\n", nameArray[i], (int)sizeof(nameArray[i]));
	}
	int len = getLength(nameArray);
	printf("len=%i\n", len);
	return nameArray;
}

int main(int argc, char *argv[]){
	// check if restore or backup
	if (argc > 1){
		if ( strcmp("-r",argv[1]) == 0){
			mode=0;
			restore();
			printf("Sucessfully copied %i files (%i bytes)\n", total_files, total_bytes);
		}
		else {
			printf("Argument not recognized. Exiting. \n");
			return 0;
		}
	}
	else {
		mode=1;
		backup();
		printf("Sucessfully copied %i files (%i bytes)\n", total_files, total_bytes);
	}
	return 0;
}
