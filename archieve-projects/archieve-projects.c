#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_PROJECTS 2
#define MAX_PROJECT_AGE 60 // If it's greater than 60 days, it's considered old

typedef struct s_project {
    char name[100];
    char path[1024];
    time_t lastAccessed;
    unsigned int lastAccessedDaysAgo;
} t_project;

char *projectRootPath = "/Users/sharik.shaikh/projects";
char *archivePath = "/Users/sharik.shaikh/archive/projects";

void archive_project(char *projectPath, char *archievePath) {
    printf("Archiving project %s to %s\n", projectPath, archievePath);
    char command[2048];
    snprintf(command, sizeof(command), "zip -r '%s.zip' '%s'", archievePath,
             projectPath);
    system(command);
}

void get_projects(t_project *projects) {
    printf("Getting projects\n");
    struct dirent *de;
    struct stat st;
    char fullPath[1024];
    int i = 0;

    time_t now = time(NULL);
    DIR *dr = opendir(projectRootPath);

    if (dr == NULL) {
        perror("Could not open current directory");
        return;
    }

    while ((de = readdir(dr)) != NULL) {

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        snprintf(fullPath, sizeof(fullPath), "%s/%s", projectRootPath,
                 de->d_name);

        if (stat(fullPath, &st) == -1) {
            perror("stat");
            continue;
        }

        if (((now - st.st_atime) / (60 * 60 * 24)) > MAX_PROJECT_AGE) {
            continue;
        }

        strcpy(projects[i].name, de->d_name);
        strcpy(projects[i].path, fullPath);
        projects[i].lastAccessed = st.st_atime;
        projects[i].lastAccessedDaysAgo = (now - st.st_atime) / (60 * 60 * 24);
    }

    closedir(dr);
}

int main() {
    printf("Starting project archiving\n");
    t_project projects[MAX_PROJECTS];
    int projectCount = 0;
    get_projects(projects);

    for (int i = 0; i < MAX_PROJECTS; i++) {
        printf("Project name: %s\n", projects[i].name);
        printf("Project path: %s\n", projects[i].path);
        printf("Project last accessed: %ld\n", projects[i].lastAccessed);
        printf("Project last accessed days ago: %d\n",
               projects[i].lastAccessedDaysAgo);
    }

    projectCount = sizeof(projects) / sizeof(projects[0]);

    if (projectCount == 0) {
        printf("No projects found\n");
        return 0;
    }

    printf("Project count: %d\n", projectCount);

    int i;
    for (i = 0; i < projectCount; i++) {
        printf("in the loop");
        if (projects[i].lastAccessedDaysAgo <= MAX_PROJECT_AGE) {
            continue;
        }

        if (projects[i].name[0] == '\0') {
            printf("Project name is empty\n");
            continue;
        }

        printf("size of name %lu\n", sizeof(projects[i].name));
        printf("type of name %s\n", projects[i].name);

        printf("Project %s is old, archiving\n", projects[i].name);

        char vendorDeleteCommand[1024];
        char nodeModulesDeleteCommand[1024];
        char projectDeleteCommand[1024];
        char archiveProjectPath[1024];

        snprintf(vendorDeleteCommand, sizeof(vendorDeleteCommand),
                 "rm -rf %s/vendor", projects[i].path);
        snprintf(nodeModulesDeleteCommand, sizeof(nodeModulesDeleteCommand),
                 "rm -rf %s/node_modules", projects[i].path);
        snprintf(projectDeleteCommand, sizeof(projectDeleteCommand),
                 "rm -rf %s", projects[i].path);
        snprintf(archiveProjectPath, sizeof(archiveProjectPath), "%s/%s",
                 archivePath, projects[i].name);

        printf("Archiving project %s\n", projects[i].name);
        system(vendorDeleteCommand);
        system(nodeModulesDeleteCommand);

        archive_project(projects[i].path, archiveProjectPath);
        system(projectDeleteCommand);
        printf("Archived project %s\n", projects[i].name);
    }

    return 0;
}
