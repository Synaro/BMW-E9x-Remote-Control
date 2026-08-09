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

Aucune broche n'est choisie dans le dépôt. Par défaut l'acquisition est
désactivée et toutes les broches valent `-1`. Pour préparer un banc revu :

```powershell
Copy-Item `
  .\config\bench-twai.local.hpp.example `
  .\config\bench-twai.local.hpp
```

Le fichier local est ignoré par Git. Il faut y reporter les quatre broches du
schéma de banc : RX, TX vers la barrière indépendante, silent et inhibition TX,
ainsi que leurs niveaux actifs. Le build refuse une configuration activée avec
broches manquantes, dupliquées ou débit autre que 100/500 kbit/s au moment où
l'acquisition tente de démarrer.

Ces broches sont **BENCH_ONLY**. Elles ne décrivent aucun faisceau BMW validé.

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
