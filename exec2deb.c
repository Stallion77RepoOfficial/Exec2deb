#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

void capitalize_first_letter(char *str) {
    if (str && str[0]) {
        str[0] = toupper((unsigned char)str[0]);
    }
}

// Geçici dosyayı silme
void remove_directory(const char *path) {
    char command[256];
    snprintf(command, sizeof(command), "rm -rf %s", path);
    system(command);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path> <version>\n", argv[0]);
        return 1;
    }

    char *file_path = argv[1];  // Yürütülebilir dosyanın tam yolu
    char *version = argv[2];     // Versiyon numarası

    // Dosya ismini çıkartma (tam yoldan)
    char *file_name = strrchr(file_path, '/');
    if (file_name != NULL) {
        file_name++;  // '/' karakterinden sonrasını al
    } else {
        file_name = file_path;  // Eğer yol yoksa, dosya adını olduğu gibi al
    }

    // Açıklama oluşturma: Dosya adı ile açıklama başlatılır
    char description[256];
    snprintf(description, sizeof(description), "%s packaged version", file_name);
    capitalize_first_letter(description);  // Açıklamadaki ilk harfi büyük yap

    // Control dosyasındaki içerik
    char control_data[1024];
    snprintf(control_data, sizeof(control_data),
             "Package: %s\n"
             "Version: %s\n"
             "Section: utils\n"
             "Priority: optional\n"
             "Architecture: arm64\n"
             "Description: %s\n", 
             file_name, version, description);

    // Geçici dizinleri oluştur ve DEBIAN kontrol dosyasını yaz
    char command[512];
    snprintf(command, sizeof(command), "mkdir -p /tmp/deb-package/DEBIAN /tmp/deb-package/usr/local/bin");
    system(command);

    FILE *control_file = fopen("/tmp/deb-package/DEBIAN/control", "w");
    if (!control_file) {
        perror("Failed to create control file");
        return 1;
    }

    fputs(control_data, control_file);  // Control verisini yaz
    fclose(control_file);

    // Yürütülebilir dosyayı kopyala
    snprintf(command, sizeof(command), "cp %s /tmp/deb-package/usr/local/bin/", file_path);
    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "Failed to copy executable file\n");
        return 1;
    }

    // .deb paketini oluştur
    snprintf(command, sizeof(command), "dpkg-deb --build /tmp/deb-package %s_%s_arm64.deb", file_name, version);
    result = system(command);
    
    if (result != 0) {
        fprintf(stderr, "Failed to create the .deb package\n");
        return 1;
    }

    printf("Created package: %s_%s_arm64.deb\n", file_name, version);

    // .deb paketini sistem geneline kur
    snprintf(command, sizeof(command), "sudo dpkg -i %s_%s_arm64.deb", file_name, version);
    result = system(command);
    
    if (result != 0) {
        fprintf(stderr, "Failed to install the .deb package\n");
        return 1;
    }

    // Geçici dizini silme (veya başka bir işlem yapmak isterseniz)
    remove_directory("/tmp/deb-package");

    // Kaynak dosya artık kurulum için /usr/local/bin altında olacak
    printf("Installation successful. Package and executable are ready.\n");

    return 0;
}