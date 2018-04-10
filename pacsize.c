#include <stdio.h> /*(f)printf, s(n)printf, scanf, fopen, fclose, remove, fgetc, fputc, perror,
				   rename, sscanf*/
#include <string.h> //str(n)cpy, str(n)cat, str(n)cmp, strstr, strlen, memset
#include <stdlib.h> /*getenv, malloc, calloc, realloc, free, atoi, realpath, EXIT_FAILURE and 
					EXIT_SUCCESS macros*/
#include <ctype.h> //isspace, isdigit, tolower
#include <dirent.h> //scandir
#include <sys/stat.h> //stat, lstat, mkdir
#include <errno.h>

#define PROGRAM_NAME "pacsize"
#define VERSION "0.3.4"
#define DATE "Mar, 2018"
#define AUTHOR "L. M. Abramovich"
#define PATH "/var/lib/pacman/local"

/*As of today, pacman has no option to list installed packages by size without relying on some
external parsing program like sed or awk. Pacsize is intended to fill this whole.*/

/*TODO list
- If compiled with the -O flag the program segfaults when sorting the arrays (line 168).
*/

int skip_implied_dot (const struct dirent *entry)
{
	if (strcmp(entry->d_name, ".") !=0 && strcmp(entry->d_name, "..") !=0 && strncmp(entry->d_name, "ALPM_DB_VERSION", 15) !=0) {
		return 1;
	}
	else
		return 0;
}

void pkg_size (char *pkg)
{
	//Check pkg existence
	struct dirent **dirlist;
	int i=0, files=0;
	files=scandir(PATH, &dirlist, skip_implied_dot, alphasort);
	if (files == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		exit (EXIT_FAILURE);
	}
	//Store all matches into a dynamic two-dimensional array
	int pkg_found=0;
	char **pkg_name=NULL;
	for (i=0;i<files;i++) {
		if (strncmp(dirlist[i]->d_name, pkg, strlen(pkg)) == 0) {
			char **tmp_pkg_name=realloc(pkg_name, sizeof(char *)*(pkg_found+1));
			if (tmp_pkg_name == NULL) {
				fprintf(stderr, "%s: %s failed to allocate %zu bytes\n", PROGRAM_NAME, __func__, 
															  sizeof(char *)*(pkg_found+1));
				exit (EXIT_FAILURE);
			}
			else 
				pkg_name=tmp_pkg_name;
			pkg_name[pkg_found]=calloc(strlen(dirlist[i]->d_name)+1, sizeof(char));
			strcpy(pkg_name[pkg_found++], dirlist[i]->d_name);
		}
		free(dirlist[i]);
	}
	free(dirlist);
	if (!pkg_found) {
		fprintf(stderr, "'%s': Package not found\n", pkg);
		return;
	}
	//Get pkg name and size for each matching pkg
	FILE *fp;
	for (i=0;i<pkg_found;i++) {
		char *pkg_path=NULL;
		pkg_path=calloc(strlen(pkg_name[i])+1, sizeof(char *));
		sprintf(pkg_path, "/var/lib/pacman/local/%s/desc", pkg_name[i]);
		fp=fopen(pkg_path, "r");
		if (fp == NULL) {
			fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));		
			for (i=0;i<pkg_found;i++)
				free(pkg_name[i]);
			free(pkg_name);
			return;
		}
		char line[128]="", name[128]="";
		int name_found=0, size_found=0; 
		float size=0;
		while(fgets(line, sizeof(line), fp)) {
			if (name_found) strcpy(name, line);
			if (strncmp(line, "%NAME%", 6) == 0) {
				name_found=1;
				continue; //if %NAME% is found, go to the next line and save it
			}
			name_found=0;
			if (size_found) size=atoi(line);
			if (strncmp(line, "%SIZE%", 6) == 0) {
				size_found=1;
				continue; //if %SIZE% is found, go to the next line and save it
			}
			size_found=0;
		}
		fclose(fp);
		free(pkg_path);
		name[strlen(name)-1]='\0'; //remove trailing new line char
		printf("%-25.24s %.2f Mb\n", name, size/1024/1024);
	}
	//free the array of matching pkgs
	for (i=0;i<pkg_found;i++)
		free(pkg_name[i]);
	free(pkg_name);
}

void total_size (int limit)
{
	struct dirent **dirlist;
	int i=0, files=0;
	files=scandir(PATH, &dirlist, skip_implied_dot, alphasort);
	if (files == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		exit (EXIT_FAILURE);
	}
	char pkg_name[files-1][128]; //files-1 is the amount of pkgs; 128 is names max length
	float pkg_size[files-1];
	FILE *fp;
	//Read description file ('desc') for all packages
	for (i=0;i<files;i++) {
		char *pkg_path=NULL;
		pkg_path=calloc(strlen(dirlist[i]->d_name)+1, sizeof(char *));
		sprintf(pkg_path, "/var/lib/pacman/local/%s/desc", dirlist[i]->d_name);
		fp=fopen(pkg_path, "r");
		if (fp == NULL) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, pkg_path, strerror(errno));
			if (i == (files-1)) exit (EXIT_FAILURE); //if last pkg name
			else continue;
		}
		char line[128]="", name[128]="";
		int name_found=0, size_found=0; 
		float size=0;
		while(fgets(line, sizeof(line), fp)) {
			if (name_found) strcpy(name, line);
			if (strncmp(line, "%NAME%", 6) == 0) {
				name_found=1;
				continue; //if %NAME% is found, go to the next line and save it
			}
			name_found=0;
			if (size_found) size=atoi(line);
			if (strncmp(line, "%SIZE%", 6) == 0) {
				size_found=1;
				continue; //if %SIZE% is found, go to the next line and save it
			}
			size_found=0;
		}
		name[strlen(name)-1]='\0'; //remove trailing new line char
		//store package name and size into two arrays, pkg_name and pkg_size
		strcpy(pkg_name[i], name);
		pkg_size[i]=size;
		fclose(fp);
		free(pkg_path);
		free(dirlist[i]);
		//Go back and read the next package
	}
	free(dirlist);	
	//Sort the two arrays (pkg_name and pkg_size) in ascending order according to pkg_size
	for (i=0;i<files;i++) {
		for (int j=i+1;j<files;j++) {
			if (pkg_size[i] > pkg_size[j]) {
				//sort pkg_size
				float tmp_size=pkg_size[i];
				pkg_size[i]=pkg_size[j];
				pkg_size[j]=tmp_size;				
				//sort pkg_name
				char *tmp_name=calloc(128, sizeof(char));
//				printf("files: %d i: %d j: %d\n", files, i, j);
				strcpy(tmp_name, pkg_name[i]);
				strcpy(pkg_name[i], pkg_name[j]);
				strcpy(pkg_name[j], tmp_name);
				free(tmp_name);
			}
		}
	}
	//Print the results
	if (limit < 0 || limit > files) i=0;
	else i=(files-limit);
	float total=0;
	int count=0;
	for (;i<files;i++) { //ommit first parameter, since i is already defined above
		total+=pkg_size[i];
		float pkg_size_final=pkg_size[i]/1024/1024;
		if (pkg_size_final >= 1) {
			printf("%d %-24.23s%.2f Mb\n", ++count, pkg_name[i], pkg_size_final);
		}
		else { //if less than 1Mb, print in Kb
			printf("%d %-24.23s%.0f Kb\n", ++count, pkg_name[i], pkg_size[i]/1024);
		}
	}
	float total_final=total/1024/1024/1024;
	if (total_final >= 1) printf("\nTotal:%-22s%.2f Gb\n", "", total/1024/1024/1024);
	else printf("\nTotal:%-22s%.2f Mb\n", "", total/1024/1024);	
}

void help (void)
{
	printf("Get Arch Linux installed package(s) size\n");
	printf("\nUsage: pacsize [limit] [pkg_name pkg_name ...]\n");
	printf("\nWith no arguments, pacsize will show all installed packages size in ascending order \
plus a grand total. If the first argument is a number ('limit'), pacsize will \
show only the 'limit' largest installed packages.\n");
}

void version (void)
{
	printf("%s %s (%s, by %s)\n", PROGRAM_NAME, VERSION, DATE, AUTHOR);
}

int main (int argc, char **argv)
{
	struct stat file_attrib;
	if (stat(PATH, &file_attrib) != 0) {
		printf("%s: Could not found '%s'\n", PROGRAM_NAME, PATH);
		exit (EXIT_FAILURE);
	}
	if (argc > 1) {
		int alpha=0;
		for (int i=0;i<strlen(argv[1]);i++) {
			if (!isdigit(argv[1][i]))
				alpha=1;
			else
				alpha=0;
		}
		if (!alpha) { //if first argument is a number
			total_size(atoi(argv[1]));
			exit (EXIT_SUCCESS);
		}
		//if first argument not a number
		for (int i=1;i<argc;i++) {
			if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
				help ();
				exit (EXIT_SUCCESS);
			}
			if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
				version ();
				exit (EXIT_SUCCESS);
			}
			//if neither help nor version, arguments are taken to mean pkg names
			pkg_size (argv[i]);
		}
		exit (EXIT_SUCCESS);
	}
	//If no arguments, print all pkgs size and a grand total
	total_size (-1);
	return EXIT_SUCCESS;
}
