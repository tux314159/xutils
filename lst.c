#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"

int strcmp_vp(const void *s1, const void *s2)
{
    return strcmp(*(const char **)s1, *(const char **)s2);
}

int main(int argc, char **argv)
{
	/* INIT */

    if (argc == 1)
        die("Usage: %s <dirname>\n", argv[0]);

    DIR *dir = opendir(argv[1]);
    dienz(dir, "ERROR opening dir %s.\n", argv[1]);

    // Set behavioural flags
    bool show_all = false;
    int tbl_sep = 2;
    size_t max_cols;

	struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	max_cols = ws.ws_col;

	/* FS IO */

    // Get dir entries
    struct dirent *ent;
    // First time to find number of entries
    size_t n_ents = 0;
    while ((ent = readdir(dir))) {
        if (!show_all && ent->d_name[0] == '.')
            continue;
        n_ents++;
    }
    char **ents = malloc_s(n_ents * sizeof(*ents));
    for (size_t i = 0; i < n_ents; i++)
        ents[i] = calloc_s(256, sizeof(*ents[i]));

    // Second time to push into array
    rewinddir(dir);
    {
        size_t i = 0;
        while ((ent = readdir(dir))) {
            if (!show_all && ent->d_name[0] == '.')
                continue;
            strcpy(ents[i++], ent->d_name);
        }
    }
    dienz(!closedir(dir), "ERROR closing dir %s.\n", argv[1]);

    // Sort entries in alphabetical order
    qsort(ents, n_ents, sizeof(*ents), strcmp_vp);

	/* FORMATTING ALGO */

    size_t *entname_lens = malloc_s(n_ents * sizeof(*entname_lens));
    for (size_t i = 0; i < n_ents; i++)
		entname_lens[i] = strlen(ents[i]);

	// Brute-force optimal rows and cols

	ssize_t opt_c = 1;
	ssize_t opt_r = n_ents;
	size_t *opt_c_conf = calloc_s(n_ents, sizeof(*opt_c_conf));
	// We want to try with c cells per row
	for (size_t c = 1; c <= n_ents; c++) {
		int r = n_ents / c + (n_ents % c != 0);
		size_t tablen_c = 0;
		for (size_t i = 0; i < c; i++) {
			size_t mx = 0;
			for (size_t j = i * r; j < min(n_ents, (i + 1) * r); j++)
				mx = max(mx, entname_lens[j]);
			tablen_c += mx;
		}

		if (tablen_c + (c - 1) * tbl_sep <= max_cols && r < opt_r) {
			// We want smallest c minimising number of rows
			opt_c = c;
			opt_r = r;
		}
	}

	// Find the desired column widths
	for (size_t i = 0; i < opt_c; i++)
		for (size_t j = i * opt_r; j < min(n_ents, (i + 1) * opt_r); j++)
			opt_c_conf[i] = max(opt_c_conf[i], entname_lens[j]);

	/* OUTPUT */

	// Create table for printing
	char ***tbl = calloc_s(opt_r, sizeof(*tbl));
	for (size_t i = 0; i < opt_r; i++)
		tbl[i] = calloc(opt_c, sizeof(*tbl[i]));

	{
		int k = 0;
		for (size_t i = 0; i < opt_c; i++)
			for (size_t j = 0; j < opt_r && k < n_ents; j++)
				tbl[j][i] = ents[k++];
	}

	// Print the table
	for (int i = 0; i < opt_r; i++) {
		for (int j = 0; j < opt_c; j++) {
			if (tbl[i][j]) {
				struct stat statbuf;
				char *fullpath = malloc(strlen(argv[1]) + strlen(tbl[i][j]) + 1);
				size_t argv1len = strlen(argv[1]);
				strcpy(fullpath, argv[1]);
				fullpath[argv1len] = '/';
				strcpy(fullpath + argv1len + 1, tbl[i][j]);
				if (lstat(fullpath, &statbuf) == -1)
					;
				else if (S_ISDIR(statbuf.st_mode))
					printf("%s", TERM_BOLD TERM_BLUE);
				else if (S_ISLNK(statbuf.st_mode))
					printf("%s", TERM_BOLD TERM_CYAN);
				free(fullpath);
					
				printf("%s%s", tbl[i][j], TERM_COLRESET);
			} if (j == opt_c - 1) {
				printf("\n");
			} else {
				for (int k = 0; k < opt_c_conf[j] - strlen(tbl[i][j]) + tbl_sep; k++)
					printf(" ");
			}
		}
	}

	for (size_t i = 0; i < opt_r; i++)
		free(tbl[i]);
	free(tbl);

	/* RELEASE REMAINING RESOURCES */

    for (int i = 0; i < n_ents; i++)
        free(ents[i]);
    free(ents);
	free(entname_lens);
	free(opt_c_conf);
}
