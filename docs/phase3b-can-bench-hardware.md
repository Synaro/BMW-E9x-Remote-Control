# Phase 3B — Matériel CAN de banc et qualification électrique

## Statut et limite

Ce document fige un **banc CAN haute vitesse classique** pour qualifier la
réception passive de Phase 3. Il ne constitue ni un schéma automobile final, ni
une autorisation de connexion à une BMW. Toutes les cases de la checklist de
qualification doivent être `PASS`, avec mesures archivées, avant qu'une demande
de connexion véhicule puisse seulement être examinée.

Le banc accepte 100 kbit/s et 500 kbit/s sur une couche physique
ISO 11898-2 haute vitesse. Un essai haute vitesse à 100 kbit/s ne reproduit pas
la couche physique K-CAN basse vitesse tolérante aux défauts. Le matériel retenu
ici ne doit donc jamais être présenté comme un adaptateur K-CAN.

## Décision du transceiver

La référence retenue est **Texas Instruments TCAN1057AVDRQ1** : variante
`TCAN1057AV-Q1`, boîtier SOIC-8 `D`, qualifiée automobile. `VCC` est alimenté en
5 V et `VIO` en 3,3 V.

### Comparaison constructeur

| Référence | Interface 3,3 V | Réception avec TX matériellement coupé | État flottant/hors tension | Décision |
|---|---|---|---|---|
| TI `TCAN1057AV-Q1` | `VIO` 1,7 à 5,5 V | `S=HIGH` : driver désactivé, récepteur actif | pull-up interne sur `S` et `TXD`, bus et logique haute impédance hors tension | **Retenu** : simple SOIC-8, AEC-Q100 Grade 1, actif et documenté |
| NXP `TJA1043` | `VIO` pour MCU 3 à 5 V | véritable mode listen-only | comportement passif hors tension et fonctions fail-safe | techniquement valable, mais 14 broches et gestion d'alimentation plus complexe ; NXP recommande une génération suivante |
| Microchip `MCP2558FD` | `VIO` 1,8 à 5,5 V | `S=HIGH` : driver désactivé, récepteur actif | pull-up interne sur `S`, bus déconnecté hors tension | alternative crédible, documentation et produit plus anciens |
| Microchip `ATA6564` | `VIO` compatible 3,3/5 V | véritable Silent avec récepteur actif | `TXD` tiré haut mais `S` tiré **bas**, donc Normal si laissé flottant | écarté : la polarité interne de `S` est moins sûre pour notre état par défaut |

Le TCAN1057AV-Q1 satisfait ISO 11898-2:2016, le CAN classique et CAN FD, est
AEC-Q100 Grade 1 et protège le bus contre les défauts jusqu'à ±58 V. Son tableau
de modes est sans ambiguïté : `S=HIGH` désactive le driver et maintient le
récepteur actif. TI indique aussi que `S` et `TXD` sont tirés en interne vers
l'état sûr et que le bus reste haute impédance hors tension. Ces protections
internes restent des compléments : les résistances externes ci-dessous sont
obligatoires.

Références primaires :

- [TI TCAN1057A(V)-Q1, datasheet Rev. C](https://www.ti.com/lit/gpn/TCAN1057A-Q1)
- [TI TCAN1057AVDRQ1, statut et boîtier](https://www.ti.com/product/TCAN1057A-Q1/part-details/TCAN1057AVDRQ1)
- [NXP TJA1043, datasheet](https://www.nxp.com/docs/en/data-sheet/TJA1043.pdf)
- [Microchip MCP2557FD/8FD, datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/20005533A.pdf)
- [Microchip ATA6564, datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/ATA6564-High-Speed-CAN-Transceiver-with-Silent-Mode-20005784D.pdf)

## Deux barrières matérielles indépendantes

La sûreté ne repose pas sur `TWAI_MODE_LISTEN_ONLY`.

1. **Barrière transceiver** : l'entrée `S` du TCAN1057AV-Q1 est maintenue haute
   par une résistance externe. Le driver CAN est alors coupé à l'intérieur du
   transceiver, tandis que RXD continue de reproduire le bus.
2. **Barrière TXD indépendante** : un `SN74LVC1G125-Q1` automobile est inséré
   entre `TWAI_TX` et `TXD`. Son entrée `/OE` est maintenue haute par une autre
   résistance ; sa sortie est alors haute impédance et un pull-up distinct force
   `TXD` au niveau récessif.

Une panne logicielle devrait donc vaincre deux composants, deux signaux de
commande et trois résistances de rappel séparés avant qu'un niveau dominant
puisse atteindre le bus. La perte du 3,3 V fait en plus tomber `VIO` sous son
seuil d'undervoltage et place le transceiver en état protégé.

Le buffer retenu est le **TI SN74LVC1G125-Q1**, référence commandable
`CLVC1G125QDBVRQ1` en SOT-23-5. Il est AEC-Q100 Grade 1, fonctionne à 3,3 V,
dispose de `Ioff` contre le back-powering et TI exige précisément de tirer `/OE`
à `VCC` pour garantir la haute impédance pendant power-up/down.

- [TI SN74LVC1G125-Q1, datasheet](https://www.ti.com/lit/ds/symlink/sn74lvc1g125-q1.pdf)

## Schéma de principe

```text
 Alimentation de labo 5,0 V limitée à 300 mA
             |
             +-------------------> ESP32-S3-DevKitC-1, pin 5V
             |
             +---- Cbulk 4,7 µF ---> U1 TCAN1057AV pin 3 VCC
                                      |-- C1 100 nF vers GND

 ESP32-S3 3V3 ----------------------> U1 pin 5 VIO
      |                               |-- C2 100 nF vers GND
      +-----------------------------> U2 SN74LVC1G125-Q1 VCC
                                      |-- C3 100 nF vers GND

 GPIO5 / TWAI_TX ---> U2.A
                |-- R4 100 kΩ vers 3V3

 GPIO7 / TX_INHIBIT -- R5 1 kΩ ---> U2./OE
                                      |-- R2 10 kΩ vers 3V3
 U2.Y ------------------------------> U1 pin 1 TXD
                                      |-- R3 10 kΩ vers 3V3

 GPIO6 / SILENT ----- R6 1 kΩ ------> U1 pin 8 S
                                      |-- R1 10 kΩ vers 3V3

 U1 pin 4 RXD ----------------------> GPIO4 / TWAI_RX

 U1 pin 7 CANH ---------------------+------ CANH banc
                                    |
                                  JP_TERM
                                    |
                                  RTERM 120 Ω, 1 %
                                    |
 U1 pin 6 CANL ---------------------+------ CANL banc

 U1 pin 2, U2 GND, ESP32 GND -------------- GND commun de banc
```

`JP_TERM` est ouvert si ce nœud n'est pas à une extrémité. Sur un banc à deux
extrémités, une résistance de 120 Ω est placée à chaque extrémité, soit environ
60 Ω mesurés entre CANH et CANL hors tension. Aucun de ces éléments ne doit être
monté sur plaque d'essai sans soudure pour les essais à 500 kbit/s : utiliser un
petit PCB ou un adaptateur SOIC soudé avec pistes courtes et paire torsadée.

Pour le boîtier SOT-23-5 `DBV` du buffer U2 : pin 1=`/OE`, pin 2=`A`,
pin 3=`GND`, pin 4=`Y`, pin 5=`VCC`. Pour le SOIC-8 `D` du transceiver U1 :
pin 1=`TXD`, 2=`GND`, 3=`VCC`, 4=`RXD`, 5=`VIO`, 6=`CANL`, 7=`CANH`,
8=`S`. Vérifier à nouveau ces pinouts sur les datasheets au moment de créer le
PCB ; les vues constructeur sont des vues de dessus.

### Valeurs de sûreté figées pour le banc

| Référence | Valeur | Rôle |
|---|---:|---|
| R1 | 10 kΩ, 1 % | force `S=HIGH`, donc Silent, pendant boot/reset |
| R2 | 10 kΩ, 1 % | force `/OE=HIGH`, donc buffer TX désactivé |
| R3 | 10 kΩ, 1 % | force `TXD=HIGH`, donc récessif, buffer désactivé |
| R4 | 100 kΩ, 1 % | évite une entrée `A` flottante sans être une barrière |
| R5, R6 | 1 kΩ, 1 % | limite les conflits/transitoires sur les commandes |
| RTERM | 120 Ω, 1 %, 0,25 W | terminaison commutable d'une extrémité du banc |
| C1, C2, C3 | 100 nF X7R | découplage au plus près de chaque alimentation |
| Cbulk | 4,7 µF X7R, ≥10 V | réserve locale sur le rail 5 V |

Ne pas remplacer les pulls de 10 kΩ par les pulls internes de l'ESP32 ou du
transceiver. Les GPIO ESP32 sont haute impédance pendant reset ; l'état sûr est
produit par le matériel externe.

## GPIO BENCH_ONLY de la DevKitC-1-N8

| Fonction | GPIO | Connecteur | Justification |
|---|---:|---|---|
| TWAI RX | 4 | J1-4 | GPIO général exposé, ni strap, ni USB, ni flash |
| TWAI TX avant barrière | 5 | J1-5 | GPIO général exposé, ni strap, ni USB, ni flash |
| Silent `S` | 6 | J1-6 | GPIO général ; R1 impose HIGH quand il est haute impédance |
| TX inhibit `/OE` | 7 | J1-7 | GPIO général ; R2 impose HIGH quand il est haute impédance |

Ce choix évite :

- les straps GPIO0, GPIO3, GPIO45 et GPIO46 ;
- GPIO19/GPIO20 réservés à l'USB natif ;
- GPIO26 à GPIO37 liés ou potentiellement liés à la flash/PSRAM ;
- GPIO43/GPIO44 utilisés par l'UART0 ;
- GPIO38 de la LED RGB de la DevKitC-1.

Les quatre broches sont contiguës sur J1 et reviennent en entrée haute impédance
au reset. Les pulls externes, et non leur état interne, définissent la sûreté.
Ce brochage est **uniquement BENCH_ONLY** pour l'ESP32-S3-DevKitC-1-N8 ; il ne
préjuge ni du PCB final ni d'un faisceau BMW.

Sources Espressif :

- [ESP32-S3-DevKitC-1 v1.1, connecteurs et alimentation](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)
- [Restrictions GPIO ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html)
- [Schéma officiel DevKitC-1 v1.1](https://dl.espressif.com/dl/schematics/PCB_ESP32-S3-DevKitC-1_V1.1_20220429.pdf)
- [Datasheet ESP32-S3](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)

## Points de mesure obligatoires

| Point | Signal |
|---|---|
| TP1 | 5 V au VCC du transceiver |
| TP2 | 3,3 V / VIO |
| TP3 | `TWAI_TX` avant U2 |
| TP4 | `/OE` de U2 |
| TP5 | `TXD` après U2 |
| TP6 | `S` du transceiver |
| TP7 | `RXD` |
| TP8, TP9 | CANH et CANL, avec mesure différentielle CANH−CANL |
| TP10 | `EN/RESET` ESP32 |
| TP11 | masse courte réservée aux sondes |

Employer de préférence une sonde différentielle. À défaut, utiliser deux
sondes appairées avec masses très courtes et la fonction mathématique CH1−CH2.
Ne jamais relier la pince de masse d'un oscilloscope secteur à CANH ou CANL.

## Procédure de qualification électrique

Chaque essai doit enregistrer : date, opérateur, révisions carte/PCB, numéros de
lot U1/U2, firmware et configuration, instruments, captures d'écran, débit,
température ambiante et résultat PASS/FAIL.

### 1. Contrôle hors tension

1. Déconnecter USB, alimentation et générateur CAN.
2. Vérifier polarité, absence de court-circuit et valeurs R1 à R6.
3. Vérifier `S -> 3V3 = 10 kΩ`, `/OE -> 3V3 = 10 kΩ` et
   `TXD -> 3V3 = 10 kΩ`, hors tolérances dues aux circuits internes.
4. Vérifier la terminaison : jumper ouvert sans terminaison locale ; jumper
   fermé environ 120 Ω seule, puis 57 à 63 Ω avec les deux extrémités du banc.

### 2. Alimentation nominale sans bus

1. Débrancher tout USB et alimenter le pin 5 V par une alimentation de labo,
   limite initiale 100 mA, puis augmenter au besoin sans dépasser 300 mA.
2. Monter progressivement de 0 à 5,0 V puis effectuer dix mises sous/hors
   tension franches.
3. Mesurer VCC, VIO, `S`, `/OE` et `TXD` avant, pendant et après le boot.
4. Vérifier `S`, `/OE` et `TXD` hauts dès que VIO devient valide.

### 3. Reset et firmware absent

1. Répéter 100 impulsions sur `EN/RESET`, dont dix maintiens de 10 secondes.
2. Refaire l'essai avec une DevKit vierge ou en maintenant l'ESP32 en reset ;
   ne pas dépendre d'une instruction exécutée par le firmware.
3. Observer TP3 à TP6 et CANH−CANL en déclenchement single-shot.

### 4. Brownout et extinction

1. Sans USB, balayer le 5 V de 5,0 V vers 0 V puis retour, lentement
   (environ 0,1 V/s) et rapidement (<1 ms).
2. Ajouter des creux 5 V→3 V→5 V de 1 ms, 10 ms, 100 ms et 1 s.
3. Couper uniquement le 3,3 V/VIO si le montage de test le permet, sans forcer
   de tension dans la DevKit.
4. Maintenir un trafic CAN externe pendant les descentes et remontées.

### 5. Bus actif et absence d'ACK

1. Utiliser un générateur CAN connu avec émission single-shot et une paire
   torsadée terminée aux deux extrémités.
2. Tester d'abord 100 kbit/s, puis 500 kbit/s, pendant au moins 10 minutes.
3. Comparer RXD à CANH−CANL : le récepteur doit suivre les bits dominants.
4. Envoyer une trame valide avec le générateur comme seul émetteur actif. Le
   générateur doit signaler une erreur d'ACK et l'ACK slot doit rester récessif.
5. Refaire l'essai pendant boot, reset et extinction du nœud passif.

### 6. Mauvais débit et erreurs CAN

1. Configurer le récepteur à 100 kbit/s sur un bus à 500 kbit/s, puis l'inverse.
2. Injecter sur le banc des erreurs de bit, stuffing et CRC au moyen d'un outil
   prévu pour l'injection d'erreurs CAN.
3. Observer pendant au moins 1 000 événements de chaque type.
4. Vérifier qu'aucun flag d'erreur, ACK ou dominant ne provient du nœud passif.

### 7. Saturation RX

1. Générer la charge maximale soutenable à 500 kbit/s, DLC variés et intervalle
   inter-trame minimal, pendant 30 minutes.
2. Ne pas consommer volontairement la file applicative pendant une partie de
   l'essai afin de provoquer ses compteurs de perte.
3. Vérifier que la saturation reste un phénomène RX : aucune transition sur
   `S`, `/OE`, `TXD` ou émission CAN ne doit apparaître.

### 8. Watchdog et blocage firmware

1. Utiliser un binaire de qualification temporaire, non fusionné au produit,
   qui bloque volontairement le cœur surveillé jusqu'au watchdog, ou un moyen
   de fault-injection validé équivalent.
2. Vérifier la séquence blocage→watchdog→reset→boot sur au moins 100 cycles.
3. Le test reste `NOT RUN`, donc globalement non qualifié, tant que ce moyen
   d'injection et les mesures associées n'existent pas.

## Critères PASS/FAIL

Un essai est `PASS` uniquement si tous les critères applicables sont vrais :

- VCC reste entre 4,5 V et 5,5 V et VIO entre 3,2 V et 3,4 V en régime établi ;
- l'alimentation ne reste pas en limitation et aucun composant ne chauffe
  anormalement ;
- `S >= 0,7 × VIO`, `/OE >= 0,7 × 3,3 V` et `TXD >= 0,7 × VIO` pendant tous
  les états où VIO est valide ;
- sur bus au repos, CANH−CANL reste dans la zone récessive et aucun pulse
  différentiel positif d'amplitude ≥0,5 V et de durée ≥50 ns n'est corrélé au
  nœud testé ;
- sur bus actif, aucun ACK, flag d'erreur ou dominant supplémentaire n'est
  attribuable au nœud passif ;
- le générateur single-shot constate l'absence d'ACK quand aucun autre nœud
  acquittant n'est présent ;
- RXD reproduit le bus aux deux débits corrects ;
- mauvais débit, erreurs, saturation, reset, brownout, extinction, firmware
  absent et watchdog ne libèrent jamais l'une des deux barrières ;
- nœud hors tension : aucune erreur supplémentaire et aucune dégradation de
  forme d'onde supérieure à 5 % par rapport au nœud physiquement débranché ;
- 100 cycles de reset, 100 cycles watchdog et 10 cycles complets d'alimentation
  sont archivés sans événement interdit.

Tout dominant inexpliqué, mesure manquante, test `NOT RUN` ou résultat ambigu
vaut **FAIL**. Un FAIL bloque toute connexion véhicule ; il n'est jamais
transformé en avertissement ou dérogation logicielle.

La checklist à renseigner se trouve dans
[templates/can-bench-qualification-checklist.md](templates/can-bench-qualification-checklist.md).

## Liste du matériel nécessaire

### Nœud passif

- 1 × ESP32-S3-DevKitC-1-N8 officielle ;
- 2 à 5 × `TCAN1057AVDRQ1` pour assemblage et rechange ;
- 2 à 5 × `CLVC1G125QDBVRQ1` (`SN74LVC1G125-Q1`) ;
- PCB/adaptateur soudé, connecteurs, points de test et jumper de terminaison ;
- résistances 10 kΩ, 100 kΩ, 1 kΩ et 120 Ω, 1 %, valeurs détaillées plus haut ;
- condensateurs X7R 100 nF et 4,7 µF ;
- paire torsadée CAN courte et connecteurs détrompés ;
- câble USB de données, uniquement lorsque l'alimentation de labo est retirée
  ou isolée conformément au schéma de la DevKit.

### Instruments et génération du bus

- alimentation de laboratoire 0–6 V, réglable, limitée en courant et capable de
  rampes/creux contrôlés ;
- multimètre ;
- oscilloscope ≥100 MHz et ≥1 Géch/s ;
- sonde différentielle CAN ou deux sondes appairées ;
- analyseur logique 3,3 V pour `S`, `/OE`, `TXD`, `RXD` et `RESET` ;
- générateur/analyseur CAN haute vitesse supportant 100/500 kbit/s, émission
  single-shot et rapport d'ACK ;
- outil CAN de banc capable d'injecter des erreurs de bit/stuffing/CRC ;
- second nœud connu et sa terminaison 120 Ω ;
- poste de soudure, loupe/microscope et protections ESD.

Les modules CAN anonymes préassemblés sont exclus : leur transceiver exact,
leur mode `S`, leurs pulls et leur terminaison sont rarement garantis.

Ce banc ne qualifie ni une alimentation 12 V automobile, ni les protections
load-dump/inversion/ESD du futur calculateur. Ces fonctions restent hors
périmètre et devront être conçues sur le PCB automobile final.
