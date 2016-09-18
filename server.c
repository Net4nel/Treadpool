//Name: Netanel Amiel
//ID: 303136972

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "threadpool.h"

//SIZES DEFINES:
#define MAX_LINE_LENGTH 500
#define MAX_READ_LENGTH 4000
#define MAX_ERR_LENGTH 512

//ERROR DEFINES:
#define ERROR302 302
#define ERROR400 400
#define ERROR403 403
#define ERROR404 404
#define ERROR500 500
#define ERROR501 501
#define ERRORDIR 998
#define ERRORFIL 999

//TIMER DEFINE:
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"


//PRIVATE MATHODS:
bool isNum(char* c);
int devideHandler(void* sd);
void lineSplitter(char* c ,int sd);
void errHandler(int err_num, char* path ,int sd);
int checkPermission(char* given_path);
char* get_mime_type(char *name);


int main(int argc, char *argv[])
{

	if(argc != 4)
	{
		perror("usage: server <port> <pool-size> <max-number-of-request> \n");
		exit(-1);
	}
	//check if argv[1,2,3] is numbers
	int i=1;
	while(i<4)
	{
		if(!isNum(argv[i]))
		{
			perror("usage: server <port> <pool-size> <max-number-of-request> \n");
			exit(-1);
		}
		i++;
	}

	int port = atoi(argv[1]);
	int pool_size = atoi(argv[2]);
	int max_req = atoi(argv[3]);



	//create the socket//
	int sd;
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("socket \n");
		exit(-1);
	}

	struct sockaddr_in srv;	 /* used by bind() */
	srv.sin_port = htons(port);	 /* bind socket ‘fd’ to port 80*/
	srv.sin_family = AF_INET;	/* use the Internet addr family */
	srv.sin_addr.s_addr = htonl(INADDR_ANY);

	//start binding//
	if(bind(sd, (struct sockaddr*) &srv, sizeof(srv)) < 0) 
	{
		perror("bind \n");
		exit(-1);
	}

	//start listening//
	if(listen(sd, 5) < 0) 
	{
		perror("listen \n");
		exit(-1);
	}

	//creating threadpool//
	threadpool* pool = create_threadpool(pool_size);
	if(pool == NULL)
	{
		perror("malloc \n");
		exit(-1);
	}
	//starts accepting//
	int newsd;	/* returned by accept() */
	for(i=0;i<max_req;i++)
	{
		newsd = accept(sd, NULL, NULL);//NULL because we are not working with real clients. 
		if(newsd < 0)
		{
			perror("accept \n");
			exit(1);
		}

		dispatch(pool, devideHandler, &newsd);
	}
	return 0;
	destroy_threadpool(pool);
}
		
int devideHandler(void* sd)
{
	char buf[MAX_READ_LENGTH];	/* used by read() */
	bzero(buf, MAX_READ_LENGTH);
	int nbytes;
	int newsd = *(int*)sd;
	int total_bytes = 0;
	char* first_line;

	//start reading//
	while(1) 
	{
		if((nbytes = read(newsd, buf+total_bytes, sizeof(buf)-total_bytes)) < 0)
		{
			perror("read \n");
			exit(-1);
		}	
		total_bytes += nbytes;

		first_line = strtok(buf,"\r\n");
		if(first_line)
			break;
	}
		lineSplitter(first_line, newsd);
		close(newsd);

}
		
void lineSplitter(char* c ,int sd)
{
	char sentence[strlen(c)];
	char* method;
	char* path;
	char* protocol;
	strcpy(sentence,c);

	int i=0;
	int counter=0;
	for(i;i<strlen(c)-1;i++)
	{
		if(c[i] == ' ')
			counter++;
	}
	if(counter != 2)
	{
		errHandler(ERROR400, path ,sd);
		return;
	}
	
		//split sentence into 3 vars
		method = strtok(sentence, " ");
		path = strtok(NULL, " ");
		protocol = strtok(NULL, " ");
	

	if(strcmp(method, "GET") < 0) //method is 'GET'
	{
		errHandler(ERROR501, path ,sd);
		return;
	}

	if(path[0] != '/') //path starts with '/'
	{
		errHandler(ERROR400, path ,sd);
		return;
	}

	if(strcmp(protocol, "HTTP/1.0") != 0 && strcmp(protocol, "HTTP/1.1") != 0) //protocol as demand
	{
		errHandler(ERROR400, path ,sd);
		return;
	}

	char new_path[MAX_READ_LENGTH];
	// bzero(new_path, MAX_READ_LENGTH); //INIT ARRAY
	memset(new_path, '\0', MAX_READ_LENGTH);
	strcat(new_path, ".");
	strcat(new_path, path);

	struct stat s;
	if(stat(new_path, &s) < 0)
	{
		if(errno < 0) //stat isnt working
		{
			perror("stat \n");
			errHandler(ERROR500, new_path , sd);
		}
		// else if(errno == ENOENT)
		else if( access(new_path,F_OK) == -1 ) //no such file
		{
			errHandler(ERROR404, new_path ,sd);
			return;
		}	
		else if(errno == ENOTDIR) //no such file
		{
			errHandler(ERROR404, new_path ,sd);
			return;
		}
		return;
	}

	//FILE \ DIRECTORY EXISTS FOR SURE:
	if(checkPermission(new_path) < 0)
	{
		errHandler(ERROR403,new_path ,sd);
		return;
	}


	if(S_ISDIR(s.st_mode)) //IS DIRECTORY
	{
		if(new_path[strlen(new_path)-1] == '/')
		{
			//CHECK IF index.html IS EXISTS.
			char file_path[MAX_READ_LENGTH];
			bzero(file_path, MAX_READ_LENGTH);
			strcpy(file_path, new_path);
			strcat(file_path, "index.html");

			if(stat(file_path, &s) < 0)
			{
				if(errno < 0) //stat isnt working
				{
					perror("stat \n");
					errHandler(ERROR500, new_path ,sd);
				}
				else if(errno == ENOENT) //no such index.html
				{
					errHandler(ERRORDIR, new_path ,sd);
				}
			}

			else //index.html is exists
			{
				if(s.st_mode & S_IROTH) //check file read permission
					errHandler(ERRORFIL, file_path ,sd);
				else
					errHandler(ERROR403,new_path ,sd);
			}
			return;

		}
		else
		{
			errHandler(ERROR302,new_path ,sd);
		}
	}
	else //IS FILE
	{
		if(S_ISREG(s.st_mode))
			errHandler(ERRORFIL, new_path ,sd);
		else
			errHandler(ERROR403,new_path ,sd);
	}
}



void errHandler(int err_num, char* path, int sd)
{

	//TIME:
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	//
	struct stat s;
	char err[MAX_READ_LENGTH];
	char err_det[MAX_ERR_LENGTH];
	char body[MAX_READ_LENGTH];
	char temp[MAX_ERR_LENGTH];
	int content_length;

	memset(err, '\0', MAX_READ_LENGTH);
	memset(err_det, '\0', MAX_ERR_LENGTH);
	memset(body, '\0', MAX_ERR_LENGTH);
	memset(temp, '\0', MAX_ERR_LENGTH);


	if(err_num != ERRORFIL && err_num != ERRORDIR)
	{
		if(err_num == ERROR302)
		{
			sprintf(err_det, "302 Found");
			content_length = 123;
			sprintf(body, "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n<BODY><H4>302 Found</H4>\nDirectories must end with a slash.\n</BODY></HTML>");
		}
		else if(err_num == ERROR400)
		{
			sprintf(err_det, "400 Bad Request");
			content_length = 113;
			sprintf(body, "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H4>400 Bad request</H4>\nBad Request.\n</BODY></HTML>");
		}
		else if(err_num == ERROR403)
		{
			sprintf(err_det, "403 Forbidden");
			content_length = 111;
			sprintf(body, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>");
		}

		else if(err_num == ERROR404)
		{
			sprintf(err_det, "404 Not Found");
			content_length = 112;
			sprintf(body, "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H4>404 Not Found</H4>\nFile not found.\n</BODY></HTML>");
		}

		else if(err_num == ERROR500)
		{
			sprintf(err_det, "500 Internal Server Error");
			content_length = 144;
			sprintf(body, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H4>500 Internal Server Error</H4>\nSome server side error.\n</BODY></HTML>");
		}

		else//(err_num == ERROR501)
		{
			sprintf(err_det, "501 Not supported");
			content_length = 129;
			sprintf(body, "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n<BODY><H4>501 Not supported</H4>\nMethod is not supported.\n</BODY></HTML>");
		}

		sprintf(err, "HTTP/1.0 ");
		strcat(err, err_det);
		strcat(err, "\r\n");
		strcat(err, "Server: webserver/1.0 \r\n");
		strcat(err, "Date: ");
		strcat(err, timebuf);
		strcat(err, "\r\n");
		if(err_num == ERROR302)
		{
			strcat(err, "Location: ");
			strcat(err,path);
			strcat(err,"/");
			strcat(err, "\r\n");
		}
		strcat(err, "Content-Type: text/html\r\n");
		strcat(err, "Content-Length: ");
		sprintf(temp,"%d", content_length);
		strcat(err, temp);
		strcat(err, "\r\n");
		strcat(err, "Connection: close \r\n");
		strcat(err, "\r\n");

	}
	else //FILE || DIR ERR -> CHECK.
	{
		struct stat s;

		sprintf(err, "HTTP/1.0 200 OK\r\n");
		strcat(err, "Server: webserver/1.0\r\n");
		strcat(err, "Date: ");
		strcat(err, timebuf);
		strcat(err, "\r\n");

		if(stat(path, &s) < 0) 
		{
			if(errno < 0) //stat isnt working
			{
				perror("stat \n");
				return;
			}
		}
		memset(timebuf, '\0', 128);
		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&s.st_mtime));
		

		strcat(err, "Content-Type: ");
		if(err_num == ERRORFIL) //IS FILE
		{
			char* type = get_mime_type(path);
			strcat(err, type);
		}
		else //IS DIR
			strcat(err, "text/html");

		strcat(err, "\r\n");
		strcat(err, "Content-Length: ");
		sprintf(temp,"%d", (int)s.st_size);
		strcat(err, temp);
		strcat(err, "\r\n");

		strcat(err, "Last-Modified: ");
		memset(timebuf, '\0', 128);
		// bzero(timebuf, 128);
		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&s.st_mtime));
		strcat(err, timebuf);
		strcat(err, "\r\n");
		strcat(err, "Connection: close \r\n");
		strcat(err, "\r\n");

		//ERR IS READY TO BE WRITTEN INTO SD!!
	}


	//BODY WRITE:
	if(err_num == ERRORDIR) //DIR CONTENT
	{

		char new_time[128];
		struct stat s;

		struct dirent* dirent;
		DIR* directory;
		
		directory = opendir(path);
    	if (directory == NULL)
    	{
    		perror("folder \n");
    		return;
    	}
    	strcat(body, "<HTML>");
    	strcat(body, "\n");
    	strcat(body, "<HEAD><TITLE>Index of ");
    	strcat(body, path+1);
    	strcat(body, "</TITLE></HEAD>\n");
    	strcat(body, "\n");

    	strcat(body, "<BODY>");
    	strcat(body, "\n");
    	strcat(body, "<H4>Index of ");
    	strcat(body, path+1);
    	strcat(body, "</H4>\n");
    	strcat(body, "\n");

    	strcat(body, "<table CELLSPACING=8>");
    	strcat(body, "\n");
    	strcat(body, "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\n");
    	strcat(body, "\n\n");

	    while ((dirent = readdir(directory)) )
	    {
	    	char tem[MAX_LINE_LENGTH];
	    	memset(temp, '\0', MAX_ERR_LENGTH);
	    	memset(tem, '\0', MAX_LINE_LENGTH);
	    	strcpy(tem , path);
	    	strcat(tem, dirent->d_name);

			if(stat(tem, &s) < 0) 
			{
				if(errno < 0) //stat isnt working
				{
					perror("stat \n");
					return;
				}
			}//stat is working

			if((strcmp(dirent->d_name, ".") != 0) && (strcmp(dirent->d_name, "..") != 0) )
			{
		        strcat(body, "<tr>");
		        strcat(body, "\n");
		        strcat(body, "<td><A HREF=\"");
		        strcat(body, dirent->d_name);
		        strcat(body, "\">");
		        strcat(body, dirent->d_name);
		        strcat(body, "</A></td><td>");

		        // strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&s.st_mtime));
		        strftime(new_time, sizeof(new_time), RFC1123FMT, gmtime(&s.st_mtime));
				strcat(body, new_time);
		        strcat(body, "</td>");
		        strcat(body, "\n");
		        strcat(body, "<td>");
		        if(!S_ISDIR(s.st_mode))
				{	
		        	int size = s.st_size;
					sprintf(temp,"%d", (int)s.st_size);
					strcat(body, temp);
		        	// strcat(body, " Bytes");
		        }

		        strcat(body, "</td>");
			    strcat(body, "\n");
			    strcat(body, "</tr>\n");
		        strcat(body, "\n");
		        
		        
	    	}
	    }
	    free(dirent);
	    closedir (directory);

	    strcat(body, "</table>\n");
	    strcat(body, "\n");
	    strcat(body, "<HR>\n");
	    strcat(body, "\n");
	    strcat(body, "<ADDRESS>webserver/1.0</ADDRESS>\n");
	    strcat(body, "\n");
	    strcat(body, "</BODY></HTML>");

	}

	if(err_num == ERRORFIL) //err_num == ERRORFIL --> write to sd
	{
		char buf[128];
		int fd;
		int s = 0;
		fd = open(path, O_RDONLY);

		while((s = read(fd, buf, 128)) >0 )
		{
			int d = write(sd, buf, s);
			if(d < 0)
			{
				perror("write \n");
				break;
			}

  			bzero(buf,128);
		}
		close(fd);
		sleep(1);
	}

	else //write all rest to sd
	{

		strcat(err, body); /////////////////////CHECK SPACES!!!
		//send into sd

		int tbytes = 0;
		int nbytes = strlen(err);
		int t_sd = 0;
		while(tbytes != nbytes)
		{
			t_sd = write(sd, err+tbytes, nbytes - tbytes);
			if(t_sd < 0)
			{
				perror("write \n");
				break;
			}
			tbytes += t_sd;
		}
	}
}


bool isNum(char* c) //check is the given row is 
{
	int i;
	for(i=0;i<strlen(c);i++)
	{
		if(!isdigit(c[i]))
			return false;
	}
	return true;
}


int checkPermission(char* given_path) //checks if the given path has permission (up to root directory)
{
	struct stat s;
	char str[80];
	strcpy(str, given_path);
	const char c[2] = "/";
	char *token;
	char build[MAX_READ_LENGTH];
	int path_flag = 0;

	/* get the first token */
	token = strtok(str, c);

	/* walk through other tokens */
	while( token != NULL ) 
	{
		strcat(build, token);
		if(path_flag == 0)
			path_flag = 1;
		else
		{
			if(stat(build, &s) < 0)
			{
				if(errno < 0) //stat isnt working
				{
					perror("stat \n");
					return -1;
				}
				else if (!(s.st_mode & S_IXOTH))//if the path is dir.
				{
					return -1;
				}
			}
		}
		strcat(build, c);
		token = strtok(NULL, c);
	}

	if(stat(given_path, &s) < 0)
	{
		if(errno < 0) //stat isnt working
		{
			perror("stat \n");
			return -1;
		}
		else if (!(s.st_mode & S_IROTH))//file does not have 'read' permission
		{
			return -1;
		}
	}
	free(token);
	return 0;
}

char* get_mime_type(char *name)
{
	char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	free(ext);
	return "";
}
