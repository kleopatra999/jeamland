/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	file.c
 * Function:	Functions associated with file / directory structure access
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <netinet/in.h>

#if defined(sgi)
/* Grumble... */
#include <sys/dir.h>
typedef struct direct Dirent;
#else
#include <dirent.h>
typedef struct dirent Dirent;
#endif

#include "jeamland.h"

extern int sysflags;
extern time_t current_time;

char *
read_file(char *fname)
{
	int fd, err;
	struct stat st;
	char *buf;

	if ((fd = open(fname, O_RDONLY)) == -1)
	{
		if (errno != ENOENT)
			log_perror("read_file open");
		return (char *)NULL;
	}
	if (fstat(fd, &st) == -1)
		fatal("Could not stat open file.");
	buf = (char *)xalloc((size_t)st.st_size + 1, "read_file");
	if ((err = read(fd, buf, (int)st.st_size)) != st.st_size)
	{
		if (err == -1)
			log_perror("read_file read");
		xfree(buf);
		close(fd);
		return (char *)NULL;
	}
	buf[st.st_size] = '\0';
	close(fd);
	return buf;
}

int
write_file(char *fname, char *buf)
{
	int fd, err;

	if ((fd = open(fname, O_WRONLY | O_TRUNC | O_CREAT, 0600)) == -1)
	{
		log_perror("write_file open");
		return 0;
	}
	if ((err = write(fd, buf, strlen(buf))) != strlen(buf))
	{
		if (err == -1)
			log_perror("write_file write");
		close(fd);
		return 0;
	}
	close(fd);
	return 1;
}

int
send_email(char *rlname, char *to, char *cc, char *subj, int del,
    char *fmt, ...)
{
	static int id = 0;
	FILE *fp;
	char buf[MAXPATHLEN + 1];
        va_list argp;

#ifdef DEBUG
	if (to == (char *)NULL || subj == (char *)NULL)
		fatal("emaild_file with NULL arguments.");
#endif

	sprintf(buf,F_EMAILD "%d", ++id);

	if ((fp = fopen(buf, "w")) == (FILE *)NULL)
		return -1;

	/* Mail headers.. */
	fprintf(fp, "To: %s\n", to);
	if (cc != (char *)NULL)
		fprintf(fp, "Cc: %s\n", cc);
	fprintf(fp, "Subject: %s\n", subj);
	/* Our own personal header.. */
	fprintf(fp, "X-Mailer: JeamLand Mail %s\n", MAIL_VERSION);
	fprintf(fp, "\n");

        va_start(argp, fmt);
        vfprintf(fp, fmt, argp);
        va_end(argp);
	fclose(fp);

	send_erq(ERQ_EMAIL, "%s;%d;%d;\n", rlname, id, del);

	return id;
}

int
send_email_file(struct user *p, char *file)
{
	char fname[MAXPATHLEN + 1];
	char *fcont;
	int retval;

	sprintf(fname, "%s%s", F_SENDME, file);

	if ((fcont = read_file(fname)) == (char *)NULL)
		return -1;

	sprintf(fname, "JeamLand sendme: %s", file);

	retval = send_email(p->rlname, p->email, (char *)NULL, fname, 1,
	    "%s", fcont);

	xfree(fcont);
	return retval;
}

int
count_files(char *path)
{
	DIR *dirp;
	Dirent *de;
	int count;

    	if ((dirp = opendir(path)) == (DIR *)NULL)
	{
		/*log_perror("opendir");*/
		return -1;
	}

    	for (count = 0, de = readdir(dirp); de != (Dirent *)NULL;
	    de = readdir(dirp))
    	{
		/* We don't want '.' or '..' */
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		count++;
    	}
	closedir(dirp);
	return count;
}

/*
 * If extra is non-zero, then the returned array will be of the form:
 * ({ "file name", file_time, file_size, "file name 2", ... })
 * else it will just be:
 * ({ "file name", "file name 2", ... })
 */
struct vector *
get_dir(char *path, int extra)
{
    	struct vector *v;
    	int i, count;
    	DIR *dirp;
	struct stat st;
	char *newpath;
	char *fname;
	Dirent *de;

	if (path == (char *)NULL)
		return (struct vector *)NULL;

    	if ((dirp = opendir(path)) == (DIR *)NULL)
	{
		/*log_perror("opendir");*/
		return (struct vector *)NULL;
	}

	newpath = (char *)xalloc(strlen(path) + 3 + MAXPATHLEN,
	    "get_dir newpath");
	strcpy(newpath, path);
	fname = newpath + strlen(path);
	if (fname[-1] != '/')
		*fname++ = '/';

	/* Loop through the files to determine array size
	 * quicker than incrementing the arrays size for each file.. */
    	for (count = 0, de = readdir(dirp); de != (Dirent *)NULL;
	    de = readdir(dirp))
    	{
		/* We don't want '.' or '..' */
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		count++;
    	}

	/* Simple case */
	if (!count)
	{
		xfree(newpath);
		closedir(dirp);
		return (struct vector *)NULL;
	}

	/* Make the array */
	if (extra)
		count *= 3;
	v = allocate_vector(count, T_STRING, "get_dir vector");

	/* Now put the files into the array.. */
    	rewinddir(dirp);
    	for (i = 0, de = readdir(dirp); de != (Dirent *)NULL;
	    de = readdir(dirp))
    	{
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		/* +2 to make room for optional '/' */
		v->items[i].u.string = (char *)xalloc(strlen(de->d_name) + 2,
		    "get_dir elem");
		strcpy(v->items[i].u.string, de->d_name);
		strcpy(fname, de->d_name);
		if (stat(newpath, &st) == -1)
			/*log_perror("get_dir stat");*/
			;
		else
		{
			if (S_IFDIR & st.st_mode)
				strcat(v->items[i].u.string, "/");
			if (extra)
			{
				i++;
				TO_NUMBER(v->items[i]);
#ifdef TIMEZONE_ADJUSTMENT
				v->items[i].u.number = st.st_mtime + 3600 *
				    TIMEZONE_ADJUSTMENT;
#else
				v->items[i].u.number = st.st_mtime;
#endif
				i++;
				TO_NUMBER(v->items[i]);
				v->items[i].u.number = st.st_size;
			}
		}
		/* It is just possible that a new file has appeared.. */
		if (i++ >= count)
			break;
    	}
	xfree(newpath);
    	closedir(dirp);
    	return v;
}

/* Reproduces some log_file code, but... 
 * could hold file open but that would waste an fd. */
void
audit(char *fmt, ...)
{
	FILE *fp;
        va_list argp;
	extern time_t start_time;
	static char path[MAXPATHLEN + 1] = "";

	if (!strlen(path))
		sprintf(path, F_AUDIT "%ld.%d", (long)start_time,
		    (int)getpid());

	if ((fp = fopen(path, "a")) == (FILE *)NULL)
	{
		log_file("error", "Error writing to audit file.\n");
		return;
	}
        va_start(argp, fmt);
        fprintf(fp, "%s: ", nctime(&current_time));
        vfprintf(fp, fmt, argp);
	fprintf(fp, "\n");
        va_end(argp);
	fclose(fp);
}

void
log_file(char *file, char *fmt, ...)
{
        static char path[MAXPATHLEN + 1];
        FILE *stream = stdout;
        static char buf[MAXPATHLEN + 1];
        struct stat st;
        va_list argp;

        sprintf(path, "log/%s", file);

        if (sysflags & SYS_LOG)
        {
                if ((stream = fopen(path, "a")) == (FILE *)NULL)
                        return;
#ifdef CYCLIC_LOGS
                if (fstat(fileno(stream), &st) == -1)
                        fatal("Could not stat an open file");
                if (st.st_size > CYCLIC_LOGS)
                {
                        fclose(stream);
                        strcpy(buf, path);
                        strcat(buf, "~");
                        rename(path, buf);
                        if ((stream = fopen(path, "a")) == (FILE *)NULL)
                                return;
                }
#endif
        }
        else
                fprintf(stream, "%s: ", file);
        fprintf(stream, "%s: ", nctime(&current_time));
        va_start(argp, fmt);
        vfprintf(stream, fmt, argp);
        va_end(argp);
        fprintf(stream, "\n");
        fflush(stream);
        if (stream != stdout)
                fclose(stream);
	if (!strcmp(file, "error"))
	{
		/* Let the Administrators know. */
		char buf[BUFFER_SIZE];

		va_start(argp, fmt);
		vsprintf(buf, fmt, argp);
		va_end(argp);

		notify_level(L_CONSUL, "[ !ERROR! %s ]\n", buf);
	}
}

int
file_time(char *file)
{
	struct stat st;

	if (stat(file, &st) == -1)
		return -1;
#ifdef TIMEZONE_ADJUSTMENT
	return st.st_mtime + 3600 * TIMEZONE_ADJUSTMENT;
#else
	return st.st_mtime;
#endif
}

int
file_size(char *file)
{
	struct stat st;

	if (stat(file, &st) == -1)
		return -1;
	if (S_IFDIR & st.st_mode)
		return -2;
	return st.st_size;
}

int
exist_line(char *fname, char *text)
{
	char buf[BUFFER_SIZE];
	FILE *fp;

	if ((fp = fopen(fname, "r")) == (FILE *)NULL)
		return 0;

	while (fscanf(fp, "%s", buf) != EOF)
		if (!strcmp(buf, text))
		{
			fclose(fp);
			return 1;
		}
	fclose(fp);
	return 0;
}

void
add_line(char *fname, char *fmt, ...)
{
	FILE *fp;
	va_list argp;

	if ((fp = fopen(fname, "a")) == (FILE *)NULL)
	{
		log_file("error", "Cannot open %s for append (add_line).",
		    fname);
		return;
	}
	va_start(argp, fmt);
	vfprintf(fp, fmt, argp);
	va_end(argp);
	fclose(fp);
}

void
remove_line(char *fname, char *line)
{
	FILE *fp, *new;
	char buf[BUFFER_SIZE];

	if ((fp = fopen(fname, "r")) == (FILE *)NULL)
		return;

	if ((new = fopen("tmpfile", "w")) == (FILE *)NULL)
	{
		log_file("error", "Cannot open temporary file. (remove_line)");
		fclose(fp);
		return;
	}

	while (fscanf(fp, "%s", buf) != EOF)
		if (!strcmp(buf, line))
			continue;
		else
			fprintf(new, "%s\n", buf);

	fclose(fp);
	fclose(new);
	if (rename("tmpfile", fname) == -1)
	{
		log_perror("rename");
		log_file("error", "Rename failed in remove_line().");
	}
}

