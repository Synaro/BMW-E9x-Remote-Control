# Phase 3G — Audit de faisabilité du prototype PCB K-CAN RX-only

## Résolution autorisée après audit

Ce document conserve l'audit initial et explique pourquoi la première
architecture a été arrêtée. Son blocker a ensuite été levé par une modification
électrique **strictement limitée à la Phase 3G** et explicitement autorisée :

`BAT_PROTECTED -> TLS715B0EJV50 -> 5V_TJA`, sans liaison au 5 V/VBUS du DevKit.

La résolution, le budget garanti, le PCB, les contrôles et les exports sont
documentés dans [phase3g-pcb-manufacturing.md](phase3g-pcb-manufacturing.md).
L'historique Phase 3F n'a pas été modifié. Le verdict ci-dessous est donc le
verdict historique de l'audit au commit d'arrêt, pas le statut final 3G-A.

## Verdict historique avant autorisation de correction

**Statut historique : `BLOCKED / NEEDS REVIEW` (blocker ensuite résolu en 3G-A).**

L'architecture RX-only reste cohérente dans son principe, mais la source
`5V_TJA` figée en Phase 3F n'offre pas une marge d'alimentation garantie au
`TJA1055T/3/2Z`. Le rail provient du VBUS USB du DevKitC-1, traverse la diode
Schottky de la carte Espressif puis le PPTC `F_5V`, alors que le TJA1055 exige
`VCC = 4,75 V à 5,25 V` en fonctionnement normal.

La valeur minimale admise à la source USB est déjà égale à la valeur minimale
requise à la charge, avant toute chute dans les deux composants en série. La
conception ne peut donc pas garantir le domaine constructeur du TJA1055 sur
toutes les tolérances autorisées. C'est un problème d'alimentation au sens des
conditions d'arrêt imposées pour cette phase.

En conséquence :

- aucun schéma KiCad n'a été créé ;
- aucun PCB n'a été routé ;
- aucun ERC ou DRC Phase 3G ne peut être revendiqué ;
- aucun fichier Gerber, Excellon, BOM PCBA, CPL ou STEP n'a été généré ;
- aucune estimation de prix d'une carte non définie n'est présentée comme un
  devis ;
- aucune modification électrique corrective n'a été appliquée sans validation
  humaine.

## État de départ et périmètre

| Élément | Valeur vérifiée |
|---|---|
| Dépôt | `Synaro/BMW-E9x-Remote-Control` |
| Branche de départ | `main` |
| `main` et `origin/main` | `66288998dfdf684a1e3bed51b67c85461c9ff444` |
| Worktree initial | propre |
| Branche d'audit | `codex/phase3g-pcb-feasibility-manufacturing` |
| Phase 3F | historique conservé, aucune réécriture du prototype perfboard |
| Phase 4 | non commencée |

Ont été confrontés : BOM, procurement, netlist, câblage, documentation de
sûreté, brochages, alimentations, GPIO, configuration ESP-IDF/TWAI et code de
réception. Le firmware reste exclusivement `TWAI_MODE_LISTEN_ONLY`, son profil
de banc reste désactivé par défaut et aucune API de transmission n'a été
ajoutée.

## Sources primaires consultées

### Circuits et carte de développement

- NXP, page produit TJA1055 :
  <https://www.nxp.com/products/TJA1055T>
- NXP, datasheet TJA1055 :
  <https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf>
- NXP, application hints AH0801 :
  <https://www.nxp.com/docs/en/application-note/AH0801.pdf>
- Texas Instruments, SN74LVC1G125-Q1 :
  <https://www.ti.com/lit/ds/symlink/sn74lvc1g125-q1.pdf>
- Texas Instruments, référence `CLVC1G125QDBVRQ1` :
  <https://www.ti.com/product/SN74LVC1G125-Q1/part-details/CLVC1G125QDBVRQ1>
- Espressif, ESP32-S3-DevKitC-1 v1.1 :
  <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html>
- Espressif, schéma v1.1 :
  <https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-DevKitC-1_V1.1_20221130.pdf>
- Espressif, dimensions v1.1 :
  <https://dl.espressif.com/dl/schematics/esp_idf/DXF_ESP32-S3-DevKitC-1_V1.1_20220429.pdf>
- Espressif, implantation PCB v1.1 :
  <https://dl.espressif.com/dl/schematics/PCB_ESP32-S3-DevKitC-1_V1.1_20220429.pdf>

### Composants et connectique

- Diodes Incorporated, `1N5819HW` employée sur le DevKit :
  <https://www.diodes.com/datasheet/download/1N5819HW.pdf>
- Littelfuse, `MINISMDC010F-2` :
  <https://www.littelfuse.com/assetdocs/ptc-minismdc010f-2-product-specification?assetguid=d165265d-845d-4031-bed2-46ae96c7afab>
- Vishay, série SS12 à SS110 : <https://www.vishay.com/doc/?88746=>
- Würth, `885012207092` :
  <https://www.we-online.com/components/products/datasheet/885012207092.pdf>
- Würth, famille WCAP-CSGP 50 V :
  <https://www.we-online.com/en/components/products/WCAP-CSGP-50VDC>
- Taiyo Yuden, fiche du successeur de `LMJ316BB7226MLHT` :
  <https://ds.yuden.co.jp/TYCOMPAS/eu/detail?pn=MCJCL31LBB7226MTPA1J&u=M>
- JST, série XH : <https://www.jst-mfg.com/product/pdf/eng/eXH.pdf>
- Samtec, `TSW-102-07-G-S` :
  <https://www.samtec.com/products/tsw-102-07-g-s?v=2>
- Keystone, points de test :
  <https://www.keystone-europe.com/wp-content/uploads/2019/11/terminal-test-points.pdf>
- USB-IF, USB 2.0 et exigences de chute de tension :
  <https://www.usb.org/document-library/usb-20-specification> et
  <https://www.usb.org/sites/default/files/USB20_32_BC12_Drop_Droop_1_4_1.pdf>

### Fabrication et assemblage

- JLCPCB, capacités : <https://jlcpcb.com/capabilities/Capabilities>
- JLCPCB, cuivre : <https://jlcpcb.com/help/article/jlcpcb-copper-weight>
- JLCPCB, finitions : <https://jlcpcb.com/help/article/jlcpcb-surface-finish>
- JLCPCB, pièces fournies par le client :
  <https://jlcpcb.com/help/article/how-to-use-my-own-parts-for-pcb-assembly-order>
- JLCPCB, exigences BOM :
  <https://jlcpcb.com/help/article/bill-of-materials-for-pcb-assembly>
- JLCPCB, assemblage THT :
  <https://jlcpcb.com/help/article/pcb-assembly-faqs>
- JLCPCB, ancienne référence TJA1055 publiquement listée :
  <https://jlcpcb.com/partdetail/NXPSemicon-TJA1055T_3_C518/C12222>
- PCBWay, capacités : <https://www.pcbway.com/capabilities.html>
- PCBWay, fichiers d'assemblage :
  <https://www.pcbway.com/helpcenter/pcb_assembly_ordering/What_files_are_requested_for_assembly_production_.html>

La disponibilité catalogue et les prix d'assemblage sont dynamiques. Les pages
ci-dessus prouvent les possibilités générales, pas un stock ni un devis ferme
pour notre BOM.

## Blocage d'alimentation `5V_TJA`

### Chemin figé en Phase 3F

```text
USB VBUS
  -> diode Schottky 1N5819HW du DevKitC-1
  -> broche 5V du DevKit
  -> F_5V MINISMDC010F-2
  -> 5V_TJA
  -> TJA1055 VCC, broche 10
```

Le schéma Espressif montre que le VBUS USB rejoint `VCC_5V` par une
`1N5819HW-7-F`. La netlist Phase 3F ajoute ensuite `F_5V` entre la broche 5 V
du DevKit et le TJA1055.

### Absence de marge garantie

| Paramètre | Limite primaire utile |
|---|---:|
| VBUS USB minimal | 4,75 V |
| VCC minimal du TJA1055 en fonctionnement | 4,75 V |
| `1N5819HW`, VF maximale spécifiée à 0,1 A | 0,320 V |
| `MINISMDC010F-2`, résistance initiale maximale à 25 °C | 12,70 Ω |
| TJA1055, ICC maximal en réception seule | 10 mA |

Le défaut de garantie existe déjà sans calcul de valeur typique :

```text
VBUS_min = VCC_TJA_min_required = 4,75 V
```

Toute chute strictement positive dans la diode du DevKit et `F_5V` place la
charge sous 4,75 V au point limite de la source. Il n'existe donc aucune marge
garantie.

À titre d'enveloppe conservatrice, et non de prédiction de la tension réelle à
10 mA :

```text
5V_TJA >= ?
4,75 V - 0,320 V - (0,010 A x 12,70 ohm) = 4,303 V
```

La datasheet de la diode ne fournit pas une limite maximale de VF spécialement
à 10 mA ; utiliser 0,320 V à 0,1 A illustre une enveloppe prudente, pas une
mesure. Ce détail ne change pas la conclusion formelle : le budget initial est
déjà nul avant les chutes série.

Une mesure correcte sur un exemplaire de banc pourrait montrer plus de 4,75 V,
mais elle ne transformerait pas ce chemin en alimentation reproductible garantie
sur sources USB, températures, PPTC et cartes différentes. Sous la limite VCC,
le fonctionnement normal et la réception ne sont plus garantis par NXP ; la
plage de basculement fail-safe de VCC impose aussi de ne pas traiter cette zone
comme un état nominal.

### Découplage et courant

Le réservoir Phase 3F de 22 uF est proche de la recommandation d'environ 20 uF
présentée par NXP pour un contexte LS/FT, mais l'application note recommande un
condensateur local plus petit de l'ordre de 10 à 47 nF alors que le design
emploie 100 nF. Ce n'est pas classé comme défaillance en réception seule, mais
les valeurs, le courant de pointe et le PPTC 0,10 A doivent être revus avec le
futur choix d'alimentation. Les chiffres de courant d'émission ou de défaut ne
doivent pas être appliqués comme s'ils décrivaient le chemin RX-only normal.

### Décision requise

Une nouvelle revue humaine doit choisir puis qualifier une topologie, par
exemple :

1. un rail 5 V régulé dédié avec budget de tolérances et gestion explicite de
   tout retour vers l'USB ;
2. une entrée 5 V externe qualifiée, avec USB limité aux données ou isolé du
   chemin d'alimentation ;
3. une autre révision documentée de l'arbre d'alimentation.

Ces options ne sont pas des modifications approuvées. Le choix devra préciser
source, protections, séquencement, courant, brownout, masse et interdiction de
back-power, puis relancer l'audit complet avant tout dessin PCB.

## Audit du TJA1055T/3/2Z

### Brochage et affectation

| Pin | Fonction NXP | Affectation Phase 3F | Résultat |
|---:|---|---|---|
| 1 | INH | ouverte, test isolé seulement | PASS |
| 2 | TXD | pull-up 10 kΩ vers `5V_TJA`; aucun lien avec U2.Y | PASS RX-only |
| 3 | RXD, drain ouvert sur variante `/3` | pull-up 3,3 kΩ, série 1 kΩ, GPIO4 | PASS |
| 4 | ERR, drain ouvert sur variante `/3` | pull-up 3,3 kΩ, série 1 kΩ, GPIO7 | PASS |
| 5 | STB | 10 kΩ vers GND, activation manuelle via 1 kΩ | PASS |
| 6 | EN | 10 kΩ vers GND, activation manuelle via 1 kΩ | PASS |
| 7 | WAKE | reliée à `BAT_LOCAL` | PASS |
| 8 | RTH | 5,62 kΩ vers CANH | PASS de principe |
| 9 | RTL | 5,62 kΩ vers CANL | PASS de principe |
| 10 | VCC | `5V_TJA` | **BLOCKED : rail non garanti** |
| 11 | CANH | J_KCAN.1 | PASS |
| 12 | CANL | J_KCAN.2 | PASS |
| 13 | GND | masse commune de banc | PASS |
| 14 | BAT | 12 V protégé, série 1 kΩ et 10 nF | PASS |

La variante `/3` est bien destinée à une interface microcontrôleur 3,3 V pour
RXD/ERR. NXP décrit la réception complète en **mode Normal**, obtenu avec STB et
EN hauts. Les modes basse consommation signalent principalement le réveil ; ils
ne remplacent pas le flux RX de trames. L'hypothèse Phase 3F est donc confirmée.

La perte de VCC force STB/EN à l'état bas interne et NXP prévoit que le nœud non
alimenté ne perturbe pas le bus. Cela soutient le fail-safe, mais ne corrige pas
la sous-tension nominale identifiée.

Relier WAKE à BAT lorsqu'aucun interrupteur de réveil externe n'est utilisé est
conforme à la recommandation NXP. La résistance WAKE externe parfois montrée
par NXP concerne le cas d'un contact de réveil vers la masse.

### Réseau RTH/RTL

AH0801 impose un total d'environ 100 Ω par ligne pour le réseau complet, un
minimum de 500 Ω par transceiver et recommande de ne pas dépasser environ 6 kΩ
par terminaison locale. Un nœud optionnel doit employer des valeurs élevées
pour charger faiblement le réseau. Les deux résistances 5,62 kΩ, appairées à
0,1 %, sont cohérentes pour **un nœud d'observation additionnel de banc**, sous
réserve que le reste du banc fournisse le réseau ISO 11898-3 actif/terminé
attendu.

Cette conclusion n'autorise pas une connexion BMW : impédance globale,
longueur de dérivation, formes d'onde et impact sur la veille restent à mesurer.
La règle NXP de constante de temps locale inférieure à environ un sixième du
temps de bit devra être vérifiée sur le banc. Aucune résistance 120 Ω entre H
et L ne doit être ajoutée.

## Audit du buffer SN74LVC1G125-Q1

| Pin DBV | Fonction TI | Affectation Phase 3F | Résultat |
|---:|---|---|---|
| 1 | `/OE` | pull-up 10 kΩ vers 3,3 V, aucun GPIO | PASS, sortie Hi-Z par défaut |
| 2 | A | GPIO5 via 1 kΩ, pull-up 100 kΩ | PASS |
| 3 | GND | GND | PASS |
| 4 | Y | `TP_GATE_Y`, puis coupure DNP | PASS RX-only |
| 5 | VCC | 3,3 V, 100 nF local | PASS |

Le boîtier DBV est bien un SOT-23-5, la plage VCC couvre 3,3 V, les entrées
tolèrent jusqu'à 5,5 V, la fonction `Ioff` protège lorsque VCC vaut 0 et TI
recommande de tirer `/OE` vers VCC pour imposer la haute impédance pendant les
transitions d'alimentation. L'emploi Phase 3F est valide.

Même si U2 sortait un niveau actif, sa sortie se termine sur un point de test :
`R_LINK_TX` est à la fois DNP et électriquement absent. Le seul élément monté
sur TXD du TJA1055 reste son pull-up vers son propre VCC.

## Audit ESP32-S3-DevKitC-1-N8R8

| Élément | Résultat |
|---|---|
| Variante | N8R8 : 8 MiB flash Quad et 8 MiB PSRAM Octal |
| Format officiel v1.1 | 62,74 mm de long ; rangées espacées de 25,40 mm |
| Headers | deux rangées de 22 broches au pas 2,54 mm |
| GPIO4 | TWAI_RX Phase 3F ; disponible |
| GPIO5 | TWAI_TX vers entrée du buffer isolé ; disponible |
| GPIO7 | signal ERR ; disponible |
| GPIO de mémoire N8R8 | 35, 36 et 37 réservés à l'Octal PSRAM, non utilisés ici |
| Strapping | GPIO4/5/7 ne sont pas les straps problématiques du module |
| EN | uniquement point de mesure reset |
| USB | doit rester accessible sur une future carte porteuse |
| Alimentation | USB, 5V+GND ou 3V3+GND sont des options exclusives selon Espressif |

Une empreinte porteuse à deux connecteurs femelles traversants est mécaniquement
réalisable, mais elle n'a pas été dessinée puisque le projet s'arrête avant la
conception. L'orientation USB et la hauteur des connecteurs devront être cotées
depuis les documents Espressif et vérifiées sur le module reçu, jamais déduites
d'une empreinte communautaire seule.

## Audit des autres composants montés

| Référence | Vérification | Statut |
|---|---|---|
| résistances Yageo RC/RT | tailles 0805/1206, valeurs et tolérances cohérentes avec la BOM | PASS documentaire |
| `MINISMDC010F-2` | 1812, 60 V, Ihold 0,10 A, Itrip 0,30 A, R initiale max 12,70 Ω | PASS composant ; **F_5V participe au blocker** |
| `SS16-E3/61T` | Schottky 1 A / 60 V, SMA/DO-214AC, polarité requise | PASS banc ; version E3 non revendiquée AEC-Q101 |
| `885012207098` | 100 nF, 50 V, X7R, 0805 | PASS documentaire |
| `885012207092` | 10 nF, 50 V, X7R, 0805 | PASS documentaire |
| `LMJ316BB7226MLHT` | 22 uF, 10 V, X7R, 1206 ; ancienne référence constructeur | NEEDS SOURCING REVIEW |
| JST `B2B-XH-A` / `B3B-XH-A` | top-entry, pas 2,50 mm, perçage nominal 1,0 mm | PASS documentaire |
| `TSW-102-07-G-S` | 2 positions, traversant, pas 2,54 mm | PASS documentaire |
| `SNT-100-BK-G` | shunt amovible associé | PASS documentaire |
| Keystone 5001 | point de test traversant, perçage à confirmer avec la finition choisie | PASS perfboard ; optionnel sur PCB |

Taiyo Yuden présente `LMJ316BB7226MLHT` comme ancienne référence au profit de
`MCJCL31LBB7226MTPA1J`. Aucune substitution n'est autorisée par cet audit : le
stock exact doit être vérifié ou une révision BOM séparée doit être approuvée.

Les adaptateurs Chip Quik `PA0003C` et `PA0086C` et la perfboard BusBoard
`PR2H1-D` sont des accessoires mécaniques du prototype Phase 3F. Une PCBA
professionnelle emploierait les empreintes natives SO-14 et SOT-23-5, sans
changer les deux circuits électriques. Ils disparaîtraient uniquement de la
BOM **PCBA Phase 3G**, pas de l'historique procurement Phase 3F.

Les Keystone 5001 pourraient être réservés aux mesures répétées à
l'oscilloscope ; des pads nus suffisamment grands conviendraient à une partie
des contrôles de production. Ce choix DFT n'a pas été figé.

## Faisabilité PCB et PCBA non exécutée

### Base manufacturière possible, après résolution du blocker

Les capacités publiées de JLCPCB et PCBWay sont largement compatibles avec une
carte simple de ce type. Un PCB 2 couches, FR-4 1,6 mm, cuivre 1 oz et règles
conservatrices nettement supérieures aux minima publiés serait a priori
suffisant à 100 kbit/s. Quatre couches ne sont pas justifiées par le débit ou
la complexité actuels.

Il ne s'agit toutefois pas d'un stackup retenu : dimensions, pistes, vias,
placement, retours de masse et dégagements n'ont pas été conçus. Ils restent
`NOT APPLICABLE — BLOCKED BEFORE LAYOUT`.

### Disponibilité des composants critiques

- La référence exacte `TJA1055T/3/2Z` est active chez NXP, mais n'a pas été
  confirmée en stock public JLCPCB/LCSC. La page JLC trouvée concerne
  `TJA1055T/3/C,518`, marquée EOL et sans stock ; ce n'est pas un substitut.
- La disponibilité publique JLC du suffixe exact TI
  `CLVC1G125QDBVRQ1G4` n'est pas démontrée.
- JLCPCB documente l'approvisionnement externe et les pièces fournies par le
  client ; PCBWay accepte BOM et centroid puis confirme le sourcing après
  revue. Ces services n'autorisent aucune substitution automatique.
- Les composants THT, connecteurs et futurs headers femelles exigeraient une
  vérification de l'offre d'assemblage et des frais manuels au moment du devis.

La stratégie prévisible serait donc assemblage SMT avec sourcing/consignment
des références exactes, THT selon devis, puis DevKit enfiché par l'utilisateur.
Elle n'est pas qualifiée tant que la BOM PCBA et le dessin n'existent pas.

### Coûts

| Quantité | PCB seul | SMT | THT | pièces hors catalogue | transport |
|---:|---|---|---|---|---|
| 5 | non estimable honnêtement avant dessin et devis | idem | idem | idem | dynamique |
| 10 | non estimable honnêtement avant dessin et devis | idem | idem | idem | dynamique |

Les tarifs d'appel d'un fabricant ne constituent pas le coût de cette carte.
Produire des montants avant dimensionnement, BOM PCBA, disponibilité et pays de
livraison créerait une fausse précision.

## Barrière TX et sûreté maintenues

La propriété qui doit survivre à toute future révision est :

```text
ESP32 GPIO5/TWAI_TX -> 1 kΩ -> U2.A
U2./OE -> 10 kΩ -> 3V3
U2.Y -> TP_GATE_Y     [FIN DU CUIVRE]

5V_TJA -> 10 kΩ -> U1.TXD -> TP_TXD
```

Il ne doit exister ni composant, ni piste, ni via, ni zone, ni jumper entre ces
deux îlots. `R_LINK_TX` reste une désignation documentaire `DNP/DNF`, pas une
résistance à monter. Le futur PCB devra porter `RX ONLY`, `TX LINK DNP`,
`DO NOT POPULATE` et `BENCH ONLY`. Cette propriété n'a pas encore été vérifiée
par DRC, puisqu'aucun PCB n'a été produit.

## Revue de faisabilité globale

| Sujet | Classement | Justification |
|---|---|---|
| Contrôleur TWAI ESP32-S3 en écoute seule | CONFIRMÉ hors véhicule | code, builds et gates CI existants |
| Couche physique ISO 11898-3 avec TJA1055 | PLAUSIBLE MAIS NON VALIDÉE sur banc | candidat constructeur adapté, mesures électriques absentes |
| Réception complète en mode Normal | CONFIRMÉ par NXP | STB et EN hauts nécessaires au flux RX complet |
| Barrière TX physique DNP | CONFIRMÉE dans le netlist Phase 3F | aucune continuité prévue ; futur PCB non créé |
| Rail VCC TJA depuis USB DevKit | **BLOQUANT** | aucune marge garantie à 4,75 V |
| RTH/RTL 5,62 kΩ | CONFIRMÉ pour un nœud optionnel de banc | charge élevée et appairée ; réseau complet à mesurer |
| Acquisition K-CAN réelle | INCONNU | aucun montage qualifié, aucune BMW autorisée |
| Détection lock/unlock | PLAUSIBLE MAIS NON VALIDÉE | protocole logiciel prêt, signal véhicule inconnu |
| Triple lock | INCONNU | aucune capture K-CAN propre au véhicule |
| IDs `0x23A` / `0x2B4` | `EXTERNAL_UNVALIDATED` | hypothèses communautaires seulement |
| Sécurité électrique de banc | INCONNU | bring-up, brownout, absence de dominant/ACK non exécutés |
| Takeover futur | INCONNU / hors périmètre | aucune qualification véhicule |
| Méthode de démarrage future | INCONNU / hors périmètre | aucune commande ou autorisation identifiée |

Aucune information issue d'un dépôt communautaire n'est promue en vérité BMW.
Aucun identifiant CAN, signal de sécurité, mécanisme de démarrage ou commande
véhicule n'a été ajouté.

## Éléments Phase 3G non produits

| Livrable demandé | Résultat |
|---|---|
| projet/schéma/PCB KiCad | non créé — arrêt avant conception |
| architecture, dimensions, stackup retenus | non applicables tant que le power tree est bloqué |
| placement/routage/ground final | non créé |
| ERC | non exécuté — aucun schéma Phase 3G |
| DRC | non exécuté — aucun PCB Phase 3G |
| revue pin-à-pin schéma/footprint/net | audit Phase 3F effectué ; revue PCB impossible |
| Gerber/Excellon | non générés |
| BOM/CPL PCBA | non générés |
| PDF/STEP/images/ZIP fabricant | non générés |
| plan de bring-up Phase 3G | différé : dépend du power tree approuvé |
| commande fabricant | interdite et non effectuée |

## Reprise autorisée après décision

Après choix explicite d'une alimentation corrigée :

1. versionner la décision et ses calculs de pire cas ;
2. mettre à jour séparément l'architecture Phase 3G sans réécrire la Phase 3F ;
3. refaire les audits TJA, buffer, DevKit, BOM/netlist et modes de défaut ;
4. créer seulement alors le schéma KiCad ;
5. exécuter ERC, revue pin-à-pin, placement, routage, DRC et revue TX DNP ;
6. vérifier le sourcing exact et obtenir des devis pour 5 et 10 cartes ;
7. générer le package de fabrication et le plan de bring-up ;
8. soumettre le tout à une revue humaine avant toute commande.

Ce document ne constitue ni une autorisation de fabrication, ni une
autorisation de raccordement au véhicule.
