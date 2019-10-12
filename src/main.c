/***************************************************************************//**

  @file         main.c

  @author       Stephen Brennan

  @date         Thursday,  8 January 2015

  @brief        LSH (Libstephen SHell)

*******************************************************************************/

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef FEATURE_CONNECT_TO_SERVER
#include <arpa/inet.h>
#include <sys/time.h>
#endif

//flush stderr and stdout
void flush() {
  fflush(stdout);
  fflush(stderr);
}

/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit"
};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit
};

int lsh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/

/**
   @brief Bultin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int lsh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }
  return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args.  Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_help(char **args)
{
  int i;
  printf("Stephen Brennan's LSH\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
   @return Always returns 0, to terminate execution.
 */
int lsh_exit(char **args)
{
  return 0;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */
int lsh_launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("lsh");
  } else {
    // Parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return lsh_launch(args);
}

#define LSH_RL_BUFSIZE 1024
/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *lsh_read_line(void)
{
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read a character
    c = getchar();

    if (c == EOF) {
      exit(EXIT_SUCCESS);
    } else if (c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
/**
   @brief Split a line into tokens (very naively).
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
		free(tokens_backup);
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

/**
   @brief Loop getting input and executing it.
 */
void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    flush();
    line = lsh_read_line();
    args = lsh_split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

/**
   @brief print the program usage
   @param prog_name Program name
 */
void show_usage(char *prog_name)
{
  printf("Usage: %s [OPTION]\n\n", prog_name);
  printf("Available options:\n");
  printf("-h\t\tShow this help message\n");
#ifdef FEATURE_CONNECT_TO_SERVER
  printf("-c ip port\tConnects to an ipv4 server\n");
  printf("-st\t\tSet tcp send timeout (in seconds)\n");
#endif
  printf("\n");
}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  int currunt_arg = 1;
#ifdef FEATURE_CONNECT_TO_SERVER
  char *ip;
  unsigned short port;
  struct sockaddr_in server_addr = { 0 };
  int connection_fd;
  struct timeval connect_timeout;

  unsigned int send_timeout = 8;
#endif

  // Parsre the arguments
  if (argc > 1) {
    while (currunt_arg < argc) {
      if (strcmp(argv[currunt_arg], "-h") == 0 || strcmp(argv[currunt_arg], "--help") == 0) {
        show_usage(argv[0]);
        return EXIT_FAILURE;
      }
#ifdef FEATURE_CONNECT_TO_SERVER
      else if (strcmp(argv[currunt_arg], "-st") == 0) {
        if (currunt_arg + 2 > argc) {
          printf("missing timeout: %s\n\n", argv[currunt_arg]);
          show_usage(argv[0]);
          return EXIT_FAILURE;
        }
        send_timeout = strtoul(argv[currunt_arg+1], NULL, 10);
        printf("Set send timeout to %hu\n", send_timeout);
        currunt_arg++;
      }
      else if (strcmp(argv[currunt_arg], "-c") == 0) {
        //Check if there is enough arguments

        if (currunt_arg + 3 > argc) {
          printf("missing ip or port: %s\n\n", argv[currunt_arg]);
          show_usage(argv[0]);
          return EXIT_FAILURE;
        }

        ip = argv[currunt_arg+1];
        port = (unsigned) atoi(argv[currunt_arg+2]);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &server_addr.sin_addr) != 1) {
          printf("Invalid ip specified!\n");
          return EXIT_FAILURE;
        }

        connection_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (connection_fd == -1) {
          printf("Cannot create a socket!\n");
          return EXIT_FAILURE;
        }

        connect_timeout.tv_sec = send_timeout;
        connect_timeout.tv_usec = 0;

        setsockopt(connection_fd, SOL_SOCKET, SO_SNDTIMEO, &connect_timeout, sizeof(connect_timeout));

        if (connect(connection_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
          printf("Cannot connect to the server!\n");
          return EXIT_FAILURE;
        }

        printf("Connected to %s:%hu\n", ip, port);

        for(int i=0; i<=2; i++)
          dup2(connection_fd, i);

        currunt_arg += 2;

      }
#endif
      else {
        printf("Invalid arg: %s\n\n", argv[currunt_arg]);
        show_usage(argv[0]);
        return EXIT_FAILURE;
      }

      currunt_arg++;
    }
  }
  // Load config files, if any.

  // Run command loop.
  lsh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}

