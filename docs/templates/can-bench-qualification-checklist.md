# Checklist de qualification CAN de banc — Phase 3B

Copier ce fichier dans un dossier de session privé avec les captures
d'oscilloscope. Une case vide ou `NOT RUN` interdit le statut global PASS.

## Identification

| Champ | Valeur |
|---|---|
| Session | |
| Date / opérateur | |
| Révision PCB | |
| DevKitC-1 / module | |
| U1 TCAN1057AV lot/marquage | |
| U2 SN74LVC1G125-Q1 lot/marquage | |
| Firmware / commit | |
| Configuration BENCH_ONLY | |
| Oscilloscope / sondes / calibration | |
| Générateur CAN | |
| Température ambiante | |

## Inspection et alimentation

| Test | Preuve | PASS/FAIL |
|---|---|---|
| Inspection, polarités et continuité | | |
| R1/R2/R3/R4/R5/R6 mesurées | | |
| Terminaison ouverte/120 Ω/60 Ω | | |
| VCC 4,5–5,5 V | | |
| VIO 3,2–3,4 V | | |
| Courant stable, aucune chauffe | | |

## Barrières

| État | S haut | /OE haut | TXD haut | Aucun dominant | PASS/FAIL |
|---|---|---|---|---|---|
| Avant initialisation firmware | | | | | |
| Firmware absent / ESP maintenu reset | | | | | |
| Boot nominal ×10 | | | | | |
| Reset ×100 | | | | | |
| Extinction ×10 | | | | | |
| Rampe lente brownout | | | | | |
| Creux 1/10/100/1000 ms | | | | | |
| Watchdog ×100 | | | | | |

## Bus

| Test | RXD suit le bus | Aucun ACK/erreur/dominant | Preuve | PASS/FAIL |
|---|---|---|---|---|
| 100 kbit/s, 10 min | | | | |
| 500 kbit/s, 10 min | | | | |
| Single-shot sans autre ACK | | | | |
| Récepteur 100 / bus 500 | | | | |
| Récepteur 500 / bus 100 | | | | |
| Erreurs de bit ×1000 | | | | |
| Erreurs de stuffing ×1000 | | | | |
| Erreurs CRC ×1000 | | | | |
| Saturation RX 500 kbit/s, 30 min | | | | |
| Nœud hors tension sur bus actif | | | | |

## Conclusion

- [ ] Toutes les lignes sont PASS.
- [ ] Les fichiers bruts et captures sont archivés.
- [ ] Aucun pulse CANH−CANL ≥0,5 V / ≥50 ns attribuable au nœud.
- [ ] Aucune trame, aucun ACK et aucun flag d'erreur émis.
- [ ] Deux relecteurs ont signé les résultats.

| Rôle | Nom | Date | Signature / référence de revue |
|---|---|---|---|
| Opérateur | | | |
| Relecteur matériel | | | |
| Relecteur sûreté | | | |

**Résultat global :** `FAIL` tant que chaque condition ci-dessus n'est pas
explicitement validée. Ce document ne délivre jamais à lui seul une autorisation
de connexion au véhicule.
