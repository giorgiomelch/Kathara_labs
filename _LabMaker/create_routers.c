#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>  

#define MAX_ROUTERS 50         // massimo numero di router
#define PATH_MAXLEN 4096       // lunghezza massima per tutti i path
#define MAX_NAME_LEN 256       // lunghezza massima per nome router
#define BUFFER_SIZE 2048       // buffer generico per copia file
// --- ENUM e costanti simboliche ---
typedef enum {
    RIP = 0,
    OSPF = 1
} IntraRoutingMode;

static const char *INTRA_ROUTING_NAMES[] = { "rip", "ospf" };

// --- Utility Functions ---

void rimuovi_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
        str[len - 1] = '\0';
}

int crea_directory(const char *path) {
    if (mkdir(path, 0777) == 0)
        return 0;

    if (errno == EEXIST)
        return 0;  // esiste già → non è un errore

    fprintf(stderr, "Errore creando directory '%s': %s\n", path, strerror(errno));
    return -1;
}

int copia_file(const char *src_path, const char *dst_path) {
    FILE *src = fopen(src_path, "r");
    if (!src) {
        fprintf(stderr, "Errore aprendo file sorgente '%s': %s\n", src_path, strerror(errno));
        return -1;
    }

    FILE *dst = fopen(dst_path, "w");
    if (!dst) {
        fprintf(stderr, "Errore creando file destinazione '%s': %s\n", dst_path, strerror(errno));
        fclose(src);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), src)) {
        fputs(buffer, dst);
    }

    fclose(src);
    fclose(dst);
    return 0;
}

int crea_directory_ricorsiva(const char *path) {
    char temp[PATH_MAXLEN];
    char *p = NULL;

    // Copia il percorso
    snprintf(temp, sizeof(temp), "%s", path);

    // Rimuove eventuale '/' finale
    size_t len = strlen(temp);
    if (temp[len - 1] == '/')
        temp[len - 1] = '\0';

    // Crea le sottodirectory una per una
    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(temp, 0777);  // ignora errori se già esiste
            *p = '/';
        }
    }

    // Crea l’ultima directory
    if (mkdir(temp, 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "Errore creando directory '%s': %s\n", temp, strerror(errno));
        return -1;
    }

    return 0;
}

// --- Moduli di configurazione router ---

void scrivi_startup(FILE *fp) {
    fprintf(fp, "ip address add MODIFICA.0.0.1/MODIFICA dev eth0\n");
    fprintf(fp, "ip address add MODIFICA.0.0.1/MODIFICA dev eth1\n\n");
    fprintf(fp, "systemctl start frr\n");
}

int crea_struttura_router(const char *cartella_padre, const char *nome_router, char *path_frr, size_t len) {
    char path[PATH_MAXLEN];

    snprintf(path, sizeof(path), "%s/%s/etc/frr", cartella_padre, nome_router);
    if (crea_directory_ricorsiva(path) != 0){
        return -1;}

    strncpy(path_frr, path, len);
    path_frr[PATH_MAXLEN - 1] = '\0';
    return 0;
}

int abilita_daemons(const char *file_daemons, IntraRoutingMode mode) {
    /**
     * Aggiorna il file daemons settando il demone corretto a "yes".
     * RIP -> modifica "ripd=no" in "ripd=yes"
     * OSPF -> modifica "ospfd=no" in "ospfd=yes"
     */
    FILE *fp = fopen(file_daemons, "r+");
    if (!fp) {
        fprintf(stderr, "Errore aprendo file '%s': %s\n", file_daemons, strerror(errno));
        return -1;
    }

    char line[BUFFER_SIZE];
    long pos;
    const char *search, *replace;

    if (mode == RIP) {
        search = "ripd=no";
        replace = "ripd=yes";
    } else { // OSPF
        search = "ospfd=no";
        replace = "ospfd=yes";
    }

    while (fgets(line, sizeof(line), fp)) {
        if ((pos = ftell(fp)) == -1) {
            perror("ftell");
            fclose(fp);
            return -1;
        }

        if (strstr(line, search)) {
            fseek(fp, pos - strlen(line), SEEK_SET);  // torna all’inizio della riga
            fprintf(fp, "%s\n", replace);
            break;
        }
    }

    fclose(fp);
    return 0;
}

void chiedi_OSPF_area_and_subnet(char *ospf_area, size_t area_len, char *ospf_subnet, size_t subnet_len) {
    printf("Inserisci area OSPF (es. 0.0.0.0 per backbone, x.x.x.x per stub): ");
    fgets(ospf_area, area_len, stdin);
    rimuovi_newline(ospf_area);

    printf("Inserisci subnet associata (es. 192.168.1.0/24): ");
    fgets(ospf_subnet, subnet_len, stdin);
    rimuovi_newline(ospf_subnet);
}

int configura_ospf_conf(const char *file_frr_conf, const char *area, const char *subnet) {
    char temp_file[PATH_MAXLEN];
    char line[BUFFER_SIZE];

    snprintf(temp_file, sizeof(temp_file), "%s.tmp", file_frr_conf);

    FILE *src = fopen(file_frr_conf, "r");
    if (!src) {
        fprintf(stderr, "Errore aprendo file '%s': %s\n", file_frr_conf, strerror(errno));
        return -1;
    }

    FILE *dst = fopen(temp_file, "w");
    if (!dst) {
        fprintf(stderr, "Errore creando file temporaneo '%s': %s\n", temp_file, strerror(errno));
        fclose(src);
        return -1;
    }

    while (fgets(line, sizeof(line), src)) {
        if (strstr(line, "network MODIFICA/ANCHEQUA area EQUA")) {
            fprintf(dst, "network %s area %s\n", subnet, area);
            if (strcmp(area, "0.0.0.0") != 0)
                fprintf(dst, "area %s stub\n", area);
        } else {
            fputs(line, dst);
        }
    }

    fclose(src);
    fclose(dst);

    if (remove(file_frr_conf) != 0 || rename(temp_file, file_frr_conf) != 0) {
        fprintf(stderr, "Errore sovrascrivendo file '%s'\n", file_frr_conf);
        return -1;
    }

    printf("Configurazione OSPF aggiornata con subnet %s e area %s.\n", subnet, area);
    return 0;
}

int crea_files_frr(const char *path_frr, IntraRoutingMode mode, const char *area, const char *subnet) {
    char file_frr_conf[PATH_MAXLEN];
    char file_daemons[PATH_MAXLEN];
    char template_frr_conf[PATH_MAXLEN];
    const char *template_daemons = "daemons_template.txt";

    snprintf(file_frr_conf, sizeof(file_frr_conf), "%s/frr.conf", path_frr);
    snprintf(file_daemons, sizeof(file_daemons), "%s/daemons", path_frr);
    snprintf(template_frr_conf, sizeof(template_frr_conf), "frrconf_%s_template.txt", INTRA_ROUTING_NAMES[mode]);

    printf("Creazione file FRR per modalità: %s\n", INTRA_ROUTING_NAMES[mode]);

    if (copia_file(template_frr_conf, file_frr_conf) != 0) return -1;
    if (copia_file(template_daemons, file_daemons) != 0) return -1;
    if (abilita_daemons(file_daemons, mode) != 0) return -1;

    if (mode == OSPF && area && subnet)
        return configura_ospf_conf(file_frr_conf, area, subnet);
    return 0;
}

int crea_lab_conf(const char *path_dir, char router_names[][PATH_MAXLEN], int router_count) {
    char lab_conf_path[PATH_MAXLEN];
    snprintf(lab_conf_path, sizeof(lab_conf_path), "%s/lab.conf", path_dir);

    // Controlla se il file esiste già
    int esiste = (access(lab_conf_path, F_OK) == 0);

    // Apri in append se esiste, altrimenti in scrittura
    FILE *lab_fp = fopen(lab_conf_path, esiste ? "a" : "w");
    if (!lab_fp) {
        fprintf(stderr, "Errore aprendo lab.conf: %s\n", strerror(errno));
        return -1;
    }

    if (esiste)
        printf("File lab.conf esistente, aggiunta nuovi router...\n");
    else
        printf("Creazione nuovo file lab.conf...\n");

    // Scrive i nuovi router
    for (int i = 0; i < router_count; i++) {
        fprintf(lab_fp, "%s[0]=\"MODIFICA\"\n", router_names[i]);
        fprintf(lab_fp, "%s[1]=\"MODIFICA\"\n", router_names[i]);
        fprintf(lab_fp, "%s[2]=\"MODIFICA\"\n", router_names[i]);
        fprintf(lab_fp, "%s[image]=\"kathara/frr\"\n\n", router_names[i]);
    }

    fclose(lab_fp);
    return 0;
}

// --- Gestione Input Utente ---

int chiedi_modalita_intrarouting(void) {
    int scelta = -1;
    do {
        printf("Scegli il protocollo di intrarouting:\n");
        printf("  0 -> RIP\n  1 -> OSPF\nScelta: ");
        if (scanf("%d", &scelta) != 1 || scelta < 0 || scelta > 1) {
            fprintf(stderr, "Scelta non valida. Riprova.\n");
            while (getchar() != '\n'); // pulisce buffer input
            scelta = -1;
        }
         else {
            while (getchar() != '\n'); // consuma il newline lasciato da scanf
        }
    } while (scelta == -1);
    return scelta;
}

// --- MAIN PROGRAM ---

int main(void) {
    char base_dir[PATH_MAXLEN];
    char nome_router[MAX_NAME_LEN];
    char path_frr[PATH_MAXLEN];
    char router_names[MAX_ROUTERS][PATH_MAXLEN];
    int router_count = 0;

    printf("=== CREAZIONE ROUTERS ===\n");
    printf("Cartella di destinazione: ");
    fgets(base_dir, sizeof(base_dir), stdin);
    rimuovi_newline(base_dir);

    char abs_path[PATH_MAXLEN + 100];
    snprintf(abs_path, sizeof(abs_path), "../%s", base_dir);
    crea_directory(abs_path);
    printf("abs path: %s\n", abs_path);

    IntraRoutingMode mode = chiedi_modalita_intrarouting();

    char ospf_area[64] = "";
    char ospf_subnet[64] = "";
    if (mode==OSPF)
        chiedi_OSPF_area_and_subnet(ospf_area, sizeof(ospf_area), ospf_subnet, sizeof(ospf_subnet));

    while (1) {
        printf("\nNome router (vuoto per terminare): ");
        fgets(nome_router, sizeof(nome_router), stdin);
        rimuovi_newline(nome_router);
        if (strlen(nome_router) == 0) break;

        strncpy(router_names[router_count++], nome_router, PATH_MAXLEN);

        // File startup
        char startup_path[PATH_MAXLEN + 500];
        snprintf(startup_path, sizeof(startup_path), "%s/%s.startup", abs_path, nome_router);

        FILE *fp = fopen(startup_path, "w");
        if (!fp) {
            fprintf(stderr, "Errore creando file startup per '%s'\n", nome_router);
            continue;
        }
        scrivi_startup(fp);
        fclose(fp);
        // Struttura e configurazioni FRR
        if (crea_struttura_router(abs_path, nome_router, path_frr, sizeof(path_frr)) == 0){
            crea_files_frr(path_frr, mode, ospf_area, ospf_subnet);
        }

        printf("Router '%s' creato correttamente.\n", nome_router);
    }

    crea_lab_conf(abs_path, router_names, router_count);
    printf("\nConfigurazione laboratorio completata con %d router.\n", router_count);

    return 0;
}
