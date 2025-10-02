#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAXLEN 256

void rimuovi_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}
int create_directory(const char *path) {
    return mkdir(path, 0777);  // permessi rwx per tutti
}

void create_router_startup(FILE *fp){
    fprintf(fp, "ip address add !100.0.0.1/24 dev eth0\n\nsystemctl start frr");
}
// Restituisce il percorso completo della cartella frr
int crea_struttura_router(const char *cartella_padre, const char *nome_router, char *path_frr) {
    char path[512];

    // <cartella_padre>/<nome_router>
    snprintf(path, sizeof(path), "%s/%s", cartella_padre, nome_router);
    create_directory(path);

    // <cartella_padre>/<nome_router>/etc
    snprintf(path, sizeof(path), "%s/%s/etc", cartella_padre, nome_router);
    create_directory(path);

    // <cartella_padre>/<nome_router>/etc/frr
    snprintf(path, sizeof(path), "%s/%s/etc/frr", cartella_padre, nome_router);
    create_directory(path);

    strncpy(path_frr, path, 512); // restituisci percorso frr
    return 0;
}

void crea_files_frr(const char *path_frr) {
    char file_frr_conf[MAXLEN];
    char file_daemons[MAXLEN];

    snprintf(file_frr_conf, sizeof(file_frr_conf), "%s/frr.conf", path_frr);
    snprintf(file_daemons, sizeof(file_daemons), "%s/daemons", path_frr);
    
    // Creazione daemons copiando da frrconf_template
    FILE *src_frrconf = fopen("frrconf_template.txt", "r");
    if (!src_frrconf) {
        perror("Errore aprendo template frr.conf");
        return;
    }

    FILE *fp = fopen(file_frr_conf, "w");
    if (!fp) {
        perror("Errore creando frr.conf");
        fclose(src_frrconf);
        return;
    }
    char buffer1[2048];
    while (fgets(buffer1, sizeof(buffer1), src_frrconf)) {
        fputs(buffer1, fp);
    }
    fclose(src_frrconf);
    fclose(fp);

    // Creazione daemons copiando da daemons_template
    FILE *src_daemons = fopen("daemons_template.txt", "r");
    if (!src_daemons) {
        perror("Errore aprendo template daemons");
        return;
    }

    fp = fopen(file_daemons, "w");
    if (!fp) {
        perror("Errore creando daemons");
        fclose(src_daemons);
        return;
    }

    char buffer2[2048];
    while (fgets(buffer2, sizeof(buffer2), src_daemons)) {
        fputs(buffer2, fp);
    }
    fclose(src_daemons);
    fclose(fp);
}



int main() {
    char directory[MAXLEN];
    char nome[MAXLEN];
    char path_frr[512];

    printf("CREAZIONE ROUTERS:\n");
    printf("Scegliere la cartella di destinazione:\n");

    if (fgets(directory, sizeof(directory), stdin) == NULL) {
        fprintf(stderr, "Errore: directory non valida.\n");
        return 1;
    }
    rimuovi_newline(directory);
    char path2directory[MAXLEN+10];
    snprintf(path2directory, sizeof(path2directory), "../%s", directory);
    if (create_directory(path2directory) == 0) {
        printf("Cartella '%s' creata con successo.\n", path2directory);
    }

    while (1) {
        printf("Nome router (vuoto per terminare): ");
        if (fgets(nome, sizeof(nome), stdin) == NULL) break;
        rimuovi_newline(nome);
        if (strlen(nome) == 0) break;

        char filename[MAXLEN * 3];
        snprintf(filename, sizeof(filename), "%s/%s.startup", path2directory, nome);

        FILE *fp = fopen(filename, "w");
        if (fp == NULL) {
            fprintf(stderr, "Errore: impossibile creare file %s\n", filename);
            continue;
        }
        create_router_startup(fp);
        crea_struttura_router(path2directory, nome, path_frr);
        crea_files_frr(path_frr);
        fclose(fp);

        printf("  -- File %s creato con successo.\n", filename);
    }

    return 0;
}
