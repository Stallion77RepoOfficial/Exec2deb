#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

void capitalize_first_letter(char *str) {
    if (str && str[0]) {
        str[0] = toupper((unsigned char)str[0]);
    }
}

int is_safe_string(const char *str) {
    while (*str) {
        if (!isalnum((unsigned char)*str) && *str != '.' && *str != '_' && *str != '-') {
            return 0;
        }
        str++;
    }
    return 1;
}

void remove_directory(const char *path) {
    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", path);
    system(cmd);
}

int main(int argc, char *argv[]) {
    int dry_run = 0;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <file_path> <version> [--dry-run]\n", argv[0]);
        return 1;
    }

    if (argc == 4 && strcmp(argv[3], "--dry-run") == 0) {
        dry_run = 1;
    }

    char *file_path = argv[1];
    char *version = argv[2];

    if (!is_safe_string(version)) {
        fprintf(stderr, "Error: Invalid characters in version.\n");
        return 1;
    }

    char *file_name = strrchr(file_path, '/');
    file_name = file_name ? file_name + 1 : file_path;

    if (!is_safe_string(file_name)) {
        fprintf(stderr, "Error: Invalid characters in file name.\n");
        return 1;
    }

    char description[256];
    snprintf(description, sizeof(description), "%s packaged version", file_name);
    capitalize_first_letter(description);

    char control_data[1024];
    snprintf(control_data, sizeof(control_data),
             "Package: %s\n"
             "Version: %s\n"
             "Section: utils\n"
             "Priority: optional\n"
             "Architecture: arm64\n"
             "Maintainer: Exec2deb Team\n"
             "Description: %s\n",
             file_name, version, description);

    const char *base_dir = "/tmp/deb-package";
    char debian_dir[PATH_MAX], bin_dir[PATH_MAX], control_path[PATH_MAX];
    snprintf(debian_dir, sizeof(debian_dir), "%s/DEBIAN", base_dir);
    snprintf(bin_dir, sizeof(bin_dir), "%s/usr/local/bin", base_dir);
    snprintf(control_path, sizeof(control_path), "%s/control", debian_dir);

    char cmd[PATH_MAX * 2];

    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' '%s'", debian_dir, bin_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to create directory structure.\n");
        return 1;
    }

    FILE *control_file = fopen(control_path, "w");
    if (!control_file) {
        perror("Failed to write control file");
        return 1;
    }
    fputs(control_data, control_file);
    fclose(control_file);

    snprintf(cmd, sizeof(cmd), "cp '%s' '%s/'", file_path, bin_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to copy executable file.\n");
        remove_directory(base_dir);
        return 1;
    }

    char deb_file[PATH_MAX];
    snprintf(deb_file, sizeof(deb_file), "%s_%s_arm64.deb", file_name, version);
    snprintf(cmd, sizeof(cmd), "dpkg-deb --build '%s' '%s'", base_dir, deb_file);

    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to create .deb package.\n");
        remove_directory(base_dir);
        return 1;
    }

    printf("Package created: %s\n", deb_file);

    if (!dry_run) {
        snprintf(cmd, sizeof(cmd), "sudo dpkg -i '%s'", deb_file);
        if (system(cmd) != 0) {
            fprintf(stderr, "Failed to install .deb package.\n");
            remove_directory(base_dir);
            return 1;
        }
        printf("Package installed successfully.\n");
    } else {
        printf("Dry-run mode: installation skipped.\n");
    }

    remove_directory(base_dir);
    return 0;
}
