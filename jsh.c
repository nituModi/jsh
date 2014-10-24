/* This file is part of jsh.
 * 
 * jsh (jo-shell): A basic shell implementation
 * Copyright (C) 2014 Jo Van Bulck <jo.vanbulck@student.kuleuven.be>
 *
 * jsh is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * jsh is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with jsh.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "jsh-common.h"
#include "alias.h"
#include "jsh-parse.c"
#include <signal.h>
#include <setjmp.h>
#include <readline/readline.h>      // GNU readline: http://cnswww.cns.cwru.edu/php/chet/readline/rltop.html
#include <readline/history.h>

// ########## macro definitions ##########
#define VERSION                 "jsh 1.1.0"
#define HISTFILE                ".jsh_history"
#define RCFILE                  ".jshrc"
#define LOGIN_FILE              ".jsh_login"
#define DEFAULT_PROMPT          "%u@%h[%s]:%d$ "    // default init prompt string: "user@host[status]:pwd$ "
#define MAX_PROMPT_LENGTH       100                 // maximum length of the displayed prompt string
#define MAX_STATUS_LENGTH       10                  // the max number of msd of a status integer in the prompt string
// ########## function declarations ##########
void option(char*);
void things_todo_at_start(void);
void things_todo_at_exit(void);
char *getprompt(int);
char *readcmd(int status);
int is_built_in(comd*);
int parse_built_in(comd*, int);
void sig_int_handler(int);
void touch_config_files(void);
char** jsh_completion(const char*, int, int);
char *jsh_built_in_generator(const char*, int);
char *jsh_alias_generator(const char*, int);
char *jsh_options_generator(const char*, int);
char *jsh_widely_used_cmds_generator(const char*, int);
char *git_completion_generator(const char*, int);
char *debug_completion_generator(const char*, int);
char *git_branch_completion_generator(const char*, int);
char *apt_compl_generator(const char*, int);

// ########## global variables ##########
#ifdef NODEBUG
bool DEBUG = false;
#else
bool DEBUG = true;
#endif
bool COLOR = true;
bool LOAD_RC = true;
bool WAITING_FOR_CHILD = false; // whether or not the jsh parent process is currently (blocking) waiting for child termination
bool I_AM_FORK = false;
bool IS_INTERACTIVE;            // initialized in things_todo_at_start; (compiler's 'constant initializer' complaints)
int nb_hist_entries = 0;        // number of saved hist entries in this jsh session
sigjmp_buf ctrlc_buf;           // buf used for setjmp/longjmp when SIGINT received
char *user_prompt_string = DEFAULT_PROMPT;
int MAX_DIR_LENGTH = 25;        // the maximum length of an expanded pwd substring in the prompt string

/*
 * built_ins[] = array of built_in cmd names; should be sorted with 'qsort(built_ins, nb_built_ins, sizeof(char*), string_cmp);'
 * built_in enum = value corresponds to index in built_ins[]
 */
const char *built_ins[] = {"", "F", "T", "alias", "cd", "color", "debug",\
"exit", "history", "prompt", "shcat", "source", "unalias"};
#define nb_built_ins (sizeof(built_ins)/sizeof(built_ins[0]))
enum built_in {EMPTY, F, T, ALIAS, CD, CLR, DBG, EXIT, HIST, PROMPT, SHCAT, SRC, UNALIAS};
typedef enum built_in built_in;

// hackhackhack
const char *widely_used_cmds[] = {"git", "cat", "grep", "ls", "exit", "sudo", "kill", \
"killall", "links", "find", "clear", "chmod", "echo", "make", "poweroff", "reboot", \
"pacman", "aptitude", "apt-cache", "apt-get", "man", "nano", "vi", "gcc", "jsh", "zsh", \
"bash"};
#define nb_widely_used_cmds (sizeof(widely_used_cmds)/sizeof(widely_used_cmds[0]))

/*
 *
 * TODO gracious malloc etc
 * TODO snprintf ipv printf for format string vulnerabilities...
 * TODO gnu readline disable on non interactive input
 * TODO read history MAX_HIST_SIZE ofzo??
 */
int main(int argc, char **argv) {
    int i, status;
	// process options
	for (i = 1; i < argc && *argv[i] == '-'; i++)
		option(argv[i]+1);
    
    things_todo_at_start();
    
    signal(SIGINT, sig_int_handler);
    // after receiving SIGINT, program is continued on the next line
    if (sigsetjmp(ctrlc_buf, 1) == 0)
        status = 0;     // get here on direct call
    else
        status = -1;    // get here by SIGINT signal
    
    char *s;
    while ((s = readcmd(status)) != NULL)
        status = parseexpr(s);
        
    exit(EXIT_SUCCESS);
}

/*
 * option: process an option [-OPTIONCHARS] string
 */
void option(char *str) {
	void optionfull(char *s); // option helper function

	char *begin = str;
	while (*str != '\0')
		switch(*str++) {
			case '-':
				if (str-1 == begin)
					return optionfull(str);
				break; // else: ignore
			case 'h':
                printf("jo-shell: A basic UNIX shell implementation in C\n");
                printf("\nRecognized options:\n");
                printf("-h, --help\tdisplay this help message\n");
                printf("-d, --debug\tturn printing of debug messages on\n");
                printf("-n, --nodebug\tturn printing of debug messages on\n");
                printf("-c, --color\tturn coloring of jsh output messages on\n");
                printf("-o, --nocolor\tturn coloring of jsh output messages off\n");
                printf("-f, --norc\tdisable autoloading of the ~/%s file\n", RCFILE);
		        printf("-l, --license\tdisplay licence information\n");
    	        printf("-v, --version\tdisplay version information\n");
    	        printf("\nConfiguration files:\n");
    	        printf("~/%s\tfile containing commands to be executed at login\n", RCFILE);
    	        printf("~/%s\tfile containing the welcome message auto printed at login of an interactive session\n", LOGIN_FILE);
    	        printf("~/%s\tfile containing the command history auto loaded and saved at login/logout\n", HISTFILE);
    	        printf("\nReport bugs to: jo.vanbulck@student.kuleuven.be\n");
                printf("jsh homepage: <https://github.com/jovanbulck/jo-shell>\n");
    	        printf("This program is free software, and you are welcome to redistribute it under\n");
                printf("the condititions of the GNU General Public License. Try 'jsh --license' for more info.\n");
		        exit(EXIT_SUCCESS);
            case 'v': 
                printf("%s\n", VERSION);
                exit(EXIT_SUCCESS);
                break;
			case 'd':
				DEBUG = true;
				break;
			case 'n':
				DEBUG = false;
				break;
			case 'c':
				COLOR = true;
				break;
			case 'o':
				COLOR = false;
				break;
		    case 'f':
		        LOAD_RC = false;
		        break;
		    case 'l':
				printf("jo-shell: A basic UNIX shell implementation in C\n");
                printf("Copyright (C) 2014 Jo Van Bulck <jo.vanbulck@student.kuleuven.be>\n");
                printf("\nThis program is free software: you can redistribute it and/or modify\n");
                printf("it under the terms of the GNU General Public License as published by\n");
                printf("the Free Software Foundation, either version 3 of the License, or\n");
                printf("(at your option) any later version.\n");
                
                printf("\nThis program is distributed in the hope that it will be useful,\n");
                printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
                printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
                printf("GNU General Public License for more details.\n");
                
                printf("\nYou should have received a copy of the GNU General Public License\n");
                printf("along with this program.  If not, see <https://www.gnu.org/licenses/>.\n");
                exit(EXIT_SUCCESS);
		        break;
			default:
				printerr("Unrecognized option '-%c'", *(str-1));
				printerr("Try 'jsh --help' for a list of regognized options");
				exit(EXIT_FAILURE);
		}
}

/*
 * optionfull: helper function to process an option string, in full [--OPTION] notation
 */
void optionfull(char *str) {
	if (strcmp(str,"nodebug") == 0)
		option("n");
	else if (strcmp(str,"debug") == 0)
		option("d");
	else if (strcmp(str,"nocolor") == 0)
		option("o");
	else if (strcmp(str,"color") == 0)  //TODO mss color=auto ... enum
	    option("c");
	else if (strcmp(str,"help") == 0)
	    option("h");
	else if (strcmp(str,"version") == 0)
	    option("v");
    else if (strcmp(str,"norc") == 0)
        option("f");
    else if (strcmp(str,"license") == 0)
        option("l");
	else {
		printerr("Unrecoginized option '--%s'\n", str);
		printerr("Try 'jsh --help' for a list of regognized options\n");
		exit(EXIT_FAILURE);
	}
}

/*
 * things_todo_at_exit: do stuff that needs to be done at jsh login
 */
void things_todo_at_start(void) {
    // assert the built_ins array is properly sorted
    #if ASSERT
        assert(is_sorted(built_ins, nb_built_ins, sizeof(char*), string_cmp));
        printdebug("built_ins array is_sorted() assertion passed :-)");
    #endif
    
    // evaluate once at startup; to maintain for forked children in a pipeline
    IS_INTERACTIVE = (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO));

    touch_config_files();
    
    // load history file if any
    char * path = concat(3, gethome(), "/", HISTFILE);
    if (read_history(path) == 0) 
        printdebug("reading history from %s succeeded", path);
    else 
        printdebug("reading history from %s failed", path);
    free(path);
    
    // register the things_todo_at_exit function atexit
    atexit(things_todo_at_exit);
    
    // enable custom rl_autocompletion
    rl_attempted_completion_function = jsh_completion;
    
    // built-in aliases
    alias("~", gethome());
    
    // read ~/.jshrc if any
    if (LOAD_RC) {
        path = concat(3, gethome(), "/", RCFILE);
        parsefile(path, (void (*)(char*)) parseexpr, false);
        free(path);
    }
    
    // print welcome message (without debugging output)
    if (IS_INTERACTIVE) {
        bool temp = DEBUG;
        DEBUG = false;        
        char * path = concat(3, gethome(), "/", LOGIN_FILE);
        parsefile(path, (void (*)(char*)) &printf, false);
        free(path);
        DEBUG = temp;
        printdebug("debugging is on. Turn it off with 'debug off'.");
    }
}

/*
 * TODO all completion stuff in separate file jsh-autocomplete.c or so
 *
 * jsh_completion: a custom GNU readline completion function for jsh built_in shell commands.
 *  This function is called by readline; if the result is non-NULL, readline wont perform 
 *  the default file completion.
 * @arg: from the readline manual: "start and end are indices in rl_line_buffer defining 
 *  the boundaries of text, which is a character string."
 * @return: an array of strings with the possible completions or NULL of no completions.
 */
char** jsh_completion(const char *text, int start, int end) {
    char **matches = NULL;
    
    // true iff user entered 'cmd text<TAB>'
    #define USR_ENTERED(cmd) \
        (start >= strlen(cmd)+1 && (strncmp(rl_line_buffer + start - strlen(cmd) - 1, \
        cmd, strlen(cmd)) == 0)) // +1 for space
     
    if (is_valid_cmd(text, rl_line_buffer, start)) {
        // try custom autocompletion iff this is a valid comd context
        if (!(matches = rl_completion_matches(text, &jsh_built_in_generator)))
            if (!(matches = rl_completion_matches(text, &jsh_alias_generator)))
                matches = rl_completion_matches(text, &jsh_widely_used_cmds_generator);
        //TODO maybe a single generator instead of 3 separate onew
    }
    else {
        // else try custom autocompletion for specific commands
        if (USR_ENTERED("git")) {
            matches = rl_completion_matches(text, &git_completion_generator);
        }
        else if (USR_ENTERED("git checkout") || USR_ENTERED("git branch") || 
         USR_ENTERED("git merge")) {
            char *pwd_is_git = strclone("git rev-parse --git-dir > /dev/null 2> /dev/null");
            if (parseexpr(pwd_is_git) == EXIT_SUCCESS)
                matches = rl_completion_matches(text, &git_branch_completion_generator);
            free(pwd_is_git);
        }
        else if (USR_ENTERED("jsh")) {
            matches = rl_completion_matches(text, &jsh_options_generator);
        }
        else if (USR_ENTERED("debug")) {
            matches = rl_completion_matches(text, &debug_completion_generator);
        }
        else if (USR_ENTERED("apt")) {
            matches = rl_completion_matches(text, &apt_compl_generator);
        }
    }
        
    return matches;
}

/*
 * A sketleton to facilitate the implementation of the custom completion generators.
 */
#define COMPLETION_SKELETON(array, nb_elements) \
    do { \
        static int len; \
        static int index; \
        \
        if (!state) { \
            index = 0; \
            len = strlen(text); \
        } \
        \
        while (index < nb_elements) \
            if (strncmp(array[index], text, len) == 0) \
                return strclone(array[index++]); \
            else \
                index++; \
        \
        return NULL; \
        } \
    while (0)

/*
 * From the readline doc: "text is the partial word to be completed. state is zero the 
 * first time the function is called. The generator function returns (char *) NULL to 
 * inform rl_completion_matches() that there are no more possibilities left. Usually the 
 * generator function computes the list of possible completions when state is zero, and 
 * returns them one at a time on subsequent calls.
 * Each string the generator function returns as a match must be allocated with malloc(); 
 * Readline frees the strings when it has finished with them."
 */
char *jsh_built_in_generator(const char *text, int state) {
    COMPLETION_SKELETON(built_ins, nb_built_ins);
}

char *jsh_alias_generator(const char *text, int state) {
    static char **alias_keys = NULL;
    static int nb_alias_keys;
    if (!state) {
        if (alias_keys)
            free(alias_keys);
        alias_keys = get_all_alias_keys(&nb_alias_keys);
    }
    COMPLETION_SKELETON(alias_keys, nb_alias_keys); 
}

char *jsh_widely_used_cmds_generator(const char *text, int state) {
    COMPLETION_SKELETON(widely_used_cmds, nb_widely_used_cmds);
}

char *git_completion_generator(const char *text, int state) {
    static const char *git_cmds[] = {"add", "bisect", "branch", "checkout", "clone", \
    "commit", "diff", "fetch", "grep", "init", "log", "merge", "mv", "pull", "push", \
    "rebase", "reset", "rm", "show", "status", "tag"};
    static const int nb_elements = (sizeof(git_cmds)/sizeof(git_cmds[0]));
      
    COMPLETION_SKELETON(git_cmds, nb_elements);
}

char *debug_completion_generator(const char *text, int state) {
    static const char *options[] = {"on", "off"};
    static const int nb_options = (sizeof(options)/sizeof(options[0]));
    
    COMPLETION_SKELETON(options, nb_options);
}

char *jsh_options_generator(const char *text, int state) {
    static const char *options[] = {"--nodebug", "--debug", "--color", "--nocolor", \
    "--norc", "--license", "--version", "--help"}; //TODO dont hardcode here?
    static const int nb_options = (sizeof(options)/sizeof(options[0]));
    
    COMPLETION_SKELETON(options, nb_options);
}

char *apt_compl_generator(const char *text, int state) {
    static const char *options[] = { "list", "search", "show", "install", "remove", \
    "edit-sources", "update", "upgrade", "full-upgrade"};
    static const int nb_elements = (sizeof(options)/sizeof(options[0]));
    
    COMPLETION_SKELETON(options, nb_elements);
}

char *git_branch_completion_generator(const char *text, int state) {
    /*static const char *branches[] = {"some", "proof-of-concept", "git", "branches"};
    static const int nb_elements = (sizeof(branches)/sizeof(branches[0]));*/
    #define MAX_NB_BRANCHES 100
    #define MAX_BRANCH_NAME_LEN 100
    static char **branches = NULL;
    static int nb_elements = 0;
    
    if (!state) {
        /*if (branches)
            free(branches);*/ //TODO free also internal
        branches =  malloc((sizeof(char*) * MAX_NB_BRANCHES));
        FILE *fp = popen("git branch --no-color", "r");
        
        nb_elements = 0;
        int i;
        bool done = false;
        for (i= 0; i < MAX_NB_BRANCHES && !done; i++) {
            branches[i] =  malloc(MAX_BRANCH_NAME_LEN * sizeof(char));
            //if (!fgets(branches[i], MAX_BRANCH_NAME_LEN, fp)) break;
            char *cur = branches[i];
            int j = 0;
            int c;
            while (true) {
                int c = getc(fp);
                if (c == EOF) {
                    done = true;
                    cur[j] = '\0';
                    break;
                }
                if (c == '\n') {
                    cur[j] = '\0';
                    break;
                }
                else if (c != ' ' && c != '*') {
                    cur[j++] = c;
                }
            }
            
            nb_elements++;
        }
        
        pclose(fp);
    }
    
     static int len; \
        static int index; \
        \
        if (!state) { \
            index = 0; \
            len = strlen(text); \
        } \
        \
        while (index < nb_elements) \
            if (strncmp(branches[index], text, len) == 0) \
                return strclone(branches[index++]); \
            else \
                index++; \
        \
        return NULL; \
    
    
    //COMPLETION_SKELETON(branches, nb_elements);
    
    /*static int len;
    static int index;
    static int nb;
    
    if (!state) {
        // get all the branches in the cwd
        len = strlen(text);
        index = 0;
        nb = 0;
        if (parseexpr(strclone("git branch --no-color > my_help_file 2> /dev/null")) != EXIT_SUCCESS)
            return NULL;
        /*parsefile("my_help_file", (void (*)(char*)) printf, false);
        printdebug("now opening");
        FILE *file = fopen("my_help_file", "r");
        if (!file) {
            printdebug("oooh");
            return NULL;
        }
        printdebug("open");
        int c, i = 0;
        char *line;
        c = fgetc(file);
        while (c != EOF) {
        printdebug("in while with '%c' ", c);
            if (c == '\n') {
                printdebug("%dth new line", nb);
                line[i] = '\0';
                branches[nb++] = strclone(line);
                i = 0;
            }
            else {
                line[i++] = c;
            }
            c = fgetc(file);
        }
        
        fclose(file);
        printdebug("closed");*/
    //}
    
    /*while (index < nb)
            if (strncmp(branches[index], text, len) == 0)
                return strclone(branches[index++]);
            else 
                index++; 
         
        return NULL;*/
    
}


/*
 * create_config_files: looks for the jsh config files;
 *  if not found creates new empty ones (rw-rw-rw; will be combined with current umask)
 */
void touch_config_files(void) {
    #define CREATE_F(name) \
        path = concat(3, gethome(), "/", name); \
        fd = open(path, O_RDWR | O_CREAT, 0666); \
        if (fd >= 0) { \
            printdebug("opened file %s", name); \
            close(fd); \
        } \
        else \
            printdebug("couldn't open/create file '%s': %s", name, strerror(errno)); \
        free(path);
    
    char* path;
    int fd;
    CREATE_F(HISTFILE);
    CREATE_F(RCFILE);
    CREATE_F(LOGIN_FILE);
}

/*
 * things_todo_at_exit: do stuff that needs to be done at jsh logout (i.e. iff I_AM_FORK == 0)
 */
void things_todo_at_exit(void) {
    if (I_AM_FORK)
        return; //ignore exiting of child processes (e.g. failed fork execv)
    
    char * path = concat(3, gethome(), "/", HISTFILE);  //TODO check this uses malloc??? fail return status?
    if (append_history(nb_hist_entries, path) == 0)
        printdebug("appending %d history entries to %s succeeded", nb_hist_entries, path);
    else
        printdebug("appending %d history entries to %s failed", nb_hist_entries, path);
    free(path);
}

/*
 * getprompt: return a string representing the command prompt (as defined by the user_prompt_string)
 *  iff IS_INTERACTIVE, else the empty string is returned. The returned string is guaranteed to be less then 
 *  MAX_PROMPT_LENGTH; when a directory is expanded in the prompt string, it is 'smart' truncated to MAX_DIR_LENGTH.
 *  When a status integer is included in the prompt string, it is truncated to MAX_STATUS_LENGTH msd digits.
 */
char* getprompt(int status) {    
    if (!IS_INTERACTIVE)
        return "";    
    
    static char prompt[MAX_PROMPT_LENGTH] = "";  // static: hold between function calls (because return value)   
    prompt[0] = '\0';
    char *next;                         // points to the next substring to add to the prompt
    char buf[MAX_STATUS_LENGTH];        // used for char / int to string conversion 
    int i;
    for (i = 0; i < strlen(user_prompt_string); i++) {
        if (user_prompt_string[i] != '%') {
            snprintf(buf, MAX_STATUS_LENGTH, "%c", user_prompt_string[i]);
            next = buf;
        }
        else {
            i++; // potentially overread the '\0' char (harmless)
            switch (user_prompt_string[i]) {
                case 'u':
                    next = getenv("USER");
                    break;
                case 'h':
                    {   // to allow declarions inside a switch)
                    int hostlen = sysconf(_SC_HOST_NAME_MAX)+1; // Plus one for null terminate
                    char hostname[hostlen];
                    gethostname(hostname, hostlen);
                    hostname[hostlen-1] = '\0'; // Always null-terminate                     
                    next = hostname;
                    break;
                    }
                case 's':
                    snprintf(buf, MAX_STATUS_LENGTH, "%d", status);
                    next = buf;
                    break;
                case 'd':
                    {
                    // get the directory                
                    char *cwd = getcwd(NULL, 0); //TODO portability: this is GNU libc specific... + errchk
                    int cwdlen = strlen(cwd);
                    // get a ptr to the first '/' within the (truncated) directory string
                    char *ptr = strchr(cwd + ((MAX_DIR_LENGTH < cwdlen) ? cwdlen - MAX_DIR_LENGTH : 0), '/');
                    next = ((ptr != NULL) ? ptr : cwd + cwdlen - MAX_DIR_LENGTH);
                    break;
                    }
                case '%':
                    next = "%";
                    break;
                default:
                    printerr("skipping unrecognized prompt option '%c'", user_prompt_string[i]);
                    next = "";
                    break;
            }
        }
        // check length of string to concat; abort to avoid an overflow
        if ((strlen(prompt) + strlen(next)) > MAX_PROMPT_LENGTH) {
            printdebug("Prompt expansion too long: not concatting '%s'. Now returning...", next);
            return prompt;
        }
        strcat(prompt, next);
    }
    return prompt;
}

/*
 * readcmd: read the next inputline from stdin, add it to the history and resolve all aliases.
 *  returns the resolved inputline or NULL if EOF on a blank line
 * TODO remove status arg?
 */
char *readcmd(int status) {
    /** static variables that remain between function calls **/
    static char *buf = (char*) NULL;            // inputbuffer for readline()
    
    // display prompt and read full line in buf
    if (buf) {
        printdebug("readcmd: freeing memory for: '%s'", buf);
        free(buf); // If the buffer has already been allocated, return the memory to the free pool.
        buf = NULL;
    }
    buf = readline(getprompt(status));  //TODO fall back to getline() when non-interactive...
    printdebug("You entered: '%s'", buf);

    // If the line has any text in it: expand history, save it to history and resolve aliases
    //  (readline returns NULL iff EOF on a blank line)
    if (buf != NULL && *buf) {
        // do history expansion
        char *expansion = '\0';
        int hist_rv;
        if ((hist_rv = history_expand(buf, &expansion)) != -1) {
            if (hist_rv == 1)
                printf("%s\n", expansion);  // bash-style print the expanded command string iff changed
            free(buf);                      // free unexpanded version
            buf = expansion;                // point to expanded cmd
        }
        else {
            printerr("readcmd: history expansion failed for '%s': '%s'", buf, expansion);
            free(expansion);
        }
        add_history(buf);
        nb_hist_entries++;
        char *ret = resolvealiases(buf);
        free(buf); // free unresolved version
        buf = ret; // point to resolved cmd
    }
    return buf;
}

/* 
 * is_built_in: returns -1 iff the provided comd isn't recognized as a built_in shell command,
 *  else returns the (positive) index in the built_in[] array.
 */
int is_built_in(comd* comd) {
   const char **rv = bsearch(comd->cmd, built_ins, nb_built_ins, sizeof(char*), string_cmp); //TODO strncmp?? buf overflow
   return ((rv == NULL)? -1: rv - built_ins);
}

/* 
 * parse_built_in: parses the provided *comd as a built_in shell command iff is_built_in(comd) != -1
 *  the provided int is an index in the built_in[] array, as provided by is_built_in(comd)
 *  returns exit status (EXIT_FAILURE || EXIT_SUCCESS) of executed built_in
 */
int parse_built_in(comd *comd, int index) {
    #if ASSERT
        assert(strcmp(*comd->cmd, built_ins[index]) == 0);
    #endif

    //######## common macro definitions ###########
    #define CHK_ARGC(cmd, argc) \
        if (comd->length != argc+1) { \
            printerr("%s: wrong number of arguments\t(expected = %d)", cmd, argc); \
            return EXIT_FAILURE; \
        }
    #define TOGGLE_VAR(name, var, arg) \
        CHK_ARGC(name, 1); \
        if (strcmp(arg, "on") == 0) { \
            printinfo("%s mode on", name); \
            var = 1; \
            return EXIT_SUCCESS; \
        } \
        else if (strcmp(arg, "off") == 0) { \
            printinfo("%s mode off", name); \
            var = 0; \
            return EXIT_SUCCESS; \
        } \
        else { \
            printerr("%s: expects argument 'on' || 'off'", name); \
            return EXIT_FAILURE; \
        }
    //######## built-in cmds switch ###########
    built_in b = (built_in) index;
    switch(b) {
        case EMPTY:
            printdebug("built-in: ignoring empty input");
            return EXIT_SUCCESS;
            break;
        case F:
            return EXIT_FAILURE; // FALSE
            break;
        case T:
            return EXIT_SUCCESS; // TRUE
            break;
        case ALIAS:
            if (comd->length == 1) {
                printaliases();
                return EXIT_SUCCESS;
            }
            else {
                CHK_ARGC("alias", 2);
                return alias(comd->cmd[1], comd->cmd[2]);
            }
            break;
        case CD:
            { // to allow declarions inside a switch)
            char *dir;
            if (comd->length == 1)
                dir = getenv("HOME");
            else {
                CHK_ARGC("cd",1);
                dir = comd->cmd[1];
            }
            CHK_ERR(chdir(dir), "cd");
            setenv("PWD", dir, 1);
            return EXIT_SUCCESS;
            break;
            }
        case CLR:
            TOGGLE_VAR("color", COLOR, comd->cmd[1]);   //TODO global var
            break;
        case DBG:
            TOGGLE_VAR("debug", DEBUG, comd->cmd[1]);
            break;
        case EXIT:
            exit(EXIT_SUCCESS);
            break;
        case HIST:
            CHK_ARGC("history", 0);
            HIST_ENTRY **hlist = history_list();
            int i;
            if (hlist)
                for (i = 0; hlist[i]; i++)
                    printf ("%s\n", hlist[i]->line);
            return EXIT_SUCCESS;
            break;
        case PROMPT:
            {
            // check for the optional dir length argument
            if (comd->length == 3) {
                MAX_DIR_LENGTH = abs(atoi(comd->cmd[2]));    // will return 0 on non-integer
                printdebug("setting MAX_DIR_LENGTH to %d", MAX_DIR_LENGTH);
            }
            else
                CHK_ARGC("prompt", 1);
            
            static bool prompt_init = true;
            if (!prompt_init)
                free(user_prompt_string);
            else
                prompt_init = false;
            printdebug("setting user_prompt_string to '%s'", comd->cmd[1]);
            user_prompt_string = malloc(strlen(comd->cmd[1])+1);
            strcpy(user_prompt_string, comd->cmd[1]);
            return EXIT_SUCCESS;
            break;
            }
        case SHCAT:
            parsestream(stdin, "stdin", (void (*)(char*)) printf);  // built_in cat; mainly for testing purposes (redirecting stdin)
            return EXIT_SUCCESS;
            break;
        case UNALIAS:
            CHK_ARGC("unalias", 1);
            return unalias(comd->cmd[1]);
            break;
		case SRC:
			CHK_ARGC("source", 1);
			parsefile(comd->cmd[1], (void (*)(char*)) parseexpr, true); // errormsg if file not found
			return EXIT_SUCCESS;
			break;
        default:
            printerr("parse_built_in: unrecognized built_in command: '%s' with index %d", *comd->cmd, index);
			exit(EXIT_FAILURE);
    }
}

/*
 * sig_int_handler: this function is called when the user enters ^C and 
 * tells GNU readline to diplay a prompt on a newline
 */
void sig_int_handler(int signo) {
    // if ^C entered in child process --> also sent to parent (jsh) process
    // --> only clear the prompt when not waiting for an executing child (allow the waitpid to return)
    if (!WAITING_FOR_CHILD) {
        rl_crlf();                  // set cursor to newline
        siglongjmp(ctrlc_buf, 1);   // jump back to main loop
    }
}
