#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_PROJECT_AGE 60 // If it's greater than 60 days, it's considered old

typedef struct s_project {
    char name[100];
    char path[1024];
    time_t lastAccessed;
    unsigned int lastAccessedDaysAgo;
} t_project;

char *projectRootPath = "/Users/sharik.shaikh/projects";
char *archivePath = "/Users/sharik.shaikh/archive/projects";

void archive_project(const char *projectPath, const char *archievePath) {
    printf("Archiving project %s to %s\n", projectPath, archievePath);
    char command[2048];
    snprintf(command, sizeof(command), "zip -r '%s.zip' '%s'", archievePath,
             projectPath);
    if (system(command) == -1) {
        perror("Failed to archive project");
    }
}

t_project *get_projects(int *count) {
    printf("Getting projects\n");
    struct dirent *de;
    struct stat st;
    char fullPath[1024];
    int i = 0, capacity = 10;
    time_t now = time(NULL);

    t_project *projects = malloc(capacity * sizeof(t_project));
    if (!projects) {
        perror("Failed to allocate memory for projects");
        return NULL;
    }

    DIR *dr = opendir(projectRootPath);
    if (dr == NULL) {
        perror("Could not open project directory");
        free(projects);
        return NULL;
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

        if (i >= capacity) {
            capacity *= 2;
            projects = realloc(projects, capacity * sizeof(t_project));
            if (!projects) {
                perror("Failed to reallocate memory for projects");
                closedir(dr);
                return NULL;
            }
        }

        strncpy(projects[i].name, de->d_name, sizeof(projects[i].name) - 1);
        strncpy(projects[i].path, fullPath, sizeof(projects[i].path) - 1);
        projects[i].lastAccessed = st.st_atime;
        projects[i].lastAccessedDaysAgo = (now - st.st_atime) / (60 * 60 * 24);
        i++;
    }
    closedir(dr);
    *count = i;
    return projects;
}

int main() {
    printf("Starting project archiving\n");
    int projectCount = 0;
    t_project *projects = get_projects(&projectCount);

    if (!projects) {
        return 1;
    }

    if (projectCount == 0) {
        printf("No projects found\n");
        free(projects);
        return 0;
    }

    printf("Project count: %d\n", projectCount);

    for (int i = 0; i < projectCount; i++) {
        printf("Project name: %s\n", projects[i].name);
        printf("Project path: %s\n", projects[i].path);
        printf("Project last accessed: %ld\n", projects[i].lastAccessed);
        printf("Project last accessed days ago: %d\n",
               projects[i].lastAccessedDaysAgo);

        if (projects[i].lastAccessedDaysAgo > MAX_PROJECT_AGE) {
            char archiveProjectPath[1024];
            snprintf(archiveProjectPath, sizeof(archiveProjectPath), "%s/%s",
                     archivePath, projects[i].name);
            archive_project(projects[i].path, archiveProjectPath);
        }
    }

    free(projects);
    return 0;
}
