# Matériel de référence V1

Ce document fige la cible du **prototype de banc**. Il ne constitue ni un
schéma d'installation, ni une autorisation de connecter le montage au véhicule.
La carte définitive destinée à rester dans la voiture sera une carte automobile
dédiée, conçue après la qualification des bus et des actionneurs.

## Décision

La carte de développement de référence est :

- **Espressif ESP32-S3-DevKitC-1-N8** ;
- module avec antenne PCB, 8 Mo de flash et sans PSRAM ;
- identifiant PlatformIO `esp32-s3-devkitc-1` ;
- environnement du dépôt `esp32s3dev`.

Le modèle `N8R8` reste compatible pour les essais si le `N8` est indisponible,
mais sa PSRAM n'est pas requise. Les cartes génériques simplement vendues sous
le nom « ESP32-S3 » ne sont pas la cible de référence : brochage, régulateur,
USB et qualité d'assemblage peuvent varier.

Cette sélection apporte un environnement largement disponible, un port USB
natif, suffisamment de mémoire, le Wi-Fi et le Bluetooth LE pour des évolutions
éventuelles, ainsi qu'un contrôleur CAN classique intégré sous le nom TWAI.
L'application V1 ne dépend toutefois ni du Wi-Fi, ni du Bluetooth.

## Ce qui peut être acheté pour la première étape

La première étape n'utilise aucun signal du véhicule. La liste minimale est :

| Quantité | Élément | Exigence |
| ---: | --- | --- |
| 1 | ESP32-S3-DevKitC-1-N8 | Carte Espressif officielle de préférence |
| 1 | Câble USB de données | Connecteur adapté à la révision reçue, pas un câble de charge seule |
| 1 | Plaque d'essai | Format compatible avec la largeur de la carte |
| 1 jeu | Fils Dupont | Pour les futures charges logiques de banc uniquement |

Un ordinateur suffit à alimenter et programmer cette première maquette par
USB. **Aucun câble du véhicule, aucune alimentation 12 V et aucun transceiver
CAN ne doit encore être raccordé.**

## Architecture de communication prévue

La documentation BMW distingue :

- le K-CAN à 100 kbit/s, capable de continuer sur un seul fil en cas de défaut ;
- le PT-CAN à 500 kbit/s, utilisant une couche physique CAN haute vitesse.

L'ESP32-S3 ne possède qu'un contrôleur TWAI. La carte définitive doit donc
prévoir deux canaux indépendants si les signaux qualifiés imposent l'observation
des deux réseaux :

```text
                           +-- contrôleur TWAI interne -- transceiver K-CAN
ESP32-S3 -- logique -------+
                           +-- SPI -- contrôleur CAN externe -- transceiver PT-CAN
```

L'affectation K-CAN/PT-CAN ci-dessus est provisoire : les contrôleurs CAN traitent
les trames, tandis que le type de transceiver impose la compatibilité électrique.
L'affectation définitive dépendra des pilotes, du réveil et des signaux retenus.

Les composants candidats pour la future carte sont :

- `TJA1055T/3` pour la couche physique K-CAN basse vitesse tolérante aux défauts ;
- `TCAN1044AV-Q1` pour la couche physique PT-CAN haute vitesse avec E/S 3,3 V ;
- `MCP2515` comme second contrôleur CAN classique sur SPI ;
- `LM5164-Q1` comme base d'étude de l'alimentation abaisseuse à large plage.

Ces références sont des **candidats de conception**, pas encore une liste
d'achat. Elles seront accompagnées des protections, filtres, modes silencieux,
circuits de réveil et mesures de courant nécessaires sur le schéma final.

## Déroulement matériel sûr

### Phase A — carte seule sur USB

1. Compiler et flasher le firmware inerte.
2. Vérifier l'identification USB et la console série.
3. Raccorder le protocole de configuration au port USB.
4. Tester les débranchements, trames partielles et redémarrages.

### Phase B — deux CAN simulés sur table

1. Ajouter les contrôleurs et transceivers sur une carte d'interface protégée.
2. Utiliser une alimentation de laboratoire limitée en courant.
3. Vérifier d'abord les modes écoute seule sur un bus CAN de test.
4. Tester les débits 100 et 500 kbit/s sans véhicule.

### Phase C — observation passive du véhicule

1. Utiliser un faisceau intermédiaire protégé et réversible.
2. Commencer par un seul bus en mode silencieux matériel et logiciel.
3. Enregistrer uniquement des traces privées sans VIN.
4. Qualifier les signaux avant toute émission.

Les actionneurs et le démarrage restent hors périmètre tant que cette phase n'a
pas produit de résultats reproductibles.

## Pourquoi la carte de développement ne restera pas dans le véhicule

Une DevKit facilite le développement, mais elle ne fournit pas à elle seule :

- la protection contre inversion, surtension, transitoires et décharges ESD ;
- une consommation de veille maîtrisée pour un branchement permanent ;
- des composants et connecteurs qualifiés pour l'environnement automobile ;
- les deux couches physiques CAN adaptées ;
- un watchdog matériel indépendant et des sorties maintenues inactives au reset ;
- une fixation, un boîtier et un faisceau adaptés aux vibrations et à la chaleur.

La carte finale reprendra donc un module ESP32-S3 approprié sur un PCB dédié,
avec alimentation, interfaces, protections et interverrouillages qualifiés.

## Références constructeur

- [Guide ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html)
- [Fiche technique ESP32-S3](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)
- [BMW Body Electronics II — Bus Systems](https://bmwtechinfo.bmwgroup.com/tech_training_manual/ST401%20Body%20Electronics%20II.pdf)
- [NXP TJA1055](https://www.nxp.com/products/TJA1055T)
- [Microchip MCP2515](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf)
- [TI TCAN1044A-Q1](https://www.ti.com/product/TCAN1044A-Q1)
- [TI LM5164-Q1](https://www.ti.com/product/LM5164-Q1)
