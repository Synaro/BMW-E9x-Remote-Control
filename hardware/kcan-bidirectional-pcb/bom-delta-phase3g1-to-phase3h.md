# Delta BOM Phase 3G.1 → Phase 3H

## Supprimé

- `CLVC1G125QDBVRQ1` (ancien U2 mono-alimentation) ;
- `R3` 10 kΩ, ancien pull-up `/OE` ;
- `JP1` `TSW-102-07-G-S` ;
- `SH_RX_MODE` `SNT-100-BK-G` ;
- `R_LINK_TX` DNP / concept `PHYSICAL_GAP` ;
- testpoints historiques `TP4` (OE) et `TP5` (GATE_Y), l'ancien `TP6` étant
  remplacé par le nouveau `TP5` TXD.

## Ajouté ou remplacé

- U2 devient `SN74LXC1T45QDCKRQ1`, traducteur automobile double alimentation
  SC70-6 ;
- `C7` 100 nF pour `VCCB=5V_TJA` ;
- `C8` 100 nF pour `VCCA=3V3` ;
- deux connexions directes GPIO vers `STB` et `EN` au travers de R10/R12.

## Inchangé

U1 `TJA1055T/3/2Z`, U3 `TLS715B0EJV50XUMA1`, l'arbre d'alimentation, la
protection d'entrée, la terminaison RTH/RTL, les sockets DevKit, les
connecteurs et l'optimisation DFM U3 restent inchangés.

## Impact estimé

Sur la base des prix unitaires de revue, et hors frais d'assemblage :

- ancien U2 : environ 0,72 € ;
- nouveau U2 : environ 0,74 € ;
- un condensateur 100 nF supplémentaire : quelques centimes ;
- suppression de R3, JP1 et du shunt : économie d'environ 0,50 à 1,00 €.

Le delta matière estimé est donc légèrement négatif, de l'ordre de
**−0,4 à −0,9 € par carte**. Ce n'est pas un devis fabricant ; les prix et la
disponibilité doivent être revérifiés avant tout achat.
