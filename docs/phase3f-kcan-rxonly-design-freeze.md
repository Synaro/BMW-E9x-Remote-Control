# Phase 3F — gel du design K-CAN RX-only

## Statut et périmètre

Ce document fige la variante de prototype `KCAN_RX_ONLY_P3F`. Elle est
constructible pour **un banc ISO 11898-3 à 100 kbit/s** et doit d'abord être
qualifiée électriquement. Elle n'est pas un calculateur automobile final et
n'autorise aucune connexion à la BMW.

Cette phase n'ajoute aucun firmware actif, aucun ID BMW, aucun rejeu, aucune
commande CAS/DDE/KL50, aucun contournement EWS et aucune Phase 4. Le contrôleur
TWAI existant reste exclusivement `TWAI_MODE_LISTEN_ONLY`; les symboles
`twai_transmit`, `twai_node_transmit`, `TWAI_MODE_NORMAL` et
`TWAI_MODE_NO_ACK` restent interdits dans les sources embarquées.

Les fichiers normatifs du montage sont :

- `hardware/kcan-rxonly/BOM.csv` pour les références et états DNP ;
- `hardware/kcan-rxonly/procurement.csv` pour l'instantané d'achat contrôlé ;
- `hardware/kcan-rxonly/netlist.csv` pour les connexions contrôlables ;
- `hardware/kcan-rxonly/wiring.md` pour le câblage et la première mise sous
  tension.

Aucun projet KiCad n'est livré dans cette phase : `kicad-cli` n'est pas installé
dans l'environnement de validation et produire un schéma CAO non compilé ou non
contrôlé donnerait une fausse garantie. Une saisie KiCad pourra reprendre le
netlist normatif après choix explicite des empreintes ; elle devra alors passer
un ERC et une revue indépendante avant fabrication.

## Conclusion de la revue Phase 3E

Le concept Phase 3E était correct sur le choix du transceiver, la nécessité du
mode normal pour disposer de RXD complet et l'emploi d'une coupure TX physique.
Il comportait toutefois deux faiblesses à corriger avant achat :

1. **TXD était tiré vers 3,3 V.** Si l'ESP32 et son 3,3 V disparaissaient alors
   que VCC/BAT du TJA1055 restaient présents, TXD n'était plus garanti HIGH.
   Il est maintenant tiré par 10 kΩ vers le **VCC 5 V local de U1**. Avec
   l'entrée TXD de la variante `/3`, le courant d'entrée maximal documenté
   (21 µA) ne fait perdre qu'environ 0,21 V : TXD reste très au-dessus du seuil
   HIGH minimal de 2,2 V.
2. **STB/EN dépendaient d'un GPIO 3,3 V.** Une perte ou un ordre d'alimentation
   défavorable pouvait compliquer l'analyse des clamps et du back-powering.
   Dans le gel RX-only, aucun GPIO ne pilote STB/EN. Deux pull-downs les
   maintiennent LOW ; un cavalier physique commun les porte au 5 V via deux
   résistances de 1 kΩ seulement au moment de l'acquisition de banc.

Une troisième mesure durcit la première barrière : `/OE` de U2 n'est relié à
aucun GPIO. Il est tiré HIGH par 10 kΩ, conformément à la recommandation TI, et
la sortie reste haute impédance. U2 est conservé pour rendre visible et
testable la frontière TX, mais la sécurité déterministe vient surtout de
`R_LINK_TX` **non monté**.

## Sources et faits de conception

La fiche NXP du TJA1055 donne : ISO 11898-3, débit maximal 125 kbit/s, VCC
4,75–5,25 V, BAT 5–40 V, entrées STB/EN/TXD HIGH à partir de 2,2 V, RXD/ERR
open-drain sur la variante `/3`, interface microcontrôleur sans chemin de
retour lorsqu'elle n'est pas alimentée et nœud non alimenté non perturbant.
Elle force en interne STB et EN à LOW sous le seuil de sous-tension VCC.

Le tableau de modes NXP est déterminant : STB=HIGH et EN=HIGH donnent le mode
normal et les données reçues sur RXD. Les modes à faible consommation ne
fournissent qu'une indication de réveil. Il n'existe donc pas de véritable
mode « silent + RX complet » sur ce composant. La séparation TX doit être
externe.

TI spécifie pour le SN74LVC1G125-Q1 une alimentation 1,65–5,5 V, la fonction
`Ioff` de partial-power-down et exige un pull-up de `/OE` vers son VCC pour
maintenir Y en haute impédance au power-up/down. Ces propriétés sont une
barrière supplémentaire, pas la preuve finale de silence.

## Schéma électrique figé

```text
                               ESP32-S3-DEVKITC-1-N8R8
                       USB ---- alimentation / programmation
                                  5V o----F_5V 100 mA----+5V_TJA
                                 3V3 o-------------------+3V3
                                 GND o-------------------GND

 GPIO5 / TWAI_TX --1k--+--> A(2) U2 SN74LVC1G125-Q1
                       |
                    100k
                       |
                      3V3                 3V3
                                            |
                                          10k R_OE
                                            |
                                   /OE(1)---+       Y(4)---TP_GATE_Y---o
                                                                         ) R_LINK_TX
                                      VCC(5)=3V3                           ) DNP, 2.54 mm
                                      GND(3)=GND                          o---TP_TXD---TXD(2) U1
                                                                               |
                                                                              10k R_TXD
                                                                               |
                                                                            +5V_TJA

 +5V_TJA ------------------------------ VCC(10) U1 TJA1055T/3/2Z
     |                                   |  +--100 nF--GND
     |                                   |  `--22 uF---GND
     |
     +-- JP_RX_MODE (normally OPEN) -- RX_MODE_FEED
                                      |             |
                                     1k            1k
                                      |             |
                                STB(5)+--10k--GND EN(6)+--10k--GND

 U1 RXD(3) --1k--> GPIO4/TWAI_RX       U1 ERR(4) --1k--> GPIO7
      |                                      |
     3.3k                                   3.3k
      |                                      |
     3V3                                    3V3

 12 V LAB --F_BAT 100 mA-->| D_BAT--1k R_BAT--+--BAT_LOCAL--> BAT(14)
                                                    |             |
                                                  10nF          WAKE(7)
                                                    |             |
                                                   GND       direct to BAT_LOCAL

 INH(1): NC

 J_KCAN.1 KCAN_H ------------------------------- CANH(11)
       `------------------5.62k 0.1%------------ RTH(8)

 J_KCAN.2 KCAN_L ------------------------------- CANL(12)
       `------------------5.62k 0.1%------------ RTL(9)

 J_KCAN.3 GND ---------------------------------- GND(13)

 ABSENT: 120 ohm H-L, TVS non qualifié, choke non qualifié, lien TX,
         connecteur véhicule, alimentation directe par batterie automobile.
```

Le 5 V du transceiver provient du rail 5 V du DevKit lorsqu'il est alimenté
par USB, derrière `F_5V`. Le 12 V BAT vient d'une alimentation de laboratoire
distincte, limitée en courant, et les masses sont communes. Le DevKit ne doit
jamais recevoir simultanément son USB et une autre source sur sa broche 5 V.

## Audit des 14 broches U1

| Pin | Rôle constructeur | Connexion figée | Boot / ESP absent | Acquisition | Reset, brownout ou perte d'alimentation |
|---:|---|---|---|---|---|
| 1 `INH` | sortie d'inhibition d'un régulateur externe | NC ; aucun fil, aucune charge | la sortie peut suivre l'état NXP mais ne commande rien | idem | NXP permet de laisser une sortie INH inutilisée ouverte |
| 2 `TXD` | entrée de données émetteur, LOW=dominant | `R_TXD=10 kΩ` vers `+5V_TJA`, TP ; pad DNP isolé | HIGH si VCC présent, indépendant de l'ESP32 | reste HIGH ; aucun signal TWAI ne l'atteint | VCC perdu : l'émetteur est hors alimentation/forced standby ; VCC présent : pull-up local conservé |
| 3 `RXD` | sortie de données reçues | open-drain `/3`, pull-up 3,3 kΩ vers 3V3, 1 kΩ vers GPIO4 | pas de source 5 V vers l'ESP32 ; peut indiquer un réveil | flux reçu complet en mode normal | 3V3 perdu : pull-up disparaît ; VCC perdu : absence de reverse-current documentée |
| 4 `ERR` | erreur/réveil/power-on actif LOW | open-drain `/3`, pull-up 3,3 kΩ vers 3V3, 1 kΩ vers GPIO7 | indication possible, sans effet bus | diagnostic seulement | mêmes propriétés d'isolation que RXD |
| 5 `STB` | sélection de mode avec EN | 10 kΩ vers GND ; 1 kΩ depuis cavalier 5 V | LOW, même ESP absent | HIGH seulement avec `JP_RX_MODE` posé | LOW par résistance ; NXP force aussi LOW sous VCC(stb) |
| 6 `EN` | sélection de mode avec STB | 10 kΩ vers GND ; 1 kΩ depuis cavalier 5 V | LOW | HIGH seulement avec `JP_RX_MODE` posé | LOW par résistance ; NXP force aussi LOW sous VCC(stb) |
| 7 `WAKE` | réveil local actif LOW, deux fronts | directement à `BAT_LOCAL` | stable, pas d'antenne flottante | stable | NXP recommande cette connexion quand le réveil local est inutilisé |
| 8 `RTH` | connexion de terminaison CANH | 5,62 kΩ 0,1 % vers `KCAN_H` | terminaison LS/FT locale | idem | influence de veille à mesurer avant toute connexion véhicule |
| 9 `RTL` | connexion de terminaison CANL | 5,62 kΩ 0,1 % vers `KCAN_L` | terminaison LS/FT locale | idem | en faible consommation RTL peut être commuté vers BAT par U1 |
| 10 `VCC` | alimentation fonctionnelle | `+5V_TJA`, 100 nF + 22 µF locaux | 4,75–5,25 V | idem | sous-tension force STB/EN LOW ; ne pas confondre avec BAT |
| 11 `CANH` | ligne bus haute LS/FT | direct `J_KCAN.1`, TP ; pas de TVS/choke monté | passif en veille | réception différentielle/single-wire gérée par U1 | NXP annonce nœud non alimenté non perturbant ; à confirmer sur le prototype |
| 12 `CANL` | ligne bus basse LS/FT | direct `J_KCAN.2`, TP ; pas de TVS/choke monté | passif/terminaison selon mode | réception | même réserve de mesure réelle |
| 13 `GND` | masse | plan commun DevKit, BAT, bus de banc | 0 V | 0 V | connecter avant H/L et BAT |
| 14 `BAT` | alimentation batterie / logique de veille | 12 V labo via PTC, diode, 1 kΩ, 10 nF | présente indépendamment de VCC selon essai | 5–40 V requis par NXP ; 12 V nominal ici | inversion bloquée ; courant limité ; aucun essai batterie véhicule |

### DNP imposés

- `R_LINK_TX` : aucune pièce et aucun header ; les deux pads restent nus ;
- `D_CAN` : TVS réservé, non sélectionné pour LS/FT ;
- `L_CAN` : choke réservé, non sélectionné sans mesure EMC ;
- aucun 120 Ω entre H et L ;
- aucune liaison INH ;
- aucun GPIO sur `/OE`, STB ou EN dans cette variante.

## Démonstration de la propriété RX-only

### Chemin intentionnel

Le trajet logique potentiel serait :

```text
GPIO5 -> U2.A -> U2.Y -> R_LINK_TX -> U1.TXD -> drivers CANH/CANL
```

Il est interrompu deux fois :

1. `/OE` est tiré vers U2.VCC, donc U2.Y est haute impédance ;
2. `R_LINK_TX` n'existe physiquement pas, donc U2.Y et U1.TXD sont deux nets
   séparés.

U1.TXD n'a qu'une source montée : `R_TXD=10 kΩ` vers son propre VCC 5 V. Un
état LOW du GPIO5, une panne firmware, un reset TWAI ou une sortie U2
inattendue ne possède aucun chemin galvanique jusqu'à TXD.

### Power-up et ordre aléatoire

| Situation | U2 | U1.TXD | U1 mode | Conséquence attendue |
|---|---|---|---|---|
| toutes alimentations absentes | Ioff / sans alimentation | sans 5 V | hors alimentation | aucun dominant soutenu possible |
| 12 V BAT seul | U2 éteint | VCC absent | forced standby / low-power | nœud bus non alimenté côté VCC ; aucun TX |
| 5 V présent, 3,3 V absent | U2 éteint, Ioff | tiré HIGH vers 5 V | STB/EN LOW | veille, TXD récessif |
| 3,3 V présent, 5 V absent | U2 `/OE` HIGH ; Y isolé | VCC absent | forced standby | aucune liaison par `R_LINK_TX`; entrées U1 non pilotées par 3,3 V |
| 5 V + 3,3 V, cavalier absent | U2 Y=Z | HIGH | standby | pas de RX complet, aucun TX |
| 5 V + 3,3 V, cavalier présent | U2 Y=Z | HIGH | normal | RX complet, driver jamais sollicité |
| reset/brownout ESP32 | U2 `/OE` reste passif via résistance | HIGH depuis 5 V | inchangé ou standby selon 5 V | aucun GPIO ne contrôle TXD/STB/EN |
| perte 3,3 V en acquisition | U2 Ioff ; RXD/ERR pulls disparaissent | HIGH depuis 5 V | normal tant que cavalier et 5 V présents | U1 peut recevoir mais ne peut pas émettre |
| perte 5 V avec BAT présent | U1 force STB/EN LOW sous VCC(stb) | non pertinent | forced standby | aucun flux RX complet, aucun TX |

### Clamps, ESD et couplage parasite

TI documente `Ioff` sur U2 et des entrées tolérant 5,5 V. NXP documente
l'absence de chemin de courant inverse sur son interface microcontrôleur et
RXD/ERR open-drain pour `/3`. Surtout, le pad DNP supprime la continuité : les
diodes de clamp d'U2 ou de l'ESP32 ne peuvent pas fournir de courant continu à
TXD.

Un circuit réel possède toujours une capacité parasite. Pour qu'elle ne
devienne pas une impulsion mesurable, le routage impose 2,54 mm de séparation,
aucune piste parallèle et aucun cuivre entre les deux pads. Le pull-up TXD de
10 kΩ absorbe la charge couplée. Cette analyse ne remplace pas la mesure : boot,
reset, branchements dans tous les ordres et brownout doivent être observés au
scope sur TXD, CANH et CANL. Tout pulse dominant vaut FAIL.

Le timeout dominant interne du TJA1055 ne compte jamais comme barrière : il
intervient après le début d'une émission indésirable.

## RTH, RTL et charge ajoutée

RTH et RTL ne forment pas une résistance de 120 Ω entre H et L. Chacun relie
sa ligne de bus au circuit de terminaison/biais interne correspondant. En mode
normal sain, ils participent au retour récessif ; en défaut monofil, le TJA1055
peut changer la terminaison de la ligne défaillante. La symétrie de la paire est
importante pour les transitions récessives et les émissions CEM.

NXP vise environ 100 Ω **par ligne pour le parallèle de tout le réseau**. Les
nœuds optionnels peuvent avoir une résistance locale plus élevée ; l'application
note recommande de ne pas dépasser environ 6 kΩ. Elle demande un appariement
RTH/RTL à 1 % ou mieux. Le manuel PEAK emploie 560 Ω pour un petit banc et
5,66 kΩ pour surveiller un réseau existant déjà terminé.

La valeur gelée est 5,62 kΩ, valeur E96 disponible, en deux résistances 0,1 % du
même lot. Elle est sous la recommandation d'environ 6 kΩ et minimise la charge
d'un nœud ajouté.

Si le réseau existant équivaut réellement à 100 Ω par ligne :

```text
R_nouveau = 100 || 5620 = 98,25 ohms
variation = -1,75 %
```

Pour une excursion approximative de 4 V sur chaque branche au dominant :

```text
I_par_branche = 4 V / 5620 ohms = 0,712 mA
charge différentielle additionnelle approximative = 1,42 mA
```

Ce calcul est un ordre de grandeur, pas une mesure de la BMW : sa terminaison
totale, le nombre de nœuds, la topologie et l'état de veille restent inconnus.
Les 5,62 kΩ sont montés sur le récepteur pour le banc et sont le seul candidat
actuel à une future observation d'un réseau existant. Une connexion véhicule
reste néanmoins interdite tant que l'effet normal/veille n'a pas été mesuré et
que le schéma BMW exact n'a pas identifié un point réversible.

Pour un petit banc, le **générateur** ou un terminator externe peut employer
560 Ω appariés. Ne jamais remplacer les 5,62 kΩ du sniffer par 560 Ω avant une
future connexion véhicule : cela multiplierait sa charge locale par environ
dix. Ne jamais ajouter 120 Ω H-L.

## Alimentation et protections

Le prototype sépare trois domaines :

- USB/5 V du DevKit, avec branche `+5V_TJA` protégée par PTC ;
- 3,3 V logique produit par le DevKit ;
- 12 V BAT de laboratoire, protégé par PTC 100 mA, diode Schottky 60 V,
  `R_BAT=1 kΩ` et 10 nF au plus près de U1.

NXP recommande 1–2 kΩ en série avec BAT et environ 10 nF local pour la tenue
aux transitoires. Le 22 µF sur VCC est volontairement supérieur au simple
100 nF de la Phase 3E et fournit une réserve locale ; l'application note montre
un découplage de l'ordre de 20 µF pour les appels de courant du transceiver.

Le TJA1055 intègre une tenue de bus ±58 V et est qualifié automobile. Cela ne
justifie pas de choisir arbitrairement un TVS ou un choke. Les empreintes
correspondantes restent DNP jusqu'à sélection à partir des impulsions visées,
de la capacité admissible et d'essais. Le prototype de banc ne prétend donc pas
encore satisfaire ISO 7637, load dump, ESD d'un faisceau réel ou exigences OEM.

## Moyens légitimes de créer un banc LS/FT

Prix indicatifs observés en octobre 2026, hors port et généralement hors TVA.
Ils doivent être revérifiés avant commande.

| Option | ISO 11898-3 / 100 kbit/s | Génération / ACK | Documentation | Coût indicatif | Décision |
|---|---|---|---|---:|---|
| second ESP32-S3 + TJA1055 actif, strictement banc | oui, si configuré à 100 kbit/s | oui ; retrait du second nœud pour no-ACK, scope nécessaire | composants documentés mais outil à qualifier soi-même | 30–60 € | **minimum économique ultérieur**, aucun code TX dans ce dépôt RX-only |
| PEAK `PCAN-USB IPEH-002021` + `PCAN-TJA1054 IPEH-002039` | oui, jusqu'à 125 kbit/s ; réglage 100 kbit/s côté PCAN | trames actives, ACK et erreurs observables avec PCAN-View/API + scope | excellente ; convertisseur à TJA1055, 560 Ω/5,66 kΩ | 199 + 112 = **311 € HT** | **référence recommandée pour qualification avancée** |
| HMS/Ixxat `USB-to-CAN V2 automotive 1.01.0283.22042` | oui, 10–125 kbit/s | interface active PC, deux CAN, LS commutable | constructeur complète, drivers Windows/Linux | ~948 € HT | valide mais disproportionné pour commencer |
| ICP DAS `I-7530-FT-G` | ISO 11898-3, mais débits publiés 10/20/50/125 kbit/s | transmission via RS-232, résistance 1 kΩ intégrée | correcte | ~300 € | **rejeté pour ce banc : 100 kbit/s non documenté** |
| Vector VN16xx + `CANcable 1055cap` | oui avec le piggyback/câble exact | fonctions professionnelles CANoe/CANalyzer | excellente | devis, généralement très élevé | laboratoire professionnel uniquement |
| adaptateur USB HS-CAN, MCP2515+TJA1050, CANable HS | non, ISO 11898-2 | sans objet | variable | 15–100 € | **interdit comme équivalent K-CAN** |

Le `PCAN-TJA1054` n'est pas nécessaire pour souder, vérifier les rails, tester
les barrières ou développer le traitement hôte. Il devient utile pour produire
un trafic LS/FT reproductible. Il fonctionne toujours en mode normal sur son
côté LS et peut influencer un véhicule endormi ; cette recommandation ne vaut
que pour le banc isolé.

Pour prouver l'absence d'ACK : générateur actif + DUT RX-only, sans autre nœud
actif, puis observation du slot ACK et des retries. Ensuite ajouter un second
nœud actif : l'ACK doit réapparaître. Le résultat doit être confirmé sur les
niveaux analogiques, pas seulement par un compteur logiciel.

## Budget et achat échelonné

L'audit d'approvisionnement du 1er octobre 2026 est documenté dans
[`phase3f-procurement-audit.md`](phase3f-procurement-audit.md). Il a
corrigé les adaptateurs CMS, les composants obsolètes, la révision du DevKit,
la carte prototype et les points de test sans modifier la topologie électrique.

Le minimum absolu des composants effectivement utilisés est d'environ
**39,17 € HT**. La commande conseillée de `procurement.csv`, avec rechanges et
marge de soudure, atteint **64,49 € HT hors port**. Une alimentation limitée et
un multimètre déjà possédés réduisent fortement le coût initial ; sinon prévoir
environ **105 € supplémentaires** selon les estimations de la BOM.

### Commander maintenant

- ESP32-S3-DevKitC-1-N8R8 officiel ;
- deux `TJA1055T/3/2Z` (un utilisé, un rechange) ;
- trois `CLVC1G125QDBVRQ1G4` disponibles, suffixe d'achat explicitement revu
  du MPN de base `CLVC1G125QDBVRQ1` ;
- passifs, PTC, diode, connecteurs, adaptateurs SO14/SOT23, perfboard et points
  de test de `procurement.csv` ;
- alimentation 12 V limitée en courant et DMM uniquement si absents.

### Attendre

- PCAN/Ixxat/Vector ou autre interface LS/FT coûteuse ;
- oscilloscope si l'objectif immédiat est seulement l'assemblage hors tension,
  mais il sera obligatoire avant de déclarer le banc PASS ;
- TVS, choke et PCB automobile ;
- boîtier véhicule, connecteur BMW, faisceau OBD ou point d'épissure ;
- tout actionneur et toute alimentation automobile finale.

## Outils

### Absolument nécessaires pour assembler et alimenter

- fer à souder à température contrôlée, flux, tresse et loupe ;
- tapis/bracelet ESD ;
- DMM avec continuité, résistance et tensions DC ;
- alimentation 12 V régulée avec affichage et limitation de courant ;
- câble USB de données et PC pour le DevKit ;
- fils JST XH `ASXHSXH22K152` présertis ; les contacts nus ne sont pas achetés
  pour ce prototype afin de ne pas dépendre d'une pince non qualifiée.

### Nécessaires pour obtenir le PASS électrique

- oscilloscope deux voies, 20 MHz minimum, 100 MHz recommandé, deux sondes x10
  et ressorts de masse courts ;
- générateur/nœud actif ISO 11898-3 réellement configurable à 100 kbit/s ;
- second nœud actif amovible ou configuration permettant de comparer ACK et
  no-ACK ;
- alimentation à rampe contrôlable ou montage permettant brownout et ordres
  d'alimentation reproductibles.

### Recommandés mais non obligatoires au début

- analyseur logique pour corréler GPIO/RXD avec le scope analogique ;
- isolation USB pour essais proches d'un système alimenté séparément ;
- caméra/loupe numérique pour archiver les DNP et soudures ;
- LCR-mètre et charge électronique ;
- interface LS/FT commerciale PEAK pour reproductibilité et support logiciel.

## Plan de qualification, sans voiture

Chaque essai archive photo du DNP, schéma de câblage, réglages instruments,
captures et verdict PASS/FAIL.

1. inspection visuelle et audit contre `netlist.csv` ;
2. mesures hors tension, dont absence de continuité U2.Y–U1.TXD ;
3. BAT seul, 5 V seul, 3,3 V seul lorsque réalisable sans dépasser les limites,
   puis tous les ordres d'apparition et disparition ;
4. boot et reset DevKit avec `JP_RX_MODE` absent ;
5. insertion/retrait du cavalier RX, vérification STB/EN et TXD ;
6. brownout progressif 5 V et 12 V ;
7. trafic LS/FT 100 kbit/s ; mauvais bitrate côté TWAI ; saturation RX ;
8. observation de CANH/CANL/TXD pendant boot, reset, watchdog et coupures ;
9. essai no-ACK, puis ajout d'un nœud actif et retour de l'ACK ;
10. défauts de ligne contrôlés uniquement si le générateur et la méthode les
    supportent sans danger.

PASS exige : TXD toujours HIGH lorsque U1 peut être en mode normal, aucun pulse
dominant produit par le DUT, aucun ACK, réception ordonnée, pulls/modes conformes
et aucune anomalie d'alimentation. Toute ambiguïté est FAIL. Même après PASS,
une revue séparée du schéma BMW et du comportement de veille sera requise avant
une éventuelle proposition de connexion réversible.

## Ce qui devient possible à réception de l'ESP32 et du TJA1055

- assembler et inspecter le récepteur ;
- vérifier continuité, DNP, rails et états de sécurité ;
- compiler/flasher le firmware listen-only existant ;
- injecter RXD avec un signal logique de test sans connecter le bus ;
- vérifier le chemin RX hôte et les buffers avec des données synthétiques ;
- préparer les scripts et manifests Capture V2 hors véhicule.

Ce qui reste bloqué sans scope et générateur LS/FT : preuve d'absence de pulse,
preuve d'absence d'ACK, validation à 100 kbit/s analogique, qualité des fronts,
impact de 5,62 kΩ, défauts monofil et qualification brownout sous trafic.

Ce qui reste bloqué même après le banc : tout branchement BMW, tout ID ou bit
K-CAN présenté comme réel, tout déclenchement par télécommande, toute émission,
tout remote-start et la Phase 4 complète.

## Références

- [NXP TJA1055, fiche technique Rev. 5](https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf)
- [NXP AH0801, Application Hints TJA1055T Rev. 1.5](https://www.nxp.com/docs/en/application-note/AH0801.pdf)
- [NXP, statut produit TJA1055T](https://www.nxp.com/products/TJA1055T)
- [TI SN74LVC1G125-Q1, fiche technique](https://www.ti.com/lit/ds/symlink/sn74lvc1g125-q1.pdf)
- [Espressif ESP32-S3, restrictions GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-reference/peripherals/gpio.html)
- [Espressif ESP32-S3-DevKitC-1, guide](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1-v1.0.html)
- [PEAK PCAN-TJA1054, manuel 3.0.0](https://www.peak-system.com/produktcd/Pdf/English/PCAN-TJA1054_UserMan_eng.pdf)
- [PEAK PCAN-TJA1054, page produit](https://www.peak-system.com/products/hardware/couplers-converters/pcan-tja1054/)
- [PEAK, tarif 2026](https://www.peak-system.com/produktcd/Catalogs/Pricelist.pdf)
- [HMS/Ixxat USB-to-CAN V2 automotive](https://www.hms-networks.com/p/1-01-0283-22042-ixxat-usb-to-can-v2-automotive)
- [ICP DAS I-7530-FT, fiche technique](https://www.icpdas.com/web/product/download/industrial_communication/can_bus/converter/uart/i-7530/i-7530/i-7530-ft%20cr/data_sheet/I-7530-FT_en.pdf)
- [Vector, compatibilité des transceivers](https://support.vector.com/sys_attachment.do?sys_id=bf96128e83e7fad0af3198747daad345)

Les prix servent uniquement à décider l'ordre d'achat. Les propriétés
électriques et les décisions de sécurité reposent sur les documents
constructeur ci-dessus.
