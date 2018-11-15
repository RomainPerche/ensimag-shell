/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */


/*struct cellule contenant les process en background*/
struct Cellule {
		char* nom;
		pid_t pid;
		struct Cellule *next;
};


void print_jobs(struct Cellule *sentinelle) {
		if (sentinelle == NULL) {
				printf("No backgound process\n");
		}
		else {
				struct Cellule *current = sentinelle;
				int i = 1;
				while (current != NULL) {
						printf("Process %d : %s \n", i, current->nom);
						printf("PID : %d \n", current->pid);
						int status;
						if (!waitpid(current->pid, &status, WNOHANG)) {
								printf("Il se passe des choses.\n");
						}
						printf("\n");
						i++;
						current = current->next;
				}
		}
}


#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	printf("Not implemented yet: can not execute %s\n", line);

	/* Remove this line when using parsecmd as it will free it */
	free(line);

	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}


int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	/*on crée une sentinelle pour que la liste ne soit pas vide à l'initialisation*/
	struct Cellule *sentinelle = NULL;
	struct Cellule *current = NULL;
	int child_nb = 0;

	while (1) {
		struct cmdline *l;
		char *line=0;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {

			terminate(0);
		}



		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");


		char **cmd = l->seq[0];

/*
		if (l->out != NULL) {
				int tuyau[2];
				int fd = open(l->out, O_WRONLY);
				if ((res1=fork()) == 0) {
					dup2(tuyau[0], fd);
					close(tuyau[1]);
					close(tuyau[0]);
					execvp(cmd[0], cmd);
			}
		}
*/

		if (l->seq[1] != 0) {
				//Pipe à faire.
				char **cmd2 = l->seq[1];
				int res0;
				int res1;
				int tuyau[2];
				//Processus pour le pipe.
				if ((res0=fork()) == 0) {
						pipe(tuyau);
						//Processus pour l'exécution des commandes.
						if ((res1=fork()) == 0) {
							dup2(tuyau[0], 0);
							close(tuyau[1]);
							close(tuyau[0]);
							execvp(cmd2[0], cmd2);
						}
						dup2(tuyau[1], 1);
						close(tuyau[0]);
						close(tuyau[1]);
						execvp(cmd[0], cmd);
				}
		}
		else {
				pid_t pid;
				switch( pid = fork() ) {
						case 0:
								if (strncmp(cmd[0], "jobs", 20) == 0) {
										print_jobs(sentinelle);
								}
								execvp(cmd[0], cmd);
								break;
						case -1:
								perror("fork:");
								break;
						default:
						{
								if (l->bg == 0) {
										int status;
										waitpid(pid, &status, 0);
								}
								else {
										//Background process.
										child_nb++;
										//Counts number of background processes.
										if (sentinelle == NULL) {
												sentinelle = malloc(sizeof(struct Cellule));
												strcpy(sentinelle->nom, cmd[0]);
												sentinelle->pid = pid;
												sentinelle->next = NULL;
												current = malloc(sizeof(struct Cellule));
												current = sentinelle;
										}
										else {
												struct Cellule *new = malloc(sizeof(struct Cellule));
												strcpy(new->nom, cmd[0]);
												new->pid = pid;
												new->next = NULL;
												current->next = new;
												current = new;
										}
								}
								break;
						}
				}
		}
	}
}
