# Phase 3G — PCB K-CAN RX-only prêt pour revue de fabrication

## Statut et limite d'autorité

La révision `3G-A` lève le blocker d'alimentation identifié par l'audit initial
et produit un schéma, un PCB deux couches et un package de fabrication
reproductible. Son statut est :

**`READY FOR MANUFACTURING REVIEW` — pas `RELEASED FOR ORDER`.**

Cette carte reste un prototype de banc en réception seule. Elle n'autorise ni
commande fabricant, ni connexion BMW, ni émission CAN, ni Phase 4. Une revue
humaine des fichiers et la procédure de bring-up restent obligatoires.

## Architecture électrique retenue

```text
J1 10..18 V de banc, limité en courant
  -> F1 MINISMDC010F-2
  -> D1 SS16-E3/61T
  -> BAT_PROTECTED
       |-> R1 1 kΩ -> BAT_LOCAL -> U1 BAT + WAKE
       `-> U3 TLS715B0EJV50 -> 5V_TJA -> U1 VCC

USB -> ESP32-S3-DevKitC-1-N8R8
ESP32 3V3 -> U2 et pull-ups RXD/ERR
GND commun uniquement
```

Le `TLS715B0EJV50XUMA1` est retenu dans son boîtier constructeur
`PG-DSO-8-52` à pad exposé. La page produit Infineon le classe
`active and preferred`, indique 4–40 V, 150 mA, -40..150 °C et une disponibilité
planifiée au moins jusqu'en 2038. Sa datasheet garantit 4,90–5,10 V dans
l'enveloppe 6–28 V, 0,05–150 mA et -40..150 °C :

- <https://www.infineon.com/part/TLS715B0EJ-V50>
- <https://www.infineon.com/assets/row/public/documents/10/49/infineon-tls715b0ej-v50-datasheet-en.pdf>
- <https://www.infineon.com/package/PG-DSO-8-52>

Le TJA1055 exige 4,75–5,25 V. Aucun fusible, diode ou résistance n'est placé en
série après U3. La marge garantie est donc de 150 mV sur chaque limite. Le
détail des chutes amont, charges, condensateurs, ESR et calculs thermiques est
versionné dans `hardware/kcan-rxonly-pcb/power-budget.md`.

La diode U3 Q→I étudiée n'est pas montée : Infineon ne la prescrit pas dans
cette application, le rail de sortie n'a aucune autre source et son ajout
créerait un chemin inverse non qualifié. La protection d'inversion d'entrée
reste D1.

## Séquencement, alimentation croisée et état sûr

Les deux domaines ne partagent que GND. `H1.21`, la broche 5 V du DevKit, est
sans net et sans cuivre. Les cas USB seul, 12 V seul, USB puis 12 V, 12 V puis
USB et les deux retraits ne créent donc aucun chemin VBUS/5V_TJA.

Le TJA1055 `/3` emploie RXD et ERR open-drain avec des pull-ups côté 3,3 V ; il
ne source pas un ESP32 éteint. U2 possède `Ioff`, `/OE` est tiré au niveau haut
et sa sortie se termine physiquement avant le TJA. STB et EN sont tirés bas et
ne peuvent recevoir 5 V qu'après pose manuelle de JP1. NXP documente le
fonctionnement hors alimentation et l'interface 3,3 V du TJA1055T/3 :

- <https://www.nxp.com/products/TJA1055T>
- <https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf>
- <https://www.nxp.com/docs/en/application-note/AH0801.pdf>

Ces propriétés sont des conclusions de conception. Elles doivent encore être
mesurées sur la carte physique aux huit scénarios du plan de bring-up.

## Empreinte U3 et thermique

L'empreinte locale `PG-DSO-8-52` reprend le dessin mécanique actuel Infineon :

- corps 3,90 x 4,90 mm ;
- pas 1,27 mm ;
- pad exposé 2,65 x 3,00 mm ;
- neuf vias thermiques, pad 0,50 mm / foret 0,20 mm ;
- ouvertures pâte segmentées, masque et courtyard conservés ;
- pad 9 et vias reliés à GND.

La zone supérieure nominale 30 x 20 mm produit 459,43 mm² de cuivre rempli
après routage, complétés par le plan GND inférieur. Elle dépasse les 300 mm²
associés par Infineon à `RthJA = 71 K/W`. À 18 V, 20 mA de calcul et 85 °C
ambiant, la dissipation est 0,263836 W ; `Tj` vaut environ 103,7 °C avec cette
aire. Même l'enveloppe très conservatrice 153 K/W donne 125,4 °C, sous la
limite de 150 °C.

Le modèle 3D KiCad est seulement une enveloppe SO-8 3,90 x 4,90 mm. Les données
de production autoritatives sont l'empreinte cuivre/pâte/masque et le dessin
Infineon, pas le rendu 3D.

## PCB et règles de fabrication

| Propriété | Valeur Phase 3G | Référence de capacité |
|---|---:|---|
| dimensions | 120,0 x 80,0 mm | mesuré par KiCad |
| couches | 2 | F.Cu / B.Cu |
| épaisseur | 1,6 mm | FR-4 |
| cuivre demandé | 1 oz | prototype standard |
| piste réellement la plus fine | 0,20 mm | JLC : 0,10 mm à 1 oz |
| clearance réellement la plus faible | 0,2016 mm | JLC : 0,10 mm à 1 oz |
| via routage | 0,60 / 0,30 mm | marge prototype confortable |
| vias thermiques U3 | 0,50 / 0,20 mm | cas spécial documenté |
| anneau via thermique | 0,15 mm | à confirmer au DFM du devis |
| trous NPTH | 4 x 3,20 mm | fixation M3 |

Capacités publiées consultées :

- JLCPCB : <https://jlcpcb.com/capabilities/Capabilities>
- PCBWay : <https://www.pcbway.com/capabilities.html>

La carte n'utilise pas le minimum de piste fabricant. Le foret thermique de
0,20 mm et son anneau de 0,15 mm sont l'exception ; ils doivent être soumis au
DFM réel sans élargissement automatique ni substitution d'empreinte.

## Placement, routage et masse

Les entrées J1/J2 se trouvent à gauche, le transceiver et son réseau au centre,
et les deux sockets DevKit à droite, USB vers le bord. Les découplages U1/U2/U3
sont locaux. B.Cu contient un plan GND continu de 8 903,395 mm² ; F.Cu contient
la zone thermique U3. Les retours de J1, J2, U1, U2 et U3 rejoignent le même
référentiel sans île GND flottante détectée.

Les nets K-CAN n'ont aucune terminaison 120 Ω entre H et L. R14/R15 fournissent
uniquement la charge faible et appairée 5,62 kΩ du nœud LS/FT de banc.

## Coupure TX physique

La séparation est absolue :

```text
U2.Y -> TX_GATE_OUT -> TP5  [fin du cuivre]

5V_TJA -> R2 10 kΩ -> TXD_SAFE -> U1.TXD + TP6
```

Il n'existe aucun `R_LINK_TX` dans le schéma ou le PCB, aucune empreinte à
monter, aucune piste, via, zone ou pad commun. Deux rule areas, une sur F.Cu et
une sur B.Cu, interdisent pistes, vias et remplissage dans la coupure. La
sérigraphie porte `TX LINK DNP`, `DO NOT POPULATE - NO COPPER` et `RX-ONLY`.

Cette coupure signifie que le TJA ne peut recevoir aucune donnée TX du
contrôleur, même en présence d'une faute firmware. Elle ne remplace pas les
essais oscilloscope d'absence de dominant et d'ACK.

## Résultats de conception

| Contrôle | Résultat |
|---|---|
| ERC KiCad 10.0.6 | 0 erreur, 0 avertissement |
| DRC KiCad 10.0.6 | 0 violation, 0 élément non connecté |
| pin review | toutes les lignes `PASS` |
| cuivre TX commun | absent |
| `R_LINK_TX` | quantité 0, aucune empreinte |
| U3 vias thermiques | 9 x 0,20 mm |
| broche DevKit 5 V | `H1.21`, NC |
| firmware | inchangé |

Les rapports sont conservés sous `hardware/kcan-rxonly-pcb/kicad/` et ces
propriétés sont vérifiées par `tools/test_phase3g_pcb.py`.

## BOM, assemblage et sourcing

`BOM_PHASE3G.csv` et `procurement_PHASE3G.csv` sont indépendants des livrables
perfboard Phase 3F, qui restent historiquement inchangés. Les adaptateurs
`PA0003C`, `PA0086C`, la perfboard `PR2H1-D` et les plots Keystone ne figurent
pas dans les achats Phase 3G.

Les références critiques sont gelées sans substitution silencieuse :

- NXP `TJA1055T/3/2Z` ;
- TI `CLVC1G125QDBVRQ1` ;
- Infineon `TLS715B0EJV50XUMA1` ;
- Espressif `ESP32-S3-DEVKITC-1-N8R8` installé par l'utilisateur.

La photographie de disponibilité du 3 octobre 2026 relève des pages
constructeur/distributeur et doit être refaite au devis. La fiche DigiKey du
`ESP32-S3-DEVKITC-1-N8R8` exact est active mais indiquait zéro stock et un
réapprovisionnement estimé en 2027 : il faut donc recontrôler un distributeur
agréé avant achat, sans changer de variante. JLC/LCSC n'est pas confirmé pour
les trois références exactes ; un consignment ou sourcing externe peut être
nécessaire. Le THT doit être chiffré séparément. Aucune substitution par
`TJA1055T/3/C,518`, buffer non-Q1 ou autre LDO n'est autorisée.

## Estimation non contractuelle

Les montants ci-dessous sont uniquement des enveloppes d'ingénierie
`ESTIMATE_NOT_QUOTE`, transport/TVA et disponibilité pouvant les modifier :

| Quantité | PCB nus | PCBA hors DevKit | DevKits séparés | Total indicatif |
|---:|---:|---:|---:|---:|
| 5, JLC-type | 15–35 € | 110–220 € | 100–150 € | 225–405 € |
| 5, PCBWay-type | 30–65 € | 140–280 € | 100–150 € | 270–495 € |
| 10, JLC-type | 25–55 € | 170–330 € | 200–300 € | 395–685 € |
| 10, PCBWay-type | 45–90 € | 220–420 € | 200–300 € | 465–810 € |

Seul un devis chargé avec le ZIP, la BOM, le CPL, les MPN exacts et l'adresse
de livraison peut produire un prix utilisable. La présente phase n'envoie rien
à un fabricant.

## Package de revue

`tools/generate_phase3g_manufacturing.ps1` relance ERC/DRC puis génère :

- Gerber X2 et job file ;
- Excellon PTH/NPTH, cartes et rapport de perçage ;
- BOM PCBA filtrée et CPL ;
- schéma, assemblage et fabrication PDF ;
- STEP et rendus 3D face/verso ;
- IPC-D-356 et statistiques ;
- rendus PyGerber indépendants des couches cuivre, masque, sérigraphie et bord ;
- ZIP fabricant et manifeste SHA-256.

Les Gerbers ont été reparsés par PyGerber. Le contour fermé, les deux couches
cuivre, les perçages, le masque, la sérigraphie, les JST, les headers, le pad
thermique et la zone sans cuivre TX sont visibles dans les sorties de revue.
Le DFM du fabricant et la revue humaine restent nécessaires.

## Inconnues et critères restant hors de cette phase

- carte non fabriquée ;
- alimentation, température et brownout non mesurés ;
- absence de pulse/dominant/ACK non mesurée à l'oscilloscope ;
- comportement du réseau LS/FT réel non qualifié ;
- aucune connexion BMW autorisée ;
- aucun ID K-CAN validé ;
- aucun déclencheur télécommande validé ;
- aucun mécanisme de démarrage ou d'autorisation déduit ;
- aucune Phase 4 commencée.
