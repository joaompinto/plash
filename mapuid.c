#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_USERLEN 32
#define MAX_USERLEN_STR "32"
// #define MAX_USERNAME_LENGTH_STRING #MAX_USERNAME_LENGTH

typedef struct find_subid_struct {
        int found;
        unsigned long start;
        unsigned long count;
} find_subid_t;

find_subid_t find_subid(unsigned long id, char *id_name, const char *file){

    char label[MAX_USERLEN];
    unsigned long ulong_label;
    find_subid_t retval;
    retval.found = 0;

    FILE *fd = fopen(file, "r");
    if (NULL == fd) {
            perror("could not open subuid/subgid file");
            exit(EXIT_FAILURE);
    }
    for (;;){
            if (3 != fscanf(fd, "%"MAX_USERLEN_STR"[^:\n]:%lu:%lu\n",
                    label, &retval.start, &retval.count))
                    break;

            if (strcmp(id_name, label) == 0){
                retval.found = 1;
                break;

            } else {
                errno = 0;
                ulong_label = strtoul(label, NULL, 10);
                if (errno == 0 && ulong_label == id) {
                        retval.found = 1;
                        break;
                };
            }
    }
    errno = 0;
    return retval;


    
}
        
int main(int argc, char* argv[]) {
        
        struct passwd *pwent = getpwuid(getuid());
        if (NULL == pwent) {perror("uid not in passwd"); exit(1);}
        find_subid_t uidmap = find_subid(pwent->pw_uid, pwent->pw_name, "/etc/subuid");

        struct group *grent = getgrgid(getgid());
        if (NULL == grent) {perror("gid not in db"); exit(1);}
        find_subid_t gidmap = find_subid(grent->gr_gid, grent->gr_name, "/etc/subgid");
        // printf("%i  %lu %lu\n", gidmap.found, gidmap.start, gidmap.count);


        pid_t child;
        int fd[2];
        char readbuffer[2];

        if (-1 == pipe(fd)){
                perror("pipe");
                exit(EXIT_FAILURE);
        }
        child = fork();
        if (-1 == child) perror("fork");
        if (0 == child){
                close(fd[1]);
                dup2(fd[0], 0);
                execlp("sh", "sh", "-e", NULL);
        }

        if (-1 == unshare(CLONE_NEWNS | CLONE_NEWUSER)){
                perror("could not unshare");
                exit(EXIT_FAILURE);
        }

        dprintf(
                fd[1],
                "newuidmap %lu %lu %lu %lu %lu %lu %lu\n" \
                "newgidmap %lu %lu %lu %lu %lu %lu %lu\n" \
                "exit 0\n",
                getpid(), 0, 1000, 1, 1, uidmap.start, uidmap.count,
                getpid(), 0, 1000, 1, 1, gidmap.start, gidmap.count
        );

        int wstatus;
        waitpid(child, &wstatus, 0);
        if (!WIFEXITED(wstatus)){
                printf("child exited abnormally");
                exit(1);
        }
        int child_exit = WEXITSTATUS(wstatus);
        if (child_exit){
                printf("child exited with %d\n", child_exit);
                exit(1);
        }

        execlp("id", "id", NULL);

}
