# Laboratori di Reti con Kathará

Questo repository raccoglie una serie di **laboratori guida** ed **esercizi d’esame** del corso di **Internet e Data Centers** dell’**Università degli Studi Roma Tre**, eseguiti utilizzando [**Kathará – Lightweight Container-based Network Emulation System**](https://www.kathara.org/).

---

## _LabMaker – Generatore automatico di laboratori

All’interno della cartella `_LabMaker` è presente lo script **`labmaker.c`**, uno strumento che consente di creare automaticamente la struttura di un nuovo laboratorio Kathará, riducendo il tempo necessario per l’impostazione manuale di file e directory.

### Funzionalità principali

Lo script `labmaker.c` permette di:
- specificare i **nodi** della topologia (host, router, switch, ecc.);
- abilitare i **protocolli di routing** desiderati *(attualmente supportati: RIP, OSPF)*;
- generare automaticamente:
  - la struttura di directory del laboratorio;
  - il file `lab.conf` con la definizione delle **LAN** e delle interfacce;
  - i file `.startup` per ciascun nodo, con gli **indirizzi IP** preconfigurati;
  - i file di configurazione **FRR**, ottenuti copiando e adattando template predefiniti.

### Obiettivo

Lo scopo dello script è **automatizzare la creazione iniziale di un laboratorio Kathará**, evitando la noiosa generazione manuale di:
- cartelle,
- file di configurazione di base,
- impostazioni di rete e routing.

Il risultato è una **prima versione del laboratorio** pronta per essere personalizzata e ampliata.

Modificare i file generati:
- impostare le LAN per ciascun ethernet nel file lab.conf del laboratorio;
- definire gli indirizzi IP per ogni interfaccia nei file .startup dei nodi;
- personalizzare i file di configurazione FRR.
