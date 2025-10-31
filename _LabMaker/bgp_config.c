#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define MAX_PATH 512
#define FRR_TEMPLATE_FILE "frrconf_bgp_template.txt"
#define DAEMONS_TEMPLATE_FILE "daemons_template.txt"

// Funzione per controllare se una directory esiste
int directory_exists(const char *path) {
    struct stat st = {0};
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// Funzione per creare directory annidate
void create_nested_dirs(const char *path) {
    char tmp[MAX_PATH];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

void copy_file(const char *src_path, const char *dst_path) {
    FILE *src = fopen(src_path, "r");
    if (!src) {
        perror("Impossibile aprire il file sorgente");
        return;
    }

    FILE *dst = fopen(dst_path, "w");
    if (!dst) {
        perror("Impossibile creare il file di destinazione");
        fclose(src);
        return;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), src)) {
        fputs(buffer, dst);
    }

    fclose(src);
    fclose(dst);
}

void append_line_to_file(const char *file_path, const char *line) {
    FILE *f = fopen(file_path, "a");
    if (!f) {
        perror("Errore apertura file per appendere riga");
        return;
    }
    fprintf(f, "%s\n", line);
    fclose(f);
}

// Funzione per sostituire una riga in un file
void replace_bgpd_line(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("Errore apertura file daemons");
        return;
    }

    FILE *tmp = fopen("tmp_daemons.txt", "w");
    if (!tmp) {
        perror("Errore creazione file temporaneo");
        fclose(fp);
        return;
    }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "bgpd=no")) {
            fprintf(tmp, "bgpd=yes\n");
            printf("bgpd= cambiato in yes\n");
            found = 1;
        } else {
            fputs(line, tmp);
        }
    }
    if (!found) printf("Riga 'bgpd=no' non trovata nel file daemons.\n");

    fclose(fp);
    fclose(tmp);
    remove(filepath);
    rename("tmp_daemons.txt", filepath);
}

// Funzione per copiare il template nel file frr.conf
void copy_template_to_frrconf(const char *template_path, const char *frrconf_path) {
    FILE *src = fopen(template_path, "r");
    if (!src) {
        perror("Impossibile aprire il file template");
        return;
    }

    FILE *dst = fopen(frrconf_path, "w");
    if (!dst) {
        perror("Impossibile creare il file frr.conf");
        fclose(src);
        return;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), src)) {
        fputs(buffer, dst);
    }
    fclose(src);
    fclose(dst);
}
// Funzione per aggiungere il template bgp all'inizio di frr.conf
void prepend_template_to_frrconf(const char *template_path, const char *target_path) {
    FILE *template_file = fopen(template_path, "r");
    if (!template_file) {
        perror("Errore apertura file template");
        return;
    }

    FILE *target_file = fopen(target_path, "r");
    if (!target_file) {
        perror("Errore apertura file frr.conf esistente");
        fclose(template_file);
        return;
    }

    // File temporaneo
    FILE *temp_file = fopen("tmp_frr.conf", "w");
    if (!temp_file) {
        perror("Errore creazione file temporaneo");
        fclose(template_file);
        fclose(target_file);
        return;
    }

    char buffer[512];

    // Prima copia il contenuto del template
    while (fgets(buffer, sizeof(buffer), template_file)) {
        fputs(buffer, temp_file);
    }

    // Poi copia il contenuto originale di frr.conf
    while (fgets(buffer, sizeof(buffer), target_file)) {
        fputs(buffer, temp_file);
    }

    fclose(template_file);
    fclose(target_file);
    fclose(temp_file);

    // Sovrascrive il file originale
    remove(target_path);
    rename("tmp_frr.conf", target_path);
}

// --- Funzione principale di configurazione di una macchina ---
void configura_macchina(
        const char *machine_name,
        const char *original_dir,
        const char *frr_template_full,
        const char *daemons_template_full) {

    char etc_frr_path[MAX_PATH];
    char daemons_path[MAX_PATH];
    char frrconf_path[MAX_PATH];
    char labconf_path[MAX_PATH];
    char startup_path[MAX_PATH];
    char line_to_append[MAX_PATH];

    // Costruisce i percorsi relativi
    snprintf(etc_frr_path, sizeof(etc_frr_path), "%s/etc/frr", machine_name);
    snprintf(daemons_path, sizeof(daemons_path), "%s/daemons", etc_frr_path);
    snprintf(frrconf_path, sizeof(frrconf_path), "%s/frr.conf", etc_frr_path);

    // Crea le directory se non esistono
    if (!directory_exists(etc_frr_path)) {
        printf("Creazione directory: %s\n", etc_frr_path);
        create_nested_dirs(etc_frr_path);
    }

    // --- Gestione file daemons ---
    FILE *f = fopen(daemons_path, "r");
    if (!f) {
        printf("File daemons non trovato, lo creo da template.\n");
        copy_file(daemons_template_full, daemons_path);
    } else {
        fclose(f);
    }

    // Ora modifica bgpd=yes
    replace_bgpd_line(daemons_path);

    // --- Gestione file frr.conf ---
    f = fopen(frrconf_path, "r");
    if (!f) {
        printf("File frr.conf non trovato, lo creo dal template.\n");
        copy_template_to_frrconf(frr_template_full, frrconf_path);

        // Aggiorna lab.conf con la riga [machine_name][image]=kathara/frr
        snprintf(labconf_path, sizeof(labconf_path), "%s/lab.conf", original_dir);
        snprintf(line_to_append, sizeof(line_to_append), "[%s][image]=kathara/frr", machine_name);
        append_line_to_file(labconf_path, line_to_append);

        // Aggiorna machine_name.startup con systemctl start frr
        snprintf(startup_path, sizeof(startup_path), "%s.startup", machine_name);
        append_line_to_file(startup_path, "systemctl start frr");
        
    } else {
        fclose(f);
        printf("File frr.conf esistente, aggiungo il template all'inizio.\n");
        prepend_template_to_frrconf(frr_template_full, frrconf_path);
    }

    printf("Configurazione completata per la macchina '%s'.\n\n", machine_name);
}

int main() {
    char folder_name[MAX_PATH];
    char machine_name[MAX_PATH];
    char original_dir[MAX_PATH];
    char frr_template_full[MAX_PATH];
    char daemons_template_full[MAX_PATH];

    // Salva la directory iniziale (dove sta lo script e i template)
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        perror("Errore nel determinare la directory corrente");
        return 1;
    }

    printf("Inserisci il nome della cartella principale (es. bgp_announcement): ");
    if (!fgets(folder_name, sizeof(folder_name), stdin)) {
        return 1;
    }
    folder_name[strcspn(folder_name, "\n")] = '\0';

    // Uscita dalla directory del programma
    if (chdir("..") != 0) {
        perror("Errore: impossibile tornare alla directory superiore");
        return 1;
    }

    // Entra nella cartella specificata dall’utente
    if (chdir(folder_name) != 0) {
        perror("Errore: impossibile entrare nella cartella specificata");
        return 1;
    }

    // Percorsi assoluti ai template
    snprintf(frr_template_full, sizeof(frr_template_full), "%s/%s", original_dir, FRR_TEMPLATE_FILE);
    snprintf(daemons_template_full, sizeof(daemons_template_full), "%s/%s", original_dir, DAEMONS_TEMPLATE_FILE);

    // --- Loop di configurazione ---
    while (1) {
        printf("Inserisci il nome della macchina per configurazione BGP (vuoto per terminare): ");
        if (!fgets(machine_name, sizeof(machine_name), stdin)){
            break;
        }
        // Rimuove il newline
        machine_name[strcspn(machine_name, "\n")] = '\0';

        if (strlen(machine_name) == 0) {
            printf("Configurazione terminata.\n");
            break;
        }
        configura_macchina(machine_name, original_dir, frr_template_full, daemons_template_full);
    }

    return 0;
}