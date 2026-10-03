# Socle ESP32-S3 sûr — Phase 2, étendu en Phases 3 et 3B

## Périmètre livré

La cible embarquée de référence est l'ESP32-S3-DevKitC-1-N8R8 sous **ESP-IDF
5.5.0**. Les 8 Mio de PSRAM de cette variante ne sont pas utilisés par le
firmware actuel et ne constituent pas une dépendance. PlatformIO 6.12.0
fournit exactement cette version afin de conserver
une commande de build unique pour les développeurs et la CI. Le code vérifie
aussi la version à la compilation ; un changement d'ESP-IDF doit donc être une
décision explicite.

La Phase 2 a livré le protocole USB de configuration existant et sa persistance.
La Phase 3 ajoute un récepteur TWAI générique en écoute seule pour bus de banc.
Il reste désactivé par défaut, ne configure aucune broche arbitraire, ne possède
aucun chemin d'émission et ne contient aucun identifiant BMW. Aucune broche
d'actionneur ni aucun bus véhicule ne sont configurés.

## Décision ESP-IDF et erratum TWAI

ESP-IDF est retenu à la place d'Arduino pour disposer des pilotes, de NVS, de
FreeRTOS, des watchdogs et des options de sûreté Espressif sans couche de
compatibilité implicite. La version 5.5.0 est figée par `platformio.ini` et
`src/main.cpp`.

Sur ESP32-S3, le contrôleur peut encore produire des bits dominants lorsqu'il
détecte une erreur en mode listen-only. Espressif fournit le correctif
`CONFIG_TWAI_ERRATA_FIX_LISTEN_ONLY_DOM`, qui force l'état error-passive à
l'initialisation. Le correctif est activé dans `sdkconfig.defaults` et sa
présence dans la configuration générée est vérifiée par la CI. Depuis la Phase
3, les symboles d'installation et de réception sont liés au firmware afin de
permettre le banc ; la CI interdit toujours tout symbole d'émission TWAI et
vérifie explicitement le mode `TWAI_MODE_LISTEN_ONLY`.

Cette option est une défense logicielle future, pas une autorisation de
connexion au véhicule. Le mode silencieux matériel et l'inhibition TX restent
obligatoires.

## Configuration reproductible

- `platformio.ini` fixe la plateforme, la carte et ESP-IDF ;
- la CI fixe PlatformIO 6.1.19 et IntelHex 2.3.0, utilisé pour générer le
  bootloader ;
- `sdkconfig.defaults` fixe la cible, l'optimisation, les watchdogs et les
  paramètres de flash ;
- `partitions.csv` fixe la partition NVS et l'image d'application ;
- le `sdkconfig` généré localement est ignoré afin que les valeurs par défaut
  versionnées restent la source de vérité.

Le firmware utilise la tâche `app_main` fournie par ESP-IDF. Le flux USB reste
borné à 64 octets traités par cycle et les buffers du pilote USB à 256 octets.
Lorsque le banc TWAI est explicitement activé, une tâche RX à pile statique de
4096 octets alimente une file SPSC fixe de 128 trames ; aucune allocation n'a
lieu dans le chemin critique. Les watchdogs d'interruptions et de tâches
surveillent les tâches idle des deux cœurs.

## Stockage

L'ancien adaptateur Arduino/EEPROM est supprimé. `EspIdfNvsSettingsStorage`
utilise la partition explicite `nvs`, le namespace `bmw_remote` et un blob
`settings` de 128 octets. Le journal portable à deux générations, son payload
de 40 octets et le protocole USB ne changent pas.

NVS fournit l'atomicité de sa transaction de commit, tandis que le journal
applicatif conserve la génération précédente et vérifie CRC, schéma et valeurs.
En cas d'erreur d'ouverture, de taille inattendue ou de commit, l'adaptateur
échoue fermé. Il n'efface jamais automatiquement la partition, car un effacement
silencieux masquerait une corruption et détruirait les preuves de diagnostic.

## HAL minimale

`hardware_abstraction.hpp` définit uniquement :

- GPIO avec configuration explicite de l'état sûr avant passage en sortie ;
- temps monotone et délai borné ;
- stockage via le contrat existant `SettingsByteStorage` ;
- un contrat de sûreté TWAI limité au maintien et à la confirmation du silence
  matériel et de l'inhibition TX.

L'adaptateur ESP-IDF implémente GPIO et temps. Le port `CanFrameReceiver` de
`libs/can-core` expose uniquement la réception et les statistiques. Son
implémentation ESP-IDF n'expose aucune opération d'émission.

## Chaîne matérielle future et état sûr

```text
ESP32-S3 -> contrôleur TWAI -> transceiver CAN externe -> bus de banc
    |                              ^
    +--> inhibition TX indépendante+-- mode silencieux matériel
```

La future carte doit satisfaire simultanément les règles suivantes :

1. le transceiver possède une entrée silent/standby matérielle ;
2. une seconde ligne inhibe physiquement TX indépendamment du logiciel TWAI ;
3. des résistances externes imposent silent et TX inhibé pendant boot, reset,
   brownout, GPIO haute impédance et firmware absent ;
4. le firmware ne pourra libérer ces deux barrières qu'après autocontrôle et
   uniquement dans une phase future explicitement autorisée ;
5. un crash ou un reset du microcontrôleur doit ramener le matériel au silence
   sans dépendre de l'exécution d'un handler ;
6. le watchdog accélère le retour au reset, mais ne remplace jamais ces états
   matériels par défaut.

La Phase 3B fige historiquement `S=HIGH`, `/OE=HIGH` et GPIO4/5/6/7 uniquement
pour le banc haute vitesse TCAN1057AV-Q1 + SN74LVC1G125-Q1. Les mêmes broches
restent exposées sur la DevKitC-1-N8R8 actuelle, sans changement de cible
PlatformIO. Ce choix ne s'applique ni au design K-CAN Phase 3F, ni au PCB
final, ni à un bus BMW. Aucun câblage improvisé de DevKit vers un véhicule
n'est autorisé.

## Porte de validation sur banc

Avant toute connexion au véhicule :

1. contrôler hors tension continuité, isolation, résistances de polarisation et
   état par défaut des deux inhibitions ;
2. alimenter sur laboratoire avec limitation de courant, sans bus, et observer
   silent/TXD pendant mise sous tension, reset et brownout ;
3. connecter uniquement un bus CAN de banc avec analyseur et charge factice ;
4. prouver à l'oscilloscope l'absence de bit dominant aux bons et mauvais
   débits, pendant erreurs, saturation, reboot et watchdog ;
5. couper/figer le firmware et vérifier que le transceiver reste silencieux ;
6. archiver schéma, mesures, versions et critères de réussite ;
7. valider les statistiques, l'ordre et les pertes de l'acquisition Phase 3 sur
   ce banc avant toute demande d'étape suivante.

Un échec à une seule étape interdit la connexion au véhicule.
Le schéma mesurable, les seuils et la checklist exhaustive sont définis dans
[phase3b-can-bench-hardware.md](phase3b-can-bench-hardware.md).

## Références primaires

- [ESP-IDF 5.5 — documentation TWAI](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/twai.html)
- [Option Espressif du correctif listen-only](https://docs.espressif.com/projects/esp-idf/en/v5.1.2/esp32s3/api-reference/kconfig.html#config-twai-errata-fix-listen-only-dom)
- [Rapport Espressif du défaut et correctif](https://github.com/espressif/esp-idf/issues/9157)
- [NVS Flash API](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/storage/nvs_flash.html)
