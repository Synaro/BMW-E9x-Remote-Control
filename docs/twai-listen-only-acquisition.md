# Acquisition TWAI listen-only — Phase 3

## Limite de sécurité

Cette phase fournit une chaîne de réception générique pour **bus de banc
uniquement**. Elle n'autorise aucune connexion au véhicule. Elle ne contient ni
identifiant BMW, ni décodeur BMW, ni fonction véhicule, ni API d'émission.

## Architecture

```text
TWAI_MODE_LISTEN_ONLY
        |
        v
file RX ESP-IDF (1..256, défaut 64)
        |
        v
tâche statique twai_rx_only (pile 4096 octets)
        |
        v
CanFrame + timestamp_us + sequence
        |
        v
BoundedSpscQueue<CanFrame, 128>
        |
        v
CanFrameReceiver::tryReceive()
```

`libs/can-core` ne dépend ni d'ESP-IDF ni de BMW. Il porte `CanFrame`, l'interface
RX, les statistiques, la file SPSC fixe et le contrat Capture V2. L'ancien
en-tête d'infrastructure est désormais un alias de compatibilité vers ce modèle
unique.

L'adaptateur `EspIdfTwaiReceiver` appartient à l'infrastructure. Son en-tête
public n'inclut pas `driver/twai.h` et n'expose aucune méthode TX. Le pilote
historique ESP-IDF 5.5 est volontairement utilisé car la contrainte impose
exactement `TWAI_MODE_LISTEN_ONLY`; il fournit aussi `twai_receive()`, les
alertes et `twai_get_status_info()`. La file TX du pilote est explicitement fixée
à zéro.

## Démarrage fermé par défaut

L'ordre est impératif :

1. valider les pins, le débit, la profondeur et la tâche ;
2. imposer le mode silent matériel ;
3. imposer l'inhibition TX indépendante ;
4. relire les deux niveaux et confirmer leur état ;
5. installer TWAI en `TWAI_MODE_LISTEN_ONLY` avec file TX nulle ;
6. démarrer le contrôleur, puis la tâche RX statique.

Tout échec avant l'étape 5 interdit l'installation du pilote. Aucun chemin ne
libère les barrières matérielles. Le correctif
`CONFIG_TWAI_ERRATA_FIX_LISTEN_ONLY_DOM=y` reste obligatoire à la compilation.

## Configuration BENCH_ONLY

La Phase 3B propose GPIO4/5/6/7 uniquement pour la DevKitC-1-N8 et son schéma
TCAN1057AV-Q1 + SN74LVC1G125-Q1. L'acquisition reste désactivée par défaut. Pour
préparer ce banc après revue électrique :

```powershell
Copy-Item `
  .\config\bench-twai.local.hpp.example `
  .\config\bench-twai.local.hpp
```

Le fichier local est ignoré par Git. L'exemple contient les broches de Phase 3B
mais conserve `BMW_REMOTE_BENCH_ONLY_TWAI_ENABLED=0`. Ne le passer à `1` que
pendant la procédure de qualification. L'acquisition refuse les broches
manquantes/dupliquées et les débits autres que 100/500 kbit/s au démarrage.

Ces broches sont **BENCH_ONLY**. Elles ne décrivent aucun faisceau BMW validé.
Voir [phase3b-can-bench-hardware.md](phase3b-can-bench-hardware.md).

Cette configuration ne doit pas être recyclée pour le K-CAN. Le TCAN1057AV
utilise ISO 11898-2 et dispose d'un mode silent matériel ; le TJA1055T/3
low-speed/fault-tolerant doit être en mode normal pour fournir les données RX.
Le futur profil K-CAN devra donc confirmer un trajet TX physiquement ouvert au
lieu de prétendre que standby est un mode silent de capture. La décision et les
essais requis sont décrits dans
[phase3e-kcan-passive-acquisition.md](phase3e-kcan-passive-acquisition.md).

## Bornes et statistiques

- aucune allocation après le démarrage dans le chemin de réception ;
- pile de tâche statique : 4096 octets ;
- file logicielle SPSC : 128 trames ;
- file RX du pilote : 64 trames par défaut, maximum accepté 256 ;
- attente de réception : 10 ms par défaut, bornée à 100 ms ;
- timestamp : `esp_timer_get_time()`, monotone à résolution microseconde ;
- séquence attribuée avant insertion afin de rendre les pertes visibles ;
- compteurs séparés pour queue logicielle, queue pilote, FIFO, erreurs bus,
  transitions warning/passive/bus-off et resets périphériques.

Les trames restent ordonnées par l'unique producteur et l'unique consommateur.
Le format d'archivage est défini dans
[capture-format-v2.md](capture-format-v2.md). Son transport automatique vers le
PC n'est pas commencé dans cette phase.

## Références constructeur

- [ESP-IDF 5.5 — migration et choix des pilotes TWAI](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/migration-guides/release-5.x/5.5/peripherals.html)
- [ESP-IDF — pilote TWAI ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/peripherals/twai.html)
- [ESP Timer — horodatage microseconde](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/esp_timer.html)
