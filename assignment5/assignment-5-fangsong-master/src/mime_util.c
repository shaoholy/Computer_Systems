/*
 * mime_util.c
 *
 * Functions for processing MIME types.
 *
 *  @since 2019-04-10
 *  @author: Philip Gust
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "file_util.h"
#include "mime_util.h"
#include "http_server.h"

#define MIMEFILE "mime.types"

static const char *DEFAULT_MIME_TYPE = "application/octet-stream";

/**
 * Lowercase a string
 */
char *strlower(char *s)
{
    for (char *p = s; *p != '\0'; p++) {
        *p = tolower(*p);
    }

    return s;
}

/**
 * Return a MIME type for a given filename.
 *
 * @param filename the name of the file
 * @param mimeType output buffer for mime type
 * @return pointer to mime type string
 */
char *getMimeType(const char *filename, char *mimeType)
{
	// get the file extension
    char *ext = strrchr(filename, '.');

    if (ext == NULL) { // default if no extension
    	strcpy(mimeType, DEFAULT_MIME_TYPE);
    	return mimeType;
    }

    char buf[MAXBUF];
    strcpy(buf, ++ext); // copy and lower-case the extension
    strlower(ext);

//    const char *mtstr;

    // hash on first char?
//    switch (*ext) {
//    case 'c':
//        if (strcmp(ext, "css") == 0) { mtstr = "text/css"; }
//        break;
//    case 'g':
//        if (strcmp(ext, "gif") == 0) { mtstr = "image/gif"; }
//        break;
//    case 'h':
//        if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) { mtstr = "text/html"; }
//        break;
//    case 'j':
//    	if (strcmp(ext, "jpeg") == 0 || strcmp(ext, "jpg") == 0) { mtstr = "image/jpg"; }
//    	else if (strcmp(ext, "js") == 0) { mtstr = "application/javascript"; }
//    	else if (strcmp(ext, "json") == 0) { mtstr = "application/json"; }
//    	break;
//    case 'p':
//        if (strcmp(ext, "png") == 0) { mtstr = "image/png"; }
//        break;
//    case 't':
//    	if (strcmp(ext, "txt") == 0) { mtstr = "text/plain"; }
//    	break;
//    default:
//    	mtstr = DEFAULT_MIME_TYPE;
//    }

//    strcpy(mimeType, mtstr);



    //--------------------Improved MIME type processing---------------------//
    char* improvedMimeType(const char*, char*);
    strcpy(mimeType, improvedMimeType(MIMEFILE, ext));

	 return mimeType;
}


/**
 * Improve the original hard_coding mechanism.
 * Search in the mime.types file.
 * Return a MIME type for a given filename.
 *
 * @param filename the name of the file
 * @param ext the file extension string
 * @return pointer to mimeType string
 */
char *improvedMimeType(const char *filename, char *ext) {
	FILE *fp = NULL;
	char line[512];
	char *delimiters = " \t\n";
	int flag = 0;
	char * mimeType = NULL;
	mimeType = malloc (128 * sizeof(char)); //IMPO!!

	fp = fopen(filename, "r");

	while(fgets(line, 512, fp) != NULL) { //For each row
		if (strstr(line, ext) == NULL) { //This line don't include ext
			continue;
		}

		char *array[128];
		int i = 0;
	    char* newString;
	    newString= strtok(line, delimiters); //Split the ext by delimiters

	    while (newString != NULL)
	    {
	    	array[i++] = newString;
	    	if(strcmp(ext, newString) == 0) { //Find a match!
	    		strcpy(mimeType, array[i - 2]);
	    		//printf("%s\n", mimeType);
	    		flag = 1;
				break;
			}

	        newString= strtok(NULL, delimiters); //Keep spliting
	    }

		if (flag == 1) {
			break;
		}
	}

	 return mimeType;
}
