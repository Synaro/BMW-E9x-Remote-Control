# Matériel de référence V1

Ce document fige la cible du **prototype de banc**. Il ne constitue ni un
schéma d'installation, ni une autorisation de connecter le montage au véhicule.
La carte définitive destinée à rester dans la voiture sera une carte automobile
dédiée, conçue après la qualification des bus et des actionneurs.

## Décision

La carte de développement de référence est :

- **Espressif ESP32-S3-DevKitC-1-N8R8** ;
- module avec antenne PCB, 8 Mio de flash et 8 Mio de PSRAM ;
- identifiant PlatformIO `esp32-s3-devkitc-1` ;
- environnement du dépôt `esp32s3dev`.

Le modèle `N8` était la sélection initiale. Sa référence distributeur n'est plus
retenue et `N8R8` est désormais la carte d'achat normative de
`hardware/kcan-rxonly/procurement.csv`. Les 8 Mio de PSRAM supplémentaires ne
sont pas requis ni utilisés par le firmware actuel. Ce changement commercial ne
modifie ni l'environnement PlatformIO, ni les GPIO4/GPIO5/GPIO7 déjà revus.
Les cartes génériques simplement vendues sous le nom « ESP32-S3 » ne sont pas la
cible de référence : brochage, régulateur, USB et qualité d'assemblage peuvent
varier.

Cette sélection apporte un environnement largement disponible, un port USB
natif, suffisamment de mémoire, le Wi-Fi et le Bluetooth LE pour des évolutions
éventuelles, ainsi qu'un contrôleur CAN classique intégré sous le nom TWAI.
L'application V1 ne dépend toutefois ni du Wi-Fi, ni du Bluetooth.

## Ce qui peut être acheté pour la première étape

La première étape n'utilise aucun signal du véhicule. La liste minimale est :

| Quantité | Élément | Exigence |
| ---: | --- | --- |
| 1 | ESP32-S3-DevKitC-1-N8R8 | Carte Espressif officielle retenue par le procurement Phase 3F |
| 1 | Câble USB de données | Connecteur adapté à la révision reçue, pas un câble de charge seule |
| 1 | Plaque d'essai | Format compatible avec la largeur de la carte |
| 1 jeu | Fils Dupont | Pour les futures charges logiques de banc uniquement |

Un ordinateur suffit à alimenter et programmer cette première maquette par
USB. Le procurement Phase 3F dans
[`../hardware/kcan-rxonly/procurement.csv`](../hardware/kcan-rxonly/procurement.csv)
est désormais historique. La révision destinée à une future fabrication est
la Phase 3H dans `hardware/kcan-bidirectional-pcb/`; aucun achat ni commande
fabricant n'est autorisé par cette documentation. **Aucun câble du véhicule et
aucune alimentation automobile 12 V ne doivent être raccordés.**

## Architecture de communication prévue

La documentation BMW distingue :

- le K-CAN à 100 kbit/s, capable de continuer sur un seul fil en cas de défaut ;
- le PT-CAN à 500 kbit/s, utilisant une couche physique CAN haute vitesse.

La cible officielle utilise exclusivement le contrôleur **TWAI interne de
l'ESP32-S3** avec un transceiver CAN externe adapté à la couche physique du bus
observé. Le profil Phase 3B historique conserve ses barrières matérielles. Le
profil Phase 3H configure le TJA1055 via STB/EN et permet au contrôleur de
sélectionner `TWAI_MODE_LISTEN_ONLY` ou `TWAI_MODE_NORMAL`. Il reste désactivé
sans configuration locale `BENCH_ONLY` et n'expose aucune API d'émission.

```text
ESP32-S3 -- contrôleur TWAI interne -- interface 3V3/5V -- TJA1055 -- K-CAN banc
 GPIO5/TX -------------------------------------------> TXD
 GPIO4/RX <------------------------------------------- RXD
```

Un seul bus sera étudié à la fois en écoute seule. Si l'observation simultanée de
plusieurs bus devient un besoin validé, elle fera l'objet d'une nouvelle décision
d'architecture ; aucun second contrôleur CAN n'est retenu aujourd'hui.

Le banc haute vitesse historique de Phase 3B retient précisément :

- `TCAN1057AVDRQ1`, alimenté en 5 V avec `VIO=3,3 V`, en Silent permanent ;
- `CLVC1G125QDBVRQ1` (`SN74LVC1G125-Q1`) comme inhibition physique TXD ;
- GPIO4/5/6/7 uniquement sur la DevKitC-1 revue et uniquement en `BENCH_ONLY` ;
- pulls externes 10 kΩ sur `S`, `/OE` et `TXD`.

Ce choix documente uniquement une couche physique ISO 11898-2 haute vitesse.
Il ne convient pas à la couche physique K-CAN basse vitesse tolérante aux
défauts. La Phase 3F a ensuite gelé un prototype RX-only autour du
`TJA1055T/3/2Z`; cette révision et la Phase 3G.1 sont conservées comme historique.
La Phase 3H garde le TJA1055 mais remplace le buffer par un traducteur
`SN74LXC1T45-Q1` double alimentation et route TX en continu. Elle n'est pas
encore qualifiée électriquement et ne vaut pas qualification véhicule. Voir
[phase3h-bidirectional-kcan.md](phase3h-bidirectional-kcan.md).
`LM5164-Q1` reste une base
d'étude de l'alimentation automobile finale, absente du banc 5 V.

Le watchdog ne remplace pas la sûreté électrique. Les protections, filtres,
circuits de réveil et mesures de courant seront revus sur le schéma final.

## Déroulement matériel sûr

### Phase A — carte seule sur USB

1. Compiler et flasher le firmware inerte.
2. Vérifier l'identification USB et l'apparition du port COM.
3. Vérifier le protocole de configuration déjà raccordé au port USB.
4. Tester les débranchements, trames partielles et redémarrages.

Lorsque la carte sera disponible, le premier parcours automatisé sera :

```powershell
.\scripts\configure.ps1 -ListDevices
.\scripts\first-usb-test.ps1 -Port COM3
```

Le second script compile, flashe, attend la réapparition du port, vérifie la
signature `E9RC`, écrit une configuration de banc avec démarrage distant
désactivé, puis la relit entièrement. Le numéro `COM3` est un exemple et doit
être remplacé par le port apparu lors du branchement.

Le port USB est réservé au protocole binaire : le firmware n'y mélange aucun
message de journalisation. Sur la cible de banc, la possession de l'accès
physique à ce connecteur constitue l'autorisation locale et le contrôleur reste
figé dans l'état applicatif `Idle`. Ce modèle devra être réévalué lorsque les
fonctions véhicule seront activées.

Sur une DevKitC-1 qui possède deux connecteurs, utiliser celui identifié
`ESP32-S3 USB` ou `USB`, relié directement au microcontrôleur, et non le port
`USB-to-UART`. Le numéro COM peut changer après le premier flashage ; dans ce
cas, le premier test tente d'adopter automatiquement l'unique nouveau port. En
utilisation manuelle, relancer `-ListDevices` et reprendre avec ce numéro.

### Phase B — qualification sur bus de banc isolé

1. Assembler uniquement une Phase 3H dont les fichiers de fabrication ont été
   vérifiés contre le manifeste SHA-256 ; aucune commande n'a encore été passée.
2. Inspecter hors tension la continuité TX/RX, les valeurs, les deux domaines
   d'alimentation et l'absence de liaison entre VBUS DevKit et `5V_TJA`.
3. Utiliser une alimentation de laboratoire limitée en courant, sans véhicule.
4. Exécuter une procédure Phase 3H dédiée : alimentation, reset, brownout,
   firmware absent, mode listen-only, mode normal sans appel TX, niveaux
   STB/EN/TXD et réception LS/FT.
5. Archiver les formes d'onde et obtenir les revues prévues.
6. Conserver le statut global `FAIL` tant qu'une seule ligne manque.

Le montage haute vitesse TCAN1057AV-Q1 de Phase 3B reste documenté comme banc
distinct ; il n'est ni le procurement K-CAN actuel, ni un équivalent de la
couche physique ISO 11898-3.

### Phase C — observation passive du véhicule, non autorisée

Cette phase n'est pas ouverte. Même après un PASS de banc complet, une nouvelle
autorisation et une décision de couche physique adaptée au bus visé seront
requises. Ni la Phase 3B ni le gel Phase 3F ne valident un câblage K-CAN ou
PT-CAN BMW.

Les actionneurs et le démarrage restent hors périmètre tant que cette phase n'a
pas produit de résultats reproductibles.

Les critères détaillés et la décision ESP-IDF sont dans
[esp32s3-safe-foundation.md](esp32s3-safe-foundation.md). Un échec de validation
sur banc interdit toute connexion au véhicule. Le schéma et les marges de la
révision actuelle sont décrits dans
[phase3h-bidirectional-kcan.md](phase3h-bidirectional-kcan.md) et
[`../hardware/kcan-bidirectional-pcb/README.md`](../hardware/kcan-bidirectional-pcb/README.md).
Les fichiers Phase 3F/3G restent historiques. Le banc haute vitesse reste dans
[phase3b-can-bench-hardware.md](phase3b-can-bench-hardware.md).

## Pourquoi la carte de développement ne restera pas dans le véhicule

Une DevKit facilite le développement, mais elle ne fournit pas à elle seule :

- la protection contre inversion, surtension, transitoires et décharges ESD ;
- une consommation de veille maîtrisée pour un branchement permanent ;
- des composants et connecteurs qualifiés pour l'environnement automobile ;
- la couche physique CAN adaptée et qualifiée ;
- un watchdog matériel indépendant et des sorties maintenues inactives au reset ;
- une fixation, un boîtier et un faisceau adaptés aux vibrations et à la chaleur.

La carte finale reprendra donc un module ESP32-S3 approprié sur un PCB dédié,
avec alimentation, interfaces, protections et interverrouillages qualifiés.

## Références constructeur

- [Guide ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html)
- [Fiche technique ESP32-S3](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)
- [BMW Body Electronics II — Bus Systems](https://bmwtechinfo.bmwgroup.com/tech_training_manual/ST401%20Body%20Electronics%20II.pdf)
- [TI TCAN1057AV-Q1](https://www.ti.com/product/TCAN1057A-Q1)
- [TI SN74LVC1G125-Q1](https://www.ti.com/product/SN74LVC1G125-Q1)
- [NXP TJA1055, transceiver K-CAN Phase 3H](https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf)
- [TI SN74LXC1T45-Q1](https://www.ti.com/lit/ds/symlink/sn74lxc1t45-q1.pdf)
- [TI LM5164-Q1](https://www.ti.com/product/LM5164-Q1)
