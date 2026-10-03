# Phase 3E — Préparation de l'acquisition K-CAN passive

## Portée et décision

Cette phase prépare un **récepteur K-CAN de banc** pour la BMW E8x/E9x. Elle
n'autorise encore aucune connexion au véhicule. Elle n'ajoute ni émission CAN,
ni rejeu, ni décodeur BMW, ni commande CAS/DDE/KL50, ni action de démarrage.

La décision de conception est la suivante :

- couche logique : CAN classique à **100 kbit/s** pour le véhicule de test ;
- contrôleur : TWAI interne de l'ESP32-S3, exclusivement en
  `TWAI_MODE_LISTEN_ONLY` ;
- couche physique : transceiver **low-speed/fault-tolerant ISO 11898-3** ;
- candidat principal : **NXP `TJA1055T/3/2Z`** ;
- le banc PT-CAN `TCAN1057AV-Q1` reste séparé et n'est pas compatible avec la
  couche physique K-CAN ;
- réception complète en mode normal du transceiver, avec TXD rendu physiquement
  inaccessible depuis l'ESP32.

Le TJA1055 ne possède pas de mode qui combine émetteur désactivé et flux RX
complet. En veille, RXD signale seulement un réveil. Par conséquent, nommer sa
veille « silent » et l'utiliser comme récepteur serait faux. La sûreté de cette
architecture repose sur deux coupures TX matérielles plus le mode listen-only
du contrôleur, pas sur une fonction inexistante du transceiver.

## K-CAN E8x/E9x et séparation des couches

Les documents de formation BMW décrivent le K-CAN de cette génération comme
un bus deux fils, 100 kbit/s, capable d'un fonctionnement dégradé sur un seul
fil. Les niveaux approximatifs observables sont différents du PT-CAN : au repos
K-CAN_H est proche de 0 V et K-CAN_L de 5 V ; en dominant K-CAN_H monte vers
4 V et K-CAN_L descend vers 1 V. La terminaison est distribuée entre les
calculateurs. Il ne faut donc ajouter **aucune résistance de 120 ohms entre
CAN-H et CAN-L**.

```text
100 kbit/s          CAN classique           ISO 11898-3
débit logique  ->   contrôleur TWAI   ->    transceiver LS/FT   -> K-CAN
                    (ESP32-S3)               (TJA1055T/3)

500 kbit/s          CAN classique           ISO 11898-2
débit logique  ->   contrôleur TWAI   ->    TCAN1057AV-Q1       -> PT-CAN banc
```

Un débit accepté par TWAI ne rend pas un transceiver haute vitesse compatible
avec la polarisation, les niveaux et la terminaison du K-CAN.

## Comparaison des transceivers

Disponibilité vérifiée le 1er octobre 2026 ; elle doit être vérifiée de nouveau
au moment de l'achat.

| Référence | Interface logique | Mode RX sans TX | Hors alimentation / reset | Disponibilité observée | Décision |
|---|---|---|---|---|---|
| NXP `TJA1055T/3/2Z` | VCC 5 V ; RXD/ERR open-drain avec pulls 3,3 V ; entrées compatibles 3,3 V | Aucun mode silent avec données complètes ; normal requis | nœud non alimenté non perturbant ; sous-tension VCC force le mode basse consommation | NXP `Active`, stock distributeur autorisé observé | **retenu pour le prototype** |
| Analog Devices `MAX3055ASD+` | VCC 5 V ; entrées acceptent 3,3 V, mais RXD/ERR sortent près de 5 V | Aucun mode silent avec données complètes | nœud non alimenté non perturbant ; standby/sleep | `Production` chez ADI | secours, exige buffer/translation 5 V vers 3,3 V |
| onsemi `AMIS-41683` | VCC 5 V ; vraie interface contrôleur 3,3 V, RXD/ERR open-drain | Aucun mode silent avec données complètes | défaut d'alimentation force un mode basse consommation | obsolète et zéro stock chez un distributeur autorisé vérifié | techniquement adapté, écarté pour obsolescence |
| NXP `TJA1054A` | logique 5 V | Aucun mode silent avec données complètes | génération antérieure | ancien / disponibilité moins favorable | non retenu : aucun avantage sur TJA1055 |

Les transceivers TI et Microchip actuellement documentés et disponibles sont
principalement ISO 11898-2/CAN FD haute vitesse. Leur mention « fault
protected » ne signifie pas ISO 11898-3 ; ils ne remplacent pas un transceiver
low-speed/fault-tolerant. Le `TLE6255` est un transceiver CAN **monofil** et ne
convient pas non plus au K-CAN deux fils.

### Pourquoi le TJA1055T/3/2Z

Il correspond directement à ISO 11898-3, accepte jusqu'à 125 kbit/s, gère le
mode dégradé monofil, protège les broches bus et ne perturbe pas le bus lorsqu'il
n'est pas alimenté. La variante `/3` permet de tirer RXD et ERR vers 3,3 V,
donc de les raccorder sans translation au domaine logique de l'ESP32-S3.

Ses limites restent explicites : VCC 5 V et BAT sont nécessaires, la référence
est ancienne, sa réception de trames exige le mode normal et sa terminaison
distribuée charge réellement le bus. Le choix est donc **candidat matériel de
banc**, pas un câblage véhicule validé.

## Architecture RX-only proposée

```text
                       +3V3
                         |
                      3,3 kΩ
                         |
TJA1055T/3 RXD ----------+----------------> ESP32 GPIO / TWAI_RX

                       +3V3
                         |
                      3,3 kΩ
                         |
TJA1055T/3 ERR ----------+----------------> ESP32 GPIO / défaut RX

ESP32 GPIO / TWAI_TX ---> U2 SN74LVC1G125-Q1 ---> R_LINK_TX DNP --X--> TXD
                              /OE pull-up 10 kΩ                 |
                                                               +-- 10 kΩ -> +5V_TJA

RX_MODE jumper +5 V --1 kΩ--> STB --10 kΩ--> GND
                    `--1 kΩ--> EN  --10 kΩ--> GND

alimentation banc 5 V ----------------------> VCC (+ 100 nF local)
alimentation banc 12 V limitée en courant --> BAT
WAKE inutilisé --------------------------------> BAT

bus LS/FT CAN-H ---- protection/mesure ---- RRTH 5,62 kΩ 1 % ---- RTH
bus LS/FT CAN-L ---- protection/mesure ---- RRTL 5,62 kΩ 1 % ---- RTL
```

`R_LINK_TX` est **non monté** dans la variante RX-only. Il ne s'agit pas d'un
strap logiciel : une inspection visuelle et une mesure de continuité doivent
confirmer l'ouverture. La sortie de U2 constitue une première barrière,
maintenue en haute impédance par `/OE=HIGH`. L'absence du lien vers TXD est une
seconde barrière indépendante. Enfin, 10 kΩ vers le VCC 5 V local maintient
TXD au niveau récessif même avec ESP32 absent, 3,3 V perdu, firmware planté ou
U2 défectueux côté commande. Le pull-up 3,3 V initialement envisagé a été
rejeté lors du gel Phase 3F car il ne garantissait pas TXD HIGH si le seul
domaine 3,3 V disparaissait.

STB et EN sont tirés vers le bas séparément. En Phase 3F, aucun GPIO ne les
pilote : un cavalier physique commun les relie au 5 V via deux résistances de
1 kΩ. Cavalier absent, le transceiver reste en veille au boot, reset, brownout
ou ESP32 absent. Cavalier présent, il reçoit le flux complet sans rétablir de
trajet TX. Le mode listen-only TWAI et sa file TX nulle restent obligatoires.

Le schéma ci-dessus reste une synthèse. Le câblage normatif et la nomenclature
sont désormais ceux de [phase3f-kcan-rxonly-design-freeze.md](phase3f-kcan-rxonly-design-freeze.md)
et de `hardware/kcan-rxonly/` ; en cas de divergence, la Phase 3F prévaut.

Les 5,62 kΩ correspondent à la terminaison faible d'un nœud optionnel recommandée
par NXP et utilisée par des outils LS-CAN. Deux empreintes appariées à 1 % sont
prévues. Leur impact doit être mesuré sur banc. Une valeur BMW observée ailleurs
ou un montage DNP ne doit pas être substitué sans nouvelle revue : NXP demande
normalement une terminaison locale entre CANH/RTH et CANL/RTL.

Points de mesure obligatoires : `3V3`, `5V`, `BAT`, `RESET`, `TWAI_TX`, sortie
U2, `TXD_TRANSCEIVER`, `RXD`, `ERR`, `STB`, `EN`, `CANH`, `CANL` et masse.

## Incompatibilité avec la configuration firmware actuelle

Le profil `config/bench-twai.local.hpp.example` décrit uniquement le banc
PT-CAN de Phase 3B : sa broche `S` place le TCAN1057AV en silent. Le réutiliser
pour le K-CAN en prétendant que STB/EN sont une barrière silent serait une
fausse preuve de sûreté. Il reste donc inchangé et désactivé.

Avant toute acquisition K-CAN par l'ESP32, il faudra une configuration
`BENCH_ONLY` distincte qui sache confirmer :

1. U2 inhibé ;
2. lien TX physiquement absent selon la variante de carte ;
3. TXD transceiver récessif ;
4. seulement ensuite, activation RX par STB/EN et installation de TWAI en
   listen-only.

Cette adaptation firmware n'est pas développée avant la présence et la
qualification du matériel. Aucun booléen logiciel ne doit simuler le contrôle
du lien DNP.

## Qualification électrique obligatoire sur banc

### Matériel de génération

Le bus de test doit comporter un générateur LS/FT 100 kbit/s indépendant et,
pour les essais d'ACK, un second nœud actif amovible. Un couple PCAN-USB en mode
approprié avec convertisseur `PCAN-TJA1054`, ou deux cartes LS/FT maîtrisées,
peut générer le trafic de banc. Ce convertisseur n'est pas automatiquement un
sniffer véhicule qualifié : son manuel indique que son transceiver LS reste en
mode normal et que sa terminaison influence le réseau.

### Essais

1. inspection hors tension, valeurs et absence de `R_LINK_TX` ;
2. continuité : aucune liaison électrique entre sortie U2 et TXD transceiver ;
3. alimentation 3,3/5/12 V limitée en courant, puis extinction et branchement
   dans tous les ordres ;
4. boot, reset manuel, watchdog, brownout progressif et firmware absent ;
5. vérification de TXD récessif et absence de pulse dominant sur CANH/CANL ;
6. trafic LS/FT 100 kbit/s avec réception ordonnée en TWAI listen-only ;
7. générateur seul + récepteur testé, sans autre nœud ACK : aucun dominant dans
   le slot ACK et le générateur doit constater l'absence d'ACK ;
8. ajout du second nœud actif : l'ACK réapparaît, ce qui valide la méthode de
   mesure sans l'attribuer au sniffer ;
9. mauvais débit TWAI, saturation RX, reset en trafic et erreurs de ligne ;
10. essais de veille/réveil et mesure de l'influence des 5,62 kΩ sur les
    niveaux, les erreurs et le courant du réseau de banc.

### PASS / FAIL

PASS exige simultanément : lien TX ouvert mesuré et photographié, `/OE` inhibé
au boot/reset/brownout, TXD transceiver toujours récessif, aucune trame émise,
aucun ACK, aucun pulse dominant indésirable, réception 100 kbit/s correcte,
aucune erreur nouvelle attribuable au sniffer et traces oscilloscope archivées.
Un seul écart vaut FAIL. Le timeout dominant interne du TJA1055 n'est jamais
accepté comme barrière : il intervient après une émission indésirable.

## Protocole futur de captures télécommande

Ce protocole reste interdit tant que le banc n'a pas obtenu PASS et qu'un point
de raccordement K-CAN réversible n'a pas été identifié dans la documentation
du véhicule.

### Scénarios

Chaque essai part d'un état physique consigné et restauré. Capturer au minimum
dix répétitions valides par scénario, plus dix contrôles sans action :

| Code | Action unique pendant la fenêtre |
|---|---|
| `REST_NO_ACTION` | aucune action, même durée |
| `LOCK_1` | un appui verrouillage |
| `LOCK_2` | deux appuis verrouillage, intervalle noté |
| `LOCK_3` | trois appuis verrouillage, intervalles notés |
| `UNLOCK_1` | un appui déverrouillage |

Faire une campagne véhicule réveillé et une campagne départ repos uniquement
si l'effet du transceiver sur la veille K-CAN a d'abord été mesuré. Les appuis
doivent être consignés hors bus avec l'heure locale de l'opérateur ; aucune
trame marqueur ne doit être injectée.

### Acquisition et analyse

- capturer **tous** les identifiants standard, sans filtre `0x23A/0x2B4` ;
- utiliser 100000 dans Capture V2, direction exclusivement `RX` ;
- identifier le manifeste comme couche `KCAN_LSFT_ISO11898_3_RX_ONLY` dans
  `interface`/`configuration_id` et conserver référence carte/transceiver ;
- mêmes durées, conditions de portières, contact, éclairage et délai depuis le
  précédent scénario ;
- ordre des scénarios randomisé pour limiter les effets du temps et du réveil ;
- comparer présence, cadence, DLC et distribution de chaque octet/bit ;
- rechercher les changements reproductibles propres à `LOCK_1`, `LOCK_2`,
  `LOCK_3` et `UNLOCK_1`, puis vérifier le retour/inverse et les contrôles nuls ;
- exiger répétitions et contrôles négatifs avant toute promotion en candidat.

Les affirmations externes `0x23A` et `0x2B4` sont conservées dans
`vehicle-data/catalog/external-kcan-hypotheses.json`. Elles servent uniquement
à classer des résultats après analyse complète. Une absence ou une différence
sur notre E90 ne constitue pas une anomalie.

## Inconnues restant à mesurer

- effet exact de la terminaison 5,62 kΩ sur le réseau K-CAN de cette E90 ;
- comportement de veille et possibilité de conserver les toutes premières
  trames après un réveil sans maintenir le véhicule éveillé ;
- point de raccordement et polarité confirmés par schéma/mesure sur le véhicule ;
- capacité du matériel assemblé à recevoir sans ACK dans tous les transitoires ;
- IDs, DLC, octets, bits, compteurs et cadence associés aux appuis réels ;
- distinction entre événement télécommande, changement d'état serrure et
  acquittement de verrouillage.

## Références

- [BMW Technical Training ST401 — Body Electronics II](https://bmwtechinfo.bmwgroup.com/tech_training_manual/ST401%20Body%20Electronics%20II.pdf)
- [NXP TJA1055 — statut produit et variantes actives](https://www.nxp.com/products/TJA1055T)
- [NXP TJA1055 — fiche technique](https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf)
- [NXP AH0801 — Application Hints TJA1055T](https://www.nxp.com/docs/en/application-note/AH0801.pdf)
- [Analog Devices MAX3055](https://www.analog.com/en/products/max3055.html)
- [onsemi AMIS-41682/41683 — fiche technique](https://www.onsemi.com/download/data-sheet/pdf/amis-41682-d.pdf)
- [DigiKey France TJA1055T/3/2Z — disponibilité observée](https://www.digikey.fr/fr/products/detail/nxp-usa-inc/TJA1055T-3-2Z/1966456)
- [Mouser France — recherche AMIS41683, statut obsolète observé](https://www.mouser.fr/c/?q=AMIS41683)
- [PEAK PCAN-TJA1054](https://www.peak-system.com/products/hardware/couplers-converters/pcan-tja1054/)
- [Dépôt communautaire source des hypothèses](https://github.com/llilakoblock/bmw-e87-e90-can-bt/tree/8da7364a86b2f3e487a61c45a958f1afbe5de4d8)
