#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/wait.h>
#include <libgen.h>
#include <errno.h>
#include <sys/utsname.h>

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

// Debian package name normalization: lowercase, replace invalid chars
void normalize_package_name(char *name) {
    for (size_t i = 0; i < strlen(name); i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '+' && name[i] != '.' && name[i] != '-') {
            name[i] = '-';
        }
        name[i] = tolower((unsigned char)name[i]);
    }
}

int run_command(char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp failed");
        exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    } else {
        perror("fork failed");
        return 1;
    }
}

void remove_directory(const char *path) {
    char *args[] = { "rm", "-rf", (char *)path, NULL };
    run_command(args);
}

char *detect_arch() {
    static char arch[64];
    struct utsname uname_buf;
    if (uname(&uname_buf) == 0) {
        if (strcmp(uname_buf.machine, "x86_64") == 0)
            return "amd64";
        else if (strcmp(uname_buf.machine, "aarch64") == 0)
            return "arm64";
        else
            snprintf(arch, sizeof(arch), "%s", uname_buf.machine);
        return arch;
    }
    return "all";
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Usage: %s <file_path> <version> [maintainer] [--dry-run]\n", argv[0]);
        return 1;
    }

    int dry_run = 0;
    char *file_path = argv[1];
    char *version = argv[2];
    char *maintainer = (argc >= 4 && argv[3][0] != '-') ? argv[3] : "Exec2deb Team";

    if (argc == 5 && strcmp(argv[4], "--dry-run") == 0) {
        dry_run = 1;
    }

    if (!is_safe_string(version)) {
        fprintf(stderr, "Invalid version format.\n");
        return 1;
    }

    char *file_name = strrchr(file_path, '/');
    file_name = file_name ? file_name + 1 : file_path;

    if (!is_safe_string(file_name)) {
        fprintf(stderr, "Invalid characters in file name.\n");
        return 1;
    }

    char package_name[256];
    strncpy(package_name, file_name, sizeof(package_name) - 1);
    package_name[sizeof(package_name) - 1] = '\0';
    normalize_package_name(package_name);

    char description[256];
    snprintf(description, sizeof(description), "%s packaged version", file_name);
    capitalize_first_letter(description);

    const char *arch = detect_arch();

    char control_data[1024];
    snprintf(control_data, sizeof(control_data),
             "Package: %s\n"
             "Version: %s\n"
             "Section: utils\n"
             "Priority: optional\n"
             "Architecture: %s\n"
             "Maintainer: %s\n"
             "Description: %s\n",
             package_name, version, arch, maintainer, description);

    char base_dir[PATH_MAX];
    snprintf(base_dir, sizeof(base_dir), "/tmp/deb-package-%d", getpid());

    char debian_dir[PATH_MAX], bin_dir[PATH_MAX], control_path[PATH_MAX];
    snprintf(debian_dir, sizeof(debian_dir), "%s/DEBIAN", base_dir);
    snprintf(bin_dir, sizeof(bin_dir), "%s/usr/local/bin", base_dir);
    snprintf(control_path, sizeof(control_path), "%s/control", debian_dir);

    char *mkdir_args[] = {"mkdir", "-p", debian_dir, bin_dir, NULL};
    if (run_command(mkdir_args) != 0) {
        fprintf(stderr, "Failed to create directory structure.\n");
        return 1;
    }

    FILE *control_file = fopen(control_path, "w");
    if (!control_file) {
        perror("control file");
        remove_directory(base_dir);
        return 1;
    }
    fputs(control_data, control_file);
    fclose(control_file);

    char dest_path[PATH_MAX];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", bin_dir, file_name);
    char *cp_args[] = {"cp", file_path, dest_path, NULL};
    if (run_command(cp_args) != 0) {
        fprintf(stderr, "Failed to copy file.\n");
        remove_directory(base_dir);
        return 1;
    }

    char deb_file[PATH_MAX];
    snprintf(deb_file, sizeof(deb_file), "%s_%s_%s.deb", package_name, version, arch);
    char *dpkg_args[] = {"dpkg-deb", "--build", (char *)base_dir, deb_file, NULL};
    if (run_command(dpkg_args) != 0) {
        fprintf(stderr, "Failed to build deb package.\n");
        remove_directory(base_dir);
        return 1;
    }

    printf("Package created: %s\n", deb_file);

    if (!dry_run) {
        char *install_args[] = {"sudo", "dpkg", "-i", deb_file, NULL};
        if (run_command(install_args) != 0) {
            fprintf(stderr, "Failed to install deb package.\n");
            remove_directory(base_dir);
            return 1;
        }
        printf("Package installed successfully.\n");
    } else {
        printf("Dry-run: Package created but not installed.\n");
    }

    remove_directory(base_dir);
    return 0;
}
