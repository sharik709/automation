/**
 * Simple ZIP/UNZIP utility using zlib and minizip libraries
 * Usage:
 *   - To zip:   ./ziputil -z source_folder destination.zip
 *   - To unzip: ./ziputil -u source.zip destination_folder
 *
 * Compilation:
 *   gcc -o ziputil ziputil.c -lz -lminizip
 *
 * Note: Requires zlib and minizip development libraries
 *   On Debian/Ubuntu: sudo apt-get install zlib1g-dev libminizip-dev
 *   On macOS: brew install zlib minizip
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unzip.h>
#include <zip.h>

#define MAX_PATH 4096
#define BUFFER_SIZE 8192

// Function to create directory if it doesn't exist
void create_directory(const char *dir) {
    struct stat st = {0};

    if (stat(dir, &st) == -1) {
#ifdef _WIN32
        mkdir(dir);
#else
        mkdir(dir, 0755);
#endif
    }
}

// Function to add a file to a zip archive
int add_file_to_zip(zipFile zf, const char *filename, const char *filepath,
                    const char *base_dir) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Error opening file %s\n", filepath);
        return 0;
    }

    zip_fileinfo zi = {0};
    struct stat file_stat;
    if (stat(filepath, &file_stat) == 0) {
        zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour = 0;
        zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
        zi.dosDate = 0;
        zi.internal_fa = 0;
        zi.external_fa = 0;
    }

    // Calculate the relative path for storage in the zip
    char relative_path[MAX_PATH] = {0};

    // If the file is in a subdirectory, we need to extract the relative path
    if (base_dir && base_dir[0] != '\0') {
        if (strstr(filepath, base_dir) == filepath) {
            strcpy(relative_path, filepath + strlen(base_dir));
            if (relative_path[0] == '/' || relative_path[0] == '\\') {
                memmove(relative_path, relative_path + 1,
                        strlen(relative_path));
            }
        } else {
            strcpy(relative_path, filename);
        }
    } else {
        strcpy(relative_path, filename);
    }

    // Replace backward slashes with forward slashes
    for (char *p = relative_path; *p; p++) {
        if (*p == '\\')
            *p = '/';
    }

    // Create a new file entry in the zip
    if (zipOpenNewFileInZip(zf, relative_path, &zi, NULL, 0, NULL, 0, NULL,
                            Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK) {
        fprintf(stderr, "Error creating file %s in zip\n", relative_path);
        fclose(file);
        return 0;
    }

    // Read the file and write it to the zip
    unsigned char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (zipWriteInFileInZip(zf, buffer, bytes_read) < 0) {
            fprintf(stderr, "Error writing %s to zip\n", relative_path);
            zipCloseFileInZip(zf);
            fclose(file);
            return 0;
        }
    }

    zipCloseFileInZip(zf);
    fclose(file);
    printf("Added: %s\n", relative_path);

    return 1;
}

// Function to recursively add a directory to a zip archive
int add_dir_to_zip(zipFile zf, const char *dirname, const char *base_dir) {
    DIR *dir;
    struct dirent *ent;
    char filepath[MAX_PATH];

    if ((dir = opendir(dirname)) == NULL) {
        fprintf(stderr, "Error opening directory %s\n", dirname);
        return 0;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        snprintf(filepath, MAX_PATH, "%s/%s", dirname, ent->d_name);

        struct stat path_stat;
        stat(filepath, &path_stat);

        if (S_ISDIR(path_stat.st_mode)) {
            // It's a directory, recursively add it
            add_dir_to_zip(zf, filepath, base_dir);
        } else {
            // It's a file, add it
            add_file_to_zip(zf, ent->d_name, filepath, base_dir);
        }
    }

    closedir(dir);
    return 1;
}

// Function to zip a folder
int zip_folder(const char *source_dir, const char *zip_file) {
    zipFile zf = zipOpen64(zip_file, APPEND_STATUS_CREATE);

    if (zf == NULL) {
        fprintf(stderr, "Error creating zip file %s\n", zip_file);
        return 0;
    }

    // Remove trailing slash if present
    char clean_source_dir[MAX_PATH];
    strcpy(clean_source_dir, source_dir);

    size_t len = strlen(clean_source_dir);
    if (len > 0 && (clean_source_dir[len - 1] == '/' ||
                    clean_source_dir[len - 1] == '\\')) {
        clean_source_dir[len - 1] = '\0';
    }

    // Get the base directory name
    char *base_dir = clean_source_dir;

    // Add all files and directories
    int result = add_dir_to_zip(zf, clean_source_dir, base_dir);

    zipClose(zf, NULL);
    return result;
}

// Function to extract a zip file
int unzip_file(const char *zip_file, const char *dest_dir) {
    unzFile zf = unzOpen64(zip_file);

    if (zf == NULL) {
        fprintf(stderr, "Error opening zip file %s\n", zip_file);
        return 0;
    }

    // Create destination directory if it doesn't exist
    create_directory(dest_dir);

    // Get info about the zip file
    unz_global_info64 global_info;
    if (unzGetGlobalInfo64(zf, &global_info) != UNZ_OK) {
        fprintf(stderr, "Error getting info from zip file\n");
        unzClose(zf);
        return 0;
    }

    // Buffer to read compressed file
    unsigned char buffer[BUFFER_SIZE];

    // Go to the first file in archive
    if (unzGoToFirstFile(zf) != UNZ_OK) {
        fprintf(stderr, "Error going to first file in zip\n");
        unzClose(zf);
        return 0;
    }

    // Extract each file
    do {
        char filename[MAX_PATH];
        unz_file_info64 file_info;

        // Get info about current file
        if (unzGetCurrentFileInfo64(zf, &file_info, filename, MAX_PATH, NULL, 0,
                                    NULL, 0) != UNZ_OK) {
            fprintf(stderr, "Error getting file info\n");
            unzClose(zf);
            return 0;
        }

        // Construct the full path
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", dest_dir, filename);

        // Check if this is a directory
        int is_dir = (filename[strlen(filename) - 1] == '/' ||
                      filename[strlen(filename) - 1] == '\\');

        if (is_dir) {
            // Create directory
            create_directory(full_path);
            printf("Created directory: %s\n", filename);
        } else {
            // Create subdirectories if needed
            char dir_path[MAX_PATH];
            strcpy(dir_path, full_path);

            // Extract directory path
            for (char *p = dir_path + strlen(dir_path); p > dir_path; p--) {
                if (*p == '/' || *p == '\\') {
                    *p = '\0';
                    break;
                }
            }

            if (strlen(dir_path) > 0) {
                create_directory(dir_path);
            }

            // Open the compressed file
            if (unzOpenCurrentFile(zf) != UNZ_OK) {
                fprintf(stderr, "Error opening file %s in zip\n", filename);
                continue;
            }

            // Create the output file
            FILE *out = fopen(full_path, "wb");
            if (out == NULL) {
                fprintf(stderr, "Error creating file %s\n", full_path);
                unzCloseCurrentFile(zf);
                continue;
            }

            // Extract file
            int bytes_read;
            do {
                bytes_read = unzReadCurrentFile(zf, buffer, BUFFER_SIZE);
                if (bytes_read < 0) {
                    fprintf(stderr, "Error reading compressed file %s\n",
                            filename);
                    break;
                }

                if (bytes_read > 0) {
                    fwrite(buffer, 1, bytes_read, out);
                }
            } while (bytes_read > 0);

            fclose(out);
            unzCloseCurrentFile(zf);
            printf("Extracted: %s\n", filename);
        }

    } while (unzGoToNextFile(zf) == UNZ_OK);

    unzClose(zf);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  To zip:   %s -z source_folder destination.zip\n",
                argv[0]);
        fprintf(stderr, "  To unzip: %s -u source.zip destination_folder\n",
                argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    const char *source = argv[2];
    const char *dest = argv[3];

    if (strcmp(mode, "-z") == 0) {
        printf("Zipping %s to %s...\n", source, dest);
        if (zip_folder(source, dest)) {
            printf("Successfully zipped folder.\n");
            return 0;
        } else {
            fprintf(stderr, "Failed to zip folder.\n");
            return 1;
        }
    } else if (strcmp(mode, "-u") == 0) {
        printf("Unzipping %s to %s...\n", source, dest);
        if (unzip_file(source, dest)) {
            printf("Successfully unzipped file.\n");
            return 0;
        } else {
            fprintf(stderr, "Failed to unzip file.\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Invalid mode: %s\n", mode);
        fprintf(stderr, "Use -z to zip or -u to unzip\n");
        return 1;
    }
}
