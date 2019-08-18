/*
 * methods.c
 *
 * Functions that implement HTTP methods, including
 * GET, HEAD, PUT, POST, and DELETE.
 *
 *  @since 2019-04-10
 *  @author: Philip Gust
 */

#include "http_methods.h"

#include <stddef.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http_server.h"
#include "http_util.h"
#include "time_util.h"
#include "mime_util.h"
#include "properties.h"
#include "file_util.h"
#include <dirent.h>

/**
 * Handle GET or HEAD request.
 *
 * @param the socket stream
 * @param uri the request URI
 * @param requestHeaders the request headers
 * @param responseHeaders the response headers
 * @param sendContent send content (GET)
 */
static void do_get_or_head(FILE *stream, const char *uri, Properties *requestHeaders, Properties *responseHeaders, bool sendContent) {
	// get path to URI in file system
		char filePath[MAXBUF];
		resolveUri(uri, filePath);

		// ensure file exists
		struct stat sb;
		if (stat(filePath, &sb) != 0) {
		}

		// ensure file is a regular file, other make a dir page to client
		if (!S_ISREG(sb.st_mode)) {
			//step1: record list of files in dir to buff

			//copy file name to Name1
			DIR *dr = opendir(filePath);
			struct dirent *de;  // Pointer for directory entry
			const char HEADER[]=
					"<html>\n"
					"<head>\n"
					"<title>index of </title>\n"
					"</head>\n<body>\n  <h1>Index of %s</h1>\n<table>\n  <tr>\n    <th valign=\"top\"></th>\n"
					"    <th>Name</th>\n    <th>Last modified</th>\n    <th>Size</th>\n   "
					" <th>Description</th>\n  </tr>\n  <tr>\n    <td colspan=\"5\"><hr></td>\n  </tr>\n";
			const char ROW[]="  <tr>\n "
							"	<td></td>\n"
							"	<td><a href=\"%s/%s\">%s</a></td>\n"
							"	<td align=\"right\">%s</td>\n"
							"	<td align=\"right\">%s</td>\n"
							"	<td></td>\n"
							"  </tr>\n";
			//copy localfile to serverfile
			char newfilepath[MAXBUF] = "content/sfs.html";
			FILE *server = fopen(newfilepath, "w");
			//fprintf (server, "%s", NAME2);
			fprintf (server, HEADER, uri);

			while ((de = readdir(dr)) != NULL) {
				//continue with 3 exceptions
				if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, ".DS_Store") == 0) {
					continue;
				}

				//read sub file/folder into stat
				char curfilePath[MAXBUF];
				strcpy(curfilePath, filePath);
				if (strcmp(filePath, "content/") != 0) {
					strcat(curfilePath,  "/");
				}

				strcat(curfilePath,  de->d_name);
				struct stat subsb;
				if (stat(curfilePath, &subsb) != 0) {
				}
				time_t lastModifyTime = sb.st_mtim.tv_sec;
				char timebuf[256];
				size_t subcontentLen = (size_t)subsb.st_size;
				char sizebuf[256];
				sprintf(sizebuf,"%lu", subcontentLen);

				//write file info to each row into tmp file server
				if (strcmp(uri, "/") == 0) {
					uri = "";
				}
				char *filename[MAXBUF] = {};
				   strcat(filename, de->d_name);
				   if (S_ISDIR(subsb.st_mode)) {
				    strcat(filename,  "/");
				   }
				   fprintf (server, ROW, uri, de->d_name, filename, milliTimeToRFC_1123_Date_Time(lastModifyTime, timebuf), sizebuf);
			}
			fprintf (server, "  <tr><td colspan=\"5\"><hr></td></tr>\n</body>\n</html>\n");
			fclose (server);

			closedir(dr);

			//invoke  do_get()
			char uri2[MAXBUF] = "/sfs.html";
			//comments to add for TA
			do_get_or_head(stream, uri2, requestHeaders, responseHeaders, true);
			if (remove("content/sfs.html") == 0)
			      printf("Deleted successfully");
			else
			      printf("Unable to delete the file");
			return;
		}
		// record the file length
		char buf[MAXBUF];
		size_t contentLen = (size_t)sb.st_size;
		sprintf(buf,"%lu", contentLen);
		putProperty(responseHeaders,"Content-Length", buf);

		// record the last-modified date/time
		time_t timer = sb.st_mtim.tv_sec;
		putProperty(responseHeaders,"Last-Modified",
					milliTimeToRFC_1123_Date_Time(timer, buf));

		// get mime type of file
		getMimeType(filePath, buf);
		putProperty(responseHeaders, "Content-type", buf);

		// send response
		sendResponseStatus(stream, 200, "OK");

		// Send response headers
		sendResponseHeaders(stream, responseHeaders);

		if (sendContent) {  // for GET
			FILE *contentStream = fopen(filePath, "r");
			sendResponseBytes(contentStream, stream, contentLen);
			fclose(contentStream);
		}

}

/**
 * Handle GET request.
 *
 * @param the socket stream
 * @param uri the request URI
 * @param requestHeaders the request headers
 * @param responseHeaders the response headers
 * @param headOnly only perform head operation
 */
void do_get(FILE *stream, const char *uri, Properties *requestHeaders, Properties *responseHeaders) {
	do_get_or_head(stream, uri, requestHeaders, responseHeaders, true);
}

/**
 * Handle HEAD request.
 *
 * @param the socket stream
 * @param uri the request URI
 * @param requestHeaders the request headers
 * @param responseHeaders the response headers
 */
void do_head(FILE *stream, const char *uri, Properties *requestHeaders, Properties *responseHeaders) {
	do_get_or_head(stream, uri, requestHeaders, responseHeaders, false);
}

/**
 * Handle PUT request.
 *
 * @param the socket stream
 * @param uri the request URI
 * @param requestHeaders the request headers
 * @param responseHeaders the response headers
 */
void do_put(FILE *stream, const char *uri, Properties *requestHeaders, Properties *responseHeaders) {
	//sendErrorResponse(stream, 405, "put Method Not Allowed", responseHeaders);
	// get path to URI in file system
		char filePath[MAXBUF];
		resolveUri(uri, filePath);

		char len[MAXBUF];
		findProperty(requestHeaders, 0, "Content-Length", len);
		size_t lent = 0;
		if (1 == sscanf(len, "%zu", &lent)) {
		}


		int flag = 0;
		struct stat sb;
		if (stat(filePath, &sb) != 0) { //newly created
			flag = 1;
		}

		//Generate the directory or file only
		char *dirPath[MAXBUF];
		getPath(filePath, dirPath);
		//printf("dirPath is: %s\n", dirPath);
		mkdirs(dirPath, 0777);

		//copy localfile to serverfile
		FILE *server = fopen(filePath, "w");
		sendResponseBytes(stream, server, lent);

		if (flag == 1) { 				//newly created
			sendResponseStatus(stream, 201, "Created");
		} else {						//overwritten
			sendResponseStatus(stream, 200, "Ok");
		}

		//record the file length
		char buf[MAXBUF];
		size_t contentLen = 0;
		sprintf(buf,"%lu", contentLen);
		putProperty(responseHeaders,"Content-Length", buf);

		// record the last-modified date/time
		time_t timer = sb.st_mtim.tv_sec;
		putProperty(responseHeaders,"Last-Modified",milliTimeToRFC_1123_Date_Time(timer, buf));

		// get mime type of file
		getMimeType(filePath, buf);
		putProperty(responseHeaders, "Content-type", buf);

		// Send response headers
		sendResponseHeaders(stream, responseHeaders);


		fclose(server);
}

/**
 * Handle POST request.
 *
 * @param the socket stream
 * @param uri the request URI
 * @param requestHeaders the request headers
 * @param responseHeaders the response headers
 */
void do_post(FILE *stream, const char *uri, Properties *requestHeaders, Properties *responseHeaders) {
	//sendErrorResponse(stream, 405, "Method Not Allowed", responseHeaders);
	do_put(stream, uri, requestHeaders, responseHeaders);
}

/**
 * Handle DELETE request.
 *
 * @param the socket stream
 * @param uri the request URI
 * @param requestHeaders the request headers
 * @param responseHeaders the response headers
 */
void do_delete(FILE *stream, const char *uri, Properties *requestHeaders, Properties *responseHeaders) {
	//sendErrorResponse(stream, 405, "Method Not Allowed", responseHeaders);
	// get path to URI in file system
	char filePath[MAXBUF];
	resolveUri(uri, filePath);

	// file not exists
	struct stat sb;
	if (stat(filePath, &sb) != 0) {
		sendErrorResponse(stream, 404, "Not Found", responseHeaders);
		return;
	}
	// file is a directory but not empty
	if (S_ISDIR(sb.st_mode)) {
		if (isDirectoryEmpty(filePath) != 1) {
			sendErrorResponse(stream, 405, "Method not Allowed", responseHeaders);
			return;
		}
	}

//	printf("idDirect? %d\n", isDirectoryEmpty(filePath));
//	// file is not a directory or is not empty
//	if (isDirectoryEmpty(filePath) != 1) {
//		sendErrorResponse(stream, 405, "Method not Allowed", responseHeaders);
//		return;
//	}

	// record the file length
	char buf[MAXBUF];
	//size_t contentLen = (size_t)sb.st_size;
	size_t contentLen = 0;
	sprintf(buf,"%lu", contentLen);
	putProperty(responseHeaders,"Content-Length", buf);

	// record the last-modified date/time
	time_t timer = sb.st_mtim.tv_sec;
	putProperty(responseHeaders,"Last-Modified",
				milliTimeToRFC_1123_Date_Time(timer, buf));

	// get mime type of file
	getMimeType(filePath, buf);
	putProperty(responseHeaders, "Content-type", buf);

	// send response
	sendResponseStatus(stream, 200, "OK");

	// Send response headers
	sendResponseHeaders(stream, responseHeaders);


	//remove("/dir/formfile.html");
	//printf("filepath: %s\n", filePath);
	remove(filePath);

}

//If directory is empty, return 1;
//doesn't exist or not empty, return 0
int isDirectoryEmpty(char *dirname) {
  int n = 0;
  struct dirent *d;
  DIR *dir = opendir(dirname); // @suppress("Type cannot be resolved")
  if (dir == NULL) //Not a directory or doesn't exist
    return 0;
  while ((d = readdir(dir)) != NULL) {
    if(++n > 2)
      break;
  }
  closedir(dir);
  if (n <= 2) //Directory Empty
    return 1;
  else
    return 0;
}
